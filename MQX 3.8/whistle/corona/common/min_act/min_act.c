/* 
 * File:   main.c
 * Author: Nate
 *
 * Created on May 22, 2013, 7:50 AM
 */
////////////////////////////////////////////////////////////////////////////////
// Includes
// Please put device specific headers in min_act.h so they aren't included in the
//  Python release
////////////////////////////////////////////////////////////////////////////////
#include "min_act.h"

// Define this to turn on debug output
//#define PNT_CALC_VERBOSE
#ifdef PNT_CALC_VERBOSE
#define D(x)    x
#else
#define D(x)
#endif

#define NUM_CHANS 3 // The number of channels of the accelerometer
#if (MEDIAN_FILTER_DATA)
// The only contents left in this file if we aren't calculating the minutes active is the median filter
static size_t g_median_filter_processed_rows = 0;
#endif

#if (CALCULATE_MINUTES_ACTIVE)
static uint16_t g_prev_block_day = 0;
static uint16_t g_prev_block_min = 0;
static uint16_t g_daily_min_active = 0;
static uint32_t g_points_this_min = 0;

/* SAMPLING CONSTANTS FOR point_calc.h */
#define SAMPLE_RATE 50 // The sample rate of the accelerometer in Hz
#define TIME_DELTA_MILLIS 20 // This is the time between samples in milliseconds (1/ Sample_Rate) * 1000
#define RESOLUTION 8
#define FULL_SCALE 16 // Max value - min value (8 - -8) = 16
/* PROCESSING CONSTANTS FOR point_calc.h */
#define ACT_MIN_THRESHOLD US(140)  // The point threshold for an active minute in Q notation
#define ROWS_PER_BLOCK 250 // The number of rows in the block.
#define NUM_BINS 100 // The number of bins to use in the median calc.  These are spread linearly as defined in g_bin_median
/* MACROS FOR point_calc.c*/
#define ARRAY_IND(row, col) ((row) * NUM_CHANS + col) // Returns the index for a row-major array with NUM_CHANS columns
#define US(a) ((a) << MIN_ACT_FLOAT_SHIFT)
// This should be okay for signed numbers: http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0489c/Cjacbgca.html
#define DS(a) ((a) >> MIN_ACT_FLOAT_SHIFT) 
#define Q_MULT(a, b) (DS((a) * (b) + g_q_one_half))

// Even though COUNTS_PER_G will never be negative, the constant is signed
// to make sure arithmetic performs as expected.
static const int32_t g_counts_per_g = ((1 << (RESOLUTION)) / FULL_SCALE);
static const int32_t g_q_one_g = US(1);
static const int32_t g_US_gs = MIN_ACT_ADJ_FACTOR / ((1 << (RESOLUTION)) / FULL_SCALE);

static const int32_t g_q_one_half = (1 << (MIN_ACT_FLOAT_SHIFT - 1));

// These constants are used to calculate the approximate distance.
// They were determined in Matlab to minimize the RMS percent error
// See the function approx_dist for their usage
int32_t g_approx_dist_factor = 1.7001953125 * MIN_ACT_ADJ_FACTOR;
int32_t g_c_min[2] =
{ -0.30078125 * MIN_ACT_ADJ_FACTOR, -0.24560546875 * MIN_ACT_ADJ_FACTOR };
int32_t g_c_max[2] =
{ 0.1728515625 * MIN_ACT_ADJ_FACTOR, 0.64013671875 * MIN_ACT_ADJ_FACTOR };
int32_t g_c_sum[2] =
{ 0.595703125 * MIN_ACT_ADJ_FACTOR, 0.3173828125 * MIN_ACT_ADJ_FACTOR };

//TODO: Fix it so that this works for variable order filters
#define FILTER_ORDER 2 // The order of the filter (coefficients are hardcoded below)
typedef struct
{
    int32_t numerator[FILTER_ORDER + 1];
    int32_t denominator[FILTER_ORDER + 1];
    int32_t offset[NUM_CHANS];
    int32_t in_store[FILTER_ORDER * NUM_CHANS];
    int32_t out_store[FILTER_ORDER * NUM_CHANS];
} filter_chans_t;
// Generated in Matlab for a 2nd order lowpass with a cutoff frequency of 3 Hz
// A low cutoff frequency is used to weight higher frequency less in the 
// same spirit as integrating the signal
static filter_chans_t g_filter_counts =
{
{ 0.02783203125 * MIN_ACT_ADJ_FACTOR, 0.0556640625 * MIN_ACT_ADJ_FACTOR,
        0.02783203125 * MIN_ACT_ADJ_FACTOR },
{ 1.0 * MIN_ACT_ADJ_FACTOR, -1.4765625 * MIN_ACT_ADJ_FACTOR, 0.5859375
        * MIN_ACT_ADJ_FACTOR },
{ 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0 } };

