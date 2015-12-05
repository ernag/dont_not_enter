/*
 * bu26507.c
 *
 *  Created on: Apr 11, 2013
 *      Author: Chris
 */

#include <mqx.h>
#include <bsp.h>
#include <i2c.h>
#include "wassert.h"
#include "bu26507.h"
#include "i2c1.h"
#include "cfg_mgr.h"

typedef enum bu_color_type
{
    BU_COLOR_RED,
    BU_COLOR_WHITE,
    BU_COLOR_GREEN,
    BU_COLOR_BLUE,
    BU_COLOR_YELLOW,
    BU_COLOR_PURPLE
}bu_color_t;

typedef enum bu_bank_type
{
	BU_BANK_A,
	BU_BANK_B,
	BU_BANK_AB
} bu_bank_t;

MQX_FILE_PTR bu26507_fd = NULL;

static const uint_8 matrix_map[] =
{ 0, 15, 1, 16, 2, 17, 3, 18, 4, 19 };

static const uint_8 color_map[] = 
{ 0, 5, 10 };

static const display_cmd_t solid_color_cmd_template[] =
{
        { 0x7F, 0x00 },          // 1)  7FH 00000000 Select control register
        { 0x21, 0x04 },          // 2)  21H 00000000 Select internal OSC for CLK,  AND make SYNC pin = 'H' control LED's on/off.
        { 0x01, 0x08 },          // 3)  01H 00001000 Start OSC
        { 0x11, 0x3F },          // 4)  11H ALL ON   Set LED1-6 enable
        { 0x20, 0x3F },          // 7)  20H 00111111 Set Max Duty at Slope
        { 0x2D, 0x06 },          // 11) 2DH 00000110 Set SLOPE control enable
        { 0x30, 0x01 },          // 12) 30H 00000001 Start SLOPE sequence
};
#define NUM_SOLID_COLOR_CMDS    ( sizeof(solid_color_cmd_template) / sizeof(display_cmd_t) )

///////////////////////////////////////////////////////
matrix_info_t rgb_data;

///////////////////////////////////////////////////////


// static void bu26507_xcfg_matrix(pmatrix_info_t matrix_led_info, uint8_t* pmatrix);
static bu26507_set_matrix( uint_8 *pdata, uint_8 num );
static bu26507_set_matrix( uint_8 *pdata, uint_8 num )
{
    uint8_t OutData[2];
    int i;
    uint_32 param;
    err_t err = ERR_OK;
      
    wassert(ERR_OK == bu26507_init());
    
    for (i = 0; i < num ; i++)
    {
        int retries = 100;
        
        OutData[0] = 0x01+i;
        OutData[1] = pdata[i];
        
        while(retries-- > 0)
        {
            wassert(2 == fwrite( OutData, 1, 2, bu26507_fd));
            
            /* Check ack (device exists) */
            wassert(I2C_OK == ioctl(bu26507_fd, IO_IOCTL_FLUSH_OUTPUT, &param));
            
            /* Stop I2C transfer */
            wassert(I2C_OK == ioctl(bu26507_fd, IO_IOCTL_I2C_STOP, NULL));
            
            if (param)
            {
                err = ERR_GENERIC;
                continue;
            }
            else
            {
                err = 0;
                break;
            }
        }
        
        if(err)
        {
            goto display_done;
        }
        _time_delay(1);
    }
    
display_done:
    wassert(ERR_OK == err);
    I2C1_liberate( &bu26507_fd );
    return err;
}


static err_t bu26507_run(display_cmd_t *display_cmds, int num_cmds)
{
    uint8_t OutData[2] = { BU_LED_EN_ADDR, 0x3F /*(LED1_MASK | LED3_MASK | LED5_MASK)*/ }; // LED enable register, try illuminating interleaved LED's.
    int i;
    uint_32 param;
    _mqx_int num_bytes_written;
    err_t err = ERR_OK;
      
    // Initialize the I2C device.
    // NOTE:  Slave Address of the BU26507 is 0x74.  This is shifted to the left since the R/W bit is LSB of
    //        the 8-bit slave address we send.  So slave address is the MS 7 bits.
    //        So PE generates this code for the address: '''DeviceDataPrv->SlaveAddr = 0xE8U;'''
    wassert(ERR_OK == bu26507_init());
    
    for (i = 0; i < num_cmds; i++)
    {
        int retries = 100;
        
        OutData[0] = display_cmds[i].reg;
        OutData[1] = display_cmds[i].data;
        
        while(retries-- > 0)
        {
            num_bytes_written = fwrite(OutData, 1, 2, bu26507_fd);
            //wassert( 2 == num_bytes_written );
            if ( 2 != num_bytes_written )
            {
                PROCESS_NINFO(ERR_LED_GENERIC, "bu err: %d i: %d retries: %d", num_bytes_written, i, retries);
                goto display_done;
            }
            
            
            /* Check ack (device exists) */
            wassert(I2C_OK == ioctl(bu26507_fd, IO_IOCTL_FLUSH_OUTPUT, &param));
            
            /* Stop I2C transfer */
            wassert(I2C_OK == ioctl(bu26507_fd, IO_IOCTL_I2C_STOP, NULL));
            
            if (param)
            {
                err = ERR_GENERIC;
                continue;
            }
            else
            {
                err = 0;
                break;
            }
        }
        
        if(err)
        {
            goto display_done;
        }
        _time_delay(1);
    }
    
display_done:
    wassert(ERR_OK == err);
    wassert(ERR_OK == I2C1_liberate( &bu26507_fd ));
    return err;
}

// Matrix colors
uint8_t M_GRN()
{
    if (LEDFLEX_EVERBRIGHT <= g_st_cfg.fac_st.lpcbarev)
    {
        return 0x01;
    }
    else 
    {
        return 0x02;
    }
}

uint8_t M_BLU()
{
    if (LEDFLEX_EVERBRIGHT <= g_st_cfg.fac_st.lpcbarev)
    {
        return 0x02;
    }
    else 
    {
        return 0x01;
    }
}

uint8_t M_RED()
{
    return 0x04;
}

uint8_t M_WHT()
{
    return 0x07;
}

uint8_t M_YEL()
{
    return M_RED() | M_GRN();
}

uint8_t M_PRP()
{
    return M_RED() | M_BLU();
}

static uint8_t _RED(bu_bank_t bank)
{
	switch(bank)
	{
        case BU_BANK_A:
            return BU_LED3_MASK;
        case BU_BANK_B:
            return BU_LED6_MASK;
        case BU_BANK_AB:
            return  BU_LED6_MASK | BU_LED3_MASK;
        default:
            break;
	}
	
	return 0x3F; // should never get here.
}

static uint8_t _WHITE(bu_bank_t bank)
{
	switch(bank)
	{
        case BU_BANK_A:
            return BU_LED_MASK_A;
        case BU_BANK_B:
            return BU_LED_MASK_B;
        case BU_BANK_AB:
            return BU_LED_MASK_AB;
        default:
            break;
	}
	
	return 0x3F; // should never get here.
}


static uint8_t _BLUE(bu_bank_t bank)
{
	switch(bank)
	{
        case BU_BANK_A:
            if( LEDFLEX_EVERBRIGHT <= g_st_cfg.fac_st.lpcbarev )
            {
                return BU_LED2_MASK;
            }
            else
            {
                return BU_LED1_MASK;
            }
        case BU_BANK_B:
            if( LEDFLEX_EVERBRIGHT <= g_st_cfg.fac_st.lpcbarev )
            {
                return BU_LED5_MASK;
            }
            else
            {
                return BU_LED4_MASK;
            }
        case BU_BANK_AB:
            if( LEDFLEX_EVERBRIGHT <= g_st_cfg.fac_st.lpcbarev )
            {
                return BU_LED5_MASK | BU_LED2_MASK;
            }
            else
            {
                return BU_LED4_MASK | BU_LED1_MASK;
            }
        default:
            break;
	}
	
	return 0x3F; // should never get here.
}

