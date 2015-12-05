////////////////////////////////////////////////////////////////////////////////
//! \addtogroup boot1
//! \ingroup corona
//! @{
//
//! \file main.c
//! \brief boot1 main
//! \version version 0.1
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
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "derivative.h" /* include peripheral declarations */
#include "PE_LDD.h"
#include "bootflags.h"

uint8_t b1h[];

////////////////////////////////////////////////////////////////////////////////
// Boot1 Header
// Pragma to place the B1 header field on correct location defined in linker file.
#pragma define_section b1header ".b1header" far_abs R

__declspec(b1header) uint8_t b1h[0x20] = {
    // Name [0,1,2]
    'B','T','1',        // 0x42U, 0x32U, 0x41U,
    // Version [3,4,5]
    0x00, 0x00, 0x02,
    // Checksum of Name and Version [6] [not used]
    0x37U,
    // Success, Update, 3 Attempt flags, BAD [7,8,9,A,B,C]
    0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
    // Img_savd [D], Used [E], Reserved [F]
    0xFFU, 0xFFU, 0xFFU,
    // Version tag [10-1F]
    'J','T','A','G',0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU
};
////////////////////////////////////////////////////////////////////////////////



#define B2A          0x00002000
#define B2B          0x00012000

#define B2A_ENTRY    B2A + 1    // 0x00002004
#define B2B_ENTRY    B2B + 1    // 0x00012004

#define B2A_HDR      B2A + 0x200
#define B2B_HDR      B2B + 0x200

#define FLAG_SET     0x55
#define FLAG_CLR     0xFF

#define GOOD         0

typedef enum
{
    A, B, N
} boot2_t;
boot2_t theChosenOne = N;   // N = Neither, as yet...

typedef enum
{
    B2H_NAME        = 0,
    B2H_VER         = 3,
    B2H_CKSUM       = 6,
    B2H_SUCCESS     = 7,
    B2H_UPDATE      = 8,
    B2H_ATTMPT1     = 9,
    B2H_ATTMPT2     = 10,
    B2H_ATTMPT3     = 11,
    B2H_BAD         = 12,
    B2H_IMG_SAVD    = 13,
    B2H_USED        = 14,
    B2H_VER_TAG     = 16
} boot2hdr_idx_t;


uint8_t cksum(uint8_t *b2hdr);
void gotoBoot2A(void);
void gotoBoot2B(void);
void gotoBoot2(void);
boot2_t ck_versions(void);
int b2_bad(boot2_t b2s);
int b2_invalid(boot2_t b2s);
int b2_success(boot2_t b2s);
int b2_attempts(boot2_t b2s);
int b2_used(boot2_t b2s);
void set_attempt_flag(boot2_t b2s);
void set_flag(uint8_t *flagp);
void set_bad_flag(boot2_t b2s);
void set_used_flag(boot2_t b2s);
void b2_write_flag(uint8_t *pdata, uint32_t t_address, uint32_t d_size);

// flash variables
volatile int               OpFinishedFlg = FALSE;
LDD_TError                 f_result;
LDD_FLASH_TOperationStatus OpStatus;
LDD_TDeviceData            *MyFLASH_Ptr;

boot2_t b2A                              = A;
boot2_t b2B                              = B;

int (*boot2)(void);
int *b2Ap                                = (int *)B2A_ENTRY;
int *b2Bp                                = (int *)B2B_ENTRY;

uint8_t *hdrp;

uint8_t b2A_invalid;
uint8_t b2B_invalid;

uint8_t *boot2Ahdr                       = (uint8_t *)B2A_HDR;
uint8_t *boot2Bhdr                       = (uint8_t *)B2B_HDR;
uint8_t x;


#ifdef MY_BOOT1_DEBUG
// 0x320000 = 1 sec
#define MY_DEBUG_1SEC_DELAY     (0x320000)
#endif


