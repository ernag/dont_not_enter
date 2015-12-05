/*
 * adc_sense.c
 * Corona driver for sensing VBAT and UID on the K22's ADC.
 *
 *  Created on: Jan 18, 2013
 *      Author: Ernie Aguilar
 *  TODO - Self Calibration
 */

#include "adc_sense.h"
#include "ADC0_Vsense.h"
#include "pwr_mgr.h"

static LDD_TError  getMeasurement(LDD_TDeviceData *pADC, LDD_ADC_TSample *pSampleGroup, ADC0_Vsense_TResultData *pMeasurement);
static uint16_t    getNatural(uint32_t channelIdx, uint16_t raw);

#define SAMPLE_GROUP_SIZE 1U

static  LDD_TDeviceData *MyADCPtr = 0;

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
		printf("ERROR: CreateSampleGroup() returned (%d)\n", Error);
		PE_DEBUGHALT();
		return Error;
	}
	
	Error = ADC0_Vsense_StartSingleMeasurement(pADC);           /* Start continuous measurement */
	while (!ADC0_Vsense_GetMeasurementCompleteStatus(pADC)) {}; /* Wait for conversion completeness */
	
	if(Error != ERR_OK)
	{
		printf("ERROR: StartSingleMeasurement() returned (%d)\n", Error);
		PE_DEBUGHALT();
		return Error;
	}
	
	Error = ADC0_Vsense_GetMeasuredValues(pADC, (LDD_TData *)pMeasurement);  /* Read measured values */
	
	if(Error != ERR_OK)
	{
		printf("ERROR: GetMeasuredValues() returned (%d)\n", Error);
		PE_DEBUGHALT();
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
 *   Sense an ADC pin, and feedback the raw counts, as well as whatever natural units corresponds to that pin.
 *     For VBatt, this is millivolts.
 *     For UID, this is ohms.
 */
corona_err_t adc_sense(uint32_t channelIdx, uint16_t *pRaw, uint16_t *pNatural)
{
	LDD_ADC_TSample SampleGroup;
	LDD_TError Error;
	
	if(!MyADCPtr)
	{
		MyADCPtr = ADC0_Vsense_Init((LDD_TUserData *)NULL);        /* Initialize the device */
	}
	
	SampleGroup.ChannelIdx = channelIdx;
	Error = getMeasurement(MyADCPtr, &SampleGroup, pRaw);
	if(Error != ERR_OK)
	{
		printf("ADC measurement FAILED with Error=%d\n!", Error);
		ADC0_Vsense_Disable(MyADCPtr);
		return Error;
	}
	
	*pNatural = getNatural(channelIdx, *pRaw);
	ADC0_Vsense_Disable(MyADCPtr);
	return SUCCESS;
}

/*
 * Continously sample UID and VBatt in an infinite loop.
 */
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
			break;
			
		case PWR_STATE_NORMAL:
			ADC0_Vsense_Enable(MyADCPtr);
			break;
			
		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}
