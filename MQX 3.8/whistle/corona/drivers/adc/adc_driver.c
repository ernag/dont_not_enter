/*
 * adc_driver.c
 *
 *  Created on: Apr 10, 2013
 *      Author: Ben McCrea
 */

#include <mqx.h>
#include <bsp.h>
#include "wmutex.h"
#include "MK60D10.h"
#include "app_errors.h"
#include "adc_driver.h"
#include "adc_kadc.h"
#include "cfg_factory.h"
#include "cfg_mgr.h"
#include "pwr_mgr.h"

#define ADC_BASE_VBAT	     0
#define ADC_BASE_VBAT_TEMP   0
#define ADC_BASE_UID	     1

#define UID_EN_MASK  (0x1000)

// We are expecting significantly greater counts than this, to represent
//    a valid battery temperature.  Otherwise, consider it noise.
#define ADC_BATT_TEMP_NOISE_THRESH  (100)


// -- Global Private Data
static  void (* ADC0_callback) ()= {0};
static MUTEX_STRUCT g_adc_mutex;
static LWGPIO_STRUCT  g_uid_en_gpio;

// -- Local Prototypes --

static void _ADC_lock(void)
{
	wmutex_lock( &g_adc_mutex );
}

static void _ADC_unlock(void)
{
	wmutex_unlock( &g_adc_mutex );
}

static void _ADC_lock_init(void)
{
    wmutex_init( &g_adc_mutex );
}

void ADC0_IRQHandler( void * param )
{

    if ( ADC0_callback )
    {
        (*ADC0_callback)();
    }
    return;
}

uint_32 adc_sample_battery( void )
{
    ADC_MemMapPtr adc_ptr = _bsp_get_adc_base_address( ADC_BASE_VBAT );
    uint_32 val = 0;
    
    _ADC_lock();
    
    // Sample the VBAT channel
    adc_setchan( ADC_BASE_VBAT, 0, ADC_VBAT_CHANNEL, FALSE );
    
    // Wait for sample to complete
    while( !(adc_ptr->SC1[0] & ADC_SC1_COCO_MASK) ) {}
    
    // Return the sampled value
    val = adc_ptr->R[0];
    
    _ADC_unlock();
    return val;
}

/*
 *   Given counts from ADC1 (UID pin), return the type
 *     of cable we think this is.
 */
cable_t adc_get_cable(const uint32_t uid_counts)
{
    
//#define FORCE_TESTING_OF_BURNIN
#ifdef FORCE_TESTING_OF_BURNIN

    return CABLE_BURNIN;

#else
    
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
	
#endif
}

/*
 *   Sample MCU temperature using the ADC.  
 *   For Transfer Function, see the K60_Temperature.xlsx attachment at:
 *      https://whistle.atlassian.net/wiki/display/COR/Corona+Hardware+Specification#CoronaHardwareSpecification-ADC
 */
uint_32 adc_sample_mcu_temp( void )
{
    ADC_MemMapPtr adc_ptr = _bsp_get_adc_base_address( ADC_BASE_VBAT );
    uint_32 val = 0;
    
    _ADC_lock();
    
    // Sample the VBAT channel
    adc_setchan( ADC_BASE_VBAT, 0, ADC_MCU_TEMP_CHANNEL, FALSE );
    
    // Wait for sample to complete
    while( !(adc_ptr->SC1[0] & ADC_SC1_COCO_MASK) ) {}
    
    // Return the sampled value
    val = adc_ptr->R[0];
    
    _ADC_unlock();
    return val;
}

/*
 *   FW-976 Sample Batt temperature using the ADC.  
 *   For Transfer Function, see the Thermistor.xlsx attachment at:
 *      https://whistle.atlassian.net/wiki/display/COR/Corona+Hardware+Specification#CoronaHardwareSpecification-ADC
 *      
 *   Return 0 if:
 *     (A) your hardware doesn't support Batt Temperature, 
 *     (B) USB is not connected so the thermistor is therefore not enabled.
 *     (C) temperature measured is "in the noise".
 *     
 *   Otherwise return counts corresponding to a temperature as modeled on the HW Wiki.
 */
