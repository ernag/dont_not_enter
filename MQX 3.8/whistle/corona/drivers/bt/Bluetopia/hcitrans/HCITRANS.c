/*****< hcitrans.c >***********************************************************/
/*      Copyright 2012 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  HCITRANS - HCI Transport Layer for use with Bluetopia.                    */
/*                                                                            */
/*  Author:  Marcus Funk                                                      */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   11/08/12  M. Funk        Initial creation.                               */
/******************************************************************************/
#include <mqx.h>
#include <bsp.h>
#include <fio.h>
#include "wassert.h"
#include "debug_led.h"
#include "BTPSKRNL.h"
#include "cormem.h"

#include "HCITRANS.h"
#include "corona_console.h"

#define DRIVER_DEV 0
#if DRIVER_DEV == 1
    #define DMA_BUFFER_SIZE 256
//    #define NUM_DMA_BUFFERS (5190 / (DMA_BUFFER_SIZE + sizeof(QUEUE_ELEMENT_STRUCT) + 4))
    #define NUM_DMA_BUFFERS 10
    #define DRIVER_DEFRAG 0
    #define DRIVER_RX_DMA_CONFIG (DMA_CSR_DREQ_MASK | DMA_CSR_INTHALF_MASK | DMA_CSR_INTMAJOR_MASK)
    #define DRIVER_TX_DMA_CONFIG (DMA_CSR_DREQ_MASK | DMA_CSR_INTMAJOR_MASK)
    #define dev_print(...) corona_print(__VA_ARGS__)
    #define dev_led(led, on) set_debug_led(led, on)
    #define dev_stat_init() memset(&uart_stats, 0, sizeof(uart_stats))
    #define dev_stat(stat) stat
#else
    #define DMA_BUFFER_SIZE 256
    #define NUM_DMA_BUFFERS 10
    #define DRIVER_DEFRAG 0
    #define DRIVER_RX_DMA_CONFIG (DMA_CSR_DREQ_MASK | DMA_CSR_INTHALF_MASK | DMA_CSR_INTMAJOR_MASK)
    #define DRIVER_TX_DMA_CONFIG (DMA_CSR_DREQ_MASK | DMA_CSR_INTMAJOR_MASK)
    #define dev_print(...) {}
    #define dev_led(...) {}
    #define dev_stat_init() {}
    #define dev_stat(stat) {}
#endif


/* UART Hardware */
#define UART_BASE                    UART0_BASE_PTR
#define UART_MUX                     3
#define GPIO_MUX                     1
#define UART_TX_FIFO_THRESHOLD       7
#define UART_RX_FIFO_THRESHOLD       2
#define UART_RX_TX_VECTOR            INT_UART0_RX_TX
#define UART_ERR_VECTOR              INT_UART0_ERR
#define UART_RX_TX_IRQ               (UART_RX_TX_VECTOR - 16)
#define UART_ERR_IRQ                 (UART_ERR_VECTOR - 16)

/* DMA Hardware */
#define UART_TXD_DMA_SOURCE          3
#define UART_RXD_DMA_SOURCE          2
#define UART_TXD_DMA_CHANNEL         5
#define UART_RXD_DMA_CHANNEL         4
#define UART_TXD_DMA_VECTOR          INT_DMA5
#define UART_RXD_DMA_VECTOR          INT_DMA4
#define UART_TXD_DMA_IRQ             (UART_TXD_DMA_VECTOR - 16)
#define UART_RXD_DMA_IRQ             (UART_RXD_DMA_VECTOR - 16)

/* The following definitions represent the ports and pins associated */
/* with the signals used by the HCI interface.                       */
#define TXD_PORT                     PORTD_BASE_PTR
#define TXD_PIN                      7
#define RXD_PORT                     PORTD_BASE_PTR
#define RXD_PIN                      6
#define RTS_PORT                     PORTD_BASE_PTR
#define RTS_PIN                      4
#define CTS_PORT                     PORTD_BASE_PTR
#define CTS_PIN                      5

/* Misc defines */
#define MAX_CTS_CLEAR_RETRY          5                
#define INTERRUPT_PRIORITY           6
#define TRANSPORT_ID                 1

/* Event codes */
#define RX_AVAILABLE 0x00000001
#define RX_TERMINATE 0x00000002
#define RX_TERMINATED 0x00000004
#define RX_UART_ERROR 0x00000008
#define RX_DMA_ERROR 0x00000010
#define TX_DONE 0x00000020

#if DRIVER_DEV == 1
static struct
{
    unsigned int uart_tx_bytes;
    unsigned int uart_rx_bytes;
    unsigned int uart_total_int_count;
    unsigned int uart_idle_int_count;
    unsigned int uart_active_int_count;
    unsigned int uart_tx_dma_int_count;
    unsigned int uart_rx_dma_int_count;
    unsigned int uart_zero_idle_count;
    unsigned int uart_rx_buffers;
    unsigned int uart_tx_buffers;
    unsigned int uart_buffer_high_water;
    unsigned int uart_errors;
    unsigned int uart_fifo_underrun;
    unsigned int uart_rx_dma_overrun;
    unsigned int uart_active_idle;
} uart_stats;
#endif

typedef struct dma_buffer
{
    QUEUE_ELEMENT_STRUCT link;
    uint16_t flags;
    uint16_t size;
    uint8_t data[DMA_BUFFER_SIZE];
} dma_buffer_t;

typedef struct uart_context
{
    TASK_TEMPLATE_STRUCT task_template;
    HCITR_COMDataCallback_t COMDataCallbackFunction;
    unsigned long COMDataCallbackParameter;

    _task_id rx_task_id;

    LWEVENT_STRUCT rx_event;
    LWEVENT_STRUCT tx_event;
    
    QUEUE_STRUCT rx_queue;
    QUEUE_STRUCT free_buffers;
    
    dma_buffer_t *current_rx;
    dma_buffer_t *next_rx;
    
    uint8_t last_uart_error;

#if DRIVER_DEFRAG == 1
    uint8_t packet[1025];
    uint8_t packet_type;
    uint16_t packet_remaining;
    uint8_t *packet_position;
    int packet_header_size;
    int packet_length;
    int defrag_state;
#endif
    
    dma_buffer_t dma_buffers[NUM_DMA_BUFFERS];
   
} uart_context_t;

