/*
 * fw_info.c
 *
 *  Created on: Apr 29, 2013
 *      Author: SpottedLabs
 */

#include <mqx.h>
#include <bsp.h>
#include <string.h>
#include "fw_info.h"

/*
 * The bellow address information is copied from boot2_mgr.h
 */
#define BT1_HDR            0x00000200

#define B2A                0x00002000
#define B2B                0x00012000

#define B2A_HDR            B2A + 0x200
#define B2B_HDR            B2B + 0x200

#define BOOT2_MAX_SIZE     0x10000

#define APP_LOAD_ADDRS     0x22000
#define APP_ENTRY          APP_LOAD_ADDRS + 4
#define APP_HDR            APP_LOAD_ADDRS + 0x200

#define FLAG_SET           0x55
#define FLAG_CLR           0xFF

// Header enums
typedef enum
{
    HDR_NAME        = 0,
    HDR_VER         = 3,
    HDR_CKSUM       = 6,
    HDR_SUCCESS     = 7,
    HDR_UPDATE      = 8,
    HDR_ATTMPT1     = 9,
    HDR_ATTMPT2     = 10,
    HDR_ATTMPT3     = 11,
    HDR_BAD         = 12,
    HDR_IMG_SAVD    = 13,
    HDR_USED        = 14,
    HDR_VER_TAG     = 16
} hdr_idx_t;


// Read the boot2 and app headers
// todo: print printable chars, otherwise, hex
void print_header_info()
{
    char        buf[32];
    char        c;
    uint8_t*    phdr;
    int         i;
    int         x;


    for (i = 0; i < 4; i++)
    {
        switch (i)
        {
            case 0:
                phdr = (uint8_t*)APP_HDR;
                break;
            case 1:
                phdr = (uint8_t*)B2A_HDR;
                break;
            case 2:
                phdr = (uint8_t*)B2B_HDR;
                break;
            case 3:
                phdr = (uint8_t*)BT1_HDR;
                break;
       }

        memset (buf, 0, sizeof(buf));
        for (x = 16; x < 32; x++)
        {
            c = phdr[x];
            if ((c >= ' ') && (c <= '~'))
            {
                buf[x - 16] = c;
            }
            else
            {
                break;
            }
        }
        
        corona_print("COMP: %c%c%c (0x%08x)\n",
                  phdr[0],phdr[1],phdr[2],phdr);
        corona_print("  VERS: %d.%d.%d-%s | CKSUM:%x\n",
                  phdr[3],phdr[4],phdr[5],buf,phdr[6]);
        corona_print("  SUCC: %x Update: %x \n  Attempts:%x %x %x Bad: %x Savd: %x Used: %x\n",
                  phdr[7],phdr[8],phdr[9],phdr[10],phdr[11],phdr[12],phdr[13],phdr[14]);
    }

}

void print_header_info_short(void)
{
    char        buf[32];
    char        c;
    uint8_t*    phdr;
    int         i;
    int         x;


    for (i = 0; i < 4; i++)
    {
        switch (i)
        {
            case 0:
                phdr = (uint8_t*)APP_HDR;
                break;
            case 1:
                phdr = (uint8_t*)B2A_HDR;
                break;
            case 2:
                phdr = (uint8_t*)B2B_HDR;
                break;
            case 3:
                phdr = (uint8_t*)BT1_HDR;
                break;
       }

        memset (buf, 0, sizeof(buf));
        for (x = 16; x < 32; x++)
        {
            c = phdr[x];
            if ((c >= ' ') && (c <= '~'))
            {
                buf[x - 16] = c;
            }
            else
            {
                break;
            }
        }
        
        corona_print("%c%c%c: %d.%d.%d-%s\n",
                  phdr[0],phdr[1],phdr[2],phdr[3],phdr[4],phdr[5],buf);
    }

}

int _get_version_string(char *ver_str, size_t ver_len, uint8_t *phdr)
{
    char        *out_buf;
    char        tag_buf[32];
    size_t      out_len;
    char        c;
    int         x;
    
    memset (tag_buf, 0, sizeof(tag_buf));
    for (x = 16; x < 32; x++)
    {
        c = phdr[x];
        if ((c >= ' ') && (c <= '~'))
        {
            tag_buf[x - 16] = c;
        }
        else
        {
            break;
        }
    }

    memset (ver_str, 0, ver_len);
    return snprintf(ver_str, ver_len, "%d.%d-%s",
              phdr[4],phdr[5],tag_buf);
}


int get_version_string(char *ver_str, size_t ver_len, FirmwareVersionImageType type)
{
    uint8_t *phdr;
    
    switch (type)
    {
        case FirmwareVersionImage_BOOT1:
            return _get_version_string(ver_str, ver_len, (uint8_t*)BT1_HDR);
            
        case FirmwareVersionImage_BOOT2:
            phdr = (uint8_t*)B2A_HDR;
            if ((phdr[HDR_SUCCESS] == FLAG_SET) && (phdr[HDR_USED] == FLAG_CLR))
            {
                return _get_version_string(ver_str, ver_len, (uint8_t*)B2A_HDR);
            }
            return _get_version_string(ver_str, ver_len, (uint8_t*)B2B_HDR);
            
        case FirmwareVersionImage_APP:
            return _get_version_string(ver_str, ver_len, (uint8_t*)APP_HDR);
    }
    
    return -1;
}