// Primary purpose in life is to determine which boot2, A or B, to use
// First checks boot2A, then boot2B at each step
int boot1_main(void)
{
    int i;
#ifdef MY_BOOT1_DEBUG
    uint32_t my_debug_delay = 10*MY_DEBUG_1SEC_DELAY; // show LED pattern for a few secs
    // init debug led outputs
    SIM_SCGC5 |= SIM_SCGC5_PORTD_MASK;
    PORTD_PCR12 = PORT_PCR_MUX(0x01);
    PORTD_PCR13 = PORT_PCR_MUX(0x01);
    PORTD_PCR14 = PORT_PCR_MUX(0x01);
    PORTD_PCR15 = PORT_PCR_MUX(0x01); 
    GPIOD_PDDR |= (0xF << 12); // dir=out
    GPIOD_PDOR |= (0xF << 12); // all LEDs turned off 
#endif
    
    // Enable clock to the LLWU
    SIM_SCGC4 |= SIM_SCGC4_LLWU_MASK;
    
    // Disable LLWU wakeup sources 
    LLWU_PE1 = LLWU_PE2 = LLWU_PE3 = LLWU_PE4 = 0; 
    
    // Handle recovery from VLLSx (ship mode)
    if ( RCM_SRS0 & RCM_SRS0_WAKEUP_MASK )
    {  
        // Store the WAKEUP flag in the VBAT register file in case it gets
        // cleared by boot1 or boot2
        // TODO: use BF magic # to coordinate w/ app if necessary
        RFVBAT_REG(1) |= BF_WAKE_FROM_SHIP_MODE;
    
        // Clear the ACKISO bit to unlatch device pins     
        PMC_REGSC |= PMC_REGSC_ACKISO_MASK;
    }
    else
    {
        RFVBAT_REG(1) &= ~(BF_WAKE_FROM_SHIP_MODE);
    }
    
    // Clear LLWU wakeup flags  
    LLWU_F1 = LLWU_F2 = LLWU_F3 = 0xff;
    
#ifdef MY_BOOT1_DEBUG
    // set debug LED's to indicate the condition  
    if ( (RCM_SRS0 == 0x00) && (RCM_SRS1 == 0x00) )
    {
      GPIOD_PDOR &= ~(0xF << 12);  // turn on LEDs 
    }
    else if ( RCM_SRS0 & RCM_SRS0_WAKEUP_MASK )
    {
      GPIOD_PDOR &= ~(0x1 << 12);  // turn on LED1
    }
    else if ( RCM_SRS1 & RCM_SRS1_LOCKUP_MASK )
    {
      GPIOD_PDOR &= ~(0x2 << 12);  // turn on LED2
    }
    else if ( RCM_SRS0 != 0x00 )
    {
      GPIOD_PDOR &= ~(0x4 << 12);  // turn on LED3
    }
    else if ( RCM_SRS1 != 0x00 )
    {
      GPIOD_PDOR &= ~(0x8 << 12);  // turn on LED4
    }
    // Pause for a few secs to display the LED debug pattern
    while(my_debug_delay--)
        ;
#endif

    MyFLASH_Ptr = FLASH1_Init(NULL); /* Initialization of FLASH1 component */

    // If header is invalid, go to the other
    if (b2_invalid(b2A))
    {
        gotoBoot2B();
    }
    else if (b2_invalid(b2B))
    {
        gotoBoot2A();
    }

    // If a bad flag set, go to the other
    if (b2_bad(b2A))
    {
        gotoBoot2B();
    }
    else if (b2_bad(b2B))
    {
        gotoBoot2A();
    }

    // If the update flag is set, go to the other
    // Not sure boot1 will ever see these now that fwu_mgr uses spi flash
    if (b2_update(b2A))
    {
        gotoBoot2B();
    }
    else if (b2_update(b2B))
    {
        gotoBoot2A();
    }

    // Don't use versions
    //if ((theChosenOne = ck_versions()) != N)
    //{
    //    gotoBoot2();
    //}

    // If no success flag, set used flag of other if it has a success flag,
    // and go to this one
    if (!b2_success(b2A))
    {
        set_used_flag(B);
        gotoBoot2A();
    }
    else if (!b2_success(b2B))
    {
        set_used_flag(A);
        gotoBoot2B();
    }

    // Both have success flags set
    // If "used" flag is set, go to other
    if (b2_used(b2A))
    {
        gotoBoot2B();
    }
    else if (b2_used(b2B))
    {
        gotoBoot2A();
    }

    // All equal, go to boot2A (as will happen with initial load)
    gotoBoot2A();

    return 0;
}


void gotoBoot2A()
{
    theChosenOne = A;
    gotoBoot2();
}


void gotoBoot2B()
{
    theChosenOne = B;
    gotoBoot2();
}


void gotoBoot2()
{
    uint8_t *b2hp;

    if (theChosenOne == A)
    {
        boot2 = (int (*)()) * b2Ap;
        b2hp  = boot2Ahdr;
    }
    else
    {
        boot2 = (int (*)()) * b2Bp;
        b2hp  = boot2Bhdr;
    }

    // If success flag is set, jump to boot2
    if (b2_success(theChosenOne))
    {
        boot2(); // Jump to boot2
    }

    // Success not set, if attempts are less than 3, set attempt, jump to boot2
    if (b2_attempts(theChosenOne) < 3)
    {
        set_attempt_flag(theChosenOne);
        boot2(); // Jump to boot2
    }
    else    // attempts are >= 3, set bad flag, go to other boot2
    {
        set_bad_flag(theChosenOne);
        theChosenOne++;
        theChosenOne %= 2; // Select other boot2
        gotoBoot2();       // Try again...
    }
}