static uint8_t _GREEN(bu_bank_t bank)
{
	switch(bank)
	{
        case BU_BANK_A:
            if( LEDFLEX_EVERBRIGHT <= g_st_cfg.fac_st.lpcbarev )
            {
                return BU_LED1_MASK;
            }
            else
            {
                return BU_LED2_MASK;
            }
        case BU_BANK_B:
            if( LEDFLEX_EVERBRIGHT <= g_st_cfg.fac_st.lpcbarev )
            {
                return BU_LED4_MASK;
            }
            else
            {
                return BU_LED5_MASK;
            }
        case BU_BANK_AB:
            if( LEDFLEX_EVERBRIGHT <= g_st_cfg.fac_st.lpcbarev )
            {
                return BU_LED4_MASK | BU_LED1_MASK;
            }
            else
            {
                return BU_LED5_MASK | BU_LED2_MASK;
            }
        default:
            break;
	}
	
	return 0x3F; // should never get here.
}

static uint8_t _YELLOW(bu_bank_t bank)
{
    return ( _RED(bank) | _GREEN(bank) );
}

static uint8_t _PURPLE(bu_bank_t bank)
{
    return ( _RED(bank) | _BLUE(bank) );
}

/*
 *   Fill up with template of solid color commands.
 *   Search for 0x11 register.  Fill up corresponding data field with passed color.
 */
static void bu26507_color( bu_color_t c, display_cmd_t *pCmds, uint32_t num_cmds, bu_bank_t bank )
{
    uint32_t i;
    uint8_t data;
    
    memcpy(pCmds, solid_color_cmd_template, num_cmds*sizeof(display_cmd_t));

    switch(c)
    {
        case BU_COLOR_RED:
            data = _RED(bank);
            break;
        case BU_COLOR_GREEN:
            data = _GREEN(bank);
            break;
        case BU_COLOR_BLUE:
            data = _BLUE(bank);
            break;
        case BU_COLOR_YELLOW:
            data = _YELLOW(bank);
            break;
        case BU_COLOR_PURPLE:
            data = _PURPLE(bank);
            break;
        case BU_COLOR_WHITE:
        default:
            data = _WHITE(bank);
            break;
    }
    
    for(i = 0; i < num_cmds; i++)
    {
        if( BU_LED_EN_ADDR == pCmds[i].reg )
        {
            /*
             *   We found the appropriate register, set the corresponding color.
             */
            pCmds[i].data = data;
            return;
        }
    }
}

/*
 *   Run a solid color pattern.
 */
static err_t bu26507_run_solid_color( bu_color_t c, bu_bank_t bank )
{
    display_cmd_t display_cmds[NUM_SOLID_COLOR_CMDS];

    bu26507_color( c, display_cmds, NUM_SOLID_COLOR_CMDS, bank );
    return bu26507_run(display_cmds, NUM_SOLID_COLOR_CMDS);
}

/*
 *   Pattern1:  Solid RED, both banks
 */
static err_t bu26507_run_pattern1(void)
{
    return bu26507_run_solid_color( BU_COLOR_RED, BU_BANK_AB );
}

/*
 *   Pattern2:  Solid WHITE, both banks
 */
static err_t bu26507_run_pattern2(void)
{
    return bu26507_run_solid_color( BU_COLOR_WHITE, BU_BANK_AB );
}

/*
 *   Pattern3:  Solid BLUE, both banks
 */
static err_t bu26507_run_pattern3(void)
{
    return bu26507_run_solid_color( BU_COLOR_BLUE, BU_BANK_AB );
}

/*
 *   Pattern4:  Solid GREEN, both banks
 */
static err_t bu26507_run_pattern18(void)
{
    return bu26507_run_solid_color( BU_COLOR_GREEN, BU_BANK_AB );
}

/*
 *   Pattern5:  Solid YELLOW  (sings Coldplay...), both banks
 */
static err_t bu26507_run_pattern54(void)
{
    return bu26507_run_solid_color( BU_COLOR_YELLOW, BU_BANK_AB );
}

/*
 *   Pattern6:  Solid RED, bank A
 */
static err_t bu26507_run_pattern101(void)
{
    return bu26507_run_solid_color( BU_COLOR_RED, BU_BANK_A );
}

/*
 *   Pattern7:  Solid WHITE, bank A
 */
static err_t bu26507_run_pattern102(void)
{
    return bu26507_run_solid_color( BU_COLOR_WHITE, BU_BANK_A );
}

/*
 *   Pattern8:  Solid BLUE, bank A
 */
static err_t bu26507_run_pattern103(void)
{
    return bu26507_run_solid_color( BU_COLOR_BLUE, BU_BANK_A );
}

/*
 *   Pattern9:  Solid GREEN, bank A
 */
static err_t bu26507_run_pattern118(void)
{
    return bu26507_run_solid_color( BU_COLOR_GREEN, BU_BANK_A );
}

/*
 *   Pattern10:  Solid YELLOW  (sings Coldplay...), bank A
 */
static err_t bu26507_run_pattern154(void)
{
    return bu26507_run_solid_color( BU_COLOR_YELLOW, BU_BANK_A );
}

/*
 *   Pattern11:  Solid RED, bank B
 */
static err_t bu26507_run_pattern201(void)
{
    return bu26507_run_solid_color( BU_COLOR_RED, BU_BANK_B );
}

/*
 *   Pattern12:  Solid WHITE, bank B
 */
static err_t bu26507_run_pattern202(void)
{
    return bu26507_run_solid_color( BU_COLOR_WHITE, BU_BANK_B );
}

/*
 *   Pattern13:  Solid BLUE, bank B
 */
static err_t bu26507_run_pattern203(void)
{
    return bu26507_run_solid_color( BU_COLOR_BLUE, BU_BANK_B );
}

/*
 *   Pattern14:  Solid GREEN, bank B
 */
static err_t bu26507_run_pattern218(void)
{
    return bu26507_run_solid_color( BU_COLOR_GREEN, BU_BANK_B );
}

/*
 *   Pattern15:  Solid YELLOW  (sings Coldplay...), bank B
 */
static err_t bu26507_run_pattern254(void)
{
    return bu26507_run_solid_color( BU_COLOR_YELLOW, BU_BANK_B );
}

/*
 *   Pattern16:  Solid PURPLE  (sings Jimi Hendrix...), both banks
 */
static err_t bu26507_run_pattern55(void)
{
    return bu26507_run_solid_color( BU_COLOR_PURPLE, BU_BANK_AB );
}

/*
 *   Pattern17:  Solid PURPLE  (sings Jimi Hendrix...), bank A
 */
static err_t bu26507_run_pattern155(void)
{
    return bu26507_run_solid_color( BU_COLOR_PURPLE, BU_BANK_A );
}

/*
 *   Pattern18:  Solid PURPLE  (sings Jimi Hendrix...), bank B
 */
static err_t bu26507_run_pattern255(void)
{
    return bu26507_run_solid_color( BU_COLOR_PURPLE, BU_BANK_B );
}

/*
 *   OFF!
 */
