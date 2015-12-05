/*
 * unit_tests.c
 *
 *  Created on: Mar 12, 2013
 *      Author: Chris
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "assert.h"
#include "unit_tests.h"
#include "button_mgr/button_mgr_ut.h"
#include "bt_mgr/bt_mgr_ut.h"
#include "wifi/wifi_ut.h"
#include "wifi/wifi_mgr_ut.h"
#include "con_mgr/con_mgr_ut.h"
#include "tpt_mgr/tpt_mgr_ut.h"
#include "evt_mgr/evt_mgr_ut.h"
#include "exp_handler/exp_handler_ut.h"
#include "wmp/wmp_ut.h"
#include "app_errors.h"
#include "cfg_mgr.h"
#include "dns/dns_ut.h"
#include "min_act/min_act_ut.h"
#include "sherlock/sherlock_ut.h"
#include "wmp_datadump_encode.h"

extern void _WIFIMGR_test_sock_delays(void);

void unit_tests(void)
{
#if WIFI_RSSI_TEST_APP
    wifi_rssi_ut();
#endif

#if 0
    _time_delay( 2 * 1000 ); // give some time for everyone's thread to get fired up.

    PROCESS_NINFO(ERR_OK, "\n\nthis should not be printed!\n\n");  // don't print anything for ERR_OK.
    PROCESS_NINFO(ERR_GENERIC, NULL);  // test with no arguments
    PROCESS_NINFO(ERR_GENERIC, "test params: 1: %i 2: %s 3: %i\n", 666, "chili con carne de la Nate Yoder", 123456); // test with arguments.
#endif
    
//#define TEST_ARM_EXCEPTION
#ifdef TEST_ARM_EXCEPTION
    char *stupid = NULL;
    *stupid = 'F';  // should cause an abort.
#endif
    
//    _WIFIMGR_test_sock_delays();
//    EVTMGR_spam_ut();
//    wifi_ut();
//    wifi_ut_udp();
//    wifi_mgr_ut();
//    wifi_mgr_chunked_ut();
//    button_mgr_ut();
//    con_mgr_ut();
//    evt_mgr_ut();
//    evt_mgr_accel_ut();
//    acc_mgr_ut();
//    wmp_accel_singleshot_ut();
//    wmp_accel_multipart_ut();
//    wmp_test_accel_encode();
//    wmp_gen_checkin_message();
//    wmp_parse_checkin_resp();
//    wmp_lm_checkin_req_gen();
//    wmp_lm_checkin_resp_parse();
//    evt_mgr_upload_ut();
//    dis_mgr_ut();
//    cfg_mgr_ut();
//    bt_mgr_ut();
//    exp_handler_ut();
//    dns_ut();
//    pnt_calc_ut();
//    sherlock_ut();
}
#endif
