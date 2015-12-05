/*
 * main implementation: use this 'C' sample to create your own application
 *
 */

#include "derivative.h" /* include peripheral declarations */
#include "Cpu.h"
#include "Events.h"
#include "FLASH1.h"
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "boot2_mgr.h"
#include "corona_console.h"
#include "corona_gpio.h"
#include "USB_OTG1.h"
#include "led_dbg.h"
#include "bootflags.h"
#include "adc_sense.h"
#include "pmem.h"
#include "bu26507_display.h"
#include <stdio.h>

#define NUM_MS_WAIT_BUTTON_PRESS    250

extern void main_k60_usb(void);
extern void set_success(void);  // Set our success flag if needed
extern void init_fwu_spi_data();
extern void ck_boot2_update();  // Check if sibling boot2 needs an update
extern void ck_boot1_update();  // Check if sibling boot2 needs an update
extern void ck_app_update();    // Check if the app needs an update
extern void ck_patch_update(boolean isBase);  // Check if the patch needs an update
extern void goto_app();         // Jump to App

extern LDD_TDeviceData* MyFLASH_Ptr;
extern cc_handle_t      g_cc_handle;

typedef enum boot_menu_result_type
{
    
    BOOTLDR_STAY_IN_BOOT      = 1,
    BOOTLDR_JUMP_TO_APP       = 2,
    BOOTLDR_RST_FACT_DEFAULTS = 3
    
}boot_menu_result_t;


#ifndef EVT_HW
    #error "EVT_HW Must be defined!  Did Cpu.h get overwritten by PE?"
#endif

uint32_t mask(uint32_t msb, uint32_t lsb);

uint8_t b2h[];
int error_abort = 0;
int usbinit     = 0;

void isr_Button__DEPCRECATED(void)
{
    return;
}

/*
 *   Return TRUE if button is down right now.
 *   Return FALSE if button is not pressed right now.
 */
