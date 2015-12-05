/*
 * rf_switch.h
 *
 *  Created on: May 30, 2013
 *      Author: Chris
 */

#ifndef RF_SWITCH_H_
#define RF_SWITCH_H_

typedef enum _tag_rf_switch_e
{
	RF_SWITCH_BT,
	RF_SWITCH_WIFI
} rf_switch_e;

//
// RF Switch could be in either orientation.
// https://whistle.atlassian.net/wiki/display/COR/Corona+assembly+revisions+with+software+impacts
//

void rf_switch_init(void);
void rf_switch(rf_switch_e position, boolean on);

#endif /* RF_SWITCH_H_ */
