/*
 * fw_info.h
 *
 *  Created on: Apr 29, 2013
 *      Author: SpottedLabs
 */

#ifndef FW_INFO_H_
#define FW_INFO_H_

typedef enum _FirmwareVersionImageType {
    FirmwareVersionImage_BOOT1 = 1,
    FirmwareVersionImage_BOOT2 = 2,
    FirmwareVersionImage_APP = 3
} FirmwareVersionImageType;

void print_header_info();
int get_version_string(char *ver_str, size_t ver_len, FirmwareVersionImageType version);

#endif /* FW_INFO_H_ */