/* Internal Variables */
static uart_context_t *uart_context = 0;
static int hci_debug_enable = 0; 

static void hci_debug(const char *msg, uint16_t flags, uint8_t *buffer, uint32_t length)
{
    int i;
    
    if (hci_debug_enable != 0)
    {
        if (flags == 0xffff)
            corona_print("%s(%u):", msg, length);
        else
            corona_print("0x%x %s(%u): ", flags, msg, length);
            
        if (hci_debug_enable > 0)
        {
            for (i = 0; i < length; ++i)
                corona_print("%02X ", buffer[i]);
        }
        corona_print("\n");
    }
}

static int change_bt_state(boolean enable)
{
    LWGPIO_STRUCT clk32K;
    int retries;
    
    dev_print("change_bt_state: %d\n", enable);
    
    /* Enable the 32 KHz Clock */
    lwgpio_init(&clk32K, BSP_GPIO_BT_32KHz_EN_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE);

    if (enable)
    {
        /* Enable the 32 KHz Clock */
        lwgpio_init(&clk32K, BSP_GPIO_BT_32KHz_EN_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE);
    
        /* Switch pin functionality (MUX) to GPIO mode */
        lwgpio_set_functionality(&clk32K, BSP_GPIO_BT_32KHz_EN_MUX_GPIO);
        
        /* Turn on the 32k bt external clock */
        lwgpio_set_value(&clk32K, LWGPIO_VALUE_HIGH);
        
        /* Wait for it to settle */
        _time_delay(100);

        /* Take the bt chip out of reset */
        PWRMGR_Enable26MHz_BT(TRUE);
        
        /* Wait for cts to clear */
        for (retries = MAX_CTS_CLEAR_RETRY; ((GPIOD_PDIR & (1 << CTS_PIN)) != 0) && retries >= 0; --retries)
        {
            if (retries == 0)
            {
                corona_print("bt cts failed\n");
                return -1;
            }
            
            _time_delay(10);
        }
        
        return 0;
    }
    else
    {
        /* put the bt chip into reset */
        PWRMGR_Enable26MHz_BT(FALSE);
        
        /* Turn off the 32k bt external clock */
        lwgpio_set_value(&clk32K, LWGPIO_VALUE_LOW);
        
        /* All good */
        return 0;
    }
}

/* The following function will reconfigure the BAUD rate without     */
/* reconfiguring the entire port.  This function is also potentially */
/* more accurate than the method used in the ST standard peripheral  */
/* libraries.                                                        */
static void set_baud_rate(unsigned long baud_rate)
{
   unsigned long source_clock;
   unsigned long divider;

   dev_print("set_baud_rate: baud_rate:%d\n", baud_rate);
   
    /* The target baud rate can be at most SourceClock / 16. */
    source_clock = _cm_get_clock(_cm_get_clock_configuration(), CM_CLOCK_SOURCE_CORE);
    wassert(source_clock >= (baud_rate * 16));

   /* The following calculates the baud rate divisor and fractional     */
   /* adjustment rounded to the nearest value. The result is formated as*/
   /* follows:                                                          */
   /*    [|     BDRH     |         BDRL          |     BRFA     |]      */
   /*    [.17.16.15.14.13.12.11.10.09.08.07.06.05.04.03.02.01.00.]      */
   divider = (((4 * source_clock) / baud_rate) + 1) / 2;
   UART_BDH_REG(UART_BASE) = (UART_BDH_REG(UART_BASE) & (UART_BDH_RXEDGIE_MASK | UART_BDH_LBKDIE_MASK)) | ((divider >> 13) & 0x1F);
   UART_BDL_REG(UART_BASE) = ((divider >> 5) & 0xFF);
   UART_C4_REG(UART_BASE) = (divider & 0x1F);
}

static inline void start_rx_dma(dma_buffer_t *buffer)
{
    /* Setup and start the rx dma channel */
    buffer->flags = 0;
    buffer->size = DMA_BUFFER_SIZE - 1;
    DMA_DADDR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = (unsigned int)(&buffer->data);
    DMA_CITER_ELINKNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = DMA_CITER_ELINKNO_CITER(DMA_BUFFER_SIZE - 1);
    DMA_BITER_ELINKNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = DMA_BITER_ELINKNO_BITER(DMA_BUFFER_SIZE - 1);
    DMA_CSR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) &= ~DMA_CSR_DONE_MASK;
    DMA_SERQ = UART_RXD_DMA_CHANNEL;

    /* Assert RTS */
    GPIOD_PCOR = GPIO_PDDR_PDD(1 << RTS_PIN);
}

static inline int stop_rx_dma(dma_buffer_t* buffer, int rts_deassert)
{
    /* Deassert RTS */
    if (rts_deassert)
    {
        GPIOD_PSOR = GPIO_PDDR_PDD(1 << RTS_PIN);
    }

    if ((DMA_CSR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) & DMA_CSR_DONE_MASK) == 0)
    {
        DMA_CERQ = UART_RXD_DMA_CHANNEL;
        while ((DMA_CSR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) & DMA_CSR_ACTIVE_MASK) != 0)
        {
        }
    }
    DMA_CINT = UART_RXD_DMA_CHANNEL;
    DMA_CDNE = UART_RXD_DMA_CHANNEL;
    
    return DMA_DADDR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) - (unsigned int)(buffer->data);
}

static inline void set_idle_detection(void)
{
    UART_S2_REG(UART_BASE) |= UART_S2_RXEDGIF_MASK;
    UART_BDH_REG(UART_BASE) &= ~UART_BDH_RXEDGIE_MASK;
    UART_C2_REG(UART_BASE) |= UART_C2_ILIE_MASK;
}

