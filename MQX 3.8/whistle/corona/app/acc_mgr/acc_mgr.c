/*
 * acc_mgr.c
 *
 *  Created on: Mar 31, 2013
 *      Author: Ben McCrea
 */

#include <mqx.h>
#include <bsp.h>
#include <watchdog.h>
#include "wassert.h"
#include "app_errors.h"
#include "rtc_alarm.h"
#include "acc_mgr.h"
#include "accel_hal.h"
#include "accdrv_spi_dma.h"
#include "cormem.h"
#include "sys_event.h"
#include "evt_mgr.h"
#include "cfg_mgr.h"
#include "min_act.h"
#include "pb.h"
#include "pb_encode.h"
#include "persist_var.h"
#include "whistlemessage.pb.h"
#include "wmp_accel_encode.h"
#include "wmp_datadump_encode.h"
#include "wmp.h"
#include "pwr_mgr.h"


#define ACCMGR_DEV 0
#if ACCMGR_DEV == 1
    #define dev_print(...) corona_print(__VA_ARGS__)
    #define dev_led(led, on) set_debug_led(led, on)
#else
    #define dev_print(...) {}
    #define dev_led(...) {}
#endif

// -- Compile Switches --

// Define this to calculate and display data encode and flush times in ms
//#define SHOW_ELAPSED_PROCESSING_TIMES

// When SHOW_ELAPSED_PROCESSING_TIMES is enabled this is used to set a 
//   limit for which times below are not logged
#ifdef SHOW_ELAPSED_PROCESSING_TIMES
#define SHOW_ELAPSED_PROCESSING_TIME_ALERT_LEVEL 10
#endif

// -- Macros and Datatypes --

#define MOTION_DETECT_EVENT             0x00000001
#define NOMOTION_TIMEOUT_EVENT          0x00000002
#define DMA_BUFFER_RDY_EVENT            0x00000004
#define DMA_LAST_BUFFER_EVENT           0x00000008
#define ACCMGR_GRACEFUL_SHUTDOWN_EVENT  0x00000010
#define ACCMGR_START_UP_EVENT           0x00000020
#define ACCMGR_RESET_EVENT              0x00000040
#define PROCESS_RAW_BUF_EVENT           0x00000080
#define FLUSH_WMP_NOW                   0x00000100
#define ACCMGR_URGENT_SHUTDOWN_EVENT    0x00000200
#define DMA_BUFFER_OVERRUN              0x00000600


// Max. size for the WMP encoded message buffer sent to EVTMGR
#define MAX_PB_STREAM_PAYLOAD_SIZE  (1024)

// If this define is TRUE than the RTC at the time of each datadump will be used
//  as the timestamp for each data message.  If it is FALSE than the assumed 
//  accelerometer sample rate will be used. 
// NCY: If we calibrate each unit's sample rate I would prefer to use the number 
//  of samples to calculate the elapsed time. However, with the current drift and
//  no calibration the RTC will reduce the accumulation of errors.
#define USE_RTC_FOR_START   1

// Buffer for encoded WMP message
// Size is 1/2 of the DMA buffer because only 8-bits of each 16-bit sample has data
#define PB_SOURCE_BUFFER_SIZE       (DMA_BUFFER_SIZE >> 1)
#define PB_ENCODING_BUFFER_SIZE     (2060)
#define PB_ENCODING_BUFFER_COUNT	(5)

#define MILLISECONDS_PER_DMA_BUFFER (1*SPIDMA_ACCEL_WTM_SIZE*MILLISECONDS_PER_SAMPLE)

#define DMA_MONITOR_TIMEOUT_MS     (MILLISECONDS_PER_DMA_BUFFER + 200)

// How many milliseconds of sleep would we consider bogus?  Nate says 10 days.
#define MAX_BOGUS_SLEEP_TIME    (10 * 24 * 60 * 60 * 1000)

typedef enum
{
    S_INIT = 0,
    S_IDLE,
    S_SNOOZE,
    S_START_LOGGING,
    S_LOGGING,
    S_SYSREBOOT,
    S_ERROR
} ACCMGR_STATE_t;

typedef enum
{
    ACCEL_LOGGING = 0,
    MOTION_START,
    MOTION_STOP,
    ACCEL_ERROR
} ACCEL_EVENT_t;

typedef struct accel_raw_data_queue_type
{
    QUEUE_ELEMENT_STRUCT HEADER;        // standard queue element header.
    int8_t               *pData;        // pointer to raw data.
    uint32_t             len;           // length of the raw data
    uint64_t             timestamp;     // timestamp corresponding to the data
    ACCEL_EVENT_t        accel_event;   // Accel motion event: new motion, end of motion, etc
    
}acc_raw_qu_elem_t, *acc_raw_qu_elem_ptr_t;

// -- Global Private Data

static LWEVENT_STRUCT g_accmgr_evt;
static ACCMGR_STATE_t g_accmgr_state = S_INIT;
static boolean g_shutdown_time = FALSE;
static uint_16 g_num_buffers = 0;
static RTC_ALARM_t g_snooze_alarm = 0;
static uint_8 g_accel_urgent_shutdown = 0;
static uint_64 g_current_buffer_start_epoch, g_current_buffer_end_epoch;
static TIME_STRUCT g_start_logging_timestamp;


#ifdef SHOW_ELAPSED_PROCESSING_TIMES
//DEBUG - determine encode and flush time
static TIME_STRUCT dbg_time_now;
#endif

static QUEUE_STRUCT g_accel_data_qu;

static evt_obj_t g_accel_obj;
static boolean g_dma_buffer_needs_processed = FALSE;

// ProtoBufs
static uint_8 g_encoding_buffer[PB_ENCODING_BUFFER_COUNT][PB_ENCODING_BUFFER_SIZE];
static uint_8 g_encoding_buffer_current_index = 0;
static uint_8 g_encoding_buffers_in_use = 0; 
static WMP_ACCENC_partial_handle_t g_accelData_payload_msg_handle;
static WMP_DATADUMP_accelData_submsg_handle_t g_dataDump_accel_msg_handle;
static uint32_t g_message_thirtytwo_bit_crc;
static pb_ostream_t g_out_stream;