typedef struct
{
    int32_t numerator[FILTER_ORDER + 1];
    int32_t denominator[FILTER_ORDER + 1];
    int32_t offset;
    int32_t in_store[FILTER_ORDER];
    int32_t out_store[FILTER_ORDER];
} filter_t;
// Generated in Matlab for a 2nd order bandpass with a cutoff frequencies of 0.6 and 12 Hz
// Used to mitigate the influence of high frequency noise and offset in the signal
static filter_t g_filter_mag =
{
    { 0.4658203125 * MIN_ACT_ADJ_FACTOR, 0, -0.4658203125 * MIN_ACT_ADJ_FACTOR },
    { 1.0 * MIN_ACT_ADJ_FACTOR, -0.99609375 * MIN_ACT_ADJ_FACTOR, 0.0703125 * MIN_ACT_ADJ_FACTOR },
    0,
    { 0, 0 },
    { 0, 0 } 
};

typedef struct
{
    uint32_t min_bin;
    uint32_t bin_width;
    uint16_t less_than;
    uint16_t bin_counts[NUM_BINS];
} median_t;
static median_t g_bin_median =
{
    0.0050 * MIN_ACT_ADJ_FACTOR, 
    ((.75 - 0.0050) * MIN_ACT_ADJ_FACTOR) / NUM_BINS, 
    0 
};

// Temp variable to store the current block for a given channel
// This should be as large as the largest block of data that will be passed to the algorithm
#ifdef PY_MODULE
#define MAX_INPUT_ROWS ROWS_PER_BLOCK 
#else
#define MAX_INPUT_ROWS SPIDMA_ACCEL_WTM_SIZE
#endif

//global vectors for storing processing output
static int32_t g_chan_filt_output[MAX_INPUT_ROWS * NUM_CHANS]; // Put this on the heap instead of the stack to save stack space
static int32_t g_dynamic_rss_output[MAX_INPUT_ROWS]; // For storing the results of the RSS calculation to avoid recalculating values
static int32_t g_mag_filter_output[MAX_INPUT_ROWS]; // For storing the results of the magnitude filter

static uint16_t g_block_min = 0;
static uint16_t g_block_day = 0;
// Variable to store how many rows have already been finished
static size_t g_processed_block_rows;

/*  LOCAL FUNCTION PROTOTYPES */
// Keeping track of points across data chunks
static void add_point_to_minute(uint32_t point);
static void check_active_minute(void);

// Filtering
void filter_counts(int8_t *input, size_t n_rows, size_t col, int32_t *output);
static void calc_dynamic_rss(int32_t *input, size_t n_rows, int32_t *output);
void filter_mag(int32_t *input, size_t n_rows, int32_t *output);
static void reset_filter_chans(filter_chans_t *filt_struct);
static void reset_filter(filter_t *filt_struct);
static int32_t dynamic_rss(int32_t *input);

static void MIN_ACT_begin(uint64_t day_milli, uint64_t device_milli,
        size_t start_row);

// Helper
static void _update_min_active(uint16_t min_active);
static void _update_minact_day(uint16_t minact_day);

// For calculating the approximate median
void bin_abs_data(int32_t *vector, size_t n_rows);
uint32_t calc_median(void);
static void reset_median(void);

// Numerical functions
static int32_t approx_dist(int32_t min, int32_t max, int32_t sum);

// Functions for dealing with time
static uint16_t current_day(uint64_t day_milli, size_t samples_offset);
static uint16_t current_min(uint64_t device_milli, size_t samples_offset);

// Calculate the points for a block
static void MIN_ACT_add(int8_t *input_array, size_t start_row, size_t num_rows);
static void handle_large_block(int8_t *input_array, size_t num_rows,
        size_t start_offset, uint64_t device_milli, uint64_t day_milli);
#endif

#ifdef PNT_CALC_VERBOSE
/* ==================== DEBUGGING FUNCTIONS ==========================*/
void print_accel_data(int8_t *input_array, size_t num_rows)
{
    int i, row_offset;
    for (i=0; i<num_rows;i++)
    {
        row_offset = i * NUM_CHANS;
        corona_print("%5d\t%5d\t%5d\n", (int)input_array[row_offset], (int)input_array[row_offset + 1], (int)input_array[row_offset + 2]);
    }
}

void print_q_data(int32_t *input_array, size_t num_rows)
{
    int i, row_offset;
    for (i=0; i<num_rows;i++)
    {
        row_offset = i * NUM_CHANS;
        corona_print("%9d\t%9d\t%9d\n", (int)input_array[row_offset], (int)input_array[row_offset + 1], (int)input_array[row_offset + 2]);
    }
}