static inline void set_active_detection(void)
{
    UART_S2_REG(UART_BASE) |= UART_S2_RXEDGIF_MASK;
    UART_C2_REG(UART_BASE) &= ~UART_C2_ILIE_MASK;
    UART_BDH_REG(UART_BASE) |= UART_BDH_RXEDGIE_MASK;
}

static void uart_irq_handler(void *param)
{
    uart_context_t *context = (uart_context_t *)(param);
    uint8_t s1_status = UART_S1_REG(UART_BASE);
    uint8_t s2_status = UART_S2_REG(UART_BASE);
    uint32_t event = 0;
    

    /* Debug stuff */
    dev_led(3, 1);
    dev_stat(++uart_stats.uart_total_int_count);

    /* Handle receiver errors */
    if (s1_status & (UART_S1_OR_MASK | UART_S1_NF_MASK | UART_S1_FE_MASK | UART_S1_PF_MASK)) 
    {
        dev_led(0, 1);
        
        /* Stop the current dma transfer */
        (void)stop_rx_dma(context->current_rx, TRUE);
        
        /* Save the last error */
        context->last_uart_error = s1_status & (UART_S1_OR_MASK | UART_S1_NF_MASK | UART_S1_FE_MASK | UART_S1_PF_MASK);
        
        /* Clear the rx fifo */
        (void)UART_D_REG(UART_BASE);
        UART_CFIFO_REG(UART_BASE) |= UART_CFIFO_RXFLUSH_MASK;
        
        /* Enable active edge detection */
        set_active_detection();

        /* Start it running */
        start_rx_dma(context->current_rx);
        
        /* Let the rx task know there has been and error */
        _lwevent_set(&context->rx_event, RX_UART_ERROR);
        
        /* No more work for this state */
        dev_stat(++uart_stats.uart_errors);
        dev_led(0, 0);
        dev_led(3, 0);
        return;
    }

    /* Handle new uart activity */
    if ((UART_BDH_REG(UART_BASE) & UART_BDH_RXEDGIE_MASK) && (s2_status & UART_S2_RXEDGIF_MASK)) 
    {
        // Debug stuff
        dev_led(2, 1);
        
        /* Switch to idle detection */
        set_idle_detection();
        
        /* Debug stuff */
        dev_stat(++uart_stats.uart_active_int_count);
        dev_led(2, 0);
        dev_led(3, 0);
        return;
    }
    
    /* Handle idle detected */
    if ((UART_C2_REG(UART_BASE) & UART_C2_ILIE_MASK) && (s1_status & UART_S1_IDLE_MASK)) 
    {
        /* Debug stuff */
        dev_led(1, 1);
        
        /* If the UART still active? */
        if ((UART_S2_REG(UART_BASE) & UART_S2_RAF_MASK) != 0)
        {
            /* Debug stuff */
            dev_led(2, 1);
            dev_stat(++uart_stats.uart_active_idle);
            dev_stat(++uart_stats.uart_idle_int_count);
            dev_led(1, 0);
            dev_led(2, 0);
            dev_led(3, 0);
            
            /* Let the dma engine continue to run */
            return;
        }
        
        /* Stop the current dma transfer */
        context->current_rx->size = stop_rx_dma(context->current_rx, TRUE);
        
        /* Try to recover next buffer early */
        context->next_rx = (!context->next_rx ? (dma_buffer_t *)(_queue_dequeue(&context->free_buffers)) : context->next_rx);

        /* Make sure we have an available buffer, otherwise reuse the current one */
        if (context->next_rx)
        {
            /* If we have data, queue it */
            if (context->current_rx->size || ((UART_SFIFO_REG(UART_BASE) & UART_SFIFO_RXEMPT_MASK) == 0))
            {
   
                /* Empty fifo, there should be 0, 1 character in the fifo, this allows clearing of the idle flag */
                context->current_rx->flags = 0;
                UART_SFIFO_REG(UART_BASE) = UART_SFIFO_RXUF_MASK;
                context->current_rx->data[context->current_rx->size] = UART_D_REG(UART_BASE);
                ++context->current_rx->size;
                context->current_rx->flags |= 0x8001;
                
                /* Flag uart rx fifo underruns and clear it */
                if (UART_SFIFO_REG(UART_BASE) & UART_SFIFO_RXUF_MASK)
                {
                    /* Clear, mark it and count it */
                    --context->current_rx->size;
                    UART_CFIFO_REG(UART_BASE) |= UART_CFIFO_RXFLUSH_MASK;
                    UART_SFIFO_REG(UART_BASE) = UART_SFIFO_RXUF_MASK;
                    context->current_rx->flags = 0x2000 | 0x8000;
                    dev_stat(++uart_stats.uart_fifo_underrun);
                }
                
                /* Add to rx queue */
                _queue_enqueue(&context->rx_queue, &context->current_rx->link);
                
                /* Move to the next buffer */
                context->current_rx = context->next_rx;
                context->next_rx = (dma_buffer_t *)(_queue_dequeue(&context->free_buffers));
                
                /* Available event */
                event = RX_AVAILABLE;
            }
            else
            {
                /* Clear the rx fifo to clear the idle interrupt */
                (void)UART_D_REG(UART_BASE);
                UART_CFIFO_REG(UART_BASE) |= UART_CFIFO_RXFLUSH_MASK;
                UART_SFIFO_REG(UART_BASE) = UART_SFIFO_RXUF_MASK;
                
                /* Switch to active detection */
                set_active_detection();
                
                /* Found an empty buffer */
                dev_stat(++uart_stats.uart_zero_idle_count);
            }
        }
        else
        {
            /* Report the overrun */
            event = RX_DMA_ERROR;
        }
        
        /* Start rx dma running */
        start_rx_dma(context->current_rx);
        
        /* Switch to active detection */
        set_active_detection();
        
        /* Let the rx thread know there is data in the buffer */
        if (event)
            _lwevent_set(&context->rx_event, event);

        /* Debug stuff */
        dev_stat(++uart_stats.uart_idle_int_count);
        dev_led(1, 0);
    }

    dev_led(3, 0);
}