// -- Local Prototypes
static err_t ACCMGR_start_logging(void);
static void accmgr_stop_on_error(void);
static err_t ACCMGR_cleanup(void);
static void ACCMGR_nomotion_timout_callback(void);
static err_t ACCMGR_setup_wmp_header(uint_64 buffer_timestamp);
static void accmgr_flush_to_evtmgr(void);
static err_t accmgr_process_dma_data( boolean motion_start, boolean motion_stop  );
static void accmgr_qu_raw_data(int8_t *pData, uint32_t len, ACCEL_EVENT_t accel_event);
static void accmgr_calc_activity_metric(acc_raw_qu_elem_ptr_t pQuElem);
static void accmgr_wakeup_callback(void);
static void accmgr_start_snooze_alarm( void );
static void ACC_verify_hw(void);
static void accmgr_execute_shutdown( boolean urgent );

#if (!USE_RTC_FOR_START)
static uint_64 accmgr_calc_buffer_timestamp( uint_16 num_buffers );
#endif

// -- Private Functions --
#ifdef SHOW_ELAPSED_PROCESSING_TIMES
static void tictoc( void )
{
    static TIME_STRUCT start_time;
    static uint8_t timing=0;
    if (timing)
    {
        TIME_STRUCT end_time;
        uint32_t elaspsed_ms;
        RTC_get_time_ms(&end_time);
        elaspsed_ms = (end_time.SECONDS - start_time.SECONDS) * 1000 + (end_time.MILLISECONDS - start_time.MILLISECONDS); 
        corona_print("%d\n", elaspsed_ms);
        timing=0;
    } else
    {
        timing=1;
        RTC_get_time_ms(&start_time);
    }
}
#endif
// Update state and notify PWRMGR if we transition to a state where we care about remaining on.
static void ACCMGR_set_state(ACCMGR_STATE_t state)
{
	boolean was_idle = S_IDLE >= g_accmgr_state;
	
	g_accmgr_state = state;
	
	if ( S_IDLE == state )
	{
		if ( !was_idle )
		{
			PWRMGR_VoteForOFF(PWRMGR_ACCMGR_VOTE);
		}
	}
	else
	{
		if ( was_idle )
		{
			PWRMGR_VoteForON(PWRMGR_ACCMGR_VOTE);
		}
	}
}

/*
 *   Return non-zero is accelerometer comm is OK.
 */
uint_8  lis3dh_comm_ok(void)
{
    uint_8 who_am_i;

    if((ERR_OK != accel_read_register (SPI_STLIS3DH_WHO_AM_I, &who_am_i, 1)) || (who_am_i != 0x33))
    {
        return 0;
    }

    return 1;
}

// Callback Routines for Interrupt Handlers
static void accmgr_wakeup_callback(void)
{
    PWRMGR_VoteForAwake( PWRMGR_ACCMGR_VOTE );

    // Send the motion detect event
    _lwevent_set(&g_accmgr_evt, MOTION_DETECT_EVENT);
}

void ACCMGR_log_buffer_end_time( void )
{
    RTC_get_ms(&g_current_buffer_end_epoch);
}

// Calculat the start time of this buffer based either on the RTC clock and the
//  assumed duration of a single buffer or on the accumulated number of buffers
//  since acquisition was started.
static int _ACCMGR_start_buffer(void)
{
    // Check for a DMA buffer overrun
    if (g_dma_buffer_needs_processed)
    {
        /* Buffer flag in wrong state, overrun? */
        _lwevent_set(&g_accmgr_evt, DMA_BUFFER_OVERRUN);
        
        /* Drop this interrupt */
        return 0;
    }
    
    g_dma_buffer_needs_processed = TRUE;
    
#if (USE_RTC_FOR_START)
    // Subtract the expected length of the buffer to get the time at the start of this buffer
    // Note that this assumes we are fully emptying the buffer at each interrupt
    // If this is not the case the calculated start time for the first buffer will be wrong
    // This value is updated in the DMA callback in accdrv_spi_dma
    g_current_buffer_start_epoch = g_current_buffer_end_epoch - MILLISECONDS_PER_DMA_BUFFER;
#else
    g_current_buffer_start_epoch = accmgr_calc_buffer_timestamp( g_num_buffers++ );
#endif
    
    return 1;
}

static void ACCMGR_dma_buffer_rdy_callback(void)
{
    PWRMGR_VoteForAwake( PWRMGR_ACCMGR_VOTE );
    
    if (_ACCMGR_start_buffer())
    {
        // Send the DMA buffer ready event
        _lwevent_set(&g_accmgr_evt, DMA_BUFFER_RDY_EVENT);
    }
}

static void ACCMGR_dma_last_buffer_callback(void)
{
    PWRMGR_VoteForAwake( PWRMGR_ACCMGR_VOTE );

    if (_ACCMGR_start_buffer())
    {
        // Send the DMA last buffer event
        _lwevent_set(&g_accmgr_evt, DMA_LAST_BUFFER_EVENT);
    }
}

static void ACCMGR_nomotion_timout_callback(void)
{   
    PWRMGR_VoteForAwake( PWRMGR_ACCMGR_VOTE );
    
    g_snooze_alarm = 0;

    // Send the no motion timeout event
    _lwevent_set(&g_accmgr_evt, NOMOTION_TIMEOUT_EVENT);
}

static void ACCMGR_system_shutdown_callback(sys_event_t sys_event)
{
    // Send the shut down event
    _lwevent_set(&g_accmgr_evt, ACCMGR_GRACEFUL_SHUTDOWN_EVENT);
}

