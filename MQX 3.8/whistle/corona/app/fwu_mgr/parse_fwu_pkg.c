//
//  parse_fwu_pkg.c
//
//  Created by Kevin Lloyd on 3/24/13.
//  Copyright (c) 2013 Kevin Lloyd. All rights reserved.
//

#include <string.h>
#include <stdio.h>
#include <mqx.h>

#include "parse_fwu_pkg.h"
#include "corona_console.h"


#if 1

extern cc_handle_t g_cc_handle;

size_t read_uint32(uint32_t *uint32_var, uint8_t* bp)
{
    *uint32_var = 0;
    memcpy (uint32_var, bp, sizeof(uint32_t));
    return sizeof(uint32_t);
}

size_t read_uint16(uint16_t *uint16_var, uint8_t* bp)
{
    *uint16_var = 0;
    memcpy (uint16_var, bp, sizeof(uint16_t));
    return sizeof(uint16_t);
}

void parse_pkg(uint8_t* sb, fwu_pkg_t *package)
{
    size_t sdx = 0;
    size_t last_sdx = 10;      // dummy number until pkg_raw_payload is read;
    uint16_t tlv_type;
    uint32_t tlv_length;
    uint8_t tlv_img_type;
    dep_min_ver_t *dependency = NULL;
    fwu_bin_t *binary_image = NULL;

    sdx += read_uint16(&tlv_type, &sb[sdx]);
    sdx += read_uint32(&tlv_length, &sb[sdx]);
    while (sdx < last_sdx)
    {
        switch (tlv_type)
        {
            case TLV_TYPE_FWU_PKG_INFO:
                package->pkg_info.header.type = tlv_type;
                package->pkg_info.header.length = tlv_length;
                memcpy(package->pkg_info.pkg_title, &sb[sdx], 48);
                sdx += 48;
                sdx += read_uint32(&(package->pkg_info.pkg_size), &sb[sdx]);
                sdx += read_uint32(&(package->pkg_info.pkg_crc), &sb[sdx]);
                sdx += read_uint32(&(package->pkg_info.pkg_signature), &sb[sdx]);
                sdx += read_uint16((uint16_t *) &(package->pkg_info.pkg_contents), &sb[sdx]);
                sdx += read_uint32(&(package->pkg_info.pkg_raw_payload), &sb[sdx]);
                last_sdx = package->pkg_info.pkg_raw_payload;
                break;

            case TLV_TYPE_MINIMUM_VERSION_DEPENDENCY:
                tlv_img_type = sb[sdx++];
                switch (tlv_img_type)
                {
                    case BOOT1_IMAGE:
                        dependency = &package->dep_min_boot1;
                        dependency->type = BOOT1_IMAGE;
                        break;
                    case BOOT2_IMAGE:
                        dependency = &package->dep_min_boot2;
                        dependency->type = BOOT2_IMAGE;
                        break;
                    case APP_IMAGE:
                        dependency = &package->dep_min_app;
                        dependency->type = APP_IMAGE;
                        break;
                    case BOOT2A_IMAGE:
                        dependency = &package->dep_min_boot2a;
                        dependency->type = BOOT2A_IMAGE;
                        break;
                    case BOOT2B_IMAGE:
                        dependency = &package->dep_min_boot2b;
                        dependency->type = BOOT2B_IMAGE;
                        break;
                    default:
                        break;
                }
                dependency->header.type       = tlv_type;
                dependency->header.length     = tlv_length;
                dependency->min_major_version = sb[sdx++];
                dependency->min_minor_version = sb[sdx++];
                break;

            case TLV_TYPE_BINARY_IMAGE:
                tlv_img_type = sb[sdx++];
                switch (tlv_img_type)
                {
                    case BOOT1_IMAGE:
                        binary_image = &package->boot1_img;
                        binary_image->type = BOOT1_IMAGE;
                        break;
                    case BOOT2_IMAGE:
                        binary_image = &package->boot2_img;
                        binary_image->type = BOOT2_IMAGE;
                        break;
                    case APP_IMAGE:
                        binary_image = &package->app_img;
                        binary_image->type = APP_IMAGE;
                        break;
                    case BOOT2A_IMAGE:
                        binary_image = &package->boot2a_img;
                        binary_image->type = BOOT2A_IMAGE;
                        break;
                    case BOOT2B_IMAGE:
                        binary_image = &package->boot2b_img;
                        binary_image->type = BOOT2B_IMAGE;
                        break;
                    default:
                        break;
                }
                binary_image->header.type = tlv_type;
                binary_image->header.length = tlv_length;
                binary_image->image_major_version = sb[sdx++];
                binary_image->image_minor_version = sb[sdx++];
                memcpy (binary_image->image_version_tag, &sb[sdx], 17);
                sdx += 17;
                sdx += read_uint32(&binary_image->image_size, &sb[sdx]);
                sdx += read_uint32(&binary_image->image_crc, &sb[sdx]);
                sdx += read_uint32(&binary_image->image_offset, &sb[sdx]);
                break;
        }
        // Read the next TLV prototype
        sdx += read_uint16(&tlv_type, &sb[sdx]);
        sdx += read_uint32(&tlv_length, &sb[sdx]);
    };
}