void print_accel_column(int32_t *input_array, size_t num_rows)
{
    int i;
    for (i=0; i<num_rows;i++)
    {
        corona_print("%6d\n", (int)input_array[i]);
    }
}
#endif


#if (MEDIAN_FILTER_DATA)
/* ==================== MEDIAN FILTERING FUNCTIONS ==========================*/
void MIN_ACT_reset_median_filter(void)
{
    g_median_filter_processed_rows = 0;
}

/*
 * This function is used to filter the input data using a three point median filter.
 * This function modifies the input data and is reset using the reset_median filter function above.
 */
void median_filter_data(int8_t *input_array, size_t num_points)
{
    size_t num_rows;
    static int8_t old_data[2 * NUM_CHANS];
    uint8_t c, cur_offset = 0, other_offset = NUM_CHANS;
    int8_t in, a, b;
    size_t r, rc=0;
    
    num_rows = num_points / NUM_CHANS;
    
    // This algorithm can only operate on a square matrix of data where triaxial triplets are all present
    NO_PY(wassert(num_rows * NUM_CHANS == num_points));
    
    D(corona_print("PRE-MEDIAN\n");print_accel_data(input_array, num_rows);corona_print("\n")) ;
    
    for (r=0; r < num_rows; r++)
    {          
        for (c=0; c < NUM_CHANS; c++)
        {
            in = input_array[rc];
            
            // Start median filtering only after we have processed two rows to avoid startup issues
            if (g_median_filter_processed_rows >= 2)
            {
                // Sort these points based on the fact we only care if we need to replace the input value 
                a = old_data[c + cur_offset];
                b = old_data[c + other_offset];
                if (in <= a)
                {
                    if (in < b)
                    {
                        input_array[rc] = a < b ? a : b;
                    } 
                } else
                {
                    if (in > b)
                    {
                        input_array[rc] = a > b ? a : b;
                    }
                }
                
            }

            old_data[c + cur_offset] = in;
            rc++;
        }
        
        g_median_filter_processed_rows++;
        
        other_offset = cur_offset;
        cur_offset = cur_offset == 0 ? NUM_CHANS : 0;
    }
    
    //D(corona_print("POST-MEDIAN\n");print_accel_data(input_array, num_rows);corona_print("\n");)
}
#endif

/* BEGIN CODE TO CALCULATE MINUTES ACTIVE */
#if (CALCULATE_MINUTES_ACTIVE)
/* BEGIN FUNCTION DEFINITIONS */

/* ==============  TIMING FUNCTIONS - Keeping track of time =================*/

/*
 * Calculates the current day from the dog's local time in milliseconds.
 * The day is calculated as an 
 */
static uint16_t current_day(const uint64_t day_milli,
        const size_t samples_offset)
{
    uint32_t millis_per_day = 86400000;
    return (day_milli + TIME_DELTA_MILLIS * samples_offset) / millis_per_day;
}

/*
 * Calculates the current minute of the day (out of 1440) using the devices
 * boot time in milliseconds.
 */
static uint16_t current_min(const uint64_t device_milli,
        const size_t samples_offset)
{
    uint16_t millis_per_min = 60000, mins_per_day = 1440;
    return ((device_milli + TIME_DELTA_MILLIS * samples_offset) / millis_per_min)
            % mins_per_day;
}

/* ================ Keeping track of point aggregation =======================*/
/*
 * This function adds input point to the current minute and increments 
 *  the minute active value if appropriate.
 */
static void add_point_to_minute(const uint32_t point)
{
    uint8_t was_below_threshold = 1;
    
    if (point == 0)
    {
        D(corona_print("PTS+%d=%d\n", g_points_this_block, g_points_this_min);)
        return;
    }

    // This is not appropriate if multiple blocks of data can be simultaneously calculated due to concurrency issues
    if (g_block_min == g_prev_block_min && g_block_day == g_prev_block_day)
    {
        // Another (better?) way to do this would be to use a bool to 
        //    indicate the status of the current minute
        if (g_points_this_min > ACT_MIN_THRESHOLD)
        {
            was_below_threshold = 0;
        }
        
        g_points_this_min += point;
    }
    else
    {
        g_prev_block_min = g_block_min;
        g_points_this_min = point;
        D(corona_print("Min: %u\n",g_block_min);)
    }

    D(corona_print("PTS+%d=%d\n", point, g_points_this_min);)
    if (was_below_threshold)
    {
        check_active_minute();
    }
    
    return;
}

/*
 * This function checks if POINTS_THIS_MINUTE is above the ACT_MIN_THRESHOLD
 *  and if it is the DAILY_MINUTES_ACTIVE is incremented.  If the current day
 *  is no longer the same as it was it is incremented and then the point is 
 *  added
 */