#if (!USE_RTC_FOR_START)
static uint_64 accmgr_calc_buffer_timestamp( uint_16 num_buffers )
{
    MQX_TICK_STRUCT timestamp_in_ticks;
    TIME_STRUCT new_timestamp; 
    
    // Convert starting timestamp to ticks
    _time_to_ticks( &g_start_logging_timestamp, &timestamp_in_ticks );
    
    // Use number of buffers to calculate # of milliseconds since logging was started
    _time_add_msec_to_ticks( &timestamp_in_ticks, num_buffers * MILLISECONDS_PER_DMA_BUFFER );
    
    // Convert ticks back to MQX time
    _ticks_to_time( &timestamp_in_ticks, &new_timestamp );
    
    return (uint_64) ( ((uint_64)new_timestamp.SECONDS * MILLISECONDS_PER_SECOND) + (uint_64)new_timestamp.MILLISECONDS );
}
#endif

// This callback executes when EVTMGR is done posting, so it doesn't block.
// Callers of EVTMGR_post() must be careful not to take away the resources/pointers that post() uses,
//   until this callback returns.
static void ACCMGR_event_post_callback(err_t err, evt_obj_ptr_t pAccelObj)
{
	wassert(--g_encoding_buffers_in_use >= 0);
	
    if( (err == ERR_OK) && (S_IDLE == g_accmgr_state) )
    {
        // We're shut down and EVTMGR has processed the last buffer.
        // Now request an event manager flush as our very last action.
        EVTMGR_request_flush(); 
    }
}

static err_t ACCMGR_setup_wmp_header(uint_64 buffer_timestamp)
{
    uint_64 temp_timestamp = 0;
    uint_8 accel_fullscale;
    
// Initialise the stream
//  first param: dest. buffer where stream will be stored
//  second param: max. size of the dest. buffer 
    
    if (g_encoding_buffers_in_use < PB_ENCODING_BUFFER_COUNT)
    {
    	g_encoding_buffer_current_index = (++g_encoding_buffer_current_index % PB_ENCODING_BUFFER_COUNT);
    	g_out_stream = pb_ostream_from_buffer(g_encoding_buffer[g_encoding_buffer_current_index], PB_ENCODING_BUFFER_SIZE);
    	g_encoding_buffers_in_use++;
    }
    else
    {
    	return ERR_WMP_SETUP_FAIL;
    }
    
    //Write DataDump AccelData sub-message tag
    wassert( WMP_DATADUMP_accelData_init(&g_dataDump_accel_msg_handle, &g_out_stream, true) );
    
    UTIL_time_with_boot(buffer_timestamp, &temp_timestamp);
    
    if (g_st_cfg.acc_st.full_scale == FULLSCALE_8G) // Check this first since it is default
    {
        accel_fullscale = 8;
    } else if (g_st_cfg.acc_st.full_scale == FULLSCALE_16G)
    {
        accel_fullscale = 16;
    } else if (g_st_cfg.acc_st.full_scale == FULLSCALE_4G)
    {
        accel_fullscale = 4;
    } else if (g_st_cfg.acc_st.full_scale == FULLSCALE_2G)
    {
        accel_fullscale = 2;
    } else // unsupported type
    {
        return ERR_WMP_SETUP_FAIL;
    }
    
    // Write header portion of AccelData
    WMP_ACCEL_header_encode(&g_out_stream, 
            temp_timestamp, // relative time with boot number
            time_calc_abs_utc_time(buffer_timestamp), //absolute time WITHOUT boot number
            g_st_cfg.acc_st.compression_type, //encoding_type
            SAMPLERATE_IN_HZ,
            accel_fullscale);

    //Initiate partial encoding handle / context
#ifdef SHOW_ELAPSED_PROCESSING_TIMES
    corona_print("ENC_INIT: ");    
    tictoc();
#endif
    wassert( WMP_ACCEL_payload_encode_init(&g_accelData_payload_msg_handle,
             &g_out_stream, g_st_cfg.acc_st.compression_type,
             &g_message_thirtytwo_bit_crc, true) );
#ifdef SHOW_ELAPSED_PROCESSING_TIMES
    tictoc();
#endif

    return ERR_OK;
}

/*
 * Close a WMP message and flush it to EVTMGR
 */
static void accmgr_flush_to_evtmgr(void)
{    
    // Don't allow going to sleep until the flush is complete
    PWRMGR_VoteForAwake( PWRMGR_ACCOFF_VOTE );
    
    #if (MEDIAN_FILTER_DATA)
        // Only reset the median filter when a WMP message is closed.
        // This reduces the influence of start-up transients and 
        //  should be as correct as possible
        MIN_ACT_reset_median_filter();
    #endif
    
    #if (CALCULATE_MINUTES_ACTIVE)
        if(g_st_cfg.acc_st.calc_activity_metric)
        {
            // Finalize the point calculation at the end of the WMP message
            // This will also server to reset the filters when the next block is processed
            MIN_ACT_finish();
        }
    #endif
        
    
	
    // Finish up the WMP message
    wassert( WMP_ACCEL_payload_encode_close(&g_accelData_payload_msg_handle) );
    
    WMP_ACCEL_footer_encode(g_accelData_payload_msg_handle.stream, g_message_thirtytwo_bit_crc);

    // Close DataDump AccelData sub-message     
    wassert( WMP_DATADUMP_accelData_close(&g_dataDump_accel_msg_handle) );
    
    // Set up the Accel Obj for passing to EVTMGR
    g_accel_obj.pPayload = g_encoding_buffer[g_encoding_buffer_current_index];
    g_accel_obj.payload_len = g_out_stream.bytes_written;

    corona_print("AM flush %d\n", g_out_stream.bytes_written);
    
    if( 0 == g_accel_obj.payload_len )
    {
        /*
         *   Don't post an empty event.  Instead, raise an error, and call the callback
         *     directly, rather than waiting on EVTMGR to call it.
         */
        PROCESS_NINFO( ERR_ACCMGR_ZERO_LEN_EVENT, "0 len acc" );
        ACCMGR_event_post_callback( ERR_ACCMGR_ZERO_LEN_EVENT, &g_accel_obj );
    }
    else
    {
        EVTMGR_post(&g_accel_obj, ACCMGR_event_post_callback);
    }
}


/*
 *   Track when accel goes to sleep.
 *     When we get a sleep duration, go ahead and log that to EVTMGR.
 */
