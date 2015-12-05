////////////////////////////////////////////////////////////////////////////////
//! \addtogroup drivers
//! \ingroup corona
//! @{
//
// Copyright (c) 2013 Whistle Labs
//
// Whistle Labs
// Proprietary and Confidential
//
// This source code and the algorithms implemented therein constitute
// confidential information and may comprise trade secrets of Whistle Labs
// or its associates.
//
//! \file adc_sense.c
//! \brief Driver for ADC, sensing VBat and UID.
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
//!
//! \detail This is the ADC driver.  It senses voltage on VBat and UID.
//! \todo ADC Self Calibration.
//! \todo Try out ADC in low power mode.  Does it change results?
//! \todo How frequently should we sample the ADC, and how many samples?
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS
#include <mqx.h>
#include <bsp.h>
#include <stdlib.h>
#include "adc_sense.h"

#define MY_TRIGGER    ADC_PDB_TRIGGER

// NOTE: You must set BSPCFG_ENABLE_ADC0 = 1 in user_config.h to enable the MQX ADC driver.

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
#if MQX_USE_LWEVENTS
static LWEVENT_STRUCT evn;
#endif


////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////


/* ADC device init struct */
const ADC_INIT_STRUCT adc_init =
{
    ADC_RESOLUTION_DEFAULT,     /* resolution */
};

/* Logical channel #1 init struct */
const ADC_INIT_CHANNEL_STRUCT adc_channel_param1 =
{
    ADC0_SOURCE_AD16,                                 /* physical ADC channel */
    ADC_CHANNEL_MEASURE_ONCE | ADC_CHANNEL_START_NOW, /* runs continuously after IOCTL trigger */
    10,                                               /* number of samples in one run sequence */
    0,                                                /* time offset from trigger point in us */
    300000,                                           /* period in us (= 0.3 sec) */
    0x10000,                                          /* scale range of result (not used now) */
    10,                                               /* circular buffer size (sample count) */
    MY_TRIGGER,                                       /* logical trigger ID that starts this ADC channel */
#if MQX_USE_LWEVENTS
    &evn,
    0x01            /* mask of event to be set */
#endif
};

/* Logical channel #2 init struct */
const ADC_INIT_CHANNEL_STRUCT adc_channel_param2 =
{
    ADC0_SOURCE_AD22,                                 /* physical ADC channel */
    ADC_CHANNEL_MEASURE_ONCE | ADC_CHANNEL_START_NOW, /* runs continuously after fopen */
    15,                                               /* number of samples in one run sequence */
    0,                                                /* time offset from trigger point in us */
    300000,                                           /* period in us (= 0.1 sec) */
    0x10000,                                          /* scale range of result (not used now) */
    15,                                               /* circular buffer size (sample count) */
    MY_TRIGGER,                                       /* logical trigger ID that starts this ADC channel */
};

///////////////////////////////////////////////////////////////////////////////
//! \brief Print out readings from ADC's VBat continously, for test purposes.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function  runs an infinite loop, printing ADC output for Corona_VBat.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int adc_sense_test(void)
{
    MQX_FILE_PTR f, f_ch1, f_ch2;

#if MQX_USE_LWEVENTS
    if (_lwevent_create(&evn, 0) != MQX_OK)
    {
        corona_print("\nMake event failed!\n");
        _task_block();
    }
#endif


    //ioctl(f, ADC_IOCTL_FIRE_TRIGGER, (pointer) MY_TRIGGER);

    corona_print("Sampling VBat in an infinite loop.\n");
    for ( ; ; )
    {
        ADC_RESULT_STRUCT data;

        f = fopen("adc0:", (const char *)&adc_init);
        if (f == NULL)
        {
            corona_print("adc_sense_test: fopen failed\n");
            return -1;
        }

        corona_print("Opening VBat channel ...");
        f_ch1 = fopen("adc0:VBat", (const char *)&adc_channel_param1);
        if (f_ch1 == NULL)
        {
            corona_print("adc_sense_test: fopen(VBat) failed\n");
            return -2;
        }

#if MQX_USE_LWEVENTS
        corona_print("Waiting for event...");
        if (_lwevent_wait_ticks(&evn, 0x01, TRUE, 0) != MQX_OK)
        {
            corona_print("failed!\n");
        }
        else
        {
            corona_print("succeed!\n");
        }
#endif

        /* channel 1 sample ready? */
        if (read(f_ch1, &data, sizeof(data)))
        {
            corona_print("ADC ch 1 (VBat): %4d\n", data.result);
        }
        else
        {
            corona_print("No VBat\n");
        }

        fclose(f_ch1);
        fclose(f);

        fflush(stdout);

        f = fopen("adc0:", (const char *)&adc_init);
        if (f == NULL)
        {
            corona_print("adc_sense_test: fopen failed\n");
            return -1;
        }

        corona_print("Opening UID channel ...");
        f_ch2 = fopen("adc0:UID", (const char *)&adc_channel_param2);
        if (f_ch2 == NULL)
        {
            corona_print("adc_sense_test: fopen(UID) failed\n");
            return -2;
        }

#if MQX_USE_LWEVENTS
        corona_print("Waiting for event...");
        if (_lwevent_wait_ticks(&evn, 0x01, TRUE, 0) != MQX_OK)
        {
            corona_print("failed!\n");
        }
        else
        {
            corona_print("succeed!\n");
        }
#endif

        /* channel 2 sample ready? */
        if (read(f_ch2, &data, sizeof(data)))
        {
            corona_print("ADC ch 1 (VBat): %4d\n", data.result);
        }
        else
        {
            corona_print("No VBat\n");
        }

        fclose(f_ch2);
        fclose(f);

        _time_delay(1000);
    }

    return 0;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