static void check_active_minute(void)
{
    if (g_prev_block_day == g_block_day)
    {
        if (g_points_this_min > ACT_MIN_THRESHOLD)
        {
            _update_min_active(g_daily_min_active + 1);
            D(corona_print("%u MA 4 day: %u\n", g_daily_min_active, g_block_day));
        }
    }
    else
    {
        _update_minact_day(g_block_day);
        _update_min_active(g_points_this_min > ACT_MIN_THRESHOLD ? 1 : 0);
        
        D(corona_print("Inc Day %u\n", g_prev_block_day));
        D(corona_print("Cur MA %u\n", g_daily_min_active));
    }
}

/* 
 * This function is used to allow for external unit testing
 */
void increment_processed_rows(const size_t num_to_inc)
{
    g_processed_block_rows += num_to_inc;
}

/* ==================== FILTER FUNCTIONS =============================*/
static void reset_filter_chans(filter_chans_t *filt_struct)
{
    int i;
    for (i = 0; i < NUM_CHANS; i++)
    {
        filt_struct->offset[i] = 0;
    }
    for (i = 0; i < (FILTER_ORDER * NUM_CHANS); i++)
    {
        filt_struct->in_store[i] = 0;
        filt_struct->out_store[i] = 0;
    }
}

static void reset_filter(filter_t *filt_struct)
{
    int i;
    
    filt_struct->offset = 0;
    
    for (i = 0; i < FILTER_ORDER; i++)
    {
        filt_struct->in_store[i] = 0;
        filt_struct->out_store[i] = 0;
    }
}

static int32_t dynamic_rss(int32_t *input)
{
    uint8_t c;
    int32_t q_point, min_point = INT32_MAX, max_point = 0, sum_point = 0;
   
    for (c = 0; c < NUM_CHANS; c++)
    {
        q_point = abs(input[c]);
        if (q_point > max_point)
        {
            max_point = q_point;
        }
        if (q_point < min_point)
        {
            min_point = q_point;
        }
        
        sum_point += q_point;
    }
    
    // Scale the point by the number of processed rows to weight partial blocks appropriately
    return approx_dist(min_point, max_point, sum_point) - MIN_ACT_ADJ_FACTOR;
}

/* Take in a matrix input and returns a vector which represents the filtered 
 * column of data that is specified with the col parameter.
 * The input data should be in counts and the output data will be in g's in Q notation
 * Referenced from: http://mechatronics.ece.usu.edu/yqchen/filter.c/
 * IIR Filter: http://en.wikipedia.org/wiki/Infinite_impulse_response
 * IN THE CURRENT IMPLEMENTATION THE FIRST BLOCK SIZE (n_rows) MUST BE 
 * GREATER THAN OR EQUAL TO THE FILTER ORDER!!!!!!  THIS CONDITION IS
 * FULFILLED as of (07/15/2013) where 25 rows are passed in at a time.
 */
void filter_counts(int8_t *input, size_t n_rows, size_t col, int32_t *output)
{
    size_t i, j, ai, ao = col;
    int32_t ind; // This must be an int because it can be negative
    int32_t temp_int = 0, temp_out = 0;
    
    /* The initial_offset is the value of the FIRST input element. If this offset 
     * exists, the filter will treat the first point as a step input. To fix
     * this, subtract the value of input[0] from EVERY element of input to
     * completely remove the offset.
     */
    if (g_processed_block_rows == 0)
    {
        g_filter_counts.offset[col] = (int32_t) (input[col] * g_US_gs);
        ai = col;
        for (i = 0; i < FILTER_ORDER; i++)
        {
            g_filter_counts.in_store[ai] = g_filter_counts.offset[col];
            ai += NUM_CHANS;
        }
    }

    for (i = 0; i < n_rows; i++)
    {
        output[ao] = 0;
        // Input
        for (j = 0; j < (FILTER_ORDER + 1); j++)
        {
            ind = i - j;
            if (ind < 0)
            {
                ai = ARRAY_IND(FILTER_ORDER + ind, col);
                temp_int = g_filter_counts.in_store[ai] - g_filter_counts.offset[col];
            }
            else
            {
                ai = ARRAY_IND(ind, col);
                temp_int = (int32_t) (input[ai] * g_US_gs)
                        - g_filter_counts.offset[col];
            }
            output[ao] += Q_MULT(g_filter_counts.numerator[j], temp_int);
        }
        // Output
        for (j = 1; j <= FILTER_ORDER; j++)
        {
            ind = i - j;
            if (ind < 0)
            {
                ai = ARRAY_IND(FILTER_ORDER + ind, col);
                temp_int = g_filter_counts.out_store[ai];
            }
            else
            {
                ai = ARRAY_IND(ind, col);
                temp_int = output[ai];
            }
            output[ao] -= Q_MULT(g_filter_counts.denominator[j], temp_int);
        }
        ao += NUM_CHANS;
    }

    /* Store the initial conditions for the next block*/
    j = col;
    for (i = 0; i < FILTER_ORDER; i++)
    {
        ind = n_rows - FILTER_ORDER + i;
        if (ind < 0)
        {
            temp_int = (int32_t) (input[col] * g_US_gs);
            temp_out = temp_int;
        }
        else
        {
            ai = ARRAY_IND(ind, col);
            temp_int = (int32_t) (input[ai] * g_US_gs);
            temp_out = output[ai];
        }

        g_filter_counts.in_store[j] = temp_int;
        g_filter_counts.out_store[j] = temp_out;
        j += NUM_CHANS;
    }

    ao = col;
    for (i = 0; i < n_rows; i++)
    {
        output[ao] += g_filter_counts.offset[col];
        ao += NUM_CHANS;
    }
}