void ACCMGR_wmp_track_sleep(uint8_t sleep_start)
{
    uint8_t was_sleeping = pmem_flag_get(PV_FLAG_ACCEL_WAS_SLEEPING);
    TIME_STRUCT now;

    RTC_get_time_ms(&now);
    if( sleep_start ) // Start Sleep
    {
        
        // Make sure we don't have some source of invalid pairing of sleep states.
        if (was_sleeping)
        {
            PROCESS_NINFO( ERR_ACCMGR_ALREADY_ASLEEP, "strt'd: %u  now: %u\n", __SAVED_ACROSS_BOOT.sleep_rel_start_seconds, now.SECONDS );
        }
        
        /*
         *   Make note of the time we started.
         */
        PMEM_FLAG_SET(PV_FLAG_ACCEL_WAS_SLEEPING, 1);
        PMEM_VAR_SET(PV_SLEEP_REL_START_SECONDS, &(now.SECONDS));
        PMEM_VAR_SET(PV_SLEEP_REL_START_MILLIS, &(now.MILLISECONDS));
    }
    else if( (0 == sleep_start) && (was_sleeping) ) // Close Sleep
    {
        uint64_t duration;
        uint64_t rel_start_ms;
        uint64_t rel_stop_ms;
        uint64_t abs_start_ms;
        
        /*
         *   We stopped sleeping and need to start logging, so make note of the fact that
         *      we are not sleeping anymore.  Calculate duration based on that.
         */
        PMEM_FLAG_SET(PV_FLAG_ACCEL_WAS_SLEEPING, 0);
        
        
        if( __SAVED_ACROSS_BOOT.sleep_rel_start_seconds > now.SECONDS )
        {
            PROCESS_NINFO( ERR_ACCMGR_SLEEP_START_BIGGER, "strt: %u  stp: %u\n", __SAVED_ACROSS_BOOT.sleep_rel_start_seconds, now.SECONDS );
            return;
        }
        rel_start_ms = ((uint64_t)__SAVED_ACROSS_BOOT.sleep_rel_start_seconds) * 1000 + (uint64_t) __SAVED_ACROSS_BOOT.sleep_rel_start_millis;
        rel_stop_ms = UTIL_time_ms_from_struct(&now);
        duration = rel_stop_ms - rel_start_ms;
        
        corona_print("slpt %ums\n", duration);
        
        if( duration > MAX_BOGUS_SLEEP_TIME )
        {
            PROCESS_NINFO( ERR_ACCMGR_SLEEP_TIME_BOGUS, "dur: %u strt: %u stp: %u\n", duration, rel_start_ms / 1000, rel_stop_ms / 1000);
            return;
        } else if (duration < 20) // Ignore the sleep if it was less than 20 ms long ( 1 sample @ 50 Hz)
        {
            return;
        }
        
        abs_start_ms = time_calc_abs_utc_time(rel_start_ms);
        // Add the bootnumber to the relative start time just prior to encoding
        UTIL_time_with_boot(rel_start_ms, &rel_start_ms); 
        WMP_post_accelSleep(&abs_start_ms, &rel_start_ms, &duration);
    }
}

/*
 *   Simple function to kill the system if something is wrong with ACC hardware.
 *     Upon reboot, we will come to the same conclusion and go solid yellow light.
 *     This will let us know more easily though through a Sherlock wassertion and logEntries.
 */
static void ACC_verify_hw(void)
{
    if( 0 == lis3dh_comm_ok() )
    {
        PROCESS_NINFO( ERR_ACCMGR_HARDWARE_BROKEN, NULL );
        /*
         *   Purposefully do not wassert(0) here, so that we can know about it on server side.
         */
    }
}

/*
 *   Start logging accel data.
 */
static err_t ACCMGR_start_logging(void)
{
    err_t retval = ERR_OK;

    corona_print("AM:Log\n");
    ACC_verify_hw();

    if (accdrv_bkgnd_is_running())
    {
        return ERR_ACCMGR_ALREADY_RUNNING;
    }
    if ((retval = accel_enable_wtm(TRUE)) != ERR_OK)
    {
        return retval;
    }
#if 0
// This check is not necessary
    uint_8 fifo_status;
    // Read FIFO level to determine how many bytes we should read
    if ((retval = accel_read_fifo_status(&fifo_status)) != ERR_OK)
    {
        return retval;
    }
    if (fifo_status & STLIS3DH_FIFO_LEVEL_MASK)
    {
        corona_print("AM:FIFO !empty\n");
    }
#endif
    ACCMGR_wmp_track_sleep(0);

    // Disable context switch until we've started the background: this is necessary to ensure
    // we don't miss the first DMA request 
    _task_stop_preemption(); // disable context switching

    if ((retval = accel_config_fifo(STLIS3DH_FIFO_STREAM_MODE,
            ACCEL_FIFO_WTM_THRESHOLD)) != ERR_OK)
    {
        return retval;
    }

    // The current time in seconds and milliseconds is captured here, at the time that FIFO sampling is enabled
    RTC_get_time_ms(&g_start_logging_timestamp); 
    
    // Start the background
    accdrv_start_bkgnd();

    _task_start_preemption(); // re-enable context switching
    
    return retval;
}

static void accmgr_stop_on_error(void)
{
    g_dma_buffer_needs_processed = FALSE;
    
    // force a shutdown of the driver background
    accdrv_stop_bkgnd(true);

    // Clean up the driver and display an error if the FIFO has an overrun
    ACCMGR_cleanup(); 
    
    // De-init SPI DMA before re-init is attempted
    accdrv_deinit();
}

