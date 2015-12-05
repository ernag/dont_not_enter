/*
 * corona_isr_util.c
 *
 *  Created on: Feb 12, 2013
 *      Author: Ernie Aguilar
 *
 *  Contains utilities that can be used by ISR's and clients of ISR's.
 *  TODO - This needs a mutex, since g_uEntries[] can be accessed by any of the ISR's.
 *  NOTE - None of the API's here called by ISR's can be blocking or take a long time to execute.
 */

#include "IO_Map.h"
//#include "k2x_errors.h"
#include "corona_errors.h"
#include "corona_isr_util.h"
#include "corona_console.h"
extern cc_handle_t g_cc_handle;


const char * IRQC_DISABLED_STR 		= "Interrupts and DMA disabled";
const char * IRQC_DMA_RISING_STR 	= "DMA on rising edge";
const char * IRQC_DMA_FALLING_STR 	= "DMA on falling edge";
const char * IRQC_DMA_EITHER_STR 	= "DMA on either edge";
const char * IRQC_LOGIC_0_STR 		= "DMA on logic 0";
const char * IRQC_EDGE_RISING_STR 	= "Interrupt on rising edge";
const char * IRQC_EDGE_FALLING_STR 	= "Interrupt on falling edge";
const char * IRQC_EDGE_EITHER_STR 	= "Interrupt on either edge";
const char * IRQC_LOGIC_1_STR 		= "Interrupt on logic 1";
const char * IRQC_NA_STR 			= "Not applicable setting";
const char * IRQC_INVALID_STR 		= "Invalid or Reserved IRQC setting";

/*
 *  TABLE OF ALL ENTRIES USED TO KEEP TRACK OF TRIGGER COUNTS.
 *  NOTE:  Changes here should also be reflected in the MAN pages of intr and intc.
 */
isr_uentry_t g_uEntries[] = \
{
/*
 *    	IRQ Number         Name           Trigger Count           Pin Number        IRQ Config
 */
		{INT_PORTE,      "btn",               0,                      1,       IRQC_EDGE_FALLING},
		{INT_PORTC,      "a1",                0,                      12,       IRQC_EDGE_RISING},
		{INT_PORTC,      "a2",                0,                      13,       IRQC_EDGE_RISING},
		{INT_PORTC,      "chg",               0,                      14,       IRQC_EDGE_EITHER},
		{INT_PORTC,      "pgood",             0,                      15,       IRQC_EDGE_EITHER},
		{INT_PORTE,      "wifi",              0,                       5,       IRQC_EDGE_RISING},
		{INT_Reserved119, 0,                  0,                       0,                      0}, // this is like "NULL terminating" the table.
};

#define NUM_UENTRIES	sizeof(g_uEntries) / sizeof(isr_uentry_t)


static isr_uentry_t * get_uentry(IRQInterruptIndex idx, uint8_t pin);
static void ciu_set_irqc(isr_uentry_t *pEntry, volatile isr_irqc_t *pIrqc);
static void ciu_get_irqc(isr_uentry_t *pEntry, volatile isr_irqc_t *pIrqc);
static volatile uint32_t *ciu_get_irqc_reg(isr_uentry_t *pEntry);

/*
 *   Return a pointer to the corresponding PORTx_PCRn register.
 */
static volatile uint32_t *ciu_get_irqc_reg(isr_uentry_t *pEntry)
{
	volatile uint32_t *pPcrReg = 0;

	switch(pEntry->idx)
	{
		case INT_PORTA:
			switch(pEntry->u8Pin)
			{
				case 29:
					pPcrReg = (uint32_t *)&PORTA_PCR29;
					break;

				default:
					break;
			}
			break;

		case INT_PORTC:
			switch(pEntry->u8Pin)
			{
				case 12:
					pPcrReg = (uint32_t *)&PORTC_PCR12;
					break;

				case 13:
					pPcrReg = (uint32_t *)&PORTC_PCR13;
					break;

				case 14:
					pPcrReg = (uint32_t *)&PORTC_PCR14;
					break;

				case 15:
					pPcrReg = (uint32_t *)&PORTC_PCR15;
					break;

				default:
					break;
			}
			break;

		case INT_PORTE:
			switch(pEntry->u8Pin)
			{
				case 5:
					pPcrReg = (uint32_t *)&PORTE_PCR5;
					break;

				default:
					break;
			}
			break;
	}
	return pPcrReg;
}

