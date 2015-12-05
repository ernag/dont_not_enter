/*
 * adc_sense.h
 *
 *  Created on: Jan 18, 2013
 *      Author: Ernie Aguilar
 */

#ifndef ADC_SENSE_H_
#define ADC_SENSE_H_

#include "PE_Types.h"
#include "k2x_errors.h"

corona_err_t adc_sense_test(void);
corona_err_t adc_sense(uint32_t channelIdx, uint16_t *pRaw, uint16_t *pNatural);


#endif /* ADC_SENSE_H_ */
