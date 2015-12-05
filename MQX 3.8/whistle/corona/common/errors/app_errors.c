/*
 * app_errors.c
 *
 *  Created on: May 16, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <stdio.h>
#include <stdarg.h>
#include "cormem.h"
#include "app_errors.h"
#include "corona_console.h"
#include "wmp_datadump_encode.h"
#include "pwr_mgr.h"
#include "wassert.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define MAX_ERR_PRINT_SZ    (256)
#define MAX_NINFO_PRINT_SZ  (512)


////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
const char *ERR_STRING = "\n\t* ERROR\t";
const char *STAT_STRING = "STAT";

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Standard (private) function for printing out standard errors to console,
 *      but not to the server.  Use the macro "PRINT_ERROR" instead.
 *      
 *   Returns ERR_OK unconditionally, for convenience.
 */
err_t _app_err_print(uint8_t useless, ...)
{
    char *pDescription=NULL, *pFormatter=NULL;
    va_list ap;
    int len = 0;

    /*
     *   Construct the description for logging/printing.
     */
    va_start(ap, useless);
    pFormatter = va_arg(ap, char *); // we assume the formatter is the first parameter.
    if(pFormatter && *pFormatter)    // we will get NULL here if no parameter/string was given.
    {
        pDescription = walloc(MAX_ERR_PRINT_SZ);
        len = vsnprintf(pDescription, MAX_ERR_PRINT_SZ, pFormatter, ap);
        pDescription[MAX_ERR_PRINT_SZ-1] = 0;  // make sure NULL terminated.
    }
    va_end(ap);
    
    if(len > 0)
    {
        /*
         *   Log the info to the event manager.
         */
        corona_print("%s%s\n\n", ERR_STRING, ((pDescription)?pDescription:""));
    }
    
    if(pDescription)
    {
        wfree(pDescription);
    }
    
    return ERR_OK;
}

static numbered_info_number_type_t _number_type(int number)
{
	if (UNNUMBERED_INFO == number)
	{
		return NUMBER_TYPE_UNNUMBERED;
	}
	else if (NUMBERED_INFO_NUMBER_NONE == number)
	{
		return NUMBER_TYPE_NONE;
	}
	else if (STAT_OFFSET <= number && STAT_LAST_STAT >= number)
	{
		return NUMBER_TYPE_STAT;
	}
	else
	{
		return NUMBER_TYPE_ERR;
	}
}

/*
 *    Process a "NINFO" as a length/value pair.
 *    The series of bytes passed in will be converted to a string and sent to EVTMGR.
 */
err_t _numbered_info_log_bytes(int number, const char *pFile, const char *pFunc, int line, unsigned char *payload, unsigned long num_bytes)
{
    char *pDescription = NULL;
    
    if( 0 == num_bytes )
    {
        return (err_t)number;
    }
    
    // Cap the number of bytes at 1KB.
    num_bytes = (num_bytes > 1024) ? 1024 : num_bytes;
    
    pDescription = _lwmem_alloc( num_bytes*2 + 4 );  // string will be about twice the size plus a little bit of a buffer.
    
    /*
     *   Convert the raw byte values into a hex string representation.
     */
    if(pDescription)
    {
        bytes_to_string(payload, num_bytes, pDescription);
    }
    
    WMP_post_fw_info(  number, 
                       pDescription,
                       pFile,
                       pFunc,
                       (uint32_t)line );
    
    corona_print("\n\t* %s\t0x%x\t%s\n\t%s%s\tL %i\n\n",
                                    NUMBER_TYPE_ERR == _number_type(number) ? "ERR" : STAT_STRING,
                                    number,
                                    (pDescription?pDescription:""),
                                    (pFile?pFile:""),
                                    (pFunc?pFunc:""),
                                    line);
    
    if(pDescription)
    {
        wfree(pDescription);
    }
    
    return (err_t)number;
}

/*
 *   Standard Whistle Info Handler.
 *     Clients should use PROCESS_NINFO rather than function directly.
 *     This will log to EVTMGR, then print to the console.
 */
err_t _numbered_info_log(int number, int print_to_console, const char *pFile, const char *pFunc, int line, ...)
{
	numbered_info_number_type_t type = _number_type(number);
	int len = 0;

    if( NUMBER_TYPE_NONE != type )
    {
        char *pDescription=NULL, *pFormatter=NULL;
        va_list ap;

        /*
         *   Construct the description for logging/printing.
         */
        va_start(ap, line);
        pFormatter = va_arg(ap, char *); // we assume the formatter is the first parameter.
        if(pFormatter && *pFormatter)    // we will get NULL here if no parameter/string was given.
        {
            pDescription = _lwmem_alloc(MAX_NINFO_PRINT_SZ);
            if(pDescription)
            {
                len = vsnprintf(pDescription, MAX_NINFO_PRINT_SZ, pFormatter, ap);
                pDescription[MAX_NINFO_PRINT_SZ-1] = 0;  // make sure NULL terminated.
            }
        }
        va_end(ap);
        
        /*
         *   Log the info to the event manager.
         */
        WMP_post_fw_info(  number, 
                           (len > 0)?pDescription:"NINFO vsnprintf fail?",
                           pFile,
                           pFunc,
                           (uint32_t)line );

        /*
         *   Print relevant information to the console.
         */
        if (print_to_console)
        {
        	if( NUMBER_TYPE_UNNUMBERED == type )
        	{
        		corona_print("\n\tINFO\t%s\n", ((pDescription)?pDescription:""));
        	}
        	else
        	{
        		corona_print("\n\t* %s\t0x%x\t%s\n\t%s%s\tL %i\n\n",
        				NUMBER_TYPE_ERR == type ? "ERR" : STAT_STRING,
        						number,
        						(pDescription?pDescription:""),
        						(pFile?pFile:""),
        						(pFunc?pFunc:""),
        						line);
        	}
        }
        
        if(pDescription)
        {
            _lwmem_free(pDescription);
        }
    }
    
    return (err_t)number;
}
