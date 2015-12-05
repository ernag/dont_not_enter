/*
 * pb_encode_whistle.h
 *
 *  Created on: Apr 16, 2013
 *      Author: SpottedLabs
 */

#ifndef PB_ENCODE_WHISTLE_H_
#define PB_ENCODE_WHISTLE_H_

#include <stdint.h>
#include <stddef.h>
//#include <stdbool.h>

bool pb_ostream_buffer_shift_contents(pb_ostream_t *stream, uint8_t *src_ptr, size_t payload_length, int8_t offset);

#endif /* PB_ENCODE_WHISTLE_H_ */
