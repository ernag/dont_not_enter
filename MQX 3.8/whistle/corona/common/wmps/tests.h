/*
 * wmps_tests.h
 *
 *  Created on: June 12, 2013
 *      Author: Nicole Yu
 */

#ifndef WMPS_TESTS_H_
#define WMPS_TESTS_H_

#ifdef MQX_ENV
#include "app_errors.h"
#endif

#ifndef MQX_ENV
typedef enum  {
    OK = 0,
    ROUNDTRIP_FAIL    = 1 << 0,
    MAKE_MESSAGE_FAIL = 1 << 1,
    ERROR_CHECK_FAIL  = 1 << 2
} err_t;
#endif

err_t test_decode_errors(void);
err_t test_unpack(void);
err_t test_make_msg_noalloc(void);

typedef uint8_t result_t;

#endif /* WMPS_TESTS_H_ */

/* EOF */
