/* 
 * File:   point_calc.h
 * Author: Nate
 *
 * Created on May 28, 2013, 12:35 PM
 */
#ifndef POINT_CALC_H
#define	POINT_CALC_H

#ifdef PY_MODULE
    #define CALCULATE_MINUTES_ACTIVE 1  // Always calculate minutes active on the server
	#define boolean int
	#define TRUE 1
    #define MEDIAN_FILTER_DATA 0  // The median filter should never be applied on the server
	#define corona_print printf
 	#define NO_PY(x)
	#define SKIP_IRRELEVANT_POINTS 0  // If true points after the point threshold has been passed will not be calculated
	#include <stdio.h>
	#include <stdlib.h>
#else
    #include "wassert.h"
    #define MEDIAN_FILTER_DATA 1  // The median filter is applied to ALL data if defined to be TRUE.
    #define CALCULATE_MINUTES_ACTIVE 0  // Define to be true to calculate minutes active on the device
    #define NO_PY(x) x
    #if (CALCULATE_MINUTES_ACTIVE)
        #define SKIP_IRRELEVANT_POINTS 1 // If true points after the point threshold has been passed will not be calculated
        #include <mqx.h>
        #include "cfg_mgr.h"
        #include "persist_var.h"
        #include "app_errors.h"
        #include "accdrv_spi_dma.h"
    #endif
#endif

#include <stdint.h>
#include <stddef.h>

#if (MEDIAN_FILTER_DATA)
// Even if we don't calculate minutes active we may still median filter the data
void    median_filter_data(int8_t *input_array, size_t num_points);
void    MIN_ACT_reset_median_filter(void);
#endif

#ifndef PY_MODULE
    uint8_t MIN_ACT_met_daily_goal(void);
#endif


#if (CALCULATE_MINUTES_ACTIVE)
    /* Floating point number arithmetic is computationally expensive for the cortex.
     * Instead of doing the filtering and point calculation with floating point numbers,
     * we convert everything to integers while keeping some accuracy by dealing with
     * everything in QFLOAT_SHIFT format where FLOAT_SHIFT is defined below.
     * For reference on the Q format, see
     *
     * http://en.wikipedia.org/wiki/Q_(number_format)
     *
     * Note the higher the FLOAT_SHIFT value, the higher the accuracy. However, since
     * we are working with 32 bit integers, and the input can be between -128 & 127,
     * a FLOAT_SHIFT > 11 MIGHT cause overflow in the worst case of input = -128 and
     * the offset is -128 in filter_gs. FLOAT_SHIFT of 11 is safe. Probably....
     * 2013-10-21: This should really be done using the multiply accumulate function
     * which uses a 64 bit accumulator and then downshift the result.
     *
     */
    #define MIN_ACT_FLOAT_SHIFT 11
    #define MIN_ACT_ADJ_FACTOR 2048 // 2^FLOAT_SHIFT
    
    void    MIN_ACT_init(void);
    void    MIN_ACT_calc(int8_t *input_array, size_t num_rows, size_t start_row, uint64_t device_milli, uint64_t day_milli);
    void    MIN_ACT_finish(void);
    void    MIN_ACT_print_vals(void);
    void    MIN_ACT_reset_median_filter(void);
    
    
    #ifdef PY_MODULE
        void MIN_ACT_reset_filters(void);
        float MIN_ACT_get_points_this_minute(void);
        float MIN_ACT_get_active_threshold(void);
        uint16_t MIN_ACT_get_prev_block_min(void);
        uint16_t MIN_ACT_get_prev_block_day(void);
        void MIN_ACT_clear(void);
    #else
        uint16_t MIN_ACT_current_minutes_active(void);
    #endif
#endif
#endif	/* NEWFILE_H */
