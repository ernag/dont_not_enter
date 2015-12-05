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
#include "k2x_errors.h"
//#include "corona_errors.h"
#include "corona_isr_util.h"

/*
 *  TABLE OF ALL ENTRIES USED TO KEEP TRACK OF TRIGGER COUNTS.
 *  NOTE:  Changes here should also be reflected in the MAN pages of intr and intc.
 */
static isr_uentry_t g_uEntries[] = \
{
/*
 *    	IRQ Number         Name           Trigger Count           Pin Number
 */
		{INT_PORTA,      "btn",               0,                      29},
		{INT_PORTC,      "a1",                0,                      12},
		{INT_PORTC,      "a2",                0,                      13},
		{INT_PORTC,      "chg",               0,                      14},
		{INT_PORTC,      "pgood",             0,                      15},
		{INT_PORTE,      "wifi",              0,                       5},
};

#define NUM_UENTRIES	sizeof(g_uEntries) / sizeof(isr_uentry_t)


static isr_uentry_t * get_uentry(IRQInterruptIndex idx, uint8_t pin);
static isr_uentry_t * get_uentry_str(const char *pName);

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
static isr_uentry_t * get_uentry_str(const char *pName)
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
corona_err_t ciu_inc(IRQInterruptIndex idx, uint8_t pin)
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
corona_err_t ciu_clr(IRQInterruptIndex idx, uint8_t pin)
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
uint32_t ciu_cnt(IRQInterruptIndex idx, uint8_t pin)
{
	isr_uentry_t *pEntry = get_uentry(idx, pin);
	
	if(!pEntry)
	{
		return 0;
	}
	return pEntry->u32Count;
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