// Clean up after receiving last DMA buffer 
static err_t ACCMGR_cleanup(void)
{
    err_t retval = ERR_OK;

    // Last DMA buffer was received, so turn off DMA and SPI
    accdrv_cleanup();

    // if ISR didn't shut down properly then force the background off now
    if (accdrv_bkgnd_is_running())
    {
        // Error: background didn't stop!
        retval = ERR_ACCMGR_STOP_ERROR;
        PROCESS_NINFO(retval, "couldn't stop");
    }

    // Turn off the FIFO & Disable Watermark Interrupt
    if (accel_config_fifo(STLIS3DH_FIFO_BYPASS_MODE, ACCEL_FIFO_WTM_THRESHOLD)
            || accel_enable_wtm(FALSE))
    {
        // Above error takes precedence for returning an error code
        if (retval == ERR_OK)
        {
            retval = ERR_ACCDRV_REG_WRITE_FAIL;
            PROCESS_NINFO(retval, NULL);
        }
    }

    return retval;
}

/*
 * Process DMA buffer data and place in a queue for the offload task
 */
static err_t accmgr_process_dma_data( boolean motion_start, boolean motion_stop  )
{
    err_t err = ERR_OK;
    ACCEL_EVENT_t evt = ACCEL_LOGGING; 
    
    if ( motion_start )
    {    
        g_num_buffers = 0;
        evt = MOTION_START;
    }
    else if ( motion_stop )
    {
        evt = MOTION_STOP;
    }
    
    // Pack the DMA buffer data
    if ((err = accdrv_pack_dmabuf()) != ERR_OK)
    {
        PROCESS_NINFO(err, "bad FIFO");

        // Tell the offload task to flush the WMP now
        PWRMGR_VoteForON(PWRMGR_ACCOFF_VOTE); // here guarantees task runs
        _lwevent_set(&g_accmgr_evt, FLUSH_WMP_NOW);

        // Returns ERR_ACCDRV_BAD_BUFFER and relies on the caller to handle the
        // catastrophic error by initiating a ACCMGR_Reset().
        return err;
    }
    else
    {        
        // Place the DMA data in a queue for the offload task
        accmgr_qu_raw_data((int_8 *) accdrv_get_bufptr(), PACKED_DMA_BUFFER_SIZE , evt );  
    }
    
    g_dma_buffer_needs_processed = FALSE;
    
    return err;
}

/*
 *   Queue raw data up to be processed by the offload task.
 */
static void accmgr_qu_raw_data(int8_t *pData, uint32_t len, ACCEL_EVENT_t accel_event )
{
    acc_raw_qu_elem_ptr_t pQuElem = NULL;

    /*
     *   Alloc memory for queue and memcpy'd data.
     *     We will free it later in the offload task, after it is consumed.
     */
    pQuElem = walloc( sizeof(acc_raw_qu_elem_t) );
    pQuElem->pData = walloc(len);
    
//    corona_print("Elapsed time: %d\n", (int)(g_current_buffer_epoch - (uint_64) (((uint_64) g_start_logging_timestamp.SECONDS) * 1000 + (uint_64) g_start_logging_timestamp.MILLISECONDS)));
//    g_current_last_buffer_timestamp = g_current_buffer_epoch;
    /*
     *   Copy over data to queue element.
     */
    pQuElem->len = len;
    pQuElem->timestamp = g_current_buffer_start_epoch;
    pQuElem->accel_event = accel_event;
    memcpy(pQuElem->pData, pData, len);
    
//    // This is if you want to overwite the old accel data in order to ensure that you aren't reading stale accel data
//    memset((uint8_t *) pData, 0x7F, len);  
    
    /*
     *   Put queue element into the queue.
     */
    wassert( TRUE == _queue_enqueue(&g_accel_data_qu, (QUEUE_ELEMENT_STRUCT_PTR) pQuElem) );
    
    /*
     *   Tell the offload task it has raw data to process.
     */
    PWRMGR_VoteForON(PWRMGR_ACCOFF_VOTE); // here guarantees task runs
    _lwevent_set(&g_accmgr_evt, PROCESS_RAW_BUF_EVENT);
}

#if (MEDIAN_FILTER_DATA)
/*
 *   Applies a three point median filter to the input data given a fully populated qu element.
 */
static void accmgr_median_filter(acc_raw_qu_elem_ptr_t pQuElem)
{
    median_filter_data( pQuElem->pData, pQuElem->len);
}
#endif

/*
 *   Calculates the activity metric, given a fully populated qu element.
 */
#if (CALCULATE_MINUTES_ACTIVE)
    static void accmgr_calc_activity_metric(acc_raw_qu_elem_ptr_t pQuElem)
    {
        #ifdef SHOW_ELAPSED_PROCESSING_TIMES
            TIME_STRUCT before, after;
            uint32_t time_elapsed;
            static uint16_t buffer_count = 0;
            RTC_get_time_ms(&before);
        #endif
       
        MIN_ACT_calc( pQuElem->pData,
                      pQuElem->len,
                      0,
                      pQuElem->timestamp,
                      time_calc_abs_local_time(pQuElem->timestamp) );
        
        #ifdef SHOW_ELAPSED_PROCESSING_TIMES
        RTC_get_time_ms(&after);
            if ( after.SECONDS > before.SECONDS )
                time_elapsed = 1000*(after.SECONDS - before.SECONDS) - before.MILLISECONDS + after.MILLISECONDS;
            else 
                time_elapsed = after.MILLISECONDS - before.MILLISECONDS;
            if (++buffer_count > 4)
            {
                buffer_count= 0;
                #if !defined(PWRMGR_BREADCRUMB_ENABLED) || PWRMGR_BREADCRUMB_ENABLED == 1
                    corona_print("ACT %d\n", time_elapsed ); 
                #endif
                #if PWRMGR_BREADCRUMB_ENABLED
                    pwr_corona_print("ACT %d\n", time_elapsed );
                #endif  
            }
        #endif   
    }
#endif

static void accmgr_start_snooze_alarm( void )
{   
    // If the old alarm is still active, kill it now
    RTC_cancel_alarm( &g_snooze_alarm );
    
    // Restart the snooze alarm
    g_snooze_alarm = RTC_add_alarm_in_seconds( g_st_cfg.acc_st.seconds_to_wait_before_sleeping, (rtc_alarm_cbk_t) ACCMGR_nomotion_timout_callback );
    wassert( g_snooze_alarm );
}