uint_32 adc_sample_batt_temp( void )
{
    ADC_MemMapPtr adc_ptr;
    uint_32 val = 0;
    
    /*
     *   Certain boards can't support reading battery temperature.
     */
    switch( g_st_cfg.fac_st.board )
    {
        case FCFG_BOARD_P3_DEV:
        case FCFG_BOARD_MLB_REV_3:
        case FCFG_BOARD_MLB_REV_4:
        case FCFG_BOARD_INVALID:
            return 0;
            
        default:
            break;
    }
    
    /*
     *   If PGOOD_B is asserted, the thermistor is not enabled, so return
     *     0 in that case.
     */
    if( !PWRMGR_usb_is_inserted() )
    {
        return 0;
    }
    
    adc_ptr = _bsp_get_adc_base_address( ADC_BASE_VBAT_TEMP );
    
    _ADC_lock();
    
    // Sample the VBAT channel
    adc_setchan( ADC_BASE_VBAT_TEMP, 0, ADC_VBAT_TEMP_CHANNEL, FALSE );
    
    // Wait for sample to complete
    while( !(adc_ptr->SC1[0] & ADC_SC1_COCO_MASK) ) {}
    
    // Return the sampled value
    val = adc_ptr->R[0];
    
    _ADC_unlock();
    
    if( val < ADC_BATT_TEMP_NOISE_THRESH )
    {
        /*
         *   Not a reasonable temperature.  Assume that the thermistor
         *     is either not installed or not enabled.
         */
        return 0;
    }
    
    return val;
}

/*
 *   Sample UID pin (used for detecting cable type on Whistle) using the ADC.
 */
uint_32 adc_sample_uid( void )
{
    ADC_MemMapPtr adc_ptr = _bsp_get_adc_base_address( ADC_BASE_UID );
    uint_32 val = 0;
    
    _ADC_lock();
    
    /*
     *   Certain boards don't care about the UID_EN, while others do.
     */
    switch( g_st_cfg.fac_st.board )
    {
        case FCFG_BOARD_P3_DEV:
        case FCFG_BOARD_MLB_REV_3:
        case FCFG_BOARD_MLB_REV_4:
        case FCFG_BOARD_INVALID:
            break;
            
        default:
			GPIOA_PDDR |= GPIO_PDDR_PDD(UID_EN_MASK);
            PORTA_PCR12 |= PORT_PCR_MUX(1);  // enable the GPIO and drive it high.
   		    GPIOA_PDOR |= UID_EN_MASK;
			
            lwgpio_set_value(&g_uid_en_gpio, LWGPIO_VALUE_HIGH);
            _time_delay(2);
            break;
    }
    
    // Sample the VBAT channel
    adc_setchan( ADC_BASE_UID, 0, ADC_UID_CHANNEL, FALSE );
    
    // Wait for sample to complete
    while( !(adc_ptr->SC1[0] & ADC_SC1_COCO_MASK) ) {}
    
    // Return the sampled value
    val = adc_ptr->R[0];
    
    GPIOA_PDDR &= ~GPIO_PDDR_PDD(UID_EN_MASK);  // make it an input.
    GPIOA_PDOR &= ~UID_EN_MASK;
    PORTA_PCR12 &= ~PORT_PCR_MUX_MASK;    // float the pin by default.
    
    _ADC_unlock();
    
    return val;
}

void adc_setchan( uint_8 base, uint_8 index, uint_8 chan_num, boolean diff)
{
    ADC_MemMapPtr adc_ptr = _bsp_get_adc_base_address( base );
    
    uint_32 regval;
    regval = adc_ptr->SC1[index];
    regval &= ~ADC_SC1_ADCH_MASK;
    regval |= (chan_num & 0x1F) << ADC_SC1_ADCH_SHIFT;
    if (diff)
    {
        regval |= ADC_SC1_DIFF_MASK; // Configure Differential Conversions
    }
    else
    {
        regval &= ~ADC_SC1_DIFF_MASK; // Configure Single-ended Conversions
    }

    // NOTE: for S/W trigger mode, writing SC1 triggers the ADC
    adc_ptr->SC1[index] = regval;
    
    while(adc_ptr->SC2 & ADC_SC2_ADACT_MASK) {}

    // note: if chan = 0x1f the mux is turned off. we could also mask off
    // the ADC0 peripheral clock in this case.
}

