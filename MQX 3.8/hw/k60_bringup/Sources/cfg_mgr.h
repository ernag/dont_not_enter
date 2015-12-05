/*
 * cfg_mgr.h
 *
 *  Created on: Jun 12, 2013
 *      Author: Ernie
 */

#ifndef CFG_MGR_H_
#define CFG_MGR_H_

#include "cfg_factory.h"


#define EXT_FLASH_FACTORY_CFG_ADDR           0xC000

/*
 *   Populate a factory string to the provided buffer.
 *   If factory string doesn't exist, memset to zero.
 */
void CFGMGR_faccfg_get(  fcfg_handle_t *pHandle, 
                         fcfg_tag_t tag, 
                         unsigned long max_len, 
                         void *pVal,
                         const char *pDesc );

/*
 *   Factory-Set Items
 */
void CFGMGR_load_fcfgs(void);


unsigned long fcfg_free(void *pAddr);
void *fcfg_malloc(unsigned long size);


#endif /* CFG_MGR_H_ */