static err_t bu26507_run_pattern_off(void)
{    
    // Turn off LEDs and place Rohm chip in standby
    return bu26507_standby();
}

//
// Pattern_MY3S: Matrix yellow 3 seconds, 1 slope slow cycle
//
static err_t bu26507_run_pattern_MY3S(void)
{
    matrix_setup_t  mxs;
    
    memset (&mxs, 0, sizeof(mxs));
    mxs.a_led_mask   = ALL_LEDS;
    mxs.a_color_mask = M_YEL();
    mxs.brightness   = FULL_BRIGHT;
    mxs.slope_cyc    = SLP_CYC_MAX;
    bu26507_run_matrix (&mxs, IS_SLOPE);

    _time_delay(2500);
    return ERR_OK;
}

//
// Pattern_MY3P: Matrix yellow 3 quick pulses, 3 slope min cycles
//
static err_t bu26507_run_pattern_MY3P(void)
{
    matrix_setup_t  mxs;
    
    memset (&mxs, 0, sizeof(mxs));
    mxs.a_led_mask   = ALL_LEDS;
    mxs.a_color_mask = M_YEL();
    mxs.brightness   = FULL_BRIGHT;
    mxs.slope_cyc    = SLP_CYC_MIN;
    bu26507_run_matrix (&mxs, IS_SLOPE);

    _time_delay(3000);

    return ERR_OK;
}

//
// Pattern_MR1P: Matrix red 1 quick pulses, 1 slope min cycles
//
static err_t bu26507_run_pattern_MR1P(void)
{
    matrix_setup_t  mxs;
    memset (&mxs, 0, sizeof(mxs));
    
    mxs.a_led_mask   = ALL_LEDS;
    mxs.a_color_mask = M_RED();
    mxs.brightness   = FULL_BRIGHT;
    mxs.slope_cyc    = SLP_CYC_MIN;
    bu26507_run_matrix (&mxs, IS_SLOPE);

    _time_delay(1000);
    return ERR_OK;
}

//
// Pattern_MWCDC: Matrix blue continuous slow slope cycle
//
static err_t bu26507_run_pattern_BT_PAIR(void)
{
    matrix_setup_t  mxs;
    memset (&mxs, 0, sizeof(mxs));
    
    mxs.a_led_mask   = ALL_LEDS;
    mxs.a_color_mask = M_GRN() | M_BLU();
    mxs.brightness   = XXXX_BRIGHT;
    mxs.g_level      = 0x05;
    mxs.b_level      = 0x15;
    mxs.slope_cyc    = SLP_CYC_MAX;
    bu26507_run_matrix (&mxs, IS_SLOPE);

    return ERR_OK;
}

//
// Pattern_CHGING: Matrix solid reddish orange
//
static err_t bu26507_run_pattern_CHGING(void)
{
    matrix_setup_t  mxs;
    memset (&mxs, 0, sizeof(mxs));
    
    mxs.a_led_mask   = ALL_LEDS;
    mxs.a_color_mask = M_YEL();   // red | green, lower level of grn makes amber/orange...
    mxs.brightness   = XXXX_BRIGHT;
    mxs.g_level      = 0x01;
    mxs.r_level      = FULL_BRIGHT;
    // mxs.slope_cyc    = SLP_CYC_MAX;
    bu26507_run_matrix (&mxs, IS_SLOPE);

    return ERR_OK;
}

//
// Pattern_CHGING: Matrix solid reddish green
//
static err_t bu26507_run_pattern_CHGD(void)
{
    matrix_setup_t  mxs;
    memset (&mxs, 0, sizeof(mxs));
    
    mxs.a_led_mask   = ALL_LEDS;
    mxs.a_color_mask = M_WHT();
    mxs.brightness   = XXXX_BRIGHT;
    mxs.g_level      = 0x015;
    bu26507_run_matrix (&mxs, IS_SLOPE);

    return ERR_OK;
}

//
// Pattern_MW3S: Matrix white 3 seconds, 1 slope slow cycle
//
static err_t bu26507_run_pattern_MW3S(void)
{
    matrix_setup_t  mxs;
    memset (&mxs, 0, sizeof(mxs));
    
    mxs.a_led_mask   = ALL_LEDS;
    mxs.a_color_mask = M_WHT();
    mxs.brightness   = FULL_BRIGHT;
    mxs.slope_cyc    = SLP_CYC_MAX;
    bu26507_run_matrix (&mxs, IS_SLOPE);

    _time_delay(2500);
    return ERR_OK;
}

//
// Pattern_MW2S: Matrix white 2 seconds, 1 slope med cycle
//
static err_t bu26507_run_pattern_MW2S(void)
{
    matrix_setup_t  mxs;
    memset (&mxs, 0, sizeof(mxs));
    
    mxs.a_led_mask   = ALL_LEDS;
    mxs.brightness   = FULL_BRIGHT;
    mxs.a_color_mask = M_WHT();
    mxs.slope_cyc    = SLP_CYC_MED;
    mxs.g_level      = 0x15;

    bu26507_run_matrix (&mxs, IS_SLOPE);

    _time_delay(1500);
    return ERR_OK;
}

//
// Pattern_MA3S: Matrix activity color 3 seconds, 1 slope slow cycle
//
static err_t bu26507_run_pattern_MA3S(void)
{
    matrix_setup_t  mxs;
    uint8_t color;
    
    memset (&mxs, 0, sizeof(mxs));
    color = MIN_ACT_met_daily_goal() ? M_GRN() : M_BLU();
    
    mxs.a_led_mask   = ALL_LEDS;
    mxs.a_color_mask = color;
    mxs.brightness   = FULL_BRIGHT;
    mxs.slope_cyc    = SLP_CYC_MAX;
    bu26507_run_matrix (&mxs, IS_SLOPE);

    _time_delay(2500);
    return ERR_OK;
}

//
// Pattern_BA3S: Matrix battery color 3 seconds, 1 slope slow cycle
//
static err_t bu26507_run_pattern_BA3S(void)
{
    matrix_setup_t  mxs;
    uint8_t color;
    
    memset (&mxs, 0, sizeof(mxs));
    // color = PWRMGR_is_battery_low() ? M_RED() : M_GRN();
    mxs.a_led_mask   = ALL_LEDS;
    mxs.brightness   = XXXX_BRIGHT;
    mxs.a_color_mask = M_WHT();
    mxs.slope_cyc    = SLP_CYC_MAX;

    if (PWRMGR_is_battery_low())    // Reddish Orange
    {
        mxs.r_level      = 0x15;
        mxs.g_level      = 0x01;
        // mxs.b_level      = 0x00;
    }
    else    // Green
    {
        // mxs.r_level      = 0x03;
        mxs.g_level      = 0x15;
        // mxs.b_level      = 0x00;
    }
    bu26507_run_matrix (&mxs, IS_SLOPE);

    _time_delay(2500);
    return ERR_OK;
}

//
// Pattern_pwr_drain_test: Matrix purple continuous pulses, fast slope cycles
//
#if 0
static err_t bu26507_run_pattern_pwr_drain_test(void)
{
    matrix_setup_t  mxs;
    memset (&mxs, 0, sizeof(mxs));
    
    mxs.a_led_mask   = ALL_LEDS;
    mxs.a_color_mask = M_PRP();   // red | blue
    mxs.brightness   = FULL_BRIGHT;
    // mxs.b_level      = 0x05;
    // mxs.r_level      = FULL_BRIGHT;
    mxs.slope_cyc    = SLP_CYC_MIN;
    bu26507_run_matrix (&mxs, IS_SLOPE);

    return ERR_OK;
}
#endif