#endif

//PKG_CONTAINS_BOOT1       = 0x0001,
//PKG_CONTAINS_BOOT2       = 0X0002,
//PKG_CONTAINS_APP         = 0x0004,
//PKG_CONTAINS_DEV_CONFIG  = 0x0008,

void print_contents(contents_mask_t pkg_contents)
{
    int idx = 0;
    char contents[30];

    if (pkg_contents & PKG_CONTAINS_BOOT1)
    {
        memcpy (&contents[idx], "boot1 ", 6);
        idx += 6;
    }
    if (pkg_contents & PKG_CONTAINS_BOOT2)
    {
        memcpy (&contents[idx], "boot2 ", 6);
        idx += 6;
    }
    if (pkg_contents & PKG_CONTAINS_APP)
    {
        memcpy (&contents[idx], "App ", 4);
        idx += 4;
    }
    if (pkg_contents & PKG_CONTAINS_DEV_CONFIG)
    {
        memcpy (&contents[idx], "Config", 6);
        idx += 6;
    }
    if (pkg_contents & PKG_CONTAINS_BOOT2A)
    {
        memcpy (&contents[idx], "boot2A ", 7);
        idx += 7;
    }
    if (pkg_contents & PKG_CONTAINS_BOOT2B)
    {
        memcpy (&contents[idx], "boot2B ", 7);
        idx += 7;
    }
    corona_print( "  Pkg Contents: 0x%X %s\n", pkg_contents, contents);
}


void print_image_type(image_type_t type) {
    switch (type) {
        case BOOT1_IMAGE:
            corona_print( "\tImg Type: 0x%02X (Boot1)\n", type);
            break;
        case BOOT2_IMAGE:
            corona_print( "\tImg Type: 0x%02X (Boot2)\n", type);
            break;
        case APP_IMAGE:
            corona_print( "\tImg Type: 0x%02X (App)\n", type);
            break;
        case BOOT2A_IMAGE:
            corona_print( "\tImg Type: 0x%02X (Boot2A)\n", type);
            break;
        case BOOT2B_IMAGE:
            corona_print( "\tImg Type: 0x%02X (Boot2B)\n", type);
            break;
    }
}

void print_tlv_fwu_dep_min_ver(dep_min_ver_t *dependency) {
    if (dependency->header.type != 0) {
        corona_print( "TLV Type: 0x%02X (Img Dependency)\n", (uint16_t) (dependency->header.type));
        corona_print( "\tTLV Length: %d\n", dependency->header.length);
        print_image_type(dependency->type);
        corona_print( "\tMinimum Version: %d.%d\n", dependency->min_major_version, dependency->min_minor_version);
    }
}


void print_tlv_fwu_binary_image(fwu_bin_t *image) {
    if (image->header.type != 0) {
        corona_print( "TLV Type: 0x%02X (Binary Img)\n", image->header.type);
        corona_print( "  TLV Length: %d\n", image->header.length);
        print_image_type(image->type);
        corona_print( "  Img Size: %d\n", image->image_size);
        corona_print( "  Version: %d.%d-%s\n", image->image_major_version, image->image_minor_version, image->image_version_tag);
        corona_print( "  Img CRC: 0x%X\n", image->image_crc);
        corona_print( "  Img Offset: %u\n", image->image_offset);
    }
}

void print_package(fwu_pkg_t *package)
{
    if (package == NULL)
        return;

    corona_print( "TLV Type: 0x%X (Package Info)\n", package->pkg_info.header.type);
    corona_print( "\tTLV Length: %d\n", package->pkg_info.header.length);
    corona_print( "\tTitle: %s\n", package->pkg_info.pkg_title);
    corona_print( "\tSize: %d\n", package->pkg_info.pkg_size);
    corona_print( "\tCRC: 0x%X\n", package->pkg_info.pkg_crc);
    corona_print( "\tSignature: 0x%X\n", package->pkg_info.pkg_signature);
    print_contents (package->pkg_info.pkg_contents);
    corona_print( "\tBinary Payload Offset: %u\n", package->pkg_info.pkg_raw_payload);

    print_tlv_fwu_dep_min_ver(&package->dep_min_boot1);
    print_tlv_fwu_dep_min_ver(&package->dep_min_boot2);
    print_tlv_fwu_dep_min_ver(&package->dep_min_app);
    print_tlv_fwu_binary_image(&package->boot1_img);
    print_tlv_fwu_binary_image(&package->boot2_img);
    print_tlv_fwu_binary_image(&package->app_img);
    print_tlv_fwu_binary_image(&package->boot2a_img);
    print_tlv_fwu_binary_image(&package->boot2b_img);
}