static void calc_dynamic_rss(int32_t *input, size_t n_rows, int32_t *output)
{
    size_t i;
    for (i=0; i < n_rows; i++)
    {
        output[i] = dynamic_rss(input + (i * NUM_CHANS));
    }
}

/* Take in a vector that has already been converted to g's and filtered
 * The input and output data will be in g's in Q notation
 * Referenced from: http://mechatronics.ece.usu.edu/yqchen/filter.c/
 * IIR Filter: http://en.wikipedia.org/wiki/Infinite_impulse_response
 * IN THE CURRENT IMPLEMENTATION THE FIRST BLOCK SIZE (n_rows) MUST BE 
 * GREATER THAN OR EQUAL TO THE FILTER ORDER!!!!!!  THIS CONDITION IS
 * FULFILLED as of (07/15/2013) where 25 rows are passed in at a time.
 */
void filter_mag(int32_t *input, size_t n_rows, int32_t *output)
{
    size_t i, j;
    int32_t ind; // THIS MUST BE AN INT SINCE IT IS SIGNED!!!!!!!!!
    int32_t temp_int = 0, temp_out = 0;
    
    /* The initial_offset is the value of the FIRST input element. If this offset 
     * exists, the filter will treat the first point as a step input. To fix
     * this, subtract the value of input[0] from EVERY element of input to
     * completely remove the offset.
     */
    if (g_processed_block_rows == 0)
    {
        g_filter_mag.offset = input[0];
        for (i = 0; i < FILTER_ORDER; i++)
        {
            g_filter_mag.in_store[i] = g_filter_mag.offset;
        }
    }

    for (i = 0; i < n_rows; i++)
    {
        output[i] = 0;
        // Input
        for (j = 0; j < (FILTER_ORDER + 1); j++)
        {
            ind = i - j;
            if (ind < 0)
            {
                temp_int = g_filter_mag.in_store[FILTER_ORDER + ind] - g_filter_mag.offset;
            }
            else
            {
                temp_int = input[ind] - g_filter_mag.offset;
            }
            
            output[i] += Q_MULT(g_filter_mag.numerator[j], temp_int);
        }
        // Output
        for (j = 1; j <= FILTER_ORDER; j++)
        {
            ind = i - j;
            if (ind < 0)
            {
                temp_out = g_filter_mag.out_store[FILTER_ORDER + ind];
            }
            else
            {
                temp_out = output[ind];
            }
            output[i] -= Q_MULT(g_filter_mag.denominator[j], temp_out);
        }
    }
    
    /* Store the initial conditions for the next block*/
    for (i = 0; i < FILTER_ORDER; i++)
    {
        ind = n_rows - FILTER_ORDER + i;
        if (ind < 0)
        {
            temp_int = g_filter_mag.offset;
            temp_out = temp_int;
        }
        else
        {
            temp_int = input[ind];
            temp_out = output[ind];
        }

        g_filter_mag.in_store[i] = temp_int;
        g_filter_mag.out_store[i] = temp_out;
    }
}

/* ==================== MEDIAN FUNCTIONS =============================*/
static void reset_median(void)
{
    int i;
    
    for (i = 0; i < NUM_BINS; i++)
        g_bin_median.bin_counts[i] = 0;
    
    g_bin_median.less_than = 0;
}