//
// Pattern_rgb command
//
static err_t bu26507_run_pattern_rgb(void)
{
    matrix_setup_t  mxs;
    
    memset (&mxs, 0, sizeof(mxs));
    
    mxs.a_led_mask   = ALL_LEDS;
    mxs.a_color_mask = 0x07;            // all colors
    mxs.brightness   = XXXX_BRIGHT;
    mxs.g_level      = rgb_data.g_level;
    mxs.b_level      = rgb_data.b_level;
    mxs.r_level      = rgb_data.r_level;
    mxs.slope_cyc    = SLP_CYC_MAX;
    bu26507_run_matrix (&mxs, IS_SLOPE);
    
    _time_delay(2500);


    
    
#if 0
#define SZO(A)   sizeof(A)/2

//Patternmaker-B Macro File 
//I2C FORMAT
//
static display_cmd_t pm1[] =
{
    {0x00, 0x01},  //Software reset
    {0x01, 0x08},  //OSC EN
};
//
//Side A matrix register setting for IC1
static display_cmd_t pm2[] =
{
    {0x7f, 0x01},  //RMCG, OAB, IAB
    {0x01, 0x00},  //LED 00
    {0x02, 0x00},  //LED 01
    {0x03, 0x00},  //LED 02
    {0x04, 0x00},  //LED 03
    {0x05, 0x00},  //LED 04
    {0x06, 0x00},  //LED 05
    {0x07, 0x00},  //LED 06
    {0x08, 0x00},  //LED 07
    {0x09, 0x00},  //LED 08
    {0x0a, 0x00},  //LED 09
    {0x0b, 0x00},  //LED 0A
    {0x0c, 0x00},  //LED 0B
    {0x0d, 0x00},  //LED 0C
    {0x0e, 0x00},  //LED 0D
    {0x0f, 0x00},  //LED 0E
    {0x10, 0x00},  //LED 0F
    {0x11, 0x00},  //LED 10
    {0x12, 0x00},  //LED 11
    {0x13, 0x00},  //LED 12
    {0x14, 0x00},  //LED 13
    {0x15, 0x00},  //LED 14
    {0x16, 0x00},  //LED 15
    {0x17, 0x00},  //LED 16
    {0x18, 0x00},  //LED 17
    {0x19, 0x00},  //LED 18
    {0x1a, 0x00},  //LED 19
    {0x1b, 0x00},  //LED 1A
    {0x1c, 0x00},  //LED 1B
    {0x1d, 0x00},  //LED 1C
    {0x1e, 0x00},  //LED 1D
};
//
//Side B matrix register setting for IC1
static display_cmd_t pm3[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x01, 0x00},  //LED 00
    {0x02, 0x00},  //LED 01
    {0x03, 0x00},  //LED 02
    {0x04, 0x00},  //LED 03
    {0x05, 0x00},  //LED 04
    {0x06, 0x00},  //LED 05
    {0x07, 0x00},  //LED 06
    {0x08, 0x00},  //LED 07
    {0x09, 0x00},  //LED 08
    {0x0a, 0x00},  //LED 09
    {0x0b, 0x00},  //LED 0A
    {0x0c, 0x00},  //LED 0B
    {0x0d, 0x00},  //LED 0C
    {0x0e, 0x00},  //LED 0D
    {0x0f, 0x00},  //LED 0E
    {0x10, 0x00},  //LED 0F
    {0x11, 0x00},  //LED 10
    {0x12, 0x00},  //LED 11
    {0x13, 0x00},  //LED 12
    {0x14, 0x00},  //LED 13
    {0x15, 0x00},  //LED 14
    {0x16, 0x00},  //LED 15
    {0x17, 0x00},  //LED 16
    {0x18, 0x00},  //LED 17
    {0x19, 0x00},  //LED 18
    {0x1a, 0x00},  //LED 19
    {0x1b, 0x00},  //LED 1A
    {0x1c, 0x00},  //LED 1B
    {0x1d, 0x00},  //LED 1C
    {0x1e, 0x00},  //LED 1D
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//Control register setting for IC1
static display_cmd_t pm4[] =
{
    {0x01, 0x08},  //OSCEN
    {0x11, 0x3f},  //LED1ON-LED6ON
    {0x20, 0x3f},  //PWMSET
    {0x21, 0x02},  //CLKIN, CLKOUT, SYNCON, SYNCACT
    {0x2d, 0x00},  //SCLEN, SLPEN, PWMEN
    {0x2e, 0x00},  //SCLRST
    {0x2f, 0x01},  //LEFT, RIGHT, DOWN, UP, SCLSPEED
};
//
static display_cmd_t pm5[] =
{
    {0x30, 0x01},  //START
};
//
//Side A-1 matrix register setting for IC1
static display_cmd_t pm6[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x01, 0x01},  //LED 00
    {0x02, 0x00},  //LED 01
    {0x03, 0x00},  //LED 02
    {0x04, 0x00},  //LED 03
    {0x05, 0x00},  //LED 04
    {0x06, 0x00},  //LED 10
    {0x07, 0x00},  //LED 11
    {0x08, 0x00},  //LED 12
    {0x09, 0x00},  //LED 13
    {0x0a, 0x00},  //LED 14
    {0x0b, 0x01},  //LED 20
    {0x0c, 0x00},  //LED 21
    {0x0d, 0x00},  //LED 22
    {0x0e, 0x00},  //LED 23
    {0x0f, 0x00},  //LED 24
    {0x10, 0x00},  //LED 30
    {0x11, 0x00},  //LED 31
    {0x12, 0x00},  //LED 32
    {0x13, 0x00},  //LED 33
    {0x14, 0x00},  //LED 34
    {0x15, 0x00},  //LED 40
    {0x16, 0x00},  //LED 41
    {0x17, 0x00},  //LED 42
    {0x18, 0x00},  //LED 43
    {0x19, 0x00},  //LED 44
    {0x1a, 0x00},  //LED 50
    {0x1b, 0x00},  //LED 51
    {0x1c, 0x00},  //LED 52
    {0x1d, 0x00},  //LED 53
    {0x1e, 0x00},  //LED 54
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-1 matrix register setting for IC1
static display_cmd_t pm7[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x01, 0x03},  //LED 00
    {0x02, 0x00},  //LED 01
    {0x03, 0x00},  //LED 02
    {0x04, 0x00},  //LED 03
    {0x05, 0x00},  //LED 04
    {0x06, 0x00},  //LED 10
    {0x07, 0x00},  //LED 11
    {0x08, 0x00},  //LED 12
    {0x09, 0x00},  //LED 13
    {0x0a, 0x00},  //LED 14
    {0x0b, 0x03},  //LED 20
    {0x0c, 0x00},  //LED 21
    {0x0d, 0x00},  //LED 22
    {0x0e, 0x00},  //LED 23
    {0x0f, 0x00},  //LED 24
    {0x10, 0x01},  //LED 30
    {0x11, 0x00},  //LED 31
    {0x12, 0x00},  //LED 32
    {0x13, 0x00},  //LED 33
    {0x14, 0x00},  //LED 34
    {0x15, 0x00},  //LED 40
    {0x16, 0x00},  //LED 41
    {0x17, 0x00},  //LED 42
    {0x18, 0x00},  //LED 43
    {0x19, 0x00},  //LED 44
    {0x1a, 0x01},  //LED 50
    {0x1b, 0x00},  //LED 51
    {0x1c, 0x00},  //LED 52
    {0x1d, 0x00},  //LED 53
    {0x1e, 0x00},  //LED 54
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//
//
//Side A-2 matrix register setting for IC1
static display_cmd_t pm8[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x01, 0x09},  //LED 00
    {0x02, 0x01},  //LED 01
    {0x0b, 0x09},  //LED 20
    {0x0c, 0x01},  //LED 21
    {0x10, 0x03},  //LED 30
    {0x1a, 0x03},  //LED 50
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-2 matrix register setting for IC1
static display_cmd_t pm9[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x01, 0x0c},  //LED 00
    {0x02, 0x03},  //LED 01
    {0x0b, 0x0c},  //LED 20
    {0x0c, 0x03},  //LED 21
    {0x10, 0x09},  //LED 30
    {0x11, 0x01},  //LED 31
    {0x1a, 0x09},  //LED 50
    {0x1b, 0x01},  //LED 51
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//
//
//Side A-3 matrix register setting for IC1
static display_cmd_t pm10[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x01, 0x0c},  //LED 00
    {0x02, 0x06},  //LED 01
    {0x03, 0x01},  //LED 02
    {0x0b, 0x0c},  //LED 20
    {0x0c, 0x06},  //LED 21
    {0x0d, 0x01},  //LED 22
    {0x10, 0x09},  //LED 30
    {0x11, 0x03},  //LED 31
    {0x1a, 0x09},  //LED 50
    {0x1b, 0x03},  //LED 51
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-3 matrix register setting for IC1
static display_cmd_t pm11[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x01, 0x0f},  //LED 00
    {0x02, 0x09},  //LED 01
    {0x03, 0x03},  //LED 02
    {0x0b, 0x0f},  //LED 20
    {0x0c, 0x09},  //LED 21
    {0x0d, 0x03},  //LED 22
    {0x10, 0x0c},  //LED 30
    {0x11, 0x06},  //LED 31
    {0x12, 0x01},  //LED 32
    {0x1a, 0x0c},  //LED 50
    {0x1b, 0x06},  //LED 51
    {0x1c, 0x01},  //LED 52
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//
//
//Side A-4 matrix register setting for IC1
static display_cmd_t pm12[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x02, 0x0c},  //LED 01
    {0x03, 0x06},  //LED 02
    {0x04, 0x01},  //LED 03
    {0x0c, 0x0c},  //LED 21
    {0x0d, 0x06},  //LED 22
    {0x0e, 0x01},  //LED 23
    {0x10, 0x0f},  //LED 30
    {0x11, 0x09},  //LED 31
    {0x12, 0x03},  //LED 32
    {0x1a, 0x0f},  //LED 50
    {0x1b, 0x09},  //LED 51
    {0x1c, 0x03},  //LED 52
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-4 matrix register setting for IC1
static display_cmd_t pm13[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x01, 0x0c},  //LED 00
    {0x02, 0x0f},  //LED 01
    {0x03, 0x09},  //LED 02
    {0x04, 0x03},  //LED 03
    {0x0b, 0x0c},  //LED 20
    {0x0c, 0x0f},  //LED 21
    {0x0d, 0x09},  //LED 22
    {0x0e, 0x03},  //LED 23
    {0x10, 0x0f},  //LED 30
    {0x11, 0x0c},  //LED 31
    {0x12, 0x06},  //LED 32
    {0x13, 0x01},  //LED 33
    {0x1a, 0x0f},  //LED 50
    {0x1b, 0x0c},  //LED 51
    {0x1c, 0x06},  //LED 52
    {0x1d, 0x01},  //LED 53
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//
//
//Side A-5 matrix register setting for IC1
static display_cmd_t pm14[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x01, 0x09},  //LED 00
    {0x02, 0x0f},  //LED 01
    {0x03, 0x0c},  //LED 02
    {0x04, 0x06},  //LED 03
    {0x05, 0x01},  //LED 04
    {0x0b, 0x09},  //LED 20
    {0x0c, 0x0f},  //LED 21
    {0x0d, 0x0c},  //LED 22
    {0x0e, 0x06},  //LED 23
    {0x0f, 0x01},  //LED 24
    {0x10, 0x0c},  //LED 30
    {0x11, 0x0f},  //LED 31
    {0x12, 0x09},  //LED 32
    {0x13, 0x03},  //LED 33
    {0x1a, 0x0c},  //LED 50
    {0x1b, 0x0f},  //LED 51
    {0x1c, 0x09},  //LED 52
    {0x1d, 0x03},  //LED 53
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-5 matrix register setting for IC1
static display_cmd_t pm15[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x01, 0x06},  //LED 00
    {0x02, 0x0c},  //LED 01
    {0x03, 0x0f},  //LED 02
    {0x04, 0x09},  //LED 03
    {0x05, 0x03},  //LED 04
    {0x0b, 0x06},  //LED 20
    {0x0c, 0x0c},  //LED 21
    {0x0d, 0x0f},  //LED 22
    {0x0e, 0x09},  //LED 23
    {0x0f, 0x03},  //LED 24
    {0x10, 0x09},  //LED 30
    {0x11, 0x0f},  //LED 31
    {0x12, 0x0c},  //LED 32
    {0x13, 0x06},  //LED 33
    {0x14, 0x01},  //LED 34
    {0x1a, 0x09},  //LED 50
    {0x1b, 0x0f},  //LED 51
    {0x1c, 0x0c},  //LED 52
    {0x1d, 0x06},  //LED 53
    {0x1e, 0x01},  //LED 54
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//
//
//Side A-6 matrix register setting for IC1
static display_cmd_t pm16[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x01, 0x03},  //LED 00
    {0x02, 0x09},  //LED 01
    {0x03, 0x0f},  //LED 02
    {0x04, 0x0c},  //LED 03
    {0x05, 0x06},  //LED 04
    {0x0b, 0x03},  //LED 20
    {0x0c, 0x09},  //LED 21
    {0x0d, 0x0f},  //LED 22
    {0x0e, 0x0c},  //LED 23
    {0x0f, 0x06},  //LED 24
    {0x10, 0x06},  //LED 30
    {0x11, 0x0c},  //LED 31
    {0x12, 0x0f},  //LED 32
    {0x13, 0x09},  //LED 33
    {0x14, 0x03},  //LED 34
    {0x1a, 0x06},  //LED 50
    {0x1b, 0x0c},  //LED 51
    {0x1c, 0x0f},  //LED 52
    {0x1d, 0x09},  //LED 53
    {0x1e, 0x03},  //LED 54
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-6 matrix register setting for IC1
static display_cmd_t pm17[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x01, 0x01},  //LED 00
    {0x02, 0x06},  //LED 01
    {0x03, 0x0c},  //LED 02
    {0x04, 0x0f},  //LED 03
    {0x05, 0x09},  //LED 04
    {0x0b, 0x01},  //LED 20
    {0x0c, 0x06},  //LED 21
    {0x0d, 0x0c},  //LED 22
    {0x0e, 0x0f},  //LED 23
    {0x0f, 0x09},  //LED 24
    {0x10, 0x03},  //LED 30
    {0x11, 0x09},  //LED 31
    {0x12, 0x0f},  //LED 32
    {0x13, 0x0c},  //LED 33
    {0x14, 0x06},  //LED 34
    {0x1a, 0x03},  //LED 50
    {0x1b, 0x09},  //LED 51
    {0x1c, 0x0f},  //LED 52
    {0x1d, 0x0c},  //LED 53
    {0x1e, 0x06},  //LED 54
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//
//
//Side A-7 matrix register setting for IC1
static display_cmd_t pm18[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x01, 0x00},  //LED 00
    {0x02, 0x03},  //LED 01
    {0x03, 0x09},  //LED 02
    {0x04, 0x0f},  //LED 03
    {0x05, 0x0c},  //LED 04
    {0x0b, 0x00},  //LED 20
    {0x0c, 0x03},  //LED 21
    {0x0d, 0x09},  //LED 22
    {0x0e, 0x0f},  //LED 23
    {0x0f, 0x0c},  //LED 24
    {0x10, 0x01},  //LED 30
    {0x11, 0x06},  //LED 31
    {0x12, 0x0d},  //LED 32
    {0x13, 0x0f},  //LED 33
    {0x14, 0x09},  //LED 34
    {0x1a, 0x01},  //LED 50
    {0x1b, 0x06},  //LED 51
    {0x1c, 0x0d},  //LED 52
    {0x1d, 0x0f},  //LED 53
    {0x1e, 0x09},  //LED 54
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-7 matrix register setting for IC1
static display_cmd_t pm19[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x01, 0x00},  //LED 00
    {0x02, 0x01},  //LED 01
    {0x03, 0x06},  //LED 02
    {0x04, 0x0c},  //LED 03
    {0x05, 0x0f},  //LED 04
    {0x0b, 0x00},  //LED 20
    {0x0c, 0x01},  //LED 21
    {0x0d, 0x06},  //LED 22
    {0x0e, 0x0c},  //LED 23
    {0x0f, 0x0f},  //LED 24
    {0x10, 0x00},  //LED 30
    {0x11, 0x03},  //LED 31
    {0x12, 0x09},  //LED 32
    {0x13, 0x0f},  //LED 33
    {0x14, 0x0c},  //LED 34
    {0x1a, 0x00},  //LED 50
    {0x1b, 0x03},  //LED 51
    {0x1c, 0x09},  //LED 52
    {0x1d, 0x0f},  //LED 53
    {0x1e, 0x0c},  //LED 54
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//
//
//Side A-8 matrix register setting for IC1
static display_cmd_t pm20[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x02, 0x00},  //LED 01
    {0x03, 0x03},  //LED 02
    {0x04, 0x09},  //LED 03
    {0x05, 0x0f},  //LED 04
    {0x0c, 0x00},  //LED 21
    {0x0d, 0x03},  //LED 22
    {0x0e, 0x09},  //LED 23
    {0x0f, 0x0f},  //LED 24
    {0x10, 0x00},  //LED 30
    {0x11, 0x01},  //LED 31
    {0x12, 0x06},  //LED 32
    {0x13, 0x0c},  //LED 33
    {0x14, 0x0f},  //LED 34
    {0x1a, 0x00},  //LED 50
    {0x1b, 0x01},  //LED 51
    {0x1c, 0x06},  //LED 52
    {0x1d, 0x0c},  //LED 53
    {0x1e, 0x0f},  //LED 54
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-8 matrix register setting for IC1
static display_cmd_t pm21[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x02, 0x00},  //LED 01
    {0x03, 0x01},  //LED 02
    {0x04, 0x06},  //LED 03
    {0x05, 0x0c},  //LED 04
    {0x0c, 0x00},  //LED 21
    {0x0d, 0x01},  //LED 22
    {0x0e, 0x06},  //LED 23
    {0x0f, 0x0c},  //LED 24
    {0x11, 0x00},  //LED 31
    {0x12, 0x03},  //LED 32
    {0x13, 0x09},  //LED 33
    {0x14, 0x0f},  //LED 34
    {0x1b, 0x00},  //LED 51
    {0x1c, 0x03},  //LED 52
    {0x1d, 0x09},  //LED 53
    {0x1e, 0x0f},  //LED 54
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//
//
//Side A-9 matrix register setting for IC1
static display_cmd_t pm22[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x03, 0x00},  //LED 02
    {0x04, 0x03},  //LED 03
    {0x05, 0x09},  //LED 04
    {0x0d, 0x00},  //LED 22
    {0x0e, 0x03},  //LED 23
    {0x0f, 0x09},  //LED 24
    {0x11, 0x00},  //LED 31
    {0x12, 0x01},  //LED 32
    {0x13, 0x06},  //LED 33
    {0x14, 0x0c},  //LED 34
    {0x1b, 0x00},  //LED 51
    {0x1c, 0x01},  //LED 52
    {0x1d, 0x06},  //LED 53
    {0x1e, 0x0c},  //LED 54
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-9 matrix register setting for IC1
static display_cmd_t pm23[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x03, 0x00},  //LED 02
    {0x04, 0x01},  //LED 03
    {0x05, 0x06},  //LED 04
    {0x0d, 0x00},  //LED 22
    {0x0e, 0x01},  //LED 23
    {0x0f, 0x06},  //LED 24
    {0x12, 0x00},  //LED 32
    {0x13, 0x03},  //LED 33
    {0x14, 0x09},  //LED 34
    {0x1c, 0x00},  //LED 52
    {0x1d, 0x03},  //LED 53
    {0x1e, 0x09},  //LED 54
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//
//
//Side A-10 matrix register setting for IC1
static display_cmd_t pm24[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x04, 0x00},  //LED 03
    {0x05, 0x03},  //LED 04
    {0x0e, 0x00},  //LED 23
    {0x0f, 0x03},  //LED 24
    {0x12, 0x00},  //LED 32
    {0x13, 0x01},  //LED 33
    {0x14, 0x06},  //LED 34
    {0x1c, 0x00},  //LED 52
    {0x1d, 0x01},  //LED 53
    {0x1e, 0x06},  //LED 54
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-10 matrix register setting for IC1
static display_cmd_t pm25[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x04, 0x00},  //LED 03
    {0x05, 0x01},  //LED 04
    {0x0e, 0x00},  //LED 23
    {0x0f, 0x01},  //LED 24
    {0x13, 0x00},  //LED 33
    {0x14, 0x03},  //LED 34
    {0x1d, 0x00},  //LED 53
    {0x1e, 0x03},  //LED 54
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//
//
//Side A-11 matrix register setting for IC1
static display_cmd_t pm26[] =
{
    {0x7f, 0x03},  //RMCG, OAB, IAB
    {0x05, 0x00},  //LED 04
    {0x0f, 0x00},  //LED 24
    {0x13, 0x00},  //LED 33
    {0x14, 0x01},  //LED 34
    {0x1d, 0x00},  //LED 53
    {0x1e, 0x01},  //LED 54
    {0x7f, 0x00},  //RMCG, OAB, IAB
};
//
//
//Side B-11 matrix register setting for IC1
static display_cmd_t pm27[] =
{
    {0x7f, 0x05},  //RMCG, OAB, IAB
    {0x05, 0x00},  //LED 04
    {0x0f, 0x00},  //LED 24
    {0x14, 0x00},  //LED 34
    {0x1e, 0x00},  //LED 54
    {0x7f, 0x02},  //RMCG, OAB, IAB
};
//

display_cmd_t* pmx[] =
{
    pm1, pm2, pm3, pm4, pm5, pm6, pm7, pm8, pm9, pm10, 
    pm11, pm12, pm13, pm14, pm15, pm16, pm17, pm18, pm19, pm20, 
    pm21, pm22, pm23, pm24, pm25, pm26, pm27, 
};

int pmsz[] = 
{
    SZO(pm1), SZO(pm2), SZO(pm3), SZO(pm4), SZO(pm5), 
    SZO(pm6), SZO(pm7), SZO(pm8), SZO(pm9), SZO(pm10), 
    SZO(pm11), SZO(pm12), SZO(pm13), SZO(pm14), SZO(pm15), 
    SZO(pm16), SZO(pm17), SZO(pm18), SZO(pm19), SZO(pm20), 
    SZO(pm21), SZO(pm22), SZO(pm23), SZO(pm24), SZO(pm25), 
    SZO(pm26), SZO(pm27), 
};

int i;
int k;
for (k = 0; k < 1; k++)
{
    for (i = 0; i < 27; i++)
    {
        wassert(ERR_OK == bu26507_run(pmx[i], pmsz[i]));
        _time_delay(100);
    }
}

    bu26507_standby();
#endif
    return ERR_OK;
}