static void hci_txd_irq_handler(void *param)
{
    uart_context_t *context = (uart_context_t *)(param);

    /* Clear the dma interrupt */
    DMA_CINT = UART_TXD_DMA_CHANNEL;
    
    /* Transfer complete*/
    _lwevent_set(&context->tx_event, TX_DONE);

    /* Debug stuff */
    dev_stat(++uart_stats.uart_tx_dma_int_count;)
}

static void hci_rxd_irq_handler(void *param)
{
    dma_buffer_t *current;
    uart_context_t *context = (uart_context_t *)(param);

    /* Ensure dma is stopped */
    context->current_rx->size = stop_rx_dma(context->current_rx, TRUE);
    
    /* Try to recover next buffer early */
    context->next_rx = (context->next_rx == NULL ? (dma_buffer_t *)(_queue_dequeue(&context->free_buffers)) : context->next_rx);
    
    /* If a another buffer is available use it */
    if (context->next_rx)
    {
        /* Switch buffers */
        current = context->current_rx;
        context->current_rx = context->next_rx;
        
        /* Let the dogs of DMA out */
        start_rx_dma(context->current_rx);

        /* Queue it */
        current->flags = 0x4000;
        _queue_enqueue(&context->rx_queue, &current->link);
        
        /* Queue up another buffer */
        context->next_rx = (dma_buffer_t *)(_queue_dequeue(&context->free_buffers));

        /* Let the rx task not we have data */
        _lwevent_set(&context->rx_event, RX_AVAILABLE);
    }
    else
    {
        /* We overran the dma buffers, drop the packet and reuse the current one */
        start_rx_dma(context->current_rx);
        
        /* Try queue up another buffer */
        context->next_rx = (dma_buffer_t *)(_queue_dequeue(&context->free_buffers));

        /* Let the rx thread that we over ran the current rx buffer or over ran the queue */
        dev_stat(++uart_stats.uart_rx_dma_overrun);
        _lwevent_set(&context->rx_event, RX_DMA_ERROR);
    }

    /* Debug stuff */
    dev_stat(++uart_stats.uart_rx_dma_int_count);
}

#if DRIVER_DEFRAG == 1
/* This is the defragment version */
static void post_packet(uart_context_t *context, dma_buffer_t *buffer)
{
    uint8_t *current = buffer->data;
    uint8_t *end = buffer->data + buffer->size;
    int copy;
    int i;
    int result = 0;
    
    corona_print("0x%x raw %u - %02x", buffer->flags, buffer->size, buffer->data[0]);
    for (i = 1; i < buffer->size; ++i)
        corona_print(":%02x", buffer->data[i]);
    corona_print("\n");

    if (context->defrag_state < 0)
        return;
    
    while (current < end && context->defrag_state >= 0)
    {
        if (context->defrag_state == 0)
        {
            context->packet_type = *current;
            context->packet_length = 0;
            context->packet_remaining = 0;
            context->packet_position = context->packet;
            context->packet[0] = *current;
            switch (context->packet_type)
            {
                case 1:
                    context->packet_header_size = 3;
                    break;
                case 2:
                    context->packet_header_size = 4;
                    break;
                case 3:
                    context->packet_header_size = 3;
                    break;
                case 4:
                    context->packet_header_size = 2;
                    break;
                default:
                    dev_led(0, 1);
                    context->defrag_state = -1;
                    corona_print("sync lost 0 %u %u ", context->packet_type, end - current);
                    while (current < end)
                    {
                        corona_print(":%02x", *current);
                        ++current;
                    }
                    corona_print("\n");
                    continue; 
            }
            context->packet_remaining = context->packet_header_size;
            ++current;
            ++context->packet_position;
            context->defrag_state = 1;
        }
        
        if (context->defrag_state == 1)
        {
            copy = (end - current) < context->packet_remaining ? (end - current) : context->packet_remaining;
            memcpy(context->packet_position, current, copy);
            current += copy;
            context->packet_position += copy;
            context->packet_remaining -= copy;
            if (context->packet_remaining == 0)
            {
                switch (context->packet_type)
                {
                    case 1:
                    case 3:
                        context->packet_length = context->packet[3];
                        break;
                    case 2:
                        context->packet_length = (context->packet[4] << 8) + context->packet[3];
                        break;
                    case 4:
                        context->packet_length = context->packet[2];
                        break;
                    default:
                        dev_led(0, 1);
                        context->defrag_state = -1;
                        corona_print("sync lost 1 %u %u ", context->packet_type, end - current);
                        while (current < end)
                        {
                            corona_print(":%02x", *current);
                            ++current;
                        }
                        corona_print("\n");
                        continue;
                }
                context->packet_remaining = context->packet_length;
                context->defrag_state = 2;
            }
        }
        
        if (context->defrag_state == 2)
        {
            if (context->packet_remaining > 0)
            {
                copy = (end - current) < context->packet_remaining ? (end - current) : context->packet_remaining;
                memcpy(context->packet_position, current, copy);
                current += copy;
                context->packet_position += copy;
                context->packet_remaining -= copy;
            }
            
            if (context->packet_remaining == 0)
            {
                context->defrag_state = 3;
            }
        }
        
        if (context->defrag_state == 3)
        {
            corona_print("type: %u length: %u - %02x", context->packet_type, context->packet_header_size + context->packet_length + 1, context->packet[0]);
            for (i = 1; i < context->packet_header_size + context->packet_length + 1; ++i)
                corona_print(":%02x", context->packet[i]);
            corona_print("\n");

            (*context->COMDataCallbackFunction)(TRANSPORT_ID, context->packet_header_size + context->packet_length + 1, context->packet, context->COMDataCallbackParameter);

            context->packet_type = 0;
            context->packet_header_size = 0;
            context->packet_length = 0;
            context->packet_remaining = 0;
            context->packet_position = 0;
            context->defrag_state = 0;
            memset(context->packet, 0, 1025);
        }
        
        if (current == buffer->data)
        {
            dev_led(0, 1);
            context->defrag_state = -1;
            corona_print("sync lost p %u %u ", context->packet_type, end - current);
            while (current < end)
            {
                corona_print(":%02x", *current);
                ++current;
            }
            corona_print("\n");
        }
    }
}
#else
/* Just post up */
static inline void post_packet(uart_context_t *context, dma_buffer_t *buffer)
{
    (*context->COMDataCallbackFunction)(TRANSPORT_ID, buffer->size, buffer->data, context->COMDataCallbackParameter);
}
#endif

