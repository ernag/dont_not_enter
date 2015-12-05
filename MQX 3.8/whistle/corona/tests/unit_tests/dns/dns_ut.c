/*
 * dns_ut.c
 *
 *  Created on: May 16, 2013
 *      Author: SpottedLabs
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include <mqx.h>
#include "wdns.h"
#include "wifi_mgr.h"
#include "../wifi/wifi_mgr_ut.h"
#include "cfg_mgr.h"
#include <rtcs.h>

int dns_ut()
{
//    char url_to_resolve[] = "staging.whistle.com.";
    char url_to_resolve[] = "whistle-server-staging.herokuapp.com.";
    HOSTENT_STRUCT *dns_result;
    A_INT32 return_code;
    int res = 0;
    
    
    return_code = wifi_mgr_ut_connect();
    if (A_OK != return_code)
    {
        corona_print("connect_handler failed %x", return_code);
        return -3;
    }
    
    corona_print("Sending query 1\r\n");
    dns_result = DNS_query_resolver_task((uint_8 *) url_to_resolve, DNS_A);
    
    if (dns_result->h_length > 0)
    {
        corona_print("SUCCESS!\r\n");
        corona_print(" len: %d\r\n", dns_result->h_length);
        corona_print(" addr: %u.%u.%u.%u (0x%.8X)\r\n",
                (uint_8) (dns_result->h_addr_list[0][3]),
                (uint_8) (dns_result->h_addr_list[0][2]),
                (uint_8) (dns_result->h_addr_list[0][1]),
                (uint_8) (dns_result->h_addr_list[0][0]),
                (uint_32) (((uint_32 *) (dns_result->h_addr_list[0]))[0]));
    }
    else
    {
        corona_print("FAILED\r\n");
        res = -1;   
    }
    
    wifi_ut_disconnect();
    return 0;
}
#endif