uint32_t calc_median(void)
{
    size_t i;
    size_t k = (g_processed_block_rows + 1) / 2;
    uint32_t right_val, left_val, points_outside_bin, count = g_bin_median.less_than;
    
    if (count >= k)
    {
        return 0;
    }

    for (i = 0; i < NUM_BINS; i++)
    {
        count += g_bin_median.bin_counts[i];
        if (count >= k)
        {
            
            left_val = g_bin_median.min_bin + i * g_bin_median.bin_width;
            right_val = g_bin_median.min_bin + (i + 1) * g_bin_median.bin_width;
            
            points_outside_bin = g_processed_block_rows - g_bin_median.bin_counts[i];
            
            if (points_outside_bin == 0)
            {
                return (left_val + right_val) / 2;
            }
            else
            {
                D(corona_print("Bin %u of %u\n", i, NUM_BINS);)
                return (left_val * (count - g_bin_median.bin_counts[i])
                        + right_val * (g_processed_block_rows - count))
                        / points_outside_bin;
            }
        }
    }
    return g_bin_median.min_bin + NUM_BINS * g_bin_median.bin_width;
}

void bin_abs_data(int32_t *vector, size_t n_rows)
{
    size_t i, bin = 0;
    int32_t point;
    for (i = 0; i < n_rows; i++)
    {
        point = abs(vector[i]);
        if (point < g_bin_median.min_bin)
        {
            g_bin_median.less_than++;
        }
        else
        {
            // TODO: Round the bin width to a factor of 2 so this can be optimized 
            bin = (point - g_bin_median.min_bin) / g_bin_median.bin_width;
            if (bin < NUM_BINS)
            {
                g_bin_median.bin_counts[bin]++;
            }
        }
    }
}

/* ================= POINT CALCULATION FUNCTIONS ======================= */

/* The following two functions calculate the number of points in the specified data block.
 * The point value for a block of time (ROWS_PER_BLOCK / 50 Hz) is the magnitude of the 
 * median of the filtered and integrated acceleration vector. The magnitude 
 * of the median vector is calculated using an approximation of the Euclidean 
 * distance from the origin and is normalized with respect to block length.
 * 
 * These two functions split the process up into two parts - the first part (MIN_ACT_add) 
 * filters, integrates, and bins the data. The second part (block_finish) calculates 
 * the median and the point value.
 *
 * Note that if start_block is called twice in a row (without finish block in between), the bins will
 * just expand accordingly. So if the block size is small so that it takes multiple tiny blocks to reach
 * a full block of ROWS_PER_BLOCK, just keep calling start_block on the tiny blocks.
 */
static void MIN_ACT_add(int8_t *input_array, size_t start_row, size_t num_rows)
{
    size_t col;
    size_t start_offset = start_row * NUM_CHANS;
    
//    corona_print("B\n");
//    print_accel_data(input_array, num_rows);
    for (col = 0; col < NUM_CHANS; col++)
    {
        filter_counts((input_array + start_offset), num_rows, col, g_chan_filt_output);
    }
    
//    corona_print("A\n");
//    print_q_data(g_chan_filt_output, num_rows);
    
    calc_dynamic_rss(g_chan_filt_output, num_rows, g_dynamic_rss_output);
    
    
//    print_accel_column(g_dynamic_rss_output, num_rows);
    
    filter_mag(g_dynamic_rss_output, num_rows, g_mag_filter_output);

//    corona_print("AA\n");
//    print_accel_column(g_mag_filter_output, num_rows);
    
    // http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0553a/BABEHFEF.html
    // Compute approximate median of the absolute value
    bin_abs_data(g_mag_filter_output, num_rows);
    
    increment_processed_rows(num_rows);
}

static uint8_t minute_already_active()
{
    return (SKIP_IRRELEVANT_POINTS && g_block_min == g_prev_block_min
            && g_points_this_min > ACT_MIN_THRESHOLD) ? 1 : 0;
}

void MIN_ACT_finish(void)
{
    // No data has been processed.  Just return
    if (g_processed_block_rows == 0 || minute_already_active())
    {
        return;
    }
    // Multiply by the processed rows to weight by block size
    add_point_to_minute(calc_median() * g_processed_block_rows);
    
    g_processed_block_rows = 0;
}

NO_PY(
void MIN_ACT_init(void) 
{
    // Update minutes active data based on whatever we saved in persistent memory.
    g_daily_min_active = __SAVED_ACROSS_BOOT.daily_minutes_active; g_prev_block_day = __SAVED_ACROSS_BOOT.minact_day; 
}
)

static void MIN_ACT_begin(uint64_t day_milli, uint64_t device_milli,
        size_t start_row)
{
    g_processed_block_rows = 0;
    
    reset_filter_chans(&g_filter_counts);
    reset_filter(&g_filter_mag);
    reset_median();
    
    g_block_day = day_milli > 0 ? current_day(day_milli, start_row) : 0;
    g_block_min = current_min(device_milli, start_row);
}

/* MIN_ACT_calc separates the input array into blocks and puts each block 
 * through calculate_point. Also updates external variables to indicate points
 * in a given minute and the number of active minutes in a day.
 */