#if 0 // unused??
// Convert LED and color masks to matrix data
static void bu26507_cfg_matrix( led_info_ptr_t pled_info, uint_8 * pmatrix );
static void bu26507_cfg_matrix( led_info_ptr_t pled_info, uint_8 * pmatrix )
{
    int led_num, color, index;
    boolean led_is_on, color_is_on;
    for (led_num=0;led_num<10;led_num++)
    {
        for (color=0;color<3;color++)
        {
            led_is_on = pled_info->led_mask & (1 << led_num) ? TRUE:FALSE;
            color_is_on = pled_info->color_mask & (1 << color) ? TRUE:FALSE;
            index = matrix_map[led_num] + color_map[color];
            *(pmatrix + index) = (led_is_on && color_is_on) ? pled_info->brightness : 0;
        }
    }
}
#endif

err_t bu26507_write(uint_8 reg, uint_8 data)
{
	display_cmd_t cmd;
	
	cmd.reg = reg;
	cmd.data = data;
	
	return bu26507_run(&cmd, 1);
}
///////////////////////////////////////////////////////////////////////////////
// Configure matrix data
///////////////////////////////////////////////////////////////////////////////
static void bu26507_xcfg_matrix(pmatrix_info_t matrix_led_info, uint8_t* pmatrix)
{
    int led_num, color, index;
    boolean led_is_on, color_is_on;
    uint8_t led_data = 0;   // data to be loaded into each led in the matrix
    int xled_data;
    int xcolor;
    
    // load the brightness and slope cycle info
    led_data = ((matrix_led_info->slope_cyc & 0x03) << 6);
    if (matrix_led_info->brightness <= 0x0f)    // > 0f signal individual settings for RGB
    {
        led_data |= (matrix_led_info->brightness & 0x0f);
    }
    
    for (led_num=0; led_num < 10; led_num++)
    {
        for (color = 0; color < 3; color++)
        {
            led_is_on = matrix_led_info->led_mask & (1 << led_num) ? TRUE:FALSE;
            color_is_on = matrix_led_info->color_mask & (1 << color) ? TRUE:FALSE;
            index = matrix_map[led_num] + color_map[color];
            xled_data = led_data;

            if (led_is_on && color_is_on)
            {
                xcolor = 1 << color;
                if (matrix_led_info->brightness > 0x0f)    // > 0f signal individual settings for RGB
                {
                    if (xcolor == M_GRN())
                    {
                        xled_data |= (matrix_led_info->g_level & 0x0f);
                    }
                    else if (xcolor == M_BLU())
                    {
                        xled_data |= (matrix_led_info->b_level & 0x0f);
                    }
                    else if (xcolor == M_RED())
                    {
                        xled_data |= (matrix_led_info->r_level & 0x0f);
                    }
                }
                *(pmatrix + index) = xled_data;
                
                if (matrix_led_info->slope_dly <= 3) // This should be 0 for scroll
                {
                    *(pmatrix + index) |= ((matrix_led_info->slope_dly & 0x03) << 4); // This should be 0 for scroll
                }
                else    // if slope_dly > 3, then do a progressive delay per led_num
                {
                    *(pmatrix + index) |= ((led_num & 0x03) << 4);  // adjust slope delay per led
                }
            }
            else
            {
                *(pmatrix + index) = 0;
            }
        }
    }
}   // bu26507_xcfg_matrix

