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
#include "adc_sense.h"
#include "ADC0_Vsense.h"
#include "PORTA_GPIO.h"

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static LDD_TError  getMeasurement(LDD_TDeviceData *pADC, LDD_ADC_TSample *pSampleGroup, ADC0_Vsense_TResultData *pMeasurement);
static uint16_t    getNatural(uint32_t channelIdx, uint16_t raw);

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define SAMPLE_GROUP_SIZE 1U

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static  LDD_TDeviceData *MyADCPtr = 0;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *  Get a measurement from the ADC pSampleGroup->ChannelIdx of your choice.
 */
static LDD_TError  getMeasurement(LDD_TDeviceData *pADC, LDD_ADC_TSample *pSampleGroup, ADC0_Vsense_TResultData *pMeasurement)
{
	LDD_TError Error;
	
	if(!MyADCPtr)
	{
		MyADCPtr = ADC0_Vsense_Init((LDD_TUserData *)NULL);        /* Initialize the device */
	}
	ADC0_Vsense_Enable(MyADCPtr);
	
	Error = ADC0_Vsense_CreateSampleGroup(pADC, (LDD_ADC_TSample *)pSampleGroup, 1);  /* Set created sample group */
	if(Error != ERR_OK)
	{
		//printf("ERROR: CreateSampleGroup() returned (%d)\n", Error);
		//PE_DEBUGHALT();
		return Error;
	}
	
	Error = ADC0_Vsense_StartSingleMeasurement(pADC);           /* Start continuous measurement */
	while (!ADC0_Vsense_GetMeasurementCompleteStatus(pADC)) {}; /* Wait for conversion completeness */
	
	if(Error != ERR_OK)
	{
		//printf("ERROR: StartSingleMeasurement() returned (%d)\n", Error);
		//PE_DEBUGHALT();
		return Error;
	}
	
	Error = ADC0_Vsense_GetMeasuredValues(pADC, (LDD_TData *)pMeasurement);  /* Read measured values */
	
	if(Error != ERR_OK)
	{
		//printf("ERROR: GetMeasuredValues() returned (%d)\n", Error);
		//PE_DEBUGHALT();
		return Error;
	}
	
	ADC0_Vsense_Disable(MyADCPtr);
	return ERR_OK;
}

/*
 *   Returns the "natural" in units we care about, for given channel IDX and raw value.
 */
static uint16_t getNatural(uint32_t channelIdx, uint16_t raw)
{
	uint32_t slope = 0;
	uint32_t intercept = 0;
	uint32_t divider = 1;
	
	switch(channelIdx)
	{
		case ADC0_Vsense_Corona_VBat:
			slope = 1076;      // it is actually 0.000107612, so we divide later to create millivolts.
			intercept = 185;   //98; // using 10, to compensate for lack of floating point.
			divider = 10000;
			break;
			
		case ADC0_Vsense_Corona_UID:
			slope = 242;      // it is actually 0.24182, but we divide by 1,000 later since we don't have floating point.
			intercept = 2694;
			divider = 1000;
			break;
			
		default:
			break;
	}
	
	return (uint16_t)( (raw * slope)/divider ) - intercept;
}

/*
 *   Given counts from ADC1 (UID pin), return the type
 *     of cable we think this is.
 */
cable_t adc_get_cable(const uint32_t uid_counts)
{
    /*
     *    Usual case is customer cable, so deal with that first.
     */
    if( uid_counts > (CABLE_CUSTOMER - 5000) )  // 5000 counts of margin is plenty, and works well enough.
    {
        return CABLE_CUSTOMER;
    }

    /*
     *   Now check if we have a burn-in cable.
     */
    if(uid_counts < (CABLE_BURNIN + ((CABLE_FACTORY_DBG - CABLE_BURNIN)/2)) )  // let the margin be the half-way point between the two.
    {
        return CABLE_BURNIN;
    }
    
    /*
     *   Otherwise, we must be somewhere in the middle, and only choice left is Factory cable.
     */
    return CABLE_FACTORY_DBG;
}


