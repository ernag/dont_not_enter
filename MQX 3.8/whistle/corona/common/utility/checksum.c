/*
 * checksum.c
 *
 *  Created on: Dec 11, 2013
 *      Author: street@whistle.com
 */

#include <checksum.h>

uint16_t str_checksum16(const char *str)
{
    char *current = (char *)str;
    uint32_t sum = 0xdeadbeef;
    
    /* Calculate simple checksum of ssid */
    while (*current != 0) {
        sum = (sum << 4) + *current++;
    }
    sum = (sum >> 16) + (sum & 0x0000ffff);
    sum += sum >> 16;
    return sum & 0xffff;
}