// Write flag data to target boot2 header
//
void b2_write_flag(uint8_t *pdata, uint32_t t_address, uint32_t d_size)
{
    OpFinishedFlg = FALSE;
    f_result      = FLASH1_Write(MyFLASH_Ptr, pdata, t_address, d_size); // Start writing to the target

    if (f_result != ERR_OK)
    {
        return;
    }
    while (!OpFinishedFlg)                                          /* Wait until the data are written */
    {
        FLASH1_Main(MyFLASH_Ptr);                                   /* Run the main method */
    }
}


int b2_invalid(boot2_t b2s)
{
    if (b2s == A)
    {
        return(cksum(boot2Ahdr));
    }
    else
    {
        return(cksum(boot2Bhdr));
    }
}


int b2_bad(boot2_t b2s)
{
    if (b2s == A)
    {
        return(boot2Ahdr[B2H_BAD] == FLAG_SET);
    }
    else
    {
        return(boot2Bhdr[B2H_BAD] == FLAG_SET);
    }
}


int b2_update(boot2_t b2s)
{
    if (b2s == A)
    {
        return(boot2Ahdr[B2H_UPDATE] == FLAG_SET);
    }
    else
    {
        return(boot2Bhdr[B2H_UPDATE] == FLAG_SET);
    }
}


int b2_success(boot2_t b2s)
{
    if (b2s == A)
    {
        return(boot2Ahdr[B2H_SUCCESS] == FLAG_SET);
    }
    else
    {
        return(boot2Bhdr[B2H_SUCCESS] == FLAG_SET);
    }
}

int b2_used(boot2_t b2s)
{
    if (b2s == A)
    {
        return(boot2Ahdr[B2H_USED] == FLAG_SET);
    }
    else
    {
        return(boot2Bhdr[B2H_USED] == FLAG_SET);
    }
}


// Count the number of attempt flags
int b2_attempts(boot2_t b2s)
{
    int     i;
    int     attempts = 0;
    uint8_t *b2hp;

    if (b2s == A)
    {
        b2hp = boot2Ahdr;
    }
    else
    {
        b2hp = boot2Bhdr;
    }

    for (i = B2H_ATTMPT1; i < B2H_ATTMPT1 + 3; i++)
    {
        if (b2hp[i] == FLAG_SET)
        {
            attempts++;
            continue;
        }
        break;
    }
    return attempts;
}


void set_attempt_flag(boot2_t b2s)
{
    int     i;
    int     attempts = 0;
    uint8_t *b2hp;

    if (b2s == A)
    {
        b2hp = boot2Ahdr;
    }
    else
    {
        b2hp = boot2Bhdr;
    }

    for (i = B2H_ATTMPT1; i < B2H_ATTMPT1 + 3; i++)
    {
        if (b2hp[i] == FLAG_SET)
        {
            continue;
        }
        else
        {
            set_flag(&b2hp[i]);
        }
        break;
    }
}

void set_used_flag(boot2_t b2s)
{
    if (b2s == A)
    {
        if (boot2Ahdr[B2H_USED] != FLAG_SET)
        {
            set_flag(&boot2Ahdr[B2H_USED]);
        }
    }
    else
    {
        if (boot2Bhdr[B2H_USED] != FLAG_SET)
        {
            set_flag(&boot2Bhdr[B2H_USED]);
        }
    }
}

void set_bad_flag(boot2_t b2s)
{
    if (b2s == A)
    {
        set_flag(&boot2Ahdr[B2H_BAD]);
    }
    else
    {
        set_flag(&boot2Bhdr[B2H_BAD]);
    }
}


void set_flag(uint8_t *flagp)
{
    uint32_t flag_word;
    uint32_t flag_set;
    uint32_t flag_clr;
    uint32_t u32Addrs;
    uint32_t flag_shift;

    u32Addrs = (uint32_t)((uint32_t)flagp & (uint32_t)0xFFFFFFFC);    // Get 4 byte address containing this flag

    flag_shift = ((uint32_t)flagp & (uint32_t)0x00000003);  // Byte position of flag
    flag_set   = FLAG_SET << (flag_shift * 8);

    flag_clr = ~(FLAG_CLR << (flag_shift * 8));

    flag_word = *(uint32_t *)u32Addrs;

    flag_word &= flag_clr;
    flag_word |= flag_set;

    // write to boot2 header
    b2_write_flag((uint8_t *)&flag_word, u32Addrs, 4);
}


uint8_t cksum(uint8_t *b2h)
{
    int     i;
    uint8_t hdrcksum = 0;

    for (i = 0; i < 7; i++)
    {
        hdrcksum += b2h[i];
    }
    return(hdrcksum);
}


//boot2_t ck_versions(void)
//{
//    int i;
//
//    for (i = B2H_VER; i < B2H_VER + 3; i++)
//    {
//        if ((boot2Ahdr[i] - boot2Bhdr[i]) != 0)
//        {
//            if (boot2Ahdr[i] > boot2Bhdr[i])
//            {
//                theChosenOne = A;
//                break;
//            }
//            else
//            {
//                theChosenOne = B;
//                break;
//            }
//        }
//    }
//    return theChosenOne;
//}