static void post(uart_context_t *context)
{
    dma_buffer_t* current;

    dev_stat(uart_stats.uart_buffer_high_water = _queue_get_size(&context->rx_queue) <= uart_stats.uart_buffer_high_water ? uart_stats.uart_buffer_high_water : _queue_get_size(&context->rx_queue));
    
    /* Loop through filled buffers, posting up */
    while ((current = (dma_buffer_t *)(_queue_dequeue(&context->rx_queue))) != NULL)
    {
        wassert(current->size <= DMA_BUFFER_SIZE);

        /* Post the buffer if there data in it*/
        if (current->size)
        {
            /* Debug dump */
            hci_debug("HCI Rd:", current->flags, current->data, current->size);
            
            /* Push up */
            post_packet(context, current);
            
            /* Debug Stuff */
            dev_stat(++uart_stats.uart_rx_buffers);
            dev_stat(uart_stats.uart_rx_bytes += current->size);
        }
        
        /* Release the buffer */
        memset(current, 0, sizeof(dma_buffer_t));
        _queue_enqueue(&context->free_buffers, &current->link);
    }
}

static void rx_task(uint32_t param)
{
    uart_context_t *context = (uart_context_t *)(param);

    dev_print("%s: 0x%x started\n", context->task_template.TASK_NAME, _task_get_id());
    
    /* Loop until terminated */
    while (TRUE)
    {
        /* Wait for an rx event */
        _lwevent_wait_for(&context->rx_event, 
                RX_AVAILABLE | RX_TERMINATE | RX_UART_ERROR | RX_DMA_ERROR, FALSE, NULL);
        
        /* Error detected */
        if (_lwevent_get_signalled() & RX_UART_ERROR) 
            PROCESS_NINFO(ERR_BT_HARDWARE_FAIL,"btrxt e:0x%02x\n", context->last_uart_error);
        
        if (_lwevent_get_signalled() & RX_DMA_ERROR)
            PROCESS_NINFO(ERR_BT_HARDWARE_FAIL, "btrxt: rq:%u fb:%u\n", _queue_get_size(&context->rx_queue), _queue_get_size(&context->free_buffers));

        /* Terminate requested */
        if (_lwevent_get_signalled() & RX_TERMINATE)
            break;

        /* Data available */
        post(context);
    }

    /* Clean up */
    dev_print("%s: 0x%x terminating\n", context->task_template.TASK_NAME, _task_get_id());

    /* Flush any remaining data to the upper layers */
    post(context);
    
    /* Signal the thread has existed */
    _lwevent_set(&context->rx_event, RX_TERMINATED);
}

