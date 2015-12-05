/*
 * cormem.h
 * 
 *  walloc()
 *  wfree()
 *  
 *  These functions were created to standardise dynamic memory handling in the Corona system.
 *     Using these API's means the user can:
 *     
 *        a)  never have to worry about checking for NULL pointers (that happens internally).
 *        b)  never have to assert() on failure (that happens internally).
 *        c)  never have to print things out or log errors (that happens internally).
 *        
 *  This also saves a lot of .text.  See FW-761 for example.
 *
 *  Created on: Oct 28, 2013
 *      Author: Ernie
 */

#ifndef WMEM_H_
#define WMEM_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <psptypes.h>

////////////////////////////////////////////////////////////////////////////////
// Public Prototypes
////////////////////////////////////////////////////////////////////////////////
#define walloc(sz) _walloc(sz, __FILE__, __LINE__)
#define wfree(ptr) _wfree(ptr, __FILE__, __LINE__)
void *_walloc(_mem_size sz, char *file, int line);
void  _wfree(void *ptr, char *file, int line);


#endif /* WMEM_H_ */