enum runSequence_e  // Correlated to the order in runSequence array
{
    RS_CTRL,
    RS_SLP_SCL,
    RS_SCL,
    RS_START
};

#define SCLEN   0x01    // Scroll enable
#define SLPEN   0x02    // Slope enable

///////////////////////////////////////////////////////////////////////////////
// run matrix cycle
///////////////////////////////////////////////////////////////////////////////
err_t bu26507_run_matrix(matrix_setup_t* mxsp, boolean isScroll)
{
    err_t err = ERR_OK;
    matrix_info_t matrix_led_info;
    uint_8 matrix_data[30];

    // Initial setup script
    display_cmd_t init_run[] = 
    {
        { 0x7F, 0x00 }, // Select control register
        { 0x21, 0x04 }, // Select internal OSC for CLK
        { 0x01, 0x08 }, // Start OSC
        { 0x11, 0x3F }, // enable LED1-6
        { 0x20, 0x3F }, // full brightness
        { 0x7F, 0x01 }  // enable write to pattern A matrix
    };

    // Enable write to pattern B matrix
    display_cmd_t enableWrtB[] =
    {
        {0x7F, 0x05}  // write pattern B matrix
    }; 

    // Command script to run sequence
    // Slope or Scroll info will be set from input args
    display_cmd_t runSequence[] =
    {
        {0x7F, 0x00}, // Select control register
        {0x2D, 0x00}, // Slope and scroll control
        {0x2F, 0x00}, // Scroll speed & direction
        {0x30, 0x01}, // Start sequence
    };
    display_cmd_t* p_runSeq = runSequence;
    
    ///////////////////////////////////////////////
    //
    if (isScroll)
    {
        // Set scroll enable
        p_runSeq[RS_SLP_SCL].data = SCLEN;
        // Set scroll speed & direction
        p_runSeq[RS_SCL].data = ((mxsp->scroll_speed & 0x07) << 4) | (mxsp->scroll_direction & 0x0f);
    }
    else // is slope sequence
    {
        p_runSeq[RS_SLP_SCL].data = SLPEN;
    }
    
    matrix_led_info.led_mask    = mxsp->a_led_mask;
    matrix_led_info.color_mask  = mxsp->a_color_mask;
    matrix_led_info.brightness  = mxsp->brightness;
    matrix_led_info.g_level     = mxsp->g_level;
    matrix_led_info.b_level     = mxsp->b_level;
    matrix_led_info.r_level     = mxsp->r_level;
    matrix_led_info.slope_dly   = mxsp->slope_dly;
    matrix_led_info.slope_cyc   = mxsp->slope_cyc;

    // initial setup and enable write to matrix A
    wassert(ERR_OK == bu26507_run( init_run, sizeof(init_run)/sizeof(display_cmd_t)));
    
    // configure matrix A's data
    bu26507_xcfg_matrix(&matrix_led_info, matrix_data);
    wassert(ERR_OK == bu26507_set_matrix( matrix_data, sizeof(matrix_data) ));
    
    if (isScroll)
    {
        // enable write to matrix B
        wassert(ERR_OK == bu26507_run( enableWrtB, sizeof(enableWrtB)/sizeof(display_cmd_t)));
        
        // matrix B data
        matrix_led_info.led_mask    = mxsp->b_led_mask;
        matrix_led_info.color_mask  = mxsp->b_color_mask;
        
        // configure matrix B's data
        bu26507_xcfg_matrix(&matrix_led_info, matrix_data);
        wassert(ERR_OK == bu26507_set_matrix( matrix_data, sizeof(matrix_data) ));
    }
    
    // Start the sequence
    wassert(ERR_OK == bu26507_run(runSequence, sizeof(runSequence)/sizeof(display_cmd_t)));
    return ERR_OK;

} // bu26507_run_matrix
//////////////////////////////////////////////////////////////////////////