/* The following function is responsible for opening the HCI         */
/* Transport layer that will be used by Bluetopia to send and receive*/
/* COM (Serial) data.  This function must be successfully issued in  */
/* order for Bluetopia to function.  This function accepts as its    */
/* parameter the HCI COM Transport COM Information that is to be used*/
/* to open the port.  The final two parameters specify the HCI       */
/* Transport Data Callback and Callback Parameter (respectively) that*/
/* is to be called when data is received from the UART.  A successful*/
/* call to this function will return a non-zero, positive value which*/
/* specifies the HCITransportID that is used with the remaining      */
/* transport functions in this module.  This function returns a      */
/* negative return value to signify an error.                        */
int BTPSAPI HCITR_COMOpen(HCI_COMMDriverInformation_t *COMMDriverInformation, HCITR_COMDataCallback_t COMDataCallback, unsigned long CallbackParameter)
{
   int ret_val;
   int i;
   
   dev_print("HCITR_COMOpen: nb:%d bz:%d\n", NUM_DMA_BUFFERS, DMA_BUFFER_SIZE);
   dev_led(0, 0);
   dev_led(1, 0);
   dev_led(2, 0);
   dev_led(3, 0);
   
   /* First, make sure that the port is not already open and make sure  */
   /* that valid COMM Driver Information was specified.                 */
   wassert(!uart_context && (COMMDriverInformation) && (COMDataCallback));

   /* Debug stuff */
   dev_stat_init();
   
   /* Initialize the context structure. */
   uart_context = (uart_context_t *)walloc(sizeof(uart_context_t));

   /* Initialize task template */
   uart_context->task_template.TASK_TEMPLATE_INDEX = 0;
   uart_context->task_template.TASK_ADDRESS = rx_task;
   uart_context->task_template.TASK_STACKSIZE = 2048;
   uart_context->task_template.TASK_PRIORITY = DEFAULT_BTPS_TASK_PRIORITY + 1;
   uart_context->task_template.TASK_NAME = "bt_rx";
   uart_context->task_template.CREATION_PARAMETER = (uint32_t)uart_context;
   uart_context->task_template.DEFAULT_TIME_SLICE = DEFAULT_BTPS_TIME_SLICE_VALUE;

   /* Save call back info */
   uart_context->COMDataCallbackFunction  = COMDataCallback;
   uart_context->COMDataCallbackParameter = CallbackParameter;
   
   /* Initialize buffer queues */
   _queue_init(&uart_context->rx_queue, 0);
   _queue_init(&uart_context->free_buffers, 0);
   
   /* Assemble free dma buffer list */
   for (i = 0; i < NUM_DMA_BUFFERS; ++i)
       _queue_enqueue(&uart_context->free_buffers, &uart_context->dma_buffers[i].link);

   /* Setup the first and next rx dma buffer, this will never fail */
   uart_context->current_rx = (dma_buffer_t *)(_queue_dequeue(&uart_context->free_buffers));
   uart_context->next_rx = (dma_buffer_t *)(_queue_dequeue(&uart_context->free_buffers));
   
   /* Create the event that will be used to signal data has arrived. */
   _lwevent_create(&uart_context->rx_event, LWEVENT_AUTO_CLEAR);
   _lwevent_create(&uart_context->tx_event, LWEVENT_AUTO_CLEAR);

   /* Make sure that the event is in the reset state. */
   _lwevent_clear(&uart_context->rx_event, RX_AVAILABLE | RX_TERMINATE | RX_TERMINATED | RX_UART_ERROR | RX_DMA_ERROR);
   _lwevent_clear(&uart_context->tx_event, TX_DONE);

   /* Create a thread that will process the received data. */
   uart_context->rx_task_id = _task_create(0, 0, (uint_32)(&uart_context->task_template));
   wassert(uart_context->rx_task_id != MQX_NULL_TASK_ID);
   
   /* Enable Port and DMAMUX/DMA Clocks */
   wint_disable();
   SIM_SCGC5 |= SIM_SCGC5_PORTD_MASK;
   SIM_SCGC4 |= SIM_SCGC4_UART0_MASK;
   SIM_SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
   SIM_SCGC7 |= SIM_SCGC7_DMA_MASK;

   /* Configure the GPIO for UART operation */
   PORT_PCR_REG(TXD_PORT, TXD_PIN) = PORT_PCR_MUX(UART_MUX) | PORT_PCR_DSE_MASK;
   PORT_PCR_REG(RXD_PORT, RXD_PIN) = PORT_PCR_MUX(UART_MUX);
   GPIOD_PDDR |= GPIO_PDDR_PDD(1 << RTS_PIN);
   GPIOD_PSOR = GPIO_PDDR_PDD(1 << RTS_PIN);
   PORT_PCR_REG(RTS_PORT, RTS_PIN) = PORT_PCR_MUX(GPIO_MUX) | PORT_PCR_DSE_MASK;

   PORT_PCR_REG(CTS_PORT, CTS_PIN) = PORT_PCR_MUX(UART_MUX);
   wint_enable();

   /* Take the bt chip out of reset */
   if (change_bt_state(TRUE) != 0)
       /* Problem taking bt out out reset */
       return HCITR_ERROR_UNABLE_TO_OPEN_TRANSPORT;

   /* Initialize the UART. */
   UART_C1_REG(UART_BASE) = UART_C1_ILT_MASK;
   UART_C2_REG(UART_BASE) = 0;
   UART_C3_REG(UART_BASE) = UART_C3_ORIE_MASK | UART_C3_NEIE_MASK | UART_C3_FEIE_MASK | UART_C3_PEIE_MASK;
   UART_C5_REG(UART_BASE) = UART_C5_TDMAS_MASK | UART_C5_RDMAS_MASK;
   UART_MODEM_REG(UART_BASE) = UART_MODEM_RXRTSE_MASK | UART_MODEM_TXCTSE_MASK;
   UART_BDH_REG(UART_BASE) |= UART_BDH_RXEDGIE_MASK;
   UART_PFIFO_REG(UART_BASE) = (UART_PFIFO_REG(UART_BASE) & (UART_PFIFO_TXFIFOSIZE_MASK | UART_PFIFO_RXFIFOSIZE_MASK)) | UART_PFIFO_TXFE_MASK | UART_PFIFO_RXFE_MASK;
   UART_TWFIFO_REG(UART_BASE) = UART_TX_FIFO_THRESHOLD;
   UART_RWFIFO_REG(UART_BASE) = UART_RX_FIFO_THRESHOLD;
   UART_CFIFO_REG(UART_BASE) = UART_CFIFO_TXFLUSH_MASK | UART_CFIFO_RXFLUSH_MASK;
   UART_SFIFO_REG(UART_BASE) = UART_SFIFO_TXOF_MASK | UART_SFIFO_RXOF_MASK | UART_SFIFO_RXUF_MASK;
   set_baud_rate(COMMDriverInformation->BaudRate);

   /* Configure the receive DMA channel.                          */
   DMAMUX_CHCFG_REG(DMAMUX_BASE_PTR, UART_RXD_DMA_CHANNEL)   = DMAMUX_CHCFG_ENBL_MASK | DMAMUX_CHCFG_SOURCE(UART_RXD_DMA_SOURCE);
   DMA_SADDR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)         = (unsigned int)(&UART_D_REG(UART_BASE));
   DMA_DADDR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)         = (unsigned int)(uart_context->current_rx->data);
   DMA_SOFF_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)          = 0;
   DMA_DOFF_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)          = 1;
   DMA_ATTR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)          = DMA_ATTR_SSIZE(0) | DMA_ATTR_DSIZE(0);
   DMA_SLAST_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)         = 0;
   DMA_DLAST_SGA_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)     = 0;
   DMA_NBYTES_MLNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)   = 1;
   DMA_CITER_ELINKNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = DMA_BUFFER_SIZE;
   DMA_BITER_ELINKNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = DMA_BUFFER_SIZE;
   DMA_CSR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)           = DRIVER_RX_DMA_CONFIG;

   /* Configure the transmit DMA channel.                         */
   DMAMUX_CHCFG_REG(DMAMUX_BASE_PTR, UART_TXD_DMA_CHANNEL)   = DMAMUX_CHCFG_ENBL_MASK | DMAMUX_CHCFG_SOURCE(UART_TXD_DMA_SOURCE);
   DMA_SADDR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)         = 0;
   DMA_DADDR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)         = (unsigned int)(&UART_D_REG(UART_BASE));
   DMA_SOFF_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)          = 1;
   DMA_DOFF_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)          = 0;
   DMA_ATTR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)          = DMA_ATTR_SSIZE(0) | DMA_ATTR_DSIZE(0);
   DMA_SLAST_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)         = 0;
   DMA_DLAST_SGA_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)     = 0;
   DMA_NBYTES_MLNO_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)   = 1;
   DMA_CITER_ELINKNO_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL) = 0;
   DMA_BITER_ELINKNO_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL) = 0;
   DMA_CSR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)           = DRIVER_TX_DMA_CONFIG;

   /* clear the interrupts */
   NVIC_ICER(UART_TXD_DMA_IRQ >> 5) = (1 << (UART_TXD_DMA_IRQ & 0x1F));
   NVIC_ICER(UART_RXD_DMA_IRQ >> 5) = (1 << (UART_RXD_DMA_IRQ & 0x1F));
   NVIC_ICER(UART_RX_TX_IRQ >> 5) = (1 << (UART_RX_TX_IRQ & 0x1F));
   NVIC_ICER(UART_ERR_IRQ >> 5) = (1 << (UART_ERR_IRQ & 0x1F));

   NVIC_ICPR(UART_TXD_DMA_IRQ >> 5) = (1 << (UART_TXD_DMA_IRQ & 0x1F));
   NVIC_ICPR(UART_RXD_DMA_IRQ >> 5) = (1 << (UART_RXD_DMA_IRQ & 0x1F));
   NVIC_ICPR(UART_RX_TX_IRQ >> 5) = (1 << (UART_RX_TX_IRQ & 0x1F));
   NVIC_ICPR(UART_ERR_IRQ >> 5) = (1 << (UART_ERR_IRQ & 0x1F));

   NVIC_IP(UART_TXD_DMA_IRQ) = (INTERRUPT_PRIORITY << 4);
   NVIC_IP(UART_RXD_DMA_IRQ) = ((INTERRUPT_PRIORITY) << 4);
   NVIC_IP(UART_RX_TX_IRQ) = (INTERRUPT_PRIORITY << 4);
   NVIC_IP(UART_ERR_IRQ) = (INTERRUPT_PRIORITY << 4);

   /* Install DMA and UART ISRs                                   */
   _int_install_isr(UART_TXD_DMA_VECTOR, hci_txd_irq_handler, uart_context);
   _int_install_isr(UART_RXD_DMA_VECTOR, hci_rxd_irq_handler, uart_context);
   _int_install_isr(UART_RX_TX_VECTOR, uart_irq_handler, uart_context);
   _int_install_isr(UART_ERR_VECTOR, uart_irq_handler, uart_context);
   
   /* Enable the UART */
   DMA_SERQ = UART_RXD_DMA_CHANNEL;
   NVIC_ISER(UART_TXD_DMA_IRQ >> 5) = (1 << (UART_TXD_DMA_IRQ & 0x1F));
   NVIC_ISER(UART_RXD_DMA_IRQ >> 5) = (1 << (UART_RXD_DMA_IRQ & 0x1F));
   NVIC_ISER(UART_RX_TX_IRQ >> 5) = (1 << (UART_RX_TX_IRQ & 0x1F));
   NVIC_ISER(UART_ERR_IRQ >> 5) = (1 << (UART_ERR_IRQ & 0x1F));
   UART_C2_REG(UART_BASE) = UART_C2_RIE_MASK | UART_C2_TIE_MASK | UART_C2_TE_MASK | UART_C2_RE_MASK;
   GPIOD_PCOR = GPIO_PDDR_PDD(1 << RTS_PIN);

   /* Check to see if we need to delay after opening the COM Port.*/
   if(COMMDriverInformation->InitializationDelay)
	   BTPS_Delay(COMMDriverInformation->InitializationDelay);

   return TRANSPORT_ID;
}