static void accmgr_execute_shutdown( boolean urgent )
{
    // Disable the wake-up pin interrupt
    accdrv_enable_wakeup( 0 );
    
    if ( !urgent )
    {
        // Perform an orderly shutdown
    
        // If we are logging, stop now
        switch (g_accmgr_state)
        {
        case S_START_LOGGING:
        case S_LOGGING:
            // Logging: Shutdown will happen after background is turned off
            g_shutdown_time = TRUE;
            accdrv_stop_bkgnd(false); // Turn off background logging  
            break;
        case S_SNOOZE:
        case S_IDLE: 
            // Close a sleep message if we were sleeping
            // The sleep tracker has an internal variable that should not let us close a sleep
            //  message that was not started.
            // This needs to fall through to shut down the accelerometer after closing the sleep method
            ACCMGR_wmp_track_sleep(0);
        default:
            // Shut down now - not logging so go directly to the idle state
            accel_shut_down(); // turn off the accelerometer
            
            // Request an event manager flush
            EVTMGR_request_flush();
            
            ACCMGR_set_state(S_IDLE);
            break;
        }
    }
    else
    {
        // Shutdown now without caring if we lose data
        accdrv_stop_bkgnd(true); // Turn off background logging forcefully
        accel_shut_down(); // turn off the accelerometer
        ACCMGR_set_state( S_SYSREBOOT );
    }
}

// -- Public Functions --

/*
 * Startup ACCMGR: Enter S_SNOOZE, enable the wakeup pin and wait to start logging
 * when motion is detected
 * 
 */
err_t ACCMGR_start_up(void)
{
    if (g_accmgr_state != S_IDLE)
    {
        return ERR_ACCMGR_NOT_IDLE_ERROR;
    }
    
    _lwevent_set(&g_accmgr_evt, ACCMGR_START_UP_EVENT);
    return ERR_OK;
}

/*
 * Shutdown ACCMGR. Finish logging, disable the wakeup pin and transition to S_IDLE
 * 
 */
err_t ACCMGR_shut_down( boolean urgent )
{
    if ( urgent )
    {
        g_accel_urgent_shutdown = 1;
        _lwevent_set(&g_accmgr_evt, ACCMGR_URGENT_SHUTDOWN_EVENT);  // purposefully don't assert(0) here.  Urgent so...
    }
    else
    {
        _lwevent_set(&g_accmgr_evt, ACCMGR_GRACEFUL_SHUTDOWN_EVENT );
    }

    return ERR_OK;
}

/*
 * Reset ACCMGR and reinitialize the accelerometer
 * 
 */
void ACCMGR_Reset(void)
{
    // Set the state to S_ERROR to signal event processing blocks that we are resetting
    ACCMGR_set_state(S_ERROR);
    
    // Send the ACCMGR reset event
    _lwevent_set(&g_accmgr_evt, ACCMGR_RESET_EVENT);
}

// Returns TRUE if ACCMGR is ready to start up, otherwise FALSE
boolean ACCMGR_is_ready(void)
{
    return (g_accmgr_state > S_INIT);
}

// Returns TRUE if ACCMGR is in the S_SNOOZE state, waiting to detect motion
boolean ACCMGR_is_snoozing(void)
{
    return (g_accmgr_state == S_SNOOZE);
}

// Returns TRUE if ACCMGR is in the S_INIT or S_IDLE state
boolean ACCMGR_is_shutdown(void)
{
    return (g_accmgr_state <= S_IDLE);
}

// Returns TRUE if the ACCMGR offload task queue is empty
boolean ACCMGR_queue_is_empty( void )
{
    return  (_queue_get_size(&g_accel_data_qu) == 0);
}

/*
 *   Apply/Flush accelerometer configs (which are all static).
 */
void ACCMGR_apply_configs(void)
{
    accel_apply_configs();
    CFGMGR_flush( CFG_FLUSH_STATIC );
}

// -- Task Functions --

/*
 *   ACCMGR task handles lower priority ACCMGR duties, that we don't want to block
 *   normal ACCMGR/DMA servicing activities.
 *   
 */
void ACCOFFLD_tsk(uint_32 dummy)
{
    _mqx_uint mask;
    static wmp_in_progress = FALSE;
    
    while(1)
    {
        
        PWRMGR_VoteForOFF(PWRMGR_ACCOFF_VOTE);
        PWRMGR_VoteForSleep(PWRMGR_ACCOFF_VOTE);
        
		_watchdog_stop();

        _lwevent_wait_ticks( &g_accmgr_evt, PROCESS_RAW_BUF_EVENT | FLUSH_WMP_NOW, FALSE, 0);
        
		_watchdog_start(60*1000);

        PWRMGR_VoteForAwake(PWRMGR_ACCOFF_VOTE);
        PWRMGR_VoteForON(PWRMGR_ACCOFF_VOTE); // This ON vote should be redundant, but do it jic
        
        mask = _lwevent_get_signalled();
        
        if( mask & PROCESS_RAW_BUF_EVENT)
        {
            dev_print("%s: PROCESS_RAW_BUF_EVENT\n", __func__);

            while( _queue_get_size(&g_accel_data_qu) > 0 )
            {
                acc_raw_qu_elem_ptr_t pQuElem = (acc_raw_qu_elem_ptr_t) _queue_dequeue(&g_accel_data_qu);
                
                wassert( NULL != pQuElem );
                /*
                 *   Apply a median filter to the data if desired
                 */
                #if (MEDIAN_FILTER_DATA)
                    accmgr_median_filter( pQuElem );
                #endif
                    
                /*
                 *   Calculate the activity metric.
                 */
                #if (CALCULATE_MINUTES_ACTIVE)
                    accmgr_calc_activity_metric( pQuElem );
                #endif
                
                /*
                 * Setup a new WMP header if this is the start of a new message
                 */
                if ( !wmp_in_progress )
                {
                    // Start a new WMP message
                    if (ACCMGR_setup_wmp_header( pQuElem->timestamp ) != ERR_OK)
                    {
                        wassert( ERR_OK == PROCESS_NINFO(ERR_WMP_SETUP_FAIL, NULL) );
                    }      
                    wmp_in_progress = TRUE;
                }

                /* 
                 * Encode data into the PB buffer             
                 */
#ifdef SHOW_ELAPSED_PROCESSING_TIMES
                corona_print("ENC_APP: ");    
                tictoc();
#endif
                if (!WMP_ACCEL_payload_encode_append(&g_accelData_payload_msg_handle,
                        (int_8 *) pQuElem->pData, pQuElem->len ))
                {
                    wassert( ERR_OK == PROCESS_NINFO(ERR_WMP_ENCODE_FAIL, NULL) );
                }
#ifdef SHOW_ELAPSED_PROCESSING_TIMES
                tictoc();
#endif
            
                /*
                 * Flush WMP message to the EVTMGR if enough data has accumulated, motion has stopped
                 * or an error has occurred
                 */
                if ((g_out_stream.bytes_written > MAX_PB_STREAM_PAYLOAD_SIZE) ||
                        (MOTION_STOP == pQuElem->accel_event) )
                {                 
                    accmgr_flush_to_evtmgr();
                    wmp_in_progress = FALSE;
                }
                

                /*
                 *   Free the memory that was allocated when queued this element up.
                 */
                wfree(pQuElem->pData);
                wfree(pQuElem);
            }
        }
        
        if ( mask & FLUSH_WMP_NOW )
        {
            dev_print("%s: FLUSH_WMP_NOW\n", __func__);
            accmgr_flush_to_evtmgr();
            wmp_in_progress = FALSE;        
        }
    }
}