#if 0
void adc_enable_compare( int_32 compare_threshold, void (*callback)() )
{
    ADC_MemMapPtr adc_ptr = _bsp_get_adc_base_address( 0 );
    uint_32 regval;

    // Enable Continuous Conversions
    adc_ptr->SC3 |= ADC_SC3_ADCO_MASK; 

    // Enable LT compare and set the threshold
    adc_ptr->SC2 &= ~ADC_SC2_ACFGT_MASK; // use LT compare
    adc_ptr->CV1 = compare_threshold;
    adc_ptr->SC2 |= ADC_SC2_ACFE_MASK; // enable compare function

    adc_enable_interrupt( 0, callback );

    // configure to sample on 'chan_num': this triggers the ADC to start sampling
    adc_setchan( ADC_BASE_VBAT, 0, ADC_VBAT_CHANNEL, FALSE );
}
#endif

#if 0
void adc_disable_compare( void )
{
    ADC_MemMapPtr adc_ptr = _bsp_get_adc_base_address( 0 );
    
    adc_ptr->SC3 |= ADC_SC3_ADCO_MASK; // Disable Continuous Conversions
    adc_ptr->SC2 &= ~ADC_SC2_ACFE_MASK; // Disable Compare
    adc_disable_interrupt( 0 ); // Disable interrupt
}
#endif

void adc_enable_interrupt( uint_8 index, void (*callback)() )
{  
    ADC_MemMapPtr adc_ptr = _bsp_get_adc_base_address( 0 );

    if ( callback )
    {
        ADC0_callback = callback; // assign the callback pointer
    }
    
    adc_ptr->SC1[index] |= ADC_SC1_AIEN_MASK; // Enable ADC0 Interrupt Requests
    
  
    // Install ADC0 interrupt vector
    _int_install_isr(INT_ADC0, ADC0_IRQHandler, (void *) adc_ptr);
    
    // Enable interrupt in NVIC 
    _bsp_int_init( INT_ADC0 , BSP_ADC0_VECTOR_PRIORITY, 0, TRUE);    
}

void adc_disable_interrupt( uint_8 index )
{
    ADC_MemMapPtr adc_ptr = _bsp_get_adc_base_address( 0 );  
    
    // Disable interrupt in NVIC 
    _bsp_int_init( INT_ADC0 , BSP_ADC0_VECTOR_PRIORITY, 0, FALSE);   
    
    adc_ptr->SC1[index] &= ~ADC_SC1_AIEN_MASK; // Disable ADC0 Interrupt Requests
}

static void adc_init_adc_addr(uint8_t base)
{	
	ADC_MemMapPtr adc_ptr = _bsp_get_adc_base_address( base );
	
    // Initialize ADC0 with Default Settings
    adc_ptr->SC1[0] = 0x0000001F;
    adc_ptr->SC1[1] = 0x0000001F;
    adc_ptr->CFG1 = 0x000000FF;
    adc_ptr->CFG2 = 0x00000000;
    adc_ptr->SC2  = 0x00000000;
    adc_ptr->SC3  = 0x00000007;
    adc_ptr->PGA  = 0x00000000;
}