/*
 * Sets IRQC field in PCR register for entry to passed in value.
 */
static void ciu_set_irqc(isr_uentry_t *pEntry, volatile isr_irqc_t *pIrqc)
{
	volatile uint32_t *pPcrReg = ciu_get_irqc_reg(pEntry);

	if(!pPcrReg)
	{
		cc_print(&g_cc_handle, "ERROR! - unsupported IRQ number in ciu_set_irqc()\n");
		return;
	}
	*pPcrReg &= ~PORT_PCR_IRQC_MASK;    // clear IRQC bitfield first.
	*pPcrReg |= PORT_PCR_IRQC(*pIrqc);	// OR in the IRQC portion passed in by the user.
}

/*
 * Gets IRQC field in PCR register for entry.
 */
static void ciu_get_irqc(isr_uentry_t *pEntry, volatile isr_irqc_t *pIrqc)
{
	volatile uint32_t *pPcrReg = ciu_get_irqc_reg(pEntry);

	if(!pPcrReg)
	{
		cc_print(&g_cc_handle, "ERROR! - unsupported IRQ number in ciu_set_irqc()\n");
		return;
	}
	*pIrqc = (*pPcrReg & PORT_PCR_IRQC_MASK) >> PORT_PCR_IRQC_SHIFT;
}

/*
 *  Get/set a status corresponding to an entry.
	Pass in the following value for pIrqc, it will return either error or what is read from the IRQC field in PORTx_PCRn register.
		IRQC_DISABLED       : A valid state to set or read, interrupts disabled.
		IRQC_DMA_RISING     : A valid state to set or read.
		IRQC_DMA_FALLING    : A valid state to set or read.
		IRQC_DMA_EITHER     : A valid state to set or read.
		IRQC_LOGIC_0        : A valid state to set or read.
		IRQC_EDGE_RISING    : A valid state to set or read.
		IRQC_EDGE_FALLING   : A valid state to set or read.
		IRQC_EDGE_EITHER    : A valid state to set or read.
		IRQC_LOGIC_1        : A valid state to set or read.
		IRQC_NOT_APPLICABLE : Do not pass this in.  It will not be returned.  It is used only for setting g_uEntries[]
		IRQC_INVALID        : Do not pass this in.  It will come back if there was a problem while processing.
		IRQC_REQUEST_STATUS : Use this when all we care about is reading status.
 */
void ciu_status(isr_uentry_t *pEntry, volatile isr_irqc_t *pIrqc)
{
	switch(*pIrqc)
	{
		case IRQC_DISABLED:     // A valid state to set or read, interrupts disabled.
		case IRQC_DMA_RISING:   // A valid state to set or read.
		case IRQC_DMA_FALLING:  // A valid state to set or read.
		case IRQC_DMA_EITHER:   // A valid state to set or read.
		case IRQC_LOGIC_0:      // A valid state to set or read.
		case IRQC_EDGE_RISING:  // A valid state to set or read.
		case IRQC_EDGE_FALLING: // A valid state to set or read.
		case IRQC_EDGE_EITHER:  // A valid state to set or read.
		case IRQC_LOGIC_1:      // A valid state to set or read.
			ciu_set_irqc(pEntry, pIrqc);
			/*
			 *  Note: just let this fall through to the status portion, so we can read back the status before returning.
			 */

		case IRQC_REQUEST_STATUS : // Use this when all we care about is reading status.
			ciu_get_irqc(pEntry, pIrqc);
			return;

		case IRQC_NOT_APPLICABLE : // Do not pass this in.  It will not be returned.  It is used only for setting g_uEntries[]
		case IRQC_INVALID        : // Do not pass this in.  It will come back if there was a problem while processing.
		default:
			*pIrqc = IRQC_INVALID;
			return;
	}
}