void ACC_tsk(uint_32 dummy)
{
    _mqx_uint mask;
    err_t err;

    while (1)
    {
    	_watchdog_stop();    	

    	PWRMGR_VoteForSleep( PWRMGR_ACCMGR_VOTE );
    	
    	// Wait for an event
        _lwevent_wait_ticks(    &g_accmgr_evt, 
                                MOTION_DETECT_EVENT | 
                                NOMOTION_TIMEOUT_EVENT | 
                                DMA_BUFFER_RDY_EVENT | 
                                DMA_LAST_BUFFER_EVENT | 
                                ACCMGR_START_UP_EVENT | 
                                ACCMGR_GRACEFUL_SHUTDOWN_EVENT | 
                                ACCMGR_URGENT_SHUTDOWN_EVENT | 
                                ACCMGR_RESET_EVENT |
                                DMA_BUFFER_OVERRUN, FALSE, 0);
        
        _watchdog_start(60*1000);
        
        // Get the event mask
        mask = _lwevent_get_signalled();

        /* DMA_BUFFER_OVERRUN */
        if (mask & DMA_BUFFER_OVERRUN)
        {
            dev_print("%s DMA_BUFFER_OVERRUN\n", __func__);

            /* Log message */
            PROCESS_NINFO(ERR_ACCMGR_DMA_OVERRUN, "mk:%x rb:%d us:%d as:%d ecb:%d np:%d qs:%d", 
                          mask, PWRMGR_is_rebooting(), 
                          g_accel_urgent_shutdown, g_accmgr_state, 
                          g_encoding_buffers_in_use, g_dma_buffer_needs_processed,
                          _queue_get_size(&g_accel_data_qu));
            
            /* Force graceful shutdown */
            PWRMGR_Reboot(500, PV_REBOOT_ACCEL_FAIL, FALSE);
        }
        
        // Shut down now if the system is going down urgently
        if ( g_accel_urgent_shutdown | (mask & ACCMGR_URGENT_SHUTDOWN_EVENT ) )
        {
            dev_print("%s: ACCMGR_URGENT_SHUTDOWN_EVENT\n", __func__);
            
            _watchdog_stop(); // turn off the task watchdog     	
            accmgr_execute_shutdown( true ); 
            
            // the task will now end
        }

        // First priority: process the DMA buffer
        if (mask & DMA_BUFFER_RDY_EVENT)
        {
            dev_print("%s: DMA_BUFFER_RDY_EVENT\n", __func__);

            if ((g_accmgr_state == S_START_LOGGING)
                    || (g_accmgr_state == S_LOGGING))
            {
                // Process the DMA data and queue it up for the offload task
                if ( ERR_ACCDRV_BAD_BUFFER == accmgr_process_dma_data( S_START_LOGGING == g_accmgr_state, false ) )
                {
                    // Error in DMA buffer! Reset the ACCDRV and start over
                    ACCMGR_Reset();
                }
                else if ( S_START_LOGGING == g_accmgr_state )              
                {
                    // If logging has just started:
                    // 1. Transition to the S_LOGGING state 
                    // 2. Re-enable the motion detect interrupt (it's left disabled until first buffer is received)                                  
                    ACCMGR_set_state(S_LOGGING);
                    accdrv_enable_wakeup( 1 );
                }                       
            } 
            else
            {
                // We should only get a DMA_BUFFER_RDY_EVENT while logging. 
                PROCESS_NINFO(ERR_ACCDRV_BAD_BUFFER, "unexp bufrdy");
            }            
        }

        // First priority: process the DMA buffer
        if (mask & DMA_LAST_BUFFER_EVENT)
        {
            dev_print("%s: DMA_LAST_BUFFER_EVENT\n", __func__);
            corona_print("AM:Lst buf\n");

            // Process the DMA data and queue it up for the offload task
            if ( ERR_ACCDRV_BAD_BUFFER == accmgr_process_dma_data( FALSE, TRUE ) )
            {                            
                // Error in DMA buffer! Reset the ACCDRV and start over
                ACCMGR_Reset();
            }
            else 
            {                              
                // Clean up after receiving last DMA buffer
                ACCMGR_cleanup(); 
                
                // Go to S_IDLE if this was a shut-down
                if (g_shutdown_time)
                {                
                    // Don't start sleeping if we are shutting down
                    g_shutdown_time = FALSE;
                    accel_shut_down(); // turn off the accelerometer
                    
                    ACCMGR_set_state(S_IDLE);
                } 
                else
                {
                    // We are not shutting down, so return to S_SNOOZE and wait for the
                    // next wake-up
                    ACCMGR_wmp_track_sleep(1);
                    ACCMGR_set_state(S_SNOOZE);
                    // Reset the wakeup pin so that we are ready for the next one
                    accdrv_enable_wakeup( 1 ); // re-enable motion detect interrupt
                }   
            }              
        }

        if (mask & ACCMGR_RESET_EVENT)
        {
            corona_print("AM:Rst\n");
            accmgr_stop_on_error();

            // Reinit the driver and accel registers
            accdrv_spidma_init();
            ACC_verify_hw();
            accel_init_regs();                   

            // Reinitialize and enable the wakeup pin, then enter the S_SNOOZE state
            // and wait for the next motion detect
            accdrv_enable_wakeup( 1 );
            ACCMGR_set_state(S_SNOOZE);
            
            // If continuous logging mode is enabled, kick off logging now by simulating a motion event
            if (g_st_cfg.acc_st.continuous_logging_mode)
            {
                accdrv_simulate_wakeup();
            } else
            {
                ACCMGR_wmp_track_sleep( 1 );
            }
        }

        if (mask & MOTION_DETECT_EVENT)
        {
            dev_print("%s: MOTION_DETECT_EVENT\n", __func__);

            switch (g_accmgr_state)
            {
            case S_SNOOZE:
                if ((err = ACCMGR_start_logging()) == ERR_OK)
                {
                    accmgr_start_snooze_alarm();
                    ACCMGR_set_state(S_START_LOGGING);                
                } 
                else
                {
                    // Error while trying to start logging. Restart ACCMGR.
                    PROCESS_NINFO(err, NULL);
                   
                    ACCMGR_Reset();                 
                }
                break;
            default:
                // All other states
                break;
            }
            // note: if we had ability to restart the one-shot it would be done here          
        }

        if (mask & NOMOTION_TIMEOUT_EVENT)
        {	
            dev_print("%s: NOMOTION_TIMEOUT_EVENT\n", __func__);

            switch (g_accmgr_state)
            {
            case S_START_LOGGING:
            case S_LOGGING:
                // Check to see if we should stop logging now
                if (!accdrv_motion_was_detected() && !g_st_cfg.acc_st.continuous_logging_mode)
                {
                    corona_print("AM:Stp\n");
                    // Turn off background logging
                    accdrv_stop_bkgnd(false);
                } 
                else
                {
                    // Reset the Snooze Alarm  
                    accmgr_start_snooze_alarm();
                }
            default:
                // All states: re-enable motion detection
                accdrv_enable_wakeup( 1 );
                break;
            }

        }

        if (mask & ACCMGR_START_UP_EVENT)
        {
            dev_print("%s: ACCMGR_START_UP_EVENT\n", __func__);

            if (g_accmgr_state == S_IDLE)
            {
                accel_start_up();

                accdrv_enable_wakeup( 1 ); // Enable the wake-up pin interrupt 
                ACCMGR_set_state(S_SNOOZE); // Enter S_SNOOZE and wait for motion
                
                // If continuous logging mode is enabled, kick off logging now by simulating a motion event
                // Otherwise start a sleep event because we are waiting for motion
                // Make sure we close any sleep messages prior to simulating the wakeup
                if (g_st_cfg.acc_st.continuous_logging_mode)
                {
                    ACCMGR_wmp_track_sleep(0);
                    accdrv_simulate_wakeup();
                } else
                {
                    ACCMGR_wmp_track_sleep(1);
                }
            }
        }

        if (mask & ACCMGR_GRACEFUL_SHUTDOWN_EVENT)
        {
            dev_print("%s: ACCMGR_GRACEFUL_SHUTDOWN_EVENT\n", __func__);

            // Shut down gracefully
            accmgr_execute_shutdown( false );
        }
        
        if ( g_accmgr_state == S_SYSREBOOT )
        {
            dev_print("%s: S_SYSREBOOT\n", __func__);

            // We've shut down and system is rebooting - exit the task
            goto DONE;
        }
    }
    
DONE:
    _watchdog_stop();    	
    PWRMGR_VoteExit(PWRMGR_ACCMGR_VOTE);
    _task_block(); // Don't exit or our MQX resources may be deleted
}