/* The following function is responsible for closing the specific HCI*/
/* Transport layer that was opened via a successful call to the      */
/* HCITR_COMOpen() function (specified by the first parameter).      */
/* Bluetopia makes a call to this function whenever an either        */
/* Bluetopia is closed, or an error occurs during initialization and */
/* the driver has been opened (and ONLY in this case).  Once this    */
/* function completes, the transport layer that was closed will no   */
/* longer process received data until the transport layer is         */
/* Re-Opened by calling the HCITR_COMOpen() function.                */
/* * NOTE * This function *MUST* close the specified COM Port.  This */
/*          module will then call the registered COM Data Callback   */
/*          function with zero as the data length and NULL as the    */
/*          data pointer.  This will signify to the HCI Driver that  */
/*          this module is completely finished with the port and     */
/*          information and (more importantly) that NO further data  */
/*          callbacks will be issued.  In other words the very last  */
/*          data callback that is issued from this module *MUST* be a*/
/*          data callback specifying zero and NULL for the data      */
/*          length and data buffer (respectively).                   */
void BTPSAPI HCITR_COMClose(unsigned int HCITransportID)
{
    dev_print("HCITR_COMClose: tb:%u rb:%u t:%u r:%u c:%u i:%u a:%u z:%u ai:%u td:%u rd:%u fu:%u hw:%u\n",
               uart_stats.uart_tx_bytes, uart_stats.uart_rx_bytes,
               uart_stats.uart_tx_buffers, uart_stats.uart_rx_buffers,  
               uart_stats.uart_total_int_count, uart_stats.uart_idle_int_count, 
               uart_stats.uart_active_int_count, uart_stats.uart_zero_idle_count, 
               uart_stats.uart_active_idle, uart_stats.uart_tx_dma_int_count, 
               uart_stats.uart_rx_dma_int_count, uart_stats.uart_fifo_underrun, 
               uart_stats.uart_buffer_high_water);
    
    /* Check to make sure that the specified Transport ID is valid.      */
    wassert((HCITransportID == TRANSPORT_ID) && uart_context);
    
    /* Shutdown the rx thread                                         */
    _lwevent_set(&uart_context->rx_event, RX_TERMINATE);
    _lwevent_wait_for(&uart_context->rx_event, RX_TERMINATED, TRUE, NULL);
    _task_destroy(uart_context->rx_task_id);
    
    /* Appears to be valid, go ahead and close the port.              */
    UART_C2_REG(UART_BASE) = 0;
    
    /* Disable DMA */
    DMA_CERQ = UART_RXD_DMA_CHANNEL;
    DMA_CERQ = UART_TXD_DMA_CHANNEL;

    /* Disable interrupts                                             */
    NVIC_ICER(UART_TXD_DMA_IRQ >> 5) = (1 << (UART_TXD_DMA_IRQ & 0x1F));
    NVIC_ICER(UART_RXD_DMA_IRQ >> 5) = (1 << (UART_RXD_DMA_IRQ & 0x1F));
    NVIC_ICER(UART_RX_TX_IRQ >> 5) = (1 << (UART_RX_TX_IRQ & 0x1F));
    NVIC_ICER(UART_ERR_IRQ >> 5) = (1 << (UART_ERR_IRQ & 0x1F));

    /* Place the Bluetooth Device in Reset.                           */
    (void)change_bt_state(FALSE);

    /* Put TX and RTS back to GPIOs, driven high */
    wint_disable();
    GPIOD_PDDR |= (GPIO_PDDR_PDD(0x10) | GPIO_PDDR_PDD(0x80));
    GPIOD_PSOR = (GPIO_PDDR_PDD(0x10) | GPIO_PDDR_PDD(0x80));
    PORT_PCR_REG(TXD_PORT, TXD_PIN) = PORT_PCR_MUX(GPIO_MUX) | PORT_PCR_DSE_MASK;
    PORT_PCR_REG(RXD_PORT, RXD_PIN) = PORT_PCR_MUX(GPIO_MUX);
    PORT_PCR_REG(RTS_PORT, RTS_PIN) = PORT_PCR_MUX(GPIO_MUX) | PORT_PCR_DSE_MASK;
    PORT_PCR_REG(CTS_PORT, CTS_PIN) = PORT_PCR_MUX(GPIO_MUX);

    /* Disable the peripheral clock for the UART */
    SIM_SCGC4 &= ~SIM_SCGC4_UART0_MASK;
    wint_enable();
    
    /* All finished, perform the callback to let the upper layer know */
    /* that this module will no longer issue data callbacks and is    */
    /* completely cleaned up.                                         */
    (*uart_context->COMDataCallbackFunction)(HCITransportID, 0, NULL, uart_context->COMDataCallbackParameter);
    
    /* Flag that the HCI Transport is no longer open.                 */
    uart_context->COMDataCallbackFunction = NULL;
    
    /* Destroy the event */
    _lwevent_destroy(&uart_context->rx_event);
    _lwevent_destroy(&uart_context->tx_event);
    
    /* Release the context */
    wfree(uart_context);
    uart_context = 0;
}