#if 0
 Turn on masked LEDs
err_t bu26507_run_select( led_info_ptr_t pled_info )
{
    err_t err = ERR_OK;
    display_cmd_t display_cmds1[] = 
    {
            { 0x7F, 0x00 }, // 1) 7FH 00000000 Select control register
            { 0x21, 0x04 }, // 2) 21H 00000000 Select internal OSC for CLK,  AND make SYNC pin = 'H' control LED's on/off.
            { 0x01, 0x08 }, // 3) 01H 00001000 Start OSC
            { 0x11, 0x3F }, // enable LED1-6
            { 0x7F, 0x01 } // write pattern A matrix data
    };
    uint_8 matrix_data[30];
    
    display_cmd_t display_cmds2[] =
    {
        { 0x7F, 0x00 }, // output A matrix data ?
        { 0x20, 0x1F }, // 50% brightness
        { 0x2D, 0x06 }, // 11) 2DH 00000110 Set SLOPE control enable
        { 0x30, 0x01 }, // 12) 30H 00000001 Start SLOPE sequence
    }; 
    int i;


    if ((err = bu26507_run( display_cmds1, sizeof(display_cmds1)/sizeof(display_cmd_t))) != ERR_OK)
    {
        return err;
    }
    
    bu26507_cfg_matrix( pled_info, matrix_data );

    if ((err = bu26507_set_matrix( matrix_data, sizeof(matrix_data) )) != ERR_OK)
    {
        return err;
    }
    
    return bu26507_run( display_cmds2, sizeof(display_cmds2)/sizeof(display_cmd_t));
}
#endif