void MIN_ACT_calc(int8_t *input_array, size_t num_points, size_t start_row,
        uint64_t device_milli, uint64_t day_milli)
{
    size_t filler_rows, rows_left, num_rows;
    
    //    D(print_accel_data(input_array, num_rows);)        
    
    num_rows = num_points / NUM_CHANS;
    
    // This algorithm can only operate on a square matrix of data where triaxial triplets are all present
    //NO_PY(wassert(num_rows * NUM_CHANS == num_points));
    
    // This algorithm can only take blocks smaller than the preallocated g_temp_vector
    NO_PY(wassert(num_rows <= MAX_INPUT_ROWS));
    
    /*
     *  If the day_milli is zero that means that we don't have the absolute time in the dog's time zone.
     *  As per conversations with Ernie, Kevin, and Steve on 7/8/2013 in this case we will not calculate
     *      points or increment the number of minutes active
     */
    if (day_milli == 0)
    {
        // If we loose the clock ignore the points until we get the clock again
        // We don't need to reset the stored data because we will show the
        //  goal was not met during this time and if the day changes when we get
        //  clock again we will automatically reset.  There is a small race condition
        //  here however when we loose clock because until we recalculate another point
        //  we will show an old value.
        D(corona_print("!time\n");)
        g_processed_block_rows = 0; // This way when we come back we won't use any old values
        return;
    }

    /* We only want to calculate points in BLOCKS where a block is a collection of ROWS_PER_BLOCK rows 
     * (where each row has NUM_CHANS elements). Meaning, if this function is passed in fewer than ROWS_PER_BLOCK
     * and no other rows have been processed yet, we don't want to calculate any points but instead just bin all
     * the new rows and wait until we get more to fill a block. The global variable EXISTING_ROWS keeps track of
     * how many rows have been binned but not finished. 
     */
    if (g_processed_block_rows == 0) // Only use MIN_ACT_begin() if we're about to start a fresh block.
    {
        MIN_ACT_begin(day_milli, device_milli, start_row);
    }
    
    if (g_processed_block_rows + num_rows < ROWS_PER_BLOCK)
    { // Still don't have enough rows to finish a block. Bin the new data and wait for more. 
    
        if (minute_already_active())
        {
            // If we have already passed the threshold for this minute there is
            //  no reason to calculate new points for this minute
            D(corona_print("Min %u act\n", g_block_min);)
            increment_processed_rows(num_rows);
        }
        else
        {
            D(corona_print("Add: %u\n", num_rows);)
            MIN_ACT_add(input_array, start_row, num_rows);
        }
    }
    else // Now have enough rows to complete a block ( possibly more than one ) 
    {
        filler_rows = ROWS_PER_BLOCK - g_processed_block_rows; // Rows remaining in this block
        rows_left = num_rows - filler_rows; // Input rows left to process
                
        // If we have already passed the threshold for this minute there is
        //  no reason to calculate new points for this minute
        if (minute_already_active())
        {
            D(corona_print("Min %u act\n", g_block_min);)
            g_processed_block_rows = 0;
        }
        else
        {
            D(corona_print("Fin: %u\n", num_rows);)
            MIN_ACT_add(input_array, start_row, filler_rows);
            MIN_ACT_finish();
        }
        
        if (rows_left > 0)
        {
            D(corona_print("New: %d\n", rows_left);)
            start_row = start_row + filler_rows;
            handle_large_block(input_array, rows_left, start_row, device_milli, day_milli);
        }
    }
}

/*
 * This function handles cases where the input data spans at least two blocks.
 */
static void handle_large_block(int8_t *input_array, size_t num_rows,
        size_t start_offset, uint64_t device_milli, uint64_t day_milli)
{
    size_t i, num_blocks, extra_rows, full_block_rows;
    
    num_blocks = num_rows / ROWS_PER_BLOCK; // This is rounded down
    full_block_rows = num_blocks * ROWS_PER_BLOCK;
    extra_rows = num_rows - full_block_rows;
    
    // Loop over full blocks within the input data set 
    // Any remaining points that did not fill a block will be handled outside of the loop
    for (i = 0; i < num_blocks; i++)
    {
        MIN_ACT_begin(day_milli, device_milli, start_offset);
        
        // If we have already passed the threshold for this minute there is
        //  no reason to calculate new points for this minute
        if (minute_already_active())
        {
            D(corona_print("Min %u act\n", g_block_min);)
            continue;
        }

        MIN_ACT_add(input_array, start_offset, ROWS_PER_BLOCK);
        MIN_ACT_finish();
        start_offset += ROWS_PER_BLOCK;
    }

    // If there are extra points in the range that have not been included in the current blocks
    if (extra_rows > 0)
    {
        // The initialization of this block is handled in MIN_ACT_calc     
        MIN_ACT_calc(input_array, extra_rows * NUM_CHANS,
                start_offset + full_block_rows, device_milli, day_milli);
    }
    return;
}