// -- Task Init  --

void ACCMGR_init(void)
{
    _lwevent_create(&g_accmgr_evt, LWEVENT_AUTO_CLEAR);
    
    _queue_init(&g_accel_data_qu, 0);

    // Try to close a sleep session if we were in one and got killed by something like a assert
    // If we were not sleeping this should have no effect 
    ACCMGR_wmp_track_sleep(0);
    
    // Initialise the accel. spi/dma driver
    accdrv_spidma_init();

    // Register driver callbacks
    accdrv_reg_cbk(ACCDRV_WAKEUP_PIN_CBK,
            (accdrv_cbk_t) accmgr_wakeup_callback);
    accdrv_reg_cbk(ACCDRV_DMA_BUFFER_RDY_CBK,
            (accdrv_cbk_t) ACCMGR_dma_buffer_rdy_callback);
    accdrv_reg_cbk(ACCDRV_DMA_LAST_BUFFER_CBK,
            (accdrv_cbk_t) ACCMGR_dma_last_buffer_callback);
    // Register system callbacks
    sys_event_register_cbk(ACCMGR_system_shutdown_callback,SYS_EVENT_SYS_SHUTDOWN);
    
    /*
     *   Make sure the accelerometer hardware is OK.
     */
    if( 0 == lis3dh_comm_ok() )
    {
        PWRMGR_Fail(ERR_ACCMGR_HARDWARE_BROKEN, "ACC");
    }

    // Configure the accelerometer registers
    accel_init_regs();

    // Start off in shut-down mode: motion detection is disabled.
    ACCMGR_set_state(S_IDLE);

    PWRMGR_VoteForAwake( PWRMGR_ACCMGR_VOTE );
}

