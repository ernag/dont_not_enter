/*
 * persist_var.h
 *
 *  Created on: Jun 5, 2013
 *      Author: SpottedLabs
 */

#ifndef PERSIST_VAR_H_
#define PERSIST_VAR_H_

#include "pmem.h"
#include "datamessage.pb.h"

/*
 *   Private Functions  (do not call these directly unless you know what you are doing).
 */
extern int  _corona_pmem_flag_set(persist_flag_t flag, const int bValue);
extern void _corona_pmem_var_set(persist_var_t var, const void *pValue);
extern void _corona_pmem_update_checksum(void);
extern void _corona_pmem_clear(void);
extern unsigned char _corona_pmem_calc_checksum(void);

/*
 *   Public Macros -- Use these rather than pmem lib directly, to make use of mutex protection.
 */
#define PMEM_FLAG_SET(flag, b_val)          _corona_pmem_flag_set(flag, b_val)
#define PMEM_VAR_SET(var, payload_ptr)      _corona_pmem_var_set(var, payload_ptr)
#define PMEM_UPDATE_CKSUM(void)             _corona_pmem_update_checksum()
#define PMEM_CALC_CKSUM(void)               _corona_pmem_calc_checksum()
#define PMEM_CLEAR(void)                    _corona_pmem_clear()

/*
 *   Public Functions
 */
void PERSIST_init(void);
void persist_var_init(void);
void persist_print_array(void);
void print_persist_data(void);
void persist_set_untimely_crumb( BreadCrumbEvent crumb );
uint8_t persist_app_has_initialized(void);


#endif /* PERSIST_VAR_H_ */