/*
 *   Sense an ADC pin, and feedback the raw counts, as well as whatever natural units corresponds to that pin.
 *     For VBatt, this is millivolts.
 *     For UID, this is ohms.
 */
corona_err_t adc_sense(uint32_t channelIdx, uint16_t *pRaw, uint16_t *pNatural)
{
	LDD_ADC_TSample SampleGroup;
	LDD_TError Error;

	if( ADC0_Vsense_Corona_UID == channelIdx )
	{
	    if( 0 == (GPIOA_PDOR & UID_EN_MASK) )
	    {
	        unsigned short delay=4000; // 3000 OK, 300 NOT OK.  give some margin here.
	        
	        /*
	         *   Enable UID_EN so we can see what type of cable this be.
	         *    (only necessary for REV B / P4 + hardware)
	         *   Don't worry about disabling.  We will do that in the customer app.
	         */
	        GPIOA_PDOR |= UID_EN_MASK;
	        while(delay--){}
	    }
	}
	
	if(!MyADCPtr)
	{
		MyADCPtr = ADC0_Vsense_Init((LDD_TUserData *)NULL);        /* Initialize the device */
	}
	
	SampleGroup.ChannelIdx = channelIdx;
	Error = getMeasurement(MyADCPtr, &SampleGroup, pRaw);
	if(Error != ERR_OK)
	{
		//printf("ADC FAILED with Error=%d\n!", Error);
		ADC0_Vsense_Disable(MyADCPtr);
		return Error;
	}
	
	*pNatural = getNatural(channelIdx, *pRaw);
	ADC0_Vsense_Disable(MyADCPtr);
	return SUCCESS;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Print out readings from ADC's VBat continously, for test purposes.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function  runs an infinite loop, printing ADC output for Corona_VBat.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t adc_sense_test(void)
{
	LDD_TError Error;
	ADC0_Vsense_TResultData MeasuredValues[2];
	LDD_ADC_TSample SampleGroup;
	
	if(!MyADCPtr)
	{
		MyADCPtr = ADC0_Vsense_Init((LDD_TUserData *)NULL);        /* Initialize the device */
	}
	
	printf("Sampling VBat in an infinite loop.\n");
	for(;;) 
	{
		unsigned int i;
		
		/*
		 *  MEASURE VBatt
		 */
		SampleGroup.ChannelIdx = ADC0_Vsense_Corona_VBat;
		Error = getMeasurement(MyADCPtr, &SampleGroup, &(MeasuredValues[0]));
		if(Error != ERR_OK)
		{
			printf("getMeasurement() returned (%d)\n", Error);
			PE_DEBUGHALT();
			return Error;
		}
		
		/*
		 *   MEASURE UID Resistance
		 */
		SampleGroup.ChannelIdx = ADC0_Vsense_Corona_UID;
		Error = getMeasurement(MyADCPtr, &SampleGroup, &(MeasuredValues[1]));
		if(Error != ERR_OK)
		{
			printf("getMeasurement() returned (%d)\n", Error);
			PE_DEBUGHALT();
			return Error;
		}
		
		printf("Batt:\t%d\t\tUID:\t%d\n", MeasuredValues[0], MeasuredValues[1]);

		// NOTE - The longer you delay, the longer the capacitor C53, going into the ADC has to charge back to a known level.
		//        Also, the more you sample, the more power will be consumed.
		//sleep(2 * 1000);
	}
	return SUCCESS;
}
#endif

#if 0
/*
 *   Handle power management for ADC sensing.
 */
corona_err_t pwr_adc(pwr_state_t state)
{
	if(!MyADCPtr)
	{
		MyADCPtr = ADC0_Vsense_Init((LDD_TUserData *)NULL);        /* Initialize the device */
	}
	
	switch(state)
	{
		case PWR_STATE_SHIP:
		case PWR_STATE_STANDBY:
			ADC0_Vsense_Disable(MyADCPtr);

			ADC0_SC1A |= 0x1F;  // ADC0 should not be enabled, but go ahead and disable anyway.
			ADC0_SC1B |= 0x1F;

			break;
			
		case PWR_STATE_NORMAL:
			ADC0_Vsense_Enable(MyADCPtr);
			break;
			
		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
