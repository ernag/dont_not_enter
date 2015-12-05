/*
 * adc_driver.h
 *
 *  Created on: Apr 10, 2013
 *      Author: Ben McCrea
 */

#ifndef _ADC_DRIVER_H_
#define _ADC_DRIVER_H_

#include "app_errors.h"

// VBAT measure input is on ADC0_SE16
#define ADC_VBAT_CHANNEL        (16)
#define ADC_UID_CHANNEL		    (16)
#define ADC_MCU_TEMP_CHANNEL	(26)
#define ADC_VBAT_TEMP_CHANNEL   (23)

/* Name of AD channel. If the "A/D channel (pin) signal" property in the channel
   pin group is not empty the constant "ComponentName_Signal" with the value of the channel
   index is generated, where the ComponentName = value of the "Component name" property
   and "Signal" is the value of the " A/D channel (pin) signal" property. This constant
   can be used in the method CreateSampleGroup() to specify required channel.
   (See ComponentName_TSample struct) */
#define ADC0_Vsense_Corona_VBat         0U
#define ADC0_Vsense_Corona_UID          1U

/*
 *   Cable type corresponds to UID pin being sampled by ADC.
 */
typedef enum cable_type
{
	CABLE_BURNIN      = 27990,  // 2.2kohm
	CABLE_FACTORY_DBG = 35950,  // 3.6kohm
	CABLE_CUSTOMER    = 65500   // open, infinite, 16-bit saturation
	
}cable_t;

void adc_default_init ( void );
void adc_setchan( uint_8 base, uint_8 index, uint_8 chan_num, boolean diff);
#if 0
void adc_enable_compare( int_32 compare_threshold, void (*callback)() );
void adc_disable_compare( void );
#endif
void adc_disable( void );
void adc_enable_interrupt( uint_8 index, void (*callback)() );
void adc_disable_interrupt( uint_8 index );
uint_32 adc_sample_battery( void );
uint_16 adc_getNatural(uint32_t channelIdx, uint16_t raw);
uint_32 adc_sample_uid( void );
uint_32 adc_sample_batt_temp( void );
cable_t adc_get_cable(const uint_32 uid_counts);

#endif /* _ADC_DRIVER_H_ */