/*
 *  Retrieve an entry.
 *    Returns NULL if entry does not exist.
 */
static isr_uentry_t * get_uentry(IRQInterruptIndex idx, uint8_t pin)
{
	int i;

	for(i = 0; i < NUM_UENTRIES; i++)
	{
		if(idx == g_uEntries[i].idx)
		{
			if( (pin == g_uEntries[i].u8Pin) ||
				(g_uEntries[i].u8Pin == PIN_NOT_APPLICABLE) ||
				(pin == PIN_NOT_APPLICABLE) )
			{
				return &(g_uEntries[i]);
			}
		}
	}
	return 0;
}

/*
 *  Retrieve an entry, based on the name.
 *     Returns NULL if entry doesn't exist.
 */
isr_uentry_t * get_uentry_str(const char *pName)
{
	int i;

	for(i = 0; i < NUM_UENTRIES; i++)
	{
		if(!strcmp(pName, g_uEntries[i].pName))
		{
			return &(g_uEntries[i]);
		}
	}
	return 0;
}


/*
 *   Increment the count of given IRQ on given pin.
 */
corona_err_t ciu_inc(uint32_t idx, uint8_t pin)
{
	isr_uentry_t *pEntry = get_uentry(idx, pin);

	if(!pEntry)
	{
		return CC_IRQ_UNSUPPORTED;
	}
	pEntry->u32Count++;
	return SUCCESS;
}

/*
 *  Reset counter for given IRQ on given pin.
 */
corona_err_t ciu_clr(uint32_t idx, uint8_t pin)
{
	isr_uentry_t *pEntry = get_uentry(idx, pin);

	if(!pEntry)
	{
		return CC_IRQ_UNSUPPORTED;
	}
	pEntry->u32Count = 0;
	return SUCCESS;
}

/*
 *  Return current count for given IRQ on given pin.
 */
uint32_t ciu_cnt(uint32_t idx, uint8_t pin)
{
	isr_uentry_t *pEntry = get_uentry(idx, pin);

	if(!pEntry)
	{
		return 0;
	}
	return pEntry->u32Count;
}

/*
 *  Print an interrupt status string.
 */
const char * ciu_status_str(volatile isr_irqc_t *pIrqc)
{
	switch(*pIrqc)
	{
		case IRQC_DISABLED:
			return IRQC_DISABLED_STR;
		case IRQC_DMA_RISING:
			return IRQC_DMA_RISING_STR;
		case IRQC_DMA_FALLING:
			return IRQC_DMA_FALLING_STR;
		case IRQC_DMA_EITHER:
			return IRQC_DMA_EITHER_STR;
		case IRQC_LOGIC_0:
			return IRQC_LOGIC_0_STR;
		case IRQC_EDGE_RISING:
			return IRQC_EDGE_RISING_STR;
		case IRQC_EDGE_FALLING:
			return IRQC_EDGE_FALLING_STR;
		case IRQC_EDGE_EITHER:
			return IRQC_EDGE_EITHER_STR;
		case IRQC_LOGIC_1:
			return IRQC_LOGIC_1_STR;
		case IRQC_NOT_APPLICABLE:
			return IRQC_NA_STR;
		case IRQC_INVALID:
		case IRQC_REQUEST_STATUS:
		default:
			return IRQC_INVALID_STR;
	}
}

/*
 *   Clear the IRQ's trigger count, based on name of IRQ.
 */
corona_err_t ciu_clr_str(const char *pName)
{
	isr_uentry_t *pEntry = get_uentry_str(pName);

	if(!pEntry)
	{
		return CC_IRQ_UNSUPPORTED;
	}

	pEntry->u32Count = 0;
	return SUCCESS;
}

/*
 *  Return the count of the given IRQ, based on name.
 */
corona_err_t ciu_cnt_str(const char *pName, uint32_t *pCount)
{
	isr_uentry_t *pEntry = get_uentry_str(pName);

	if(!pEntry)
	{
		return CC_IRQ_UNSUPPORTED;
	}

	*pCount = pEntry->u32Count;
	return SUCCESS;
}
