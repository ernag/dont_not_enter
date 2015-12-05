/*
 * pwr_mgr.h
 *
 *  Created on: Feb 14, 2013
 *      Author: Ernie Aguilar
 */

#ifndef PWR_MGR_H_
#define PWR_MGR_H_

#include "k2x_errors.h"

typedef enum power_state
{
	/*
	 *   SHIP MODE - All components are in deep sleep state.  Only button interrupt can wake the system.
	 */
	PWR_STATE_SHIP,
	
	/*
	 *   STANDYBY MODE - All components are in sleep state.  Button interrupt or accelerometer "any-motion" interrupt 
	 *                   can wake the system.
	 */
	PWR_STATE_STANDBY,
	
	/*
	 *   All components are enabled, but not necessarily being actively used.
	 */
	PWR_STATE_NORMAL
}pwr_state_t;

/*
 *  Callback definition for handling changes in power state.
 */
typedef corona_err_t (*pwr_mgr_cbk_t)(pwr_state_t state);

/*
 *   Will change the system to requested power state.
 */
corona_err_t pwr_mgr_state(pwr_state_t state);


#endif /* PWR_MGR_H_ */
