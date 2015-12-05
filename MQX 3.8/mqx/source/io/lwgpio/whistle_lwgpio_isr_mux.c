/*
 * whistle_lwgpio_isr_mux.c
 *
 *  Created on: Mar 8, 2013
 *      Author: Chris
 *      
 *  When multiple interrupts share the same GPIO port, they share the same ISR.
 *  This facility will demultiplex the interrupts. It calls all interrupting ISRs
 *  on that port when any interrupt on that port occurs.
 */

#include "whistle_lwgpio_isr_mux.h"
#include <assert.h>

extern void Custom_HW_InterruptHandler(void *pointer);

#define LWGPIO_PORT_FROM_ID(id) (((id) & LWGPIO_PORT_MASK) >> LWGPIO_PORT_SHIFT)

typedef enum port_e
{
  PORTFIRST,
  PORTA = PORTFIRST,
  PORTB,
  PORTC,
  PORTD,
  PORTE,
  PORTLAST = PORTE
} port_t;

/* Per registered ISR struct. */
struct isr_s
{
  LWGPIO_STRUCT_PTR plwgpio; // lwgpio this isr belongs to
  whistle_lwgpio_isr_mux_isr_t isr; // isr
};

#define MAX_PORT_ISRS 3 // if this isn't enough, add one
static struct isr_s g_isr[PORTLAST - PORTFIRST + 1][MAX_PORT_ISRS] = {0};

/* Main ISR */
static void whistle_lwgpio_isr_mux(void *pointer)
{
  int i;
  LWGPIO_STRUCT_PTR plwgpio = (LWGPIO_STRUCT_PTR)pointer;
  port_t port = (port_t)LWGPIO_PORT_FROM_ID(plwgpio->flags);
  
  /*
   *   WIFI interrupt could be sensitive to time, so go ahead and call/clear it right away to avoid missing ints.
   *     This is a total hack to reduce the amount of time it takes to ack this interrupt.
   */
  if( PORTE_ISFR & 0x20 )
  {
      /*
       *   Clear the WIFI_INT status.
       */
      PORTE_ISFR |= 0x20;                   // BSP_GPIO_WIFI_INT_PIN
      while( PORTE_ISFR & 0x20 ){}          // clear the interrupt status.
      
      /*
       *   Disable the WIFI_INT and wait for it to be cleared.
       */
      PORTE_PCR5 &= ~PORT_PCR_IRQC_MASK;    // BSP_GPIO_WIFI_INT_PIN
      while( PORTE_PCR5 & PORT_PCR_IRQC_MASK ){}   // disable the interrupt.
      
      Custom_HW_InterruptHandler(pointer);
      return;
  }
  
  assert(port >= PORTFIRST && port <= PORTLAST);

  /* Call all ISRs with pending interrupts */
  for (i = 0; i < MAX_PORT_ISRS; i++)
    {
      if (g_isr[port][i].isr != 0)
        {
          if (lwgpio_int_get_flag(g_isr[port][i].plwgpio))
            {
              g_isr[port][i].isr(g_isr[port][i].plwgpio);
              /*
               *   Clearing of the ISRs' flags is the responsibility of the individual ISR's.
               *   lwgpio_int_clear_flag(g_isr[port][i].plwgpio);
               */
            }
        }
    }
}

static void register_isr(LWGPIO_STRUCT_PTR plwgpio, whistle_lwgpio_isr_mux_isr_t isr)
{
    int i;
    port_t port = (port_t)LWGPIO_PORT_FROM_ID(plwgpio->flags);
    
    assert(port >= PORTFIRST && port <= PORTLAST);
    
    for (i = 0; i < MAX_PORT_ISRS; i++)
    {
        if (g_isr[port][i].isr == NULL)
        {
            g_isr[port][i].plwgpio = plwgpio;
            g_isr[port][i].isr = isr;
            return;
        }
    }
    assert(0);
}

static void unregister_isr(port_t port, whistle_lwgpio_isr_mux_isr_t isr)
{
    int i;
    
    assert(port >= PORTFIRST && port <= PORTLAST);
    
    for (i = 0; i < MAX_PORT_ISRS; i++)
    {
        if (g_isr[port][i].isr == isr)
        {
            g_isr[port][i].isr = NULL;
            return;
        }
    }
    assert(0);
}

/* Register ISR with this facility, install the interrupt, clear it, and enable it */
void whistle_lwgpio_isr_install(LWGPIO_STRUCT_PTR lwgpio, _int_priority priority, whistle_lwgpio_isr_mux_isr_t isr)
{
  uint_32 vector = lwgpio_int_get_vector(lwgpio);
  
  /* install gpio interrupt service routine */
  _int_install_isr(vector, whistle_lwgpio_isr_mux, (void *) lwgpio);

  /* set the interrupt level, and unmask the interrupt in interrupt controller*/
  _bsp_int_init(vector, priority, 0, TRUE);
  
  /* Clear interrupt status register so you don't get a "fake" interrupt immediately. */
  lwgpio_int_clear_flag(lwgpio);
  
  /* enable interrupt on GPIO peripheral module*/
  lwgpio_int_enable(lwgpio, TRUE);
  
  register_isr(lwgpio, isr);
}