void adc_default_init( void )
{
    lwgpio_init(&g_uid_en_gpio, BSP_GPIO_UID_ENABLE_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE);
    lwgpio_set_functionality(&g_uid_en_gpio, BSP_GPIO_UID_ENABLE_MUX_GPIO);
    PORTA_PCR12 |= PORT_PCR_DSE_MASK;  // high drive strength required on UID_EN
    GPIOA_PDDR &= ~GPIO_PDDR_PDD(UID_EN_MASK);  // make it an input.
    GPIOA_PDOR &= ~UID_EN_MASK;
    PORTA_PCR12 &= ~PORT_PCR_MUX_MASK;
    
    // Default Settings:
    //  ADCx_SC1=0x0000001F
    //   AIEN=0 (Interrupts Disabled)
    //   DIFF=0 (Single-ended Mode)
    //   ADCH=0x1F (Disabled)
    //  ADCx_CFG1=0X000000FC
    //   ADLPC=1 (Low-power Configuration)
    //   ADIV=3 (8x Clock Divider)
    //   ADLSMP=1 (Long Sample Time)
    //   MODE=3 (16-bit Resolution)
    //   ADICLK=3 (Async Clock)
    //  ADCx_CFG2=0x00000000
    //   MUXSEL=1 (ADxxb Channels Selected)
    //   ADACKEN=0 (Asynch. Clock Output Enabled)
    //   ADHSC=0 (Normal conversion sequence)
    //   ADLSTS=0 (Long Sample Time = 24 clocks)
    //  ADCx_SC2=0x00000001
    //   ADTRG=0 (Software Trigger)
    //   ACFE=0 (Compare Function Disabled)
    //   ACFGT=0
    //   ACREN=0 (Compare Range Function Disabled)
    //   DMAEN=0 (DMA Disabled)
    //   REFSEL=0 (External Reference)
    //  ADCx_SC3=0x00000000
    //   CAL=0
    //   ADCO=0 (Contin. Conv. Disabled)
    //   AVGE=0 (Averaging Enabled)
    //   AVGS=0 (32 Sample Averaging Depth)
    //  ADCx_PGA=0x00000000
    //   PGAEN=0 (PGA Disabled)
    //   PGAG=0 (1x PGA Gain)
    //  ADC0 calibration registers restored from flash calibration data

    // Unmask ADC0 and internal VREF peripheral clocks
	wint_disable(); // SIM_* can be accessed in other drivers not protected by our lock
    SIM_SCGC6 |= SIM_SCGC6_ADC0_MASK;
    SIM_SCGC3 |= SIM_SCGC3_ADC1_MASK;
    SIM_SCGC4 |= SIM_SCGC4_VREF_MASK;  // ERNIE:  We do NOT need VREF for Whistle, cancel it after disabling VREF module.
    wint_enable();

    adc_init_adc_addr(ADC_BASE_VBAT);
    adc_init_adc_addr(ADC_BASE_UID);

    /*
     *   Disable the VREF block altogether, and clock gate it.
     */
    VREF_SC = 0;

    // Enable the Bandgap Ref. and allow use in VLPx modes
    // PMC_REGSC |= (PMC_REGSC_BGEN_MASK | PMC_REGSC_BGBE_MASK); 
     
    // Disable internal VREF
    //VREF_SC &= ~VREF_SC_VREFEN_MASK; 
     
    // Gate the ADC0 and VREF peripheral clocks
    //SIM_SCGC6 &= ~SIM_SCGC6_ADC0_MASK;
	wint_disable(); // SIM_* can be accessed in other drivers not protected by our lock
    SIM_SCGC4 &= ~SIM_SCGC4_VREF_MASK; // ERNIE:  We do NOT need VREF for Whistle.
	wint_enable(); // SIM_* can be accessed in other drivers not protected by our lock
    
    _ADC_lock_init();
}

void adc_disable( void )
{
    ADC_MemMapPtr adc_ptr = _bsp_get_adc_base_address( 0 );  
    volatile uint_16 tmpval;
    adc_ptr->SC1[0]  = 0x0000001F; // Disable mux inputs
    adc_ptr->SC1[1]  = 0x0000001F;        
    adc_ptr->PGA &=  ~ADC_PGA_PGAEN_MASK; // ADC0 PGA Disabled
    tmpval = adc_ptr->R[0]; // read channel 0 (to clear COCO?)

    // Mask the peripheral clocks
    // TBD: determine if this is a good place to turn off the clock
    //_int_disable(); // SIM_* can be accessed in other drivers not protected by our lock
    //SIM_SCGC6 &= ~SIM_SCGC6_ADC0_MASK;
    //_int_enable();
}


/*
 *   Returns the "natural" in units we care about, for given channel IDX and raw value.
 */
uint_16 adc_getNatural(uint32_t channelIdx, uint16_t raw)
{
    uint32_t slope = 0;
    uint32_t intercept = 0;
    uint32_t divider = 1;
    
    switch(channelIdx)
    {
        case ADC0_Vsense_Corona_VBat: // returns millivolts
            // <volts> = m*<counts> + b  ||   m = .0001032, b=-0.0398
            slope = 1032;     
            intercept = 40; 
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