/* The following function is responsible for instructing the         */
/* specified HCI Transport layer (first parameter) that was opened   */
/* via a successful call to the HCITR_COMOpen() function to          */
/* reconfigure itself with the specified information.  This          */
/* information is completely opaque to the upper layers and is passed*/
/* through the HCI Driver layer to the transport untouched.  It is   */
/* the responsibility of the HCI Transport driver writer to define   */
/* the contents of this member (or completely ignore it).            */
/* * NOTE * This function does not close the HCI Transport specified */
/*          by HCI Transport ID, it merely reconfigures the          */
/*          transport.  This means that the HCI Transport specified  */
/*          by HCI Transport ID is still valid until it is closed    */
/*          via the HCI_COMClose() function.                         */
void BTPSAPI HCITR_COMReconfigure(unsigned int HCITransportID, HCI_Driver_Reconfigure_Data_t *DriverReconfigureData)
{
    uint8_t c2;
    
    dev_print("HCITR_COMReconfigure: invoked\n");
    
    /* Check to make sure that the specified Transport ID is valid.      */
    if((HCITransportID == TRANSPORT_ID) && uart_context && DriverReconfigureData)
    {
        if((DriverReconfigureData->ReconfigureCommand == HCI_COMM_DRIVER_RECONFIGURE_DATA_COMMAND_CHANGE_PARAMETERS) && (DriverReconfigureData->ReconfigureData))
        {
            /* Atomically change baud rate */
            wint_disable();
            
            /* Disable UART */
            c2 = UART_C2_REG(UART_BASE);
            UART_C2_REG(UART_BASE) = 0;

            /* Set the baud rate */
            set_baud_rate(((HCI_COMMDriverInformation_t *)(DriverReconfigureData->ReconfigureData))->BaudRate);

            /* Enable UART */
            UART_C2_REG(UART_BASE) = c2;

            /* Let the UART free */
            wint_enable();
        }
    }
}

/* The following function is responsible for actually sending data   */
/* through the opened HCI Transport layer (specified by the first    */
/* parameter).  Bluetopia uses this function to send formatted HCI   */
/* packets to the attached Bluetooth Device.  The second parameter to*/
/* this function specifies the number of bytes pointed to by the     */
/* third parameter that are to be sent to the Bluetooth Device.  This*/
/* function returns a zero if the all data was transferred           */
/* successfully or a negative value if an error occurred.  This      */
/* function MUST NOT return until all of the data is sent (or an     */
/* error condition occurs).  Bluetopia WILL NOT attempt to call this */
/* function repeatedly if data fails to be delivered.  This function */
/* will block until it has either buffered the specified data or sent*/
/* all of the specified data to the Bluetooth Device.                */
/* * NOTE * The type of data (Command, ACL, SCO, etc.) is NOT passed */
/*          to this function because it is assumed that this         */
/*          information is contained in the Data Stream being passed */
/*          to this function.                                        */
int BTPSAPI HCITR_COMWrite(unsigned int HCITransportID, unsigned int Length, unsigned char *Buffer)
{
    wassert((HCITransportID == TRANSPORT_ID) && uart_context && Length && Buffer);
    
    /* Debug stuff */
    dev_stat(++uart_stats.uart_tx_buffers);
    dev_stat(uart_stats.uart_tx_bytes += Length);

    /* Debug dump */
    hci_debug("HCI Wd", 0xffff, Buffer, Length);
    
    /* Setup tx dma */
    DMA_SADDR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL) = (unsigned int)(Buffer);
    DMA_CITER_ELINKNO_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL) = Length;
    DMA_BITER_ELINKNO_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL) = Length;
    
    /* Start it running */
    DMA_SERQ = UART_TXD_DMA_CHANNEL;

    /* Wait for completion */
    _lwevent_wait_for(&uart_context->tx_event, TX_DONE, TRUE, NULL);

    return 0;
}

void HCITR_Set_Debug(boolean enable)
{
	hci_debug_enable = enable;
}