static uint8_t button_down(void)
{
    uint32_t button = GPIO_READ(GPIOE_PDIR, 1);

    if(0 == button)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*
 *   Return 0 if there is no button event.
 *   Return non-zero otherwise.
 */
uint32_t button_was_pressed(void)
{
    uint32_t button_counter = NUM_MS_WAIT_BUTTON_PRESS;
    uint32_t button;

    /*
     *   COR-381  Check for button when booting.  Stay in bootloader if its there.
     */

    // Debounce first with a lil' delay.
    sleep(75);

#ifdef WAIT_FOR_BUTTON

    while( 0 != button_counter )
    {
        button = GPIO_READ(GPIOE_PDIR, 1);

        if( 0 == button )
        {
            /*
             *   Button was pressed, so stay in the bootloader.
             */
            return 1;
        }
        sleep(1);
        button_counter--;
    }

    return 0;

#else
    // With this approach, we simply check to see if the button is pressed when we boot.
    return (uint32_t)button_down();
#endif
}

/*
 *   The choice has been made, enumerate USB if you dare!
 */
static void usb_enumerate(void)
{
    USB_OTG1_Init();
    main_k60_usb();
    b2_recv_init();
    usbinit++;
    while(TRUE)
    {
        Watchdog_Reset();
        usb_task();
        b2_task();
    }
}

/*
 *   Let the user know they have arrived in the bootloader.
 */
static void bootloader_user_indication(void)
{
    bu26507_display_pattern( BU_PATTERN_SOLID_PURPLE );
    led_dbg(LED_P2);  // "1101" pattern - indicate we are staying in the boot.
}

#define BOOTLDR_MENU_TIMEOUT        5000
#define BOOTLDR_USR_TIMEOUT         3500
#define BOOTLDR_INITIAL_PRESS_HOLD  2500  // user holds this long before allowing fact reset or enumeration.
#define WAIT_FOR_DOUBLE_CLK          800
#define DEBOUNCE_DELAY                10

/*
 *   Bootloader menu function.  Determine what the user wants.
 *   
 *   Scenarios:
 *   
 *   (A)  User does nothing for BOOTLDR_USR_TIMEOUT milliseconds.
 *        Result = jump to app.
 *        
 *   (B)  User single clicks before the timeout.
 *        Result = usb enumeration.
 *        
 *   (C)  User double clicks before the timeout.
 *        Result = factory reset, then jump to app.
 */
static boot_menu_result_t bootloader_menu(void)
{
    uint32_t num_ms_surpassed = 0;
    uint32_t num_ms_button_start = 0;
    uint8_t button_was_pressed = 0;
    uint8_t toggle = 0;
    
    bu26507_display_pattern( BU_PATTERN_BOOTLDR_MENU );
    
    // wait for user to release the button.
    while( button_down() )
    {
        sleep(1);
    }
    
    /*
     *   At this point the user must hold the button down for BOOTLDR_INITIAL_PRESS_HOLD
     */
    while( (num_ms_surpassed < BOOTLDR_USR_TIMEOUT ) || button_was_pressed )
    {
        if( ! button_was_pressed )
        {
            // If we've never detected a button before, see if we get the first one.
            button_was_pressed = button_down();
            if( button_was_pressed )
            {
                bu26507_display_pattern( BU_PATTERN_BOOTLDR_INITIAL_PRESS );
                num_ms_button_start = num_ms_surpassed;
            }
        }
        else if( (num_ms_surpassed - num_ms_button_start) > DEBOUNCE_DELAY)
        {
            // since we detected it once, there needs to be a press and hold for BOOTLDR_INITIAL_PRESS_HOLD ms.
            if( ! button_down() )
            {
                // user let go before BOOTLDR_INITIAL_PRESS_HOLD.  Jump to app.
                goto bootloader_menu_gto;
            }
            
            // user is still holding, see how far along they are.
            if( (num_ms_surpassed - num_ms_button_start) >= BOOTLDR_INITIAL_PRESS_HOLD)
            {
                goto bootloader_menu_wait_for_click;
            }
            
            /*
             *   Flicker yellow every 30ms until the user releases the button.
             */
            if( 0 == ((num_ms_surpassed - num_ms_button_start) % 30) )
            {
                if(toggle++ % 2)
                {
                    bu26507_color(BU_COLOR_YELLOW);
                }
                else
                {
                    bu26507_color(BU_COLOR_OFF);
                }
            }
        }

        sleep(1);  // delaying 1 millisecond in the main loop here.
        num_ms_surpassed++;
    }
    
    goto bootloader_menu_gto;  // timeout occured before a button press.
    
    
bootloader_menu_wait_for_click:

    /*
     *   Once we are here, we want to wait on either a single click or double click.
     *     If neither happen, we'll just jump to the app.
     */
    bu26507_display_pattern( BU_PATTERN_BOOTLDR_WAIT_FOR_CLICK );
    while(button_down())
    {
        sleep(1); // wait for user to release.
    }
    
    /*
     *   At this point, the user has to either single or double click within N seconds.
     */
    num_ms_surpassed = 0;
    num_ms_button_start = 0;
    button_was_pressed = 0;
    
    while(num_ms_surpassed < BOOTLDR_MENU_TIMEOUT )   // we got them to the menu, lil' extra time to wait for a button instruction.
    {
        if( ! button_was_pressed )
        {
            // If we've never detected a button before, see if we get the first one.
            button_was_pressed = button_down();
            if( button_was_pressed )
            {
                // wait for upclick.
                while( button_down() )
                {
                    sleep(1);
                }
                
                // debounce after upclick.
                num_ms_surpassed = 0;
                while( num_ms_surpassed++ < DEBOUNCE_DELAY)
                {
                    sleep(1);
                }
                
                // wait for WAIT_FOR_DOUBLE_CLK ms for a double click, otherwise assume single click.
                num_ms_surpassed = 0;
                while( num_ms_surpassed++ < WAIT_FOR_DOUBLE_CLK)
                {
                    if( button_down() )
                    {
                        // double click detected.
                        return BOOTLDR_RST_FACT_DEFAULTS;
                    }
                    sleep(1);
                }
                
                // single click detected.
                return BOOTLDR_STAY_IN_BOOT;
            }
        }
        
        sleep(1);               // delaying 1 millisecond in the main loop here.
        num_ms_surpassed++;
    }
    
    goto bootloader_menu_gto;  // timeout occured before a button event.
    
bootloader_menu_gto:
    return BOOTLDR_JUMP_TO_APP;
}

/*
 *   Where all the fun bootloader logic lives.
 */
int boot2_main( cable_t cable )
{
    uint32_t pgood = 0;

    // Default "1001" pattern.
    led_dbg(LED_P6);

    cc_init(&g_cc_handle, NULL, CC_TYPE_USB);

	MyFLASH_Ptr = FLASH1_Init(NULL);  /* Initialization of FLASH1 component */
	set_success();          // Set our success flag if needed
	init_fwu_spi_data();    // Get the FWU update flags
    ck_boot2_update();      // Check if sibling boot2 needs an update
	ck_app_update();        // Check if the app needs an update
    ck_boot1_update();      // Check if boot1 needs an update
    
    /*
     *   Check for errors and stick in boot2 if we have issues.
     */
    if ( error_abort )
    {
        bu26507_display_pattern( BU_PATTERN_ABORT );
        goto usb_enumeration;
    }
    
//#define TEST_BOOTLOADER_MENU  // unconditionally goes to the bootloader menu for testing purposes.
#ifdef TEST_BOOTLOADER_MENU
    goto test_bootldr_menu;
#endif
    
    /*
     *   Make sure USB is connected if we want to stay in the boot.
     *     Otherwise why bother?
     */
    pgood = GPIO_READ(GPIOE_PDIR, 2);
    if( pgood )
    {
        goto gto;
    }
    
    /*
     *   Factory cable means we are staying in boot2.
     */
    if( CABLE_FACTORY_DBG == cable )
    {
        goto usb_enumeration;
    }
    
    /*
     *   If we are waking from ship mode, we do not stay in the boot.
     */
    if ( 0 == (RFVBAT_REG(1) & BF_WAKE_FROM_SHIP_MODE) )
    {
    }
    else
    {
        goto gto;
    }
    
    /*
     *   If we'd made it this far, we can see if we've caused a trigger for staying in boot2.
     *   One way is a (button press + USB + checksum in pmem bad).
     */
#ifdef TEST_BOOTLOADER_MENU
    test_bootldr_menu:
#else
    if( button_was_pressed() )
#endif
    {
        boot_menu_result_t result;
        
//#define BUTTON_PRESS_STAYS_IN_BOOT  // Defining this gives you "the old style MELLOW" bootloader.
#ifdef  BUTTON_PRESS_STAYS_IN_BOOT
        goto usb_enumeration;
#endif
        
        /*
         *   Time to start the "bootloader menu" for the user, which determines 
         *      what the user wants to do.
         */
        result = bootloader_menu();
        
        switch(result)
        {
            case BOOTLDR_STAY_IN_BOOT:
                bu26507_display_pattern( BU_PATTERN_BOOTLDR_STAY_IN_BOOT );
                goto usb_enumeration;
                
            case BOOTLDR_RST_FACT_DEFAULTS:
                bu26507_display_pattern( BU_PATTERN_BOOTLDR_RESET_FACTORY_DEFAULTS );
                factoryreset(0);
                goto gto;
                
            case BOOTLDR_JUMP_TO_APP:
            default:
                bu26507_display_pattern( BU_PATTERN_BOOTLDR_JUMP_TO_APP );
                goto gto;
        }
    }
    
    /*
     *   If we got through the opstacle course of ways to stay in boot2, jump to app!
     */
    goto gto;
    
usb_enumeration:
    bootloader_user_indication();
    usb_enumerate();
    
gto:
    /*
     *   Jump to the application.
     */
    bu26507_display_pattern( BU_PATTERN_OFF );
    led_dbg(LED_P13);  // indicate to the user that we are jumping to the application.
    goto_app();
    
    return (0);
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Provide a mask given bit range.
//!
//! \fntype Reentrant Function
//!
//! \detail Give a mask corresponding to msb:lsb.
//!
//! \return 32-bit mask corresponding to msb:lsb.
//!
///////////////////////////////////////////////////////////////////////////////
uint32_t mask(uint32_t msb, uint32_t lsb)
{
    uint32_t ret = 0;
    uint32_t numOnes = msb - lsb + 1;

    if(msb < lsb) {return 0;}

    while(numOnes-- > 0)
    {
        ret = 1 | (ret << 1);
    }
    return (ret << lsb);
}



// Boot2 Header
// Pragma to place the B2 header field on correct location defined in linker file.
#pragma define_section b2header ".b2header" far_abs R

__declspec(b2header) uint8_t b2h[0x20] = {
    // Name [0,1,2]
#if BOOT2_TYPE == 1
    'B','2','A',        // 0x42U, 0x32U, 0x41U,
    // Version [3,4,5]
    0x00, 0x00, 0x0b,
    // Checksum of Name and Version [6] (Computed manually)
    0x40U,
#elif BOOT2_TYPE == 2
    'B','2','B',        // 0x42U, 0x32U, 0x42U,
    // Version [3,4,5]
    0x00, 0x00, 0x0b,
    // Checksum of Name and Version [6] (Computed manually)
    0x3fU,
#else
#FAIL
#endif
    // Success, Update, 3 Attempt flags, BAD [7,8,9,A,B,C]
    0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
    // Img_savd [D], Used [E], Reserved [F]
    0xFFU, 0xFFU, 0xFFU,
    // Version tag [10-1F]
    '2','0','1','3','1','0','1','6', 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,0xFFU, 0xFFU, 0xFFU
};
