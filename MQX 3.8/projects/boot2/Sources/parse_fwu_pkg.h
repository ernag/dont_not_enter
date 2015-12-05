//
// parse_fwu_pkg.h
//
//  Created on: Apr 3, 2013
//      Author: Jim
//

#ifndef PARSE_FWU_PKG_H_
#define PARSE_FWU_PKG_H_

#include "types.h"
#include "PE_Types.h"


typedef enum tlv_type
{
    TLV_TYPE_FWU_PKG_INFO               = 1,
    TLV_TYPE_MINIMUM_VERSION_DEPENDENCY = 10,
    TLV_TYPE_BINARY_IMAGE               = 11,
    TLV_TYPE_DEVICE_CONFIG              = 20,
    TLV_TYPE_PATCH                      = 21
} tlv_type_t;

typedef struct tlv_header
{
    tlv_type_t type;    //uint16_t
    uint16_t   length;
} tlv_hdr_t;

typedef enum pkg_contents_mask
{
    PKG_CONTAINS_BOOT1       = 0x0001,
    PKG_CONTAINS_BOOT2       = 0X0002,
    PKG_CONTAINS_APP         = 0x0004,
    PKG_CONTAINS_DEV_CONFIG  = 0x0008,
    PKG_CONTAINS_BOOT2A      = 0x0010,
    PKG_CONTAINS_BOOT2B      = 0x0020,
    PKG_CONTAINS_BASEPATCH   = 0x0040,
    PKG_CONTAINS_LOWENERGY   = 0x0080,
    PKG_CONTAINS_LAST        = 0xFFFF
} contents_mask_t;

#define PKG_CRC_OFFSET      58

typedef struct tlv_fwu_pkg_info
{
    tlv_hdr_t       header;
    char            pkg_title[48];
    uint32_t        pkg_size;
    uint32_t        pkg_crc;
    uint32_t        pkg_signature;
    contents_mask_t pkg_contents;
    uint32_t        pkg_raw_payload;
} fwu_pkg_info_t;


typedef enum image_type
{
    BOOT1_IMAGE  = 1,
    BOOT2_IMAGE  = 2,
    APP_IMAGE    = 3,
    BOOT2A_IMAGE = 4,
    BOOT2B_IMAGE = 5
} image_type_t;

typedef struct tlv_fwu_dep_min_ver
{
    tlv_hdr_t     header;
    image_type_t  type;
    uint8_t       min_major_version;
    uint8_t       min_minor_version;
} dep_min_ver_t;


typedef struct tlv_fwu_binary_image
{
    tlv_hdr_t     header;
    image_type_t  type;
    uint32_t      image_size;
    uint8_t       image_major_version;
    uint8_t       image_minor_version;
    char          image_version_tag[17];
    uint32_t      image_crc;
    uint32_t      image_offset;
} fwu_bin_t;

typedef enum patch_type
{
    BASEPATCH = 1,
    LOWENERGY = 2,
} patch_type_t;

typedef struct tlv_fwu_patch
{
    tlv_hdr_t     header;
    patch_type_t  type;
    uint32_t      patch_size;
    uint32_t      patch_crc;
    uint32_t      patch_offset;
} fwu_patch_t;

typedef struct fwu_package
{
    fwu_pkg_info_t  pkg_info;
    dep_min_ver_t   dep_min_boot1;
    dep_min_ver_t   dep_min_boot2;
    dep_min_ver_t   dep_min_app;
    dep_min_ver_t   dep_min_boot2a;
    dep_min_ver_t   dep_min_boot2b;
    fwu_bin_t       boot1_img;
    fwu_bin_t       boot2_img;
    fwu_bin_t       app_img;
    fwu_bin_t       boot2a_img;
    fwu_bin_t       boot2b_img;
    fwu_patch_t     basepatch_patch;
    fwu_patch_t     lowenergy_patch;
} fwu_pkg_t;

void parse_pkg(uint8_t* sb, fwu_pkg_t *package);
void print_package(fwu_pkg_t *package);


#endif /* PARSE_FWU_PKG_H_ */
