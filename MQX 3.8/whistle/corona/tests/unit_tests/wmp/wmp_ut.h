/*
 * wmp_ut.h
 *
 *  Created on: Apr 11, 2013
 *      Author: SpottedLabs
 */

#ifndef WMP_UT_H_
#define WMP_UT_H_

int wmp_accel_ut();
int wmp_higher_level_ut();
int wmp_test_accel_encode(void);
int wmp_gen_checkin_message();
int wmp_parse_checkin_resp();
int wmp_test_decode();
int wmp_lm_checkin_req_gen();
int wmp_lm_checkin_resp_gen();

#endif /* WMP_UT_H_ */