err_t bu26507_run_pattern(bu26507_pattern_t pattern)
{
    err_t err = ERR_OK;
    
    switch(pattern)
    {
    case BU26507_PATTERN_OFF:
        err = bu26507_run_pattern_off();
        break;
    case BU26507_PATTERN1:
        err = bu26507_run_pattern1();
        break;
    case BU26507_PATTERN2:
        err = bu26507_run_pattern2();
        break;
    case BU26507_PATTERN3:
        err = bu26507_run_pattern3();
        break;
    case BU26507_PATTERN18:
        err = bu26507_run_pattern18();
        break;
    case BU26507_PATTERN_FAIL:
    case BU26507_PATTERN54:
        err = bu26507_run_pattern54();
        break;
    case BU26507_PATTERN55:
        err = bu26507_run_pattern55();
        break;
    case BU26507_PATTERN101:
        err = bu26507_run_pattern101();
        break;
    case BU26507_PATTERN102:
        err = bu26507_run_pattern102();
        break;
    case BU26507_PATTERN103:
        err = bu26507_run_pattern103();
        break;
    case BU26507_PATTERN118:
        err = bu26507_run_pattern118();
        break;
    case BU26507_PATTERN154:
        err = bu26507_run_pattern154();
        break;
    case BU26507_PATTERN155:
        err = bu26507_run_pattern155();
        break;
    case BU26507_PATTERN201:
        err = bu26507_run_pattern201();
        break;
    case BU26507_PATTERN202:
        err = bu26507_run_pattern202();
        break;
    case BU26507_PATTERN203:
        err = bu26507_run_pattern203();
        break;
    case BU26507_PATTERN218:
        err = bu26507_run_pattern218();
        break;
    case BU26507_PATTERN254:
        err = bu26507_run_pattern254();
        break;
    case BU26507_PATTERN255:
        err = bu26507_run_pattern255();
        break;
    case BU26507_PATTERN_MY3S:
        err = bu26507_run_pattern_MY3S();
        break;
    case BU26507_PATTERN_MY3P:
        err = bu26507_run_pattern_MY3P();
        break;
    case BU26507_PATTERN_MR1P:
        err = bu26507_run_pattern_MR1P();
        break;
    case BU26507_PATTERN_BT_PAIR:
        err = bu26507_run_pattern_BT_PAIR();
        break;
    case BU26507_PATTERN_CHGING:
        err = bu26507_run_pattern_CHGING();
        break;
    case BU26507_PATTERN_CHGD:
        err = bu26507_run_pattern_CHGD();
        break;
    case BU26507_PATTERN_MW3S:
        err = bu26507_run_pattern_MW3S();
        break;
    case BU26507_PATTERN_MA3S:
        err = bu26507_run_pattern_MA3S();
        break;
    case BU26507_PATTERN_BA3S:
            err = bu26507_run_pattern_BA3S();
            break;
    case BU26507_PATTERN_MW2S:
        err = bu26507_run_pattern_MW2S();
        break;
    case BU26507_PATTERN_RGB:
        err = bu26507_run_pattern_rgb();
        break;
//    case BU26507_PATTERN_PWR_DRAIN_TEST:
//        err = bu26507_run_pattern_pwr_drain_test();
//        break;
    default:
    	wassert(0);
    }
    
    return err;
}

/*
 * 
 *   Init the I2C1 driver and attributes for driving BU26507.
 *   
 *   IMPORTANT:  After calling this, the driver should do its thing quickly,
 *               then call I2C1_liberate() as quickly as possible.  Otherwise,
 *               the MFI will be starved, as it shares the I2C1 bus with BU26507.              
 */
err_t bu26507_init(void)
{
    uint_32      param;
    
    if( NULL == bu26507_fd)
    {
        I2C1_dibbs( &bu26507_fd );
    }
    else
    {
        return ERR_OK;
    }

    wassert(I2C_OK == ioctl(bu26507_fd, IO_IOCTL_I2C_SET_MASTER_MODE, NULL));

    param = BU26507_0_SLAVE_ADDR;
    wassert(I2C_OK == ioctl(bu26507_fd, IO_IOCTL_I2C_SET_DESTINATION_ADDRESS, &param));

    param = 100000;
    wassert(I2C_OK == ioctl(bu26507_fd, IO_IOCTL_I2C_SET_BAUD, &param));

    return ERR_OK;
}

err_t bu26507_standby(void)
{
    display_cmd_t display_cmds[] = 
    {
#if 0
        { 0x7F, 0x00 }, // sel. control reg
        { 0x11, 0}, // all leds off (already done by OFF?)
        //{ 0x12, 0}, // we never set this, so don't need to run this command.
        //{ 0x13, 0}, // we never set this, so don't need to run this command.
        { 0x20, 0}, // PWRM at min. duty cycle
        { 0x21, 0}, // set SYNC/clocks to off
        { 0x2d, 0}, // slope operation and PWM off
        { 0x2f, 0}, // scroll operation off
        { 0x30, 0}, // matrix lighting off
        {BU_OSC_CTRL, 0}, // turn off OSCEN.  This MUST happen last.
        //{ 0x17, 0} // https://whistle.atlassian.net/browse/COR-391
#endif
        { 0x7F, 0x00},  // sel. control reg
        { 0x11, 0},     // all leds off
        { 0x2d, 0},     // slope operation and PWM off
        { 0x2f, 0},     // scroll operation off
        { 0x30, 0},     // matrix lighting off
        { 0x2E, 0x01},  // reset scroll
        { 0x00, 0x01},  // reset Rohm chip
    };
    
    return bu26507_run(display_cmds, sizeof(display_cmds)/sizeof(display_cmd_t));
}