/* ==================== HELPER FUNCTIONS ==========================*/

/*   Handle Updating minutes active in persistent var area.
 */
static void _update_min_active(uint16_t min_active)
{
    g_daily_min_active = min_active;
    NO_PY( PMEM_VAR_SET(PV_DAILY_MINUTES_ACTIVE, &g_daily_min_active));
}

/*
 *   Handle updating the day corresponding to current minutes active in persist mem.
 */
static void _update_minact_day(uint16_t minact_day)
{
    g_prev_block_day = minact_day;
    NO_PY( PMEM_VAR_SET(PV_MIN_ACTIVE_DAY, &g_prev_block_day));
}

NO_PY(
        /*   Return zero if daily goal has not been met or the absolute time does not exist, non-zero otherwise.
         */
        uint8_t MIN_ACT_met_daily_goal(void) { return g_block_day > 0 && g_daily_min_active >= g_dy_cfg.usr_dy.daily_minutes_active_goal ? 1:0; }

        /*   Return the current number of minutes active
         */
        uint16_t MIN_ACT_current_minutes_active(void) { return g_daily_min_active; }

        )

/*
 * This function was deprecated in favor of the pound define because that was significantly faster
 *
 * Multiplication of two numbers in Q format. Taken from
 * http://en.wikipedia.org/wiki/Q_(number_format) 
 * static int32_t q_mult(int32_t a, int32_t b) 
 * {
 *     int32_t temp;
 *     temp = (a * b) + g_q_one_half; // for rounding purposes.
 *     return DS(temp);
 * }
 */

/* Distance approximation method edited and improved from
 * http://www.flipcode.com/archives/Fast_Approximate_Distance_Functions.shtml
 * Instead of using the min value as in the link above, we use the sum of the three
 * elements in the vector as it appeared to give superior performance.
 * The general approach is to approximate the RSS (vector length) without the 
 * need to square the number (which drastically increases the magnitude of the number)
 * or compute the square root (which is computationally intensive) while remaining
 * rotationally invariant. This method appeared to have ~10% peak error compared to 
 * the true RSS which should be acceptable.
 */
int32_t approx_dist(const int32_t min, const int32_t max, const int32_t sum)
{
    if (Q_MULT(g_approx_dist_factor, max) < sum)
    {
        return Q_MULT(g_c_min[0], min) + Q_MULT(g_c_max[0], max)
                + Q_MULT(g_c_sum[0], sum);
    }
    else
    {
        return Q_MULT(g_c_min[1], min) + Q_MULT(g_c_max[1], max)
                + Q_MULT(g_c_sum[1], sum);
    }
}

//void MIN_ACT_print_vals(void)
//{
//    corona_print("Min Act: %u\n", g_daily_min_active);
//    corona_print("Cur Pnts: %u\n", g_points_this_min);
//    corona_print("Cur Min: %d\n", g_prev_block_min);
//    corona_print("Cur Day: %d\n", g_prev_block_day);
//    NO_PY(corona_print("Goal %u mins", g_dy_cfg.usr_dy.daily_minutes_active_goal));
//    NO_PY(if( MIN_ACT_met_daily_goal() ) { corona_print(" met\n"); } else { corona_print(" NOT met\n"); })
//}

#ifdef PY_MODULE
/* GETTERS */
float MIN_ACT_get_points_this_minute(void)
{   
    return (float) g_points_this_min / MIN_ACT_ADJ_FACTOR;
}

float MIN_ACT_get_active_threshold(void)
{   
    return (float) ACT_MIN_THRESHOLD / MIN_ACT_ADJ_FACTOR;
}

uint16_t MIN_ACT_get_prev_block_min(void)
{   
    return g_prev_block_min;
}

uint16_t MIN_ACT_get_prev_block_day(void)
{   
    return g_prev_block_day;
}

void MIN_ACT_reset_filters(void)
{   
    /* Reset the filters */
    reset_filter_chans(&g_filter_counts);
    reset_filter(&g_filter_mag);
}

void MIN_ACT_clear(void)
{   
    /* Clear all the global variables */
    g_prev_block_day = 0;
    g_prev_block_min = 0;
    g_daily_min_active = 0;
    g_points_this_min = 0;
    g_block_min = 0;
    g_block_day = 0;
    g_processed_block_rows = 0;
}
#endif
#else
// If don't calculate the minutes active we should NEVER call this function
uint8_t MIN_ACT_met_daily_goal(void)
{   
    wassert(0);
    return 0;
}
#endif
