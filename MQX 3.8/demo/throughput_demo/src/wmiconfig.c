//------------------------------------------------------------------------------
// Copyright (c) 2011 Qualcomm Atheros, Inc.
// All Rights Reserved.
// Qualcomm Atheros Confidential and Proprietary.
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is
// hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
// USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================
/*
* $FileName: wmiconfig.c$
* $Version : $
* $Date    : May-20-2011$
*
* Comments: Handles all Atheros defined wmiconfig command implementations.
*
*END************************************************************************/


#include "main.h"
#include <string.h>
#include <shell.h>
#include <stdlib.h>
#include "rtcs.h"
#include "ipcfg.h"
#include "atheros_wifi_api.h"
#include "atheros_wifi.h"
#include "throughput.h"
#include "atheros_stack_offload.h"


/**************************Globals ************************************************/
A_UINT8 pmk_flag = 0, hidden_flag = 0;
AP_CFG_CMD ap_config_param;
mode_t mode_flag = MODE_STATION;
char ssid[MAX_SSID_LENGTH];
static A_UINT8 original_ssid[MAX_SSID_LENGTH];               //Used to store user specified SSID
wps_context_t wps_context;
uint_16 channel_array[] =
{
	0,2412,2417,2422,2427,2432,2437,2442,2447,2452,2457,2462,2467,2472,2482
};
static int wpa_mode = 0;
cipher_t cipher;
	char wpa_ver[8];
key_t key[MAX_NUM_WEP_KEYS];
char wpsPin[MAX_WPS_PIN_SIZE];
char wpa_passphrase[MAX_PASSPHRASE_SIZE];
volatile A_UINT8 wifi_state = 0;

/************ Function Prototypes *********************************************/
uint_32 set_scan
    (
        uint_32 dev_num,
        char_ptr ssid,
        uint_32 *pNum_ap /* [OUT] */
    );

void resetTarget();
void init_wep_keys();
int_32 clear_wep_keys();
static int_32 connect_handler( int_32 index, int_32 argc, char_ptr argv[]);
static int_32 disconnect_handler( );
static int_32 set_tx_power(char_ptr dBm);
static int_32 set_channel(char_ptr channel);
static int_32 set_pmparams(int_32 index, int_32 argc, char_ptr argv[]);
static int_32 set_listen_interval(char_ptr listen_int);
static int_32 set_host_mode(char_ptr host_mode);
static int_32 set_phy_mode(char_ptr phy_mode);
static int_32 set_power_mode(char_ptr pwr_mode);
static uint_32 set_cipher(uint_32 dev_num,char_ptr cipher);
static uint_32 get_rssi();
static uint_32 ext_scan();
static int_32 set_wep_key(char* key_index, char* key_val);
static int_32 set_callback();
void wifiCallback(int val);
static uint_32 set_sec_mode(uint_32 dev_num,char_ptr mode);
uint_32 get_phy_mode(uint_32 dev_num,char_ptr phy_mode);

#if ENABLE_AP_MODE
static int_32 ap_handler( int_32 index, int_32 argc, char_ptr argv[]);
static int_32 set_ap_params(A_UINT16 param_cmd, A_UINT8 *data_val);
#endif

uint_32 set_wep_keys
    (
        uint_32 dev_num,
        char_ptr wep_key1,
        char_ptr wep_key2,
        char_ptr wep_key3,
        char_ptr wep_key4,
        uint_32 key_len,
        uint_32 key_index
    );
static uint_32 get_tx_status( );
static int_32 set_passphrase(char* passphrase);
uint_32 dev_susp_enable();
uint_32 dev_susp_start(char_ptr susp_time);
uint_32 get_version();
static int_32 test_flash(void);
static int_32 ad_hoc_connect_handler( int_32 index, int_32 argc, char_ptr argv[]);
static int_32 wps_handler( int_32 index, int_32 argc, char_ptr argv[]);
uint_32 StartWPS(ATH_NETPARAMS *pNetparams, A_UINT8 use_pinmode, wps_scan_params* wpsScan_p );
static int_32 scan_control( int_32 argc, char_ptr argv[]);
static int_32 ath_assert_dump( int_32 argc, char_ptr argv[]);
static int_32 ath_reg_query( int_32 argc, char_ptr argv[]);
static int_32 ath_mem_query( int_32 argc, char_ptr argv[]);
unsigned char ascii_to_hex(char val);
static int_32 ath_driver_state( int_32 argc, char_ptr argv[]);
int handle_pmk(char* pmk);
static uint_32 wmi_set_scan(uint_32 dev_num);
int_32 wps_query(A_UINT8 block);
static int_32 test_promiscuous(int_32 argc, char_ptr argv[]);
static int_32 test_raw(int_32 argc, char_ptr argv[]);
static int_32 connect_loop(int_32 argc, char_ptr argv[]);
static int_32 allow_aggr(int_32 argc, char_ptr argv[]);
static int_32 get_power_mode(char_ptr pwr_mode);
static int_32 get_mac_addr(char_ptr mac_addr);
static int_32 get_channel_val(char_ptr channel_val);
static int_32 set_wpa( int_32 index, int_32 argc, char_ptr argv[]);
static int_32 set_wep( int_32 index, int_32 argc, char_ptr argv[]);
uint_32 get_last_error();
int_32 parse_ipv4_ad(unsigned long* ip_addr, unsigned* sbits, char* ip_addr_str);

int_32 sock_handle;


int Inet6Pton(char * src, void * dst);
int_32 tcp6_echo_test(int_32 argc, char_ptr argv[]);
int_32 tcp6_tx_test(int_32 argc, char_ptr argv[]);
int_32 ipconfig_query(int_32 argc, char_ptr argv[]);
int_32 ipconfig_static(int_32 argc, char_ptr argv[]);
int_32 ipconfig_dhcp(int_32 argc, char_ptr argv[]);
static A_INT32 get_reg_domain( );

int_32 tcp_rx_test(int_32 argc, char_ptr argv[]);
void host_reset();

int ishexdigit(char digit);
#if ENABLE_STACK_OFFLOAD
unsigned int hexnibble(char digit);
unsigned int atoh(char * buf);

extern int_32 inet_aton_offload(const char*  name, A_UINT32* ipaddr_ptr);

char * print_ip6(IP6_ADDR_T * addr, char * str);
#endif

/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : mystrtoul
* Returned Value : unsigned long
* Comments       : coverts string to unsigned long
*
*END------------------------------------------------------------------*/

static uint_32 mystrtoul(const char* arg, const char* endptr, int base)
{
	uint_32 res = 0;
	int i;

	if(arg){
		if(arg[0] == '0' && (arg[1] == 'x' || arg[1] == 'X')) arg+=2;

		i=0;
		while(arg[i] != '\0' && &arg[i] != endptr){
			if(arg[i] >= '0' && arg[i] <= '9'){
				res *= base;
				res += arg[i] - '0';
			}else if(arg[i] >= 'a' && arg[i] <= 'f' && base == 16){
				res *= base;
				res += arg[i] - 'a' + 10;
			}else if(arg[i] >= 'A' && arg[i] <= 'F' && base == 16){
				res *= base;
				res += arg[i] - 'A' + 10;
			}else{
				//error
				break;
			}

			i++;
		}

	}

	return res;
}

/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : mystrtomac
* Returned Value : NA
* Comments       : converts a string to a 6 byte MAC address.
*
*END------------------------------------------------------------------*/
static int mystrtomac(const char* arg, unsigned char* dst)
{
	int i=0;
	int j=0;
	int left_nibble = 1;
	char base_char;
	unsigned char base_val;

	if(dst == NULL || arg == NULL){
		return -1;
	}

	A_MEMZERO(dst, 6);

	do{
		if(arg[i] >= '0' && arg[i] <= '9'){
			base_char = '0';
			base_val = 0x00;
		}else if(arg[i] == ':'){
			base_char = ':';
		}else if(arg[i] >= 'a' && arg[i] <= 'f'){
			base_char = 'a';
			base_val = 0x0a;
		}else if(arg[i] >= 'A' && arg[i] <= 'F'){
			base_char = 'A';
			base_val = 0x0a;
		}else{
			return -1;//error
		}

		if(base_char != ':'){
			dst[j] |= (arg[i] - base_char + base_val)<<(4*left_nibble);
			left_nibble = (left_nibble)? 0:1;

			if(left_nibble){
				j++;

				if(j>5){
					break;
				}
			}
		}

		i++;
	}while(1);

	return 0;//success
}



/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : wmiconfig_handler
* Returned Value : ERROR code on error else success
* Comments       : 1st stop for all wmiconfig commands
*
*END------------------------------------------------------------------*/

int_32 wmiconfig_handler(int_32 argc, char_ptr argv[] )
{ /* Body */
    boolean                 shorthelp = FALSE;
    A_UINT8 					printhelp = 0;
    uint_32                 return_code = SHELL_EXIT_SUCCESS;
    uint_32                 enet_device = IPCFG_default_enet_device, index = 1;

   // print_usage = Shell_check_help_request (argc, argv, &shorthelp);

    //if (!print_usage)
    {
      /*  if (argc > index)
        {
            if (Shell_parse_number (argv[index], &enet_device))
            {
                index++;
            }
        }

        if (enet_device >= BSP_ENET_DEVICE_COUNT)
        {
            printf ("Wrong number of ethernet device (%d)!\n", enet_device);
            return_code = SHELL_EXIT_ERROR;
        }
        else*/
        {
            if (argc > index)
            {
                if (strcmp (argv[index], "--connect") == 0)
                {
                	if(argc == index+1)
                	{
                		printf("Incomplete parameters for connect command\n");
                		return SHELL_EXIT_ERROR;
                	}
                    return_code = connect_handler(index+1,argc,argv);
                }
                else if (strcmp (argv[index], "--adhoc") == 0)
                {
                	if(argc == index+1)
                	{
                		printf("Incomplete parameters for ad-hoc command\n");
                		return SHELL_EXIT_ERROR;
                	}
                	mode_flag = MODE_ADHOC;
                    return_code = ad_hoc_connect_handler(index+1,argc,argv);
                }
#if ENABLE_AP_MODE
                else if (strcmp (argv[index], "--mode") == 0)
                {
                	if(argc == index+1)
                	{
                		printf("Usage:: --mode <ap|station> [<hidden>] \n");
                		return SHELL_EXIT_ERROR;
                	}

                    iwcfg_set_essid (DEMOCFG_DEFAULT_DEVICE,"");

                    if(strcmp (argv[index+1], "ap") == 0)
                    {
						mode_flag = MODE_AP;
						if (strcmp(argv[index+2], "hidden") == 0)
						{
							hidden_flag = 1;
						}
					}
                    else if(strcmp (argv[index+1], "station") == 0)
                    {
						mode_flag = MODE_STATION;
                        if (strcmp(argv[index+2], "hidden") == 0)
                        {
                            printf("Usage:: --mode <ap|station> [<hidden>] \n");
                            return SHELL_EXIT_ERROR;
                        }
                     }
                     else
                     {
                          printf("Usage:: --mode <ap|station> [<hidden>] \n");
                          return SHELL_EXIT_ERROR;
                      }
					}
                else if (strcmp (argv[index], "--ap") == 0)
                {
                    if(argc == index+1)
                    {
                        printf ("usage:--ap [country <country code>]\n"
                                "      --ap [bconint<intvl>]\n"
                                "      --ap [inact <minutes>]> \n");
                        return SHELL_EXIT_ERROR;
                    }
                    return_code = ap_handler(index+1,argc,argv);
                }
#endif
                else if (strcmp (argv[index], "--wps") == 0)
                {
                    return_code = wps_handler(index+1,argc,argv);
                }
                else if (strcmp (argv[index], "--disc") == 0)
                {
                    return_code = disconnect_handler();
                }
                else if (strcmp (argv[index], "--settxpower") == 0)
                {
                    return_code = set_tx_power(argv[index+1]);
                }
                else if (strcmp (argv[index], "--pwrmode") == 0)
                {
                    return_code = set_power_mode(argv[index+1]);
                }
                else if (strcmp (argv[index], "--pmparams") == 0)
                {
                    return_code = set_pmparams(index+1,argc,argv);
                }
                else if (strcmp (argv[index], "--channel") == 0)
                {
                    return_code = set_channel(argv[index+1]);
                }
                else if (strcmp (argv[index], "--listen") == 0)
                {
                    return_code = set_listen_interval(argv[index+1]);
                }
                else if (strcmp (argv[index], "--wmode") == 0)
                {
                    return_code = set_phy_mode(argv[index+1]);
                }
                else if (strcmp (argv[index], "--sethostmode") == 0)
                {
                    return_code = set_host_mode(argv[index+1]);
                }
                else if (strcmp (argv[index], "--rssi") == 0)
                {
                    return_code = get_rssi();
                }
                else if (strcmp (argv[index], "--extscan") == 0)
                {
                	return_code = ext_scan();
                }
                else if (strcmp (argv[index], "--gettxstat") == 0)
                {
                    return_code = get_tx_status();
                }
                else if (strcmp (argv[index], "--wepkey") == 0)
                {
                	if((argc - index) > 2)
                    	return_code = set_wep_key(argv[index+1],argv[index+2]);
                	else
                	{
                		printf("Missing parameters\n\n");
		    			return SHELL_EXIT_ERROR;
                	}
                }
                else if (strcmp (argv[index], "--p") == 0)
                {
                	/*We expect one parameter with this command*/
                	if((argc - index) > 1)
                    	return_code = set_passphrase(argv[index+1]);
                	else
                	{
                		printf("Missing parameters\n\n");
		    			return SHELL_EXIT_ERROR;
                	}
                }
                else if (strcmp (argv[index], "--wpa") == 0)
			    {
				   return_code = set_wpa(index+1,argc,argv);
			    }
                else if (strcmp (argv[index], "--wep") == 0)
			    {
				   return_code = set_wep(index+1,argc,argv);
			    }
                else if (strcmp (argv[index], "--suspend") == 0)
                {
                	return_code = dev_susp_enable();
                }
                else if (strcmp (argv[index], "--suspstart") == 0)
                {
                	return_code = dev_susp_start(argv[index+1]);
                }
                else if (strcmp (argv[index], "--version") == 0)
                {
                	return_code = get_version();
                }
                else if (strcmp (argv[index], "--help") == 0)
                {
                 //   return_code = SHELL_EXIT_ERROR;
                 	printhelp = 1;
                }

                else if (strcmp (argv[index], "--testflash") == 0)
                {
                	return_code = test_flash();
                }
                else if (strcmp (argv[index], "--scanctrl") == 0)
                {
                	return_code = scan_control(argc-(index+1), &argv[index+1]);
                }
                else if (strcmp (argv[index], "--reg") == 0)
                {
                	return_code = ath_reg_query(argc-(index+1), &argv[index+1]);
                }
                else if(strcmp (argv[index], "--fwmem") == 0)
                {
                	return_code = ath_mem_query(argc-(index+1), &argv[index+1]);
                }
                else if (strcmp (argv[index], "--assdump") == 0)
                {
                	return_code = ath_assert_dump(argc-(index+1), &argv[index+1]);
                }
                else if (strcmp (argv[index], "--driver") == 0)
                {
                	return_code = ath_driver_state(argc-(index+1), &argv[index+1]);
                }
                else if (strcmp (argv[index], "--raw") == 0)
                {
                	return_code = test_raw(argc-(index+1), &argv[index+1]);
                }
                else if (strcmp (argv[index], "--promisc") == 0)
                {
                	return_code = test_promiscuous(argc-(index+1), &argv[index+1]);
                }
                else if (strcmp (argv[index], "--testconn") == 0)
                {
                	return_code = connect_loop(argc-(index+1), &argv[index+1]);
                }
                else if (strcmp (argv[index], "--allow_aggr") == 0)
                {
                	return_code = allow_aggr(argc-(index+1), &argv[index+1]);
                }
                 else if (strcmp (argv[index], "--reset") == 0)
                {
                	host_reset();
                }
                else if (strcmp (argv[index], "--regdomain") == 0)
                {
                	get_reg_domain();
                }
                else if (strcmp (argv[index], "--getlasterr") == 0)
                {
                	get_last_error();
                }
#if ENABLE_STACK_OFFLOAD

	        else if (strcmp (argv[index], "--ipconfig") == 0)
                {
                	return_code = ipconfig_query(argc,argv);
                }
		else if (strcmp (argv[index], "--ipstatic") == 0)
                {
                	return_code = ipconfig_static(argc,argv);
                }
		else if (strcmp (argv[index], "--ipdhcp") == 0)
                {
                	return_code = ipconfig_dhcp(argc,argv);
                }
                else if (strcmp (argv[index], "--udpecho") == 0)
                {
                	ath_udp_echo(argc,argv);
                }
#endif
                else
                {
                    printf ("Unknown wmiconfig command!\n");
                    return_code = SHELL_EXIT_ERROR;
                }
            }
            else if (argc == 1)
            {
                char data[32+1];
                uint_32 error;
                uint_16 channel_val = 0;
                error = iwcfg_get_essid (enet_device,(char_ptr)data);

                if (error != 0)
                {
					printf("ssid         =   %s\n","<none>");
				}
				else
				{
					printf("ssid         =   %s\n",data);
				}

                iwcfg_get_sectype (enet_device,(char_ptr)data);
                printf("security type=   %s\n",data);

                get_phy_mode(enet_device,(char_ptr)data);
                printf("Phy mode     =   %s\n",data);

                get_power_mode((char_ptr)data);
                printf("Power mode   =   %s\n",data);

                get_mac_addr((char_ptr)data);
                printf("mac addr     =   %x:%x:%x:%x:%x:%x \n", data[0], data[1], data[2], data[3], data[4], data[5]);

#if ENABLE_AP_MODE
                if(mode_flag == MODE_AP)
                {
                	printf("mode         =   softap \n");
				}
				else
#endif
                if(mode_flag == MODE_STATION)
                {
                	printf("mode         =   station \n");
				}
				else
				{
					printf("mode         =   adhoc \n");
				}
				if(wifi_connected_flag == 1)
				{
					get_channel_val((char *)&channel_val);
					channel_val -= ATH_IOCTL_FREQ_1;
					if((channel_val/5) == 14)
					{
						channel_val = 14;
					}
					else
					{
						channel_val = (channel_val/5) + 1;
					}
					printf("channel      =   %d \n", channel_val);
				}
                /*iwcfg_get_mode (enet_device,(char_ptr)data);
                printf("mode=%s\n",data);
                if (strcmp(data,"wep") == 0)
                {
                    uint_32 def_key_index;
                    iwcfg_get_wep_key(enet_device,(char_ptr)data,&def_key_index);
                    printf ("wep key=%s\n",data);
                    printf ("default key index=%d\n",def_key_index);
                }
                if (strcmp(data,"wpa") == 0)
                {
                    uchar p_phrase[65];
                    iwcfg_get_passphrase(enet_device,(char_ptr)p_phrase);
                    printf ("   passphrase=%s\n",p_phrase);
                }
                if (strcmp(data,"wpa2") == 0)
                {
                    uchar p_phrase[65];
                    iwcfg_get_passphrase(enet_device,(char_ptr)p_phrase);
                    printf ("   passphrase=%s\n",p_phrase);
                }*/
            }
        }
    }
    /*else
    {
        return_code = SHELL_EXIT_ERROR;
    }*/

    if ((return_code != SHELL_EXIT_SUCCESS))
    {
    	printf("To see a list of commands- wmiconfig --help\n");
    }

    if(printhelp)
    {
    	printhelp = 0;
        if (shorthelp)
        {
            printf ("%s [<device>] [<command>]\n", argv[0]);
        }
        else
        {
#if ENABLE_STACK_OFFLOAD
            printf ("IOT Stack Offload Feature Enabled\n");
#else
            printf ("IOT Stack Offload Feature Disabled\n");
#endif
            printf ("Usage: %s [<command>]\n", argv[0]);
            printf ("  Commands:\n");
            printf ("    --version        = Displays versions\n");
            printf ("    --reset          = reset host processor\n");
            printf ("    --suspend        = Enable device suspend mode(before connect command)\n");
            printf ("    --regdomain      = Get Regulatory Domain\n");
            printf ("    --getlasterr     = Prints last driver error \n");
            printf ("    --connect <ssid>\n");
            printf ("   \n ");
            printf ("   WPA Configuration\n ");
            printf ("      --p <passphrase>\n");
            printf ("      --wpa <ver> <ucipher> <mcipher>\n");
            printf ("      --connect <ssid> \n");
            printf ("   \n ");
            printf ("   WEP Configuration\n");
            printf ("      --wepkey <key_index> <key> \n");
            printf (" 	   --wep <def_keyix> \n");
            printf ("      --connect <ssid>\n");
            printf ("              where  <ssid>   :SSID of network\n");
            printf ("                     <ver>    :1-WPA, 2-RSN\n");
            printf ("                     <ucipher>:TKIP or CCMP\n");
            printf ("                     <mcipher>:TKIP or CCMP\n");
            printf ("                     <p>    :Passphrase for WPA\n");
            //printf ("                     <mode>   :<open> or <shared>\n");
            printf ("                     <key_index>: Entered WEP key index [1-4]\n");
            printf ("                     <def_keyix>: Default WEP key index [1-4]\n");
            printf ("                     <key>    :WEP key\n");
            printf ("                     *        :optional parameters\n");
            printf ("          Note: key can only be composed of Hex characters of length 10 or 26\n");
            printf ("   \n");
            printf ("    --adhoc <ssid> --channel <1-14>  = Connect in ad-hoc mode\n");
            printf ("    --adhoc <ssid> --wep  <def_keyix> --channel <1-14> = Connect in WEP ad-hoc mode\n");
            printf ("                                    where channel is optional and ranges from 1-14\n");
            printf ("   \n");
            printf ("    --wps <connect> <mode> <*pin> <*ssid> <*mac> <*channel>\n");
            printf ("              where  <connect> : 1 - Attempts to connect after wps\n");
            printf ("                               : 0 - No Attempts to connect after wps\n");
            printf ("                     <mode>    : <push> or <pin>\n");
            printf ("                     *         :optional parameters\n");
            printf ("   \n");
            printf ("    --disc            = Disconect from current AP\n");
            printf ("    --settxpower <>   = set transmit power\n");
            printf ("    --pwrmode    <>   = set power mode 1=Power save, 0= Max Perf \n");
            printf ("    --setchannel <>   = set channel hint\n");
            printf ("    --wmode      <>   = set mode <b,g,n>\n");
            printf ("    --listen     <>   = set listen interval\n");
            printf ("    --rssi       <>   = prints Link Quality (SNR) \n");
            printf ("    --gettxstat           = get current status of TX from driver\n");
            printf ("    --suspstart <time>  = Suspend device for specified time in milliseconds\n");
            printf ("    --pmparams        = Set Power management parameters\n");
            printf ("           --idle <time in ms>                        Idle period\n");
            printf ("           --np   < >                                 PS Poll Number\n");
            printf ("           --dp <1=Ignore 2=Normal 3=Stick 4=Auto>    DTIM Policy\n");
            printf ("           --txwp <1=wake 2=do not wake>              Tx Wakeup Policy\n");
            printf ("           --ntxw < >                                 Number of TX to Wakeup\n");
            printf ("           --psfp <1=send fail event 2=Ignore event>  PS Fail Event Policy\n");
            printf ("    --scanctrl [0|1] [0|1]       = Control firmware scan behavior. Specifically, disable/enable foreground OR background scanning.\n");
        	printf ("    --reg [op] [addr] [value] [mask] = read or write specified AR600X register.\n");
        	printf ("    --assdump = collects assert data from AR600X and prints it to stdout.\n");
        	printf ("    --driver [mode]     \n");
        	printf ("					mode = down - Sets the driver State 0 unloads the driver \n");
        	printf ("					mode = flushdown - Waits for TX clear before unloading the driver \n");
        	printf ("					       up   - Reloads the driver\n");
        	printf ("					       raw  - Reloads the driver in raw mode\n");
        	printf ("					       quad - Reloads the driver in quad spi flash mode\n");
        	printf ("					       rawq - Reloads the driver in raw + quad mode\n");
        	printf ("					       normal - Reloads the driver in normal mode\n");
        	printf ("					       pmu - Reloads the driver in pmu enabled mode\n");
        	printf ("    --allow_aggr <tx_tid_mask> <rx_tid_mask> Enables aggregation based on the provided bit mask where\n");
        	printf ("		    each bit represents a TID valid TID's are 0-7\n");
        	printf ("iwconfig scan <ssid*>  =scan channels, ssid is optional, if provided will scan only specified SSID\n");
#if ENABLE_AP_MODE
            printf ("    --mode [ap <hidden>|station]\n");
            printf ("    --ap [country <country code>]\n"
                    "    --ap [bconint<intvl>]\n"
                    "    --ap [inact <minutes>]> \n");
#endif

#if ENABLE_STACK_OFFLOAD
                printf ("    --ipconfig                                     = show IP parameters\n");
                printf ("    --ipstatic <ip addr> <subnet mask> <Gateway>   = set static IP parameters\n");
                printf ("    --ipdhcp                                       = Run DHCP client\n");
                printf ("    --udpecho <port>                               = run UDP echo server\n");
#endif
        }
    }

    return return_code;
} /* Endbody */



/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : handle_pmk
* Returned Value : SHELL_EXIT_ERROR on error else SHELL_EXIT_SUCCESS
* Comments       :
*
*END------------------------------------------------------------------*/

int handle_pmk(char* pmk)
{
	int j;
	A_UINT8 val = 0;
	A_UINT8 pmk_hex[32];
	ATH_IOCTL_PARAM_STRUCT  inout_param;
    uint_32 error,dev_status;


	memset(pmk_hex, 0, 32);

	for(j=0;j<64;j++)
	{
		val = ascii_to_hex(pmk[j]);
		if(val == 0xff)
		{
			printf("Invalid character\n");
			return SHELL_EXIT_ERROR;
		}
		else
		{
			if((j&1) == 0)
				val <<= 4;

			pmk_hex[j>>1] |= val;
		}
	}

	error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (ENET_OK != error || dev_status == FALSE)
    {
        return SHELL_EXIT_ERROR;
    }

	inout_param.cmd_id = ATH_SET_PMK;
    inout_param.data = pmk_hex;
    inout_param.length = 32;
	error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }

    return SHELL_EXIT_SUCCESS;
}





/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : set_passphrase
* Returned Value : SHELL_EXIT_SUCCESS
* Comments       : Store WPA Passphrase for later use
*
*END------------------------------------------------------------------*/
static int_32 set_passphrase(char* passphrase)
{
	strcpy(wpa_passphrase,passphrase);
	return SHELL_EXIT_SUCCESS;
}




/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : set_wep_key
* Returned Value : SHELL_EXIT_ERROR on error else SHELL_EXIT_SUCCESS
* Comments       : Store WEP key for later use. Size of Key must be 10 oe 26
*                  Hex characters
*
*END------------------------------------------------------------------*/
static int_32 set_wep_key(char* key_index, char* key_val)
{
	int key_idx = atoi(key_index), i;

	if(strlen(key_val) != MIN_WEP_KEY_SIZE && strlen(key_val) != MAX_WEP_KEY_SIZE &&
	    strlen(key_val) != (MIN_WEP_KEY_SIZE/2) && strlen(key_val) != (MAX_WEP_KEY_SIZE/2))
	{
		printf("Invalid WEP Key length, only 10 or 26 HEX characters allowed (or) 5 or 13 ascii keys allowed\n\n");
		return SHELL_EXIT_ERROR;
	}

	if(key_idx < 1 || key_idx > 4)
    {
    	printf("Invalid key index, Please enter between 1-4\n\n");
    	return SHELL_EXIT_ERROR;
    }

	if((strlen(key_val) == MIN_WEP_KEY_SIZE) || (strlen(key_val) == MAX_WEP_KEY_SIZE))
	{
		for (i = 0; i < strlen(key_val); i++)
		{
			if(ishexdigit(key_val[i]))
			{
				continue;
			}
			else
			{
    			printf("for hex enter [0-9] or [A-F]\n\n");
    			return SHELL_EXIT_ERROR;
			}
		}
	}

	key[key_idx-1].key_index = 1;		//set flag to indicate occupancy

	memcpy(key[key_idx-1].key,key_val,strlen(key_val));

	return SHELL_EXIT_SUCCESS;
}


/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : init_wep_keys
* Returned Value : N/A
* Comments       : Init wep keys
*
*END------------------------------------------------------------------*/

/*void init_wep_keys()
{
	int i;
	for(i=0;i<4;i++)
	{
		key[i].key[0] = '\0';
	}
}*/


/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : clear_wep_keys
* Returned Value : Success
* Comments       : Clear wep keys
*
*END------------------------------------------------------------------*/
int_32 clear_wep_keys()
{
	int i;
	for(i=0;i<4;i++)
	{
		key[i].key_index = 0;

		key[i].key[0] = '\0';
	}
        return SHELL_EXIT_SUCCESS;
}




/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : wifiCallback
* Returned Value : N/A
* Comments       : Called from driver on a WiFI connection event
*
*END------------------------------------------------------------------*/
void wifiCallback(int val)
{

	wifi_state = val;
	if(val == 1)
	{
		wifi_connected_flag = 1;
		printf("Connected\n");
	}
	else
	{
		//wifi_connected_flag = 0;
		printf("Not Connected\n");
	}
}


/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : set_callback
* Returned Value : SHELL_EXIT_ERROR on error else SHELL_EXIT_SUCCESS
* Comments       : Sets callback function for WiFi connect/disconnect event
*
*END------------------------------------------------------------------*/
static int_32 set_callback()
{
     ATH_IOCTL_PARAM_STRUCT  inout_param;
    uint_32 error,dev_status;

    inout_param.cmd_id = ATH_SET_CONNECT_STATE_CALLBACK;
    inout_param.data = (void*)wifiCallback;
    inout_param.length = 4;
    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    return SHELL_EXIT_SUCCESS;
}




/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : connect_handler()
* Returned Value  : SHELL_EXIT_SUCCESS - successful completion or
*					SHELL_EXIT_ERROR - failed.
* Comments		  : Handles Connect commands for infrastructure mode, Open
*                   WEP,WPA/WPA2 security is supported
*
*END*-----------------------------------------------------------------*/

static int_32 connect_handler( int_32 index, int_32 argc, char_ptr argv[])
{
    uint_32 error= SHELL_EXIT_SUCCESS, i;
    //cipher_t cipher;
    //char ver[8];
    int key_idx = 0;
    char ssid_str[64];
    int offset = 0, ssidLength=MAX_SSID_LENGTH ;

    set_callback();

#if ENABLE_AP_MODE
	if (mode_flag == MODE_AP)
	{
		printf("setting to ap mode \n");
    	iwcfg_set_mode (DEMOCFG_DEFAULT_DEVICE,"softap");
    	if (hidden_flag == 1)
    	{
			hidden_flag = 1;
			ap_config_param.u.wmi_ap_hidden_ssid_cmd.hidden_ssid = 1;
			if (set_ap_params(AP_SUB_CMD_HIDDEN_FLAG, (A_UINT8 *)&ap_config_param.u.wmi_ap_hidden_ssid_cmd.hidden_ssid) != SHELL_EXIT_SUCCESS)
			{
				printf("not able to set hidden mode for AP \n");
				return SHELL_EXIT_ERROR;
			}
		}
	}
	else
#endif
	{
        iwcfg_set_mode (DEMOCFG_DEFAULT_DEVICE,"managed");
	}

    if(argc > 2)
    {
    	for(i=index;i<argc;i++)
    	{
    		if(strlen(argv[i]) > ssidLength)
			{
				printf("SSID length exceeds Maximum value\n");
				return SHELL_EXIT_ERROR;
			}
    		strcpy(&ssid_str[offset], argv[i]);
    		offset += strlen(argv[i]);
    		ssidLength -= strlen(argv[i]);

    		/*Add empty space in ssid except for last word*/
    		if(i != argc-1)
    		{
    			strcpy(&ssid_str[offset], " ");
    			offset += 1;
    			ssidLength -= 1;
    		}
    	}
    }
    else
    {
    	printf("Missing ssid\n");
    	return SHELL_EXIT_ERROR;
    }

    error = iwcfg_set_essid (DEMOCFG_DEFAULT_DEVICE,ssid_str/*argv[index]*/);
    if (error != 0)
    {
        printf ("Error during setting of ssid %s error=%08x!\n",argv[index], error);
        return SHELL_EXIT_ERROR;
    }

    printf ("Setting SSID to %s \n\n",ssid_str/*argv[index]*/);
    strcpy((char*)original_ssid,ssid_str /*argv[index]*/);

	if(wpa_mode)
	{
		error = set_cipher(DEMOCFG_DEFAULT_DEVICE,(char *)&cipher);
		if(error != SHELL_EXIT_SUCCESS)
		{
			return SHELL_EXIT_ERROR;
		}
		error = iwcfg_set_sec_type (DEMOCFG_DEFAULT_DEVICE,wpa_ver);
		if(error != SHELL_EXIT_SUCCESS)
		{
			return SHELL_EXIT_ERROR;
		}
		i++;
		if(strlen(wpa_passphrase) != 64)
		{
			error = iwcfg_set_passphrase (DEMOCFG_DEFAULT_DEVICE,wpa_passphrase);
			if(error != SHELL_EXIT_SUCCESS)
			{
				return SHELL_EXIT_ERROR;
			}
		}
		else
		{
			if(handle_pmk(wpa_passphrase)!= SHELL_EXIT_SUCCESS)
			{
				printf("Unable to set pmk\n");
				return SHELL_EXIT_ERROR;
			}
		}
		wpa_mode = 0;
	}

    error = iwcfg_commit(DEMOCFG_DEFAULT_DEVICE);
    if(error != SHELL_EXIT_SUCCESS)
    {
#if ENABLE_AP_MODE
		if(mode_flag == MODE_AP)
		{
			printf("AP supports only AES and OPEN mode \n");
		}
#endif
    	return SHELL_EXIT_ERROR;
    }

#if ENABLE_AP_MODE
	if(mode_flag == MODE_AP)
	{
		/* setting the AP's default IP to 192.168.1.1 */
#if ENABLE_STACK_OFFLOAD
		A_UINT32 ip_addr = 0xc0a80101,mask = 0xffffff00,gateway = 0xc0a80101;
		t_ipconfig((void*)whistle_handle,1, &ip_addr, &mask, &gateway);
#else

#define AP_IPADDR  IPADDR(192,168,1,1)
#define AP_IPMASK  IPADDR(255,255,255,0)
#define AP_IPGATEWAY  IPADDR(192,168,1,1)

                A_UINT32 error;
                IPCFG_IP_ADDRESS_DATA   ip_data;

                ip_data.ip = AP_IPADDR;
                ip_data.mask = AP_IPMASK;
                ip_data.gateway = AP_IPGATEWAY;

                error = ipcfg_bind_staticip (DEMOCFG_DEFAULT_DEVICE, &ip_data);
                if (error != 0)
                {
                    printf ("Error during static ip bind %08x!\n", error);
                    return SHELL_EXIT_ERROR;
                }
#endif
	}
#endif

    return SHELL_EXIT_SUCCESS;
}



/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : set_wpa()
* Returned Value  : SHELL_EXIT_SUCCESS - successful completion or
*		    SHELL_EXIT_ERROR - failed.
* Comments	: Sets WPA mode
*
*END*-----------------------------------------------------------------*/
static int_32 set_wpa( int_32 index, int_32 argc, char_ptr argv[])
{

	int i = index;

	uint_32 error= SHELL_EXIT_SUCCESS;

	if(5 != argc)  //User did not enter parameter
	{
		printf("Missing parameters\n\n");
		return SHELL_EXIT_ERROR;
	}

	/********** version ****************/
	if(strcmp(argv[i],"1")==0)
	{
		strcpy(wpa_ver,"wpa");
		//this is a hack we currently only support wpa2 AES in AP mode
		if (mode_flag == MODE_AP)
		{
			strcpy(wpa_ver,"wpa2");
		}
	}
	else if(strcmp(argv[i],"2")==0)
	{
		strcpy(wpa_ver,"wpa2");
	}
	else
	{
		printf("Invalid version\n");
		return SHELL_EXIT_ERROR;
	}

	/**************** ucipher **********/
	i++;
	if(strcmp(argv[i],"TKIP")==0)
	{
		cipher.ucipher = 0x04;
	}
	else if(strcmp(argv[i],"CCMP")==0)
	{
		cipher.ucipher = 0x08;
	}
	else
	{
		printf("Invalid ucipher\n");
		return SHELL_EXIT_ERROR;
	}


	/*********** mcipher ************/
	i++;
	if(strcmp(argv[i],"TKIP")==0)
	{
		cipher.mcipher = 0x04;
	}
	else if(strcmp(argv[i],"CCMP")==0)
	{
		cipher.mcipher = 0x08;
	}
	else
	{
		printf("Invalid mcipher\n");
		return SHELL_EXIT_ERROR;
	}
    wpa_mode = 1;
    return error;
}


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : set_wep()
* Returned Value  : SHELL_EXIT_SUCCESS - successful completion or
*		    SHELL_EXIT_ERROR - failed.
* Comments	: Sets WEP parameters
*
*END*-----------------------------------------------------------------*/
static int_32 set_wep( int_32 index, int_32 argc, char_ptr argv[])
{
	int i = index;
	int key_idx = 0;
	char* key0,*key1,*key2,*key3;
	uint_32 error= SHELL_EXIT_SUCCESS;

	if( argc < 3)  //User did not enter parameter
	{
		printf("Missing parameters\n");
		return SHELL_EXIT_ERROR;
	}

#if 0
	if(strcmp(argv[i],"open")==0)
	{
		strcpy(ver,"open");
	}
	else if(strcmp(argv[i],"shared")==0)
	{
		strcpy(ver,"shared");
	}
	else
	{
		printf("Invalid mode\n");
		return SHELL_EXIT_ERROR;
	}
#endif

	//i++;
	key_idx = atoi(argv[i]);

	if(key_idx < 1 || key_idx > 4)
	{
		printf("Invalid default key index, Please enter between 1-4\n");
		return SHELL_EXIT_ERROR;
	}
	else
	{

	}

	if(key[key_idx-1].key_index != 1)
	{
		printf("This key is not present, please enter WEP key first\n");
		return SHELL_EXIT_ERROR;
	}

	/*Set wep mode - Open or Shared*/
	//error = set_sec_mode(DEMOCFG_DEFAULT_DEVICE,ver);
	//if(error != SHELL_EXIT_SUCCESS)
	//{
		//return SHELL_EXIT_ERROR;
	//}

	error = iwcfg_set_sec_type (DEMOCFG_DEFAULT_DEVICE,"wep");
	if(error != SHELL_EXIT_SUCCESS)
	{
		return SHELL_EXIT_ERROR;
	}

	if(key[0].key[0] == '\0')
		key0 = NULL;
	else
		key0 = key[0].key;

	if(key[1].key[0] == '\0')
		key1 = NULL;
	else
		key1 = key[1].key;

	if(key[2].key[0] == '\0')
		key2 = NULL;
	else
		key2 = key[2].key;

	if(key[3].key[0] == '\0')
		key3 = NULL;
	else
		key3 = key[3].key;

	error = set_wep_keys (DEMOCFG_DEFAULT_DEVICE,
						key0,
						key1,//argv[i],
						key2,//argv[i],
						key3,//argv[i],
						strlen(key[key_idx-1].key),
						key_idx);
        clear_wep_keys();

        return error;
}


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : ad_hoc_connect_handler()
* Returned Value  : 1 - successful completion or
*					0 - failed.
* Comments		  : Handles Connect commands for ad-hoc mode, Open
*                   & WEP security is supported
*
*END*-----------------------------------------------------------------*/

static int_32 ad_hoc_connect_handler( int_32 index, int_32 argc, char_ptr argv[])
{
	uint_32 error= SHELL_EXIT_SUCCESS,i;
    int key_idx = 0;
    char* key0,*key1,*key2,*key3;

    set_callback();
    iwcfg_set_mode (DEMOCFG_DEFAULT_DEVICE,"adhoc");
    error = iwcfg_set_essid (DEMOCFG_DEFAULT_DEVICE,argv[index]);
    if (error != 0)
    {
        printf ("Error during setting of ssid %s error=%08x!\n",argv[index], error);
        return SHELL_EXIT_ERROR;
    }

    printf ("Setting SSID to %s \n\n",argv[index]);
	strcpy((char*)original_ssid,argv[index]);

	if(index+1 < argc)
	{
		i = index +1;
		if(strcmp (argv[i],"--wep") == 0)
		{
		    if( argc < 4)  //User did not enter parameter
		    {
		    	printf("Missing parameters\n");
		    	return SHELL_EXIT_ERROR;
		    }

		    i++;
		    key_idx = atoi(argv[i]);

		    if(key_idx < 1 || key_idx > 4)
		    {
		    	printf("Invalid default key index, Please enter between 1-4\n");
		    	return SHELL_EXIT_ERROR;
		    }
		    else
		    {

		    }

		    if(key[key_idx-1].key_index != 1)
		    {
		    	printf("This key is not present, please enter WEP key first\n");
		    	return SHELL_EXIT_ERROR;
		    }

		    error = iwcfg_set_sec_type (DEMOCFG_DEFAULT_DEVICE,"wep");
		    if(error != SHELL_EXIT_SUCCESS)
		    {
		    	return SHELL_EXIT_ERROR;
		    }

		    if(key[0].key[0] == '\0')
		        key0 = NULL;
		    else
		    	key0 = key[0].key;

		    if(key[1].key[0] == '\0')
		        key1 = NULL;
		    else
		    	key1 = key[1].key;

		    if(key[2].key[0] == '\0')
		        key2 = NULL;
		    else
		    	key2 = key[2].key;

		    if(key[3].key[0] == '\0')
		        key3 = NULL;
		    else
		    	key3 = key[3].key;

		    error = set_wep_keys (DEMOCFG_DEFAULT_DEVICE,
	  							key0,
						      	key1,//argv[i],
						      	key2,//argv[i],
						      	key3,//argv[i],
						      	strlen(key[key_idx-1].key),
						      	key_idx);
			if(error != SHELL_EXIT_SUCCESS)
		    {
		    	return SHELL_EXIT_ERROR;
		    }
		    clear_wep_keys();

		    if(argc > 5)
            {
              i++;
              if(strcmp (argv[i],"--channel") == 0)
              {
                  if( argc < 7)  //User did not enter parameter
                  {
                      printf("Missing Channel parameter\n");
                      return A_ERROR;
                  }
                  i++;
                  printf("Setting Channel to %d\n",atoi(argv[i]));
                  if(set_channel(argv[i]) == A_ERROR)
                    return A_ERROR;
              }
            }
		}
		else if(strcmp (argv[i],"--channel") == 0)
		{
		    if( argc < 4)  //User did not enter parameter
			{
				printf("Missing Channel parameter\n");
				return A_ERROR;
			}
			i++;
		    printf("Setting Channel to %d\n",atoi(argv[i]));
		    if(set_channel(argv[i]) == A_ERROR)
		      return A_ERROR;
		}

	}
    error = iwcfg_commit(DEMOCFG_DEFAULT_DEVICE);
    if(error != SHELL_EXIT_SUCCESS)
    {
    	return SHELL_EXIT_ERROR;
    }

    return SHELL_EXIT_SUCCESS;
}



/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : StartWPS()
* Returned Value  : 1 - successful completion or
*					0 - failed.
* Comments		  : Attempts to start a WPS connection with a peer.
*					The caller must specify the type of WPS to start
*					push-button mode | pin mode.  if pin mode is used
*					the caller must specify the pin value to pass to
*					the Atheros driver.
*
*END*-----------------------------------------------------------------*/
uint_32 StartWPS(ATH_NETPARAMS *pNetparams, A_UINT8 use_pinmode, wps_scan_params* wpsScan_p )
{
#define WPS_STANDARD_TIMEOUT (30)
	uint_32 error,dev_status;
    ATH_IOCTL_PARAM_STRUCT param;
    uint_32 status = 0;
    ATH_WPS_START wps_start;

    do{
        error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
        if (ENET_OK != error || dev_status == FALSE)
        {
            break;
        }

        param.cmd_id = ATH_START_WPS;
        param.data = &wps_start;
        param.length = sizeof(wps_start);

        if(use_pinmode){
        	wps_start.wps_mode = ATH_WPS_MODE_PIN;
        	//FIXME: This hardcoded pin value needs to be changed
        	// for production to reflect what is on a sticker/label
        	memcpy(wps_start.pin, wpsPin,ATH_WPS_PIN_LEN);
        }else{
        	wps_start.wps_mode = ATH_WPS_MODE_PUSHBUTTON;
        }

        wps_start.timeout_seconds = WPS_STANDARD_TIMEOUT;

        if(wpsScan_p == NULL)
        {
        	wps_start.ssid_info.ssid_len = 0;
        }
        else
        {
        	memcpy(wps_start.ssid_info.ssid,wpsScan_p->ssid,sizeof(wpsScan_p->ssid));
        	memcpy(wps_start.ssid_info.macaddress,wpsScan_p->macaddress,6);
        	wps_start.ssid_info.channel = channel_array[wpsScan_p->channel];
        	wps_start.ssid_info.ssid_len = strlen((char *)wpsScan_p->ssid);
        }

        /* this starts WPS on the Aheros wifi */
        error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&param);
        if (ENET_OK != error)
        {
            break;
        }

        status = 1;
    }while(0);

    return status;
}

static uint_32 CompleteWPS(ATH_NETPARAMS *pNetparams, A_UINT8 block)
{
	uint_32 status = 0;
	ATH_IOCTL_PARAM_STRUCT param;
	uint_32 error;

	pNetparams->error = 0;

    pNetparams->dont_block = (block)? 0:1;

    param.cmd_id = ATH_AWAIT_WPS_COMPLETION;
    param.data = pNetparams;
    param.length = sizeof(*pNetparams);

    /* this will block until the WPS operation completes or times out */
    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&param);

    do{
	    if (ENETERR_INPROGRESS == error)
	    {
	        break;
	    }

	    status =1;

	    if(ENET_OK != error){
	    	printf("ATH_AWAIT_WPS_COMPLETION failed!\n");
	    	break;
	    }

	    if(pNetparams->error !=0)
	    {
	        switch (pNetparams->error)
	        {

	        	case ATH_WPS_ERROR_INVALID_START_INFO:

	        	printf("WPS error: invalid start info\n");
	        	break;
	        	case ATH_WPS_ERROR_MULTIPLE_PBC_SESSIONS:
	        	printf("WPS error: Multiple PBC Sessions\n");
	        	break;

	        	case ATH_WPS_ERROR_WALKTIMER_TIMEOUT:
	        	printf("WPS error: Walktimer Timeout\n");
	        	break;

	        	case ATH_WPS_ERROR_M2D_RCVD:
	        	printf("WPS error: M2D RCVD\n");
	        	break;

	        	default:
	        	printf("WPS error: unknown %d\n",pNetparams->error);
	        	break;
	        }
	    }
    }while(0);

    return status;
}



/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : ascii_to_hex()
* Returned Value  : hex counterpart for ascii
* Comments	: Converts ascii character to correesponding hex value
*
*END*-----------------------------------------------------------------*/
static unsigned char ascii_to_hex(char val)
{
	if('0' <= val && '9' >= val){
		return (unsigned char)(val - '0');
	}else if('a' <= val && 'f' >= val){
		return (unsigned char)(val - 'a') + 0x0a;
	}else if('A' <= val && 'F' >= val){
		return (unsigned char)(val - 'A') + 0x0a;
	}

	return 0xff;/* error */
}



/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : NetConnect()
* Returned Value  : 0 - success, 1 - failure
* Comments	: Handles connection to AP after WPS
*
*END*-----------------------------------------------------------------*/
static int_32 NetConnect(ATH_NETPARAMS *pNetparams)
{
	int_32 status = 1;
	ATH_IOCTL_PARAM_STRUCT ath_param;
	char ver[8];
	do{
		if(pNetparams->ssid_len == 0)
		{
		//	printf("WPS failed\n");
			break;
		}
		else
		{

			printf("SSID received %s\n",pNetparams->ssid);
			if(pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WPA2)
			{
		    	printf("Security type is WPA2\n");
				printf("Passphrase %s\n",(char_ptr)pNetparams->u.passphrase);
			}
			else if(pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WPA)
		    {
		    	printf("Security type is WPA\n");
				printf("Passphrase %s\n",(char_ptr)pNetparams->u.passphrase);
			}
			else if(pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WEP)
		    {
		    	printf("Security type is WEP\n");
				printf("WEP key %s\n",(char_ptr)pNetparams->u.wepkey);
				printf("Key index %d\n",pNetparams->key_index);
			}
			else if(pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_NONE)
		    {
		    	printf("Security type is None\n");
		    }


	    	set_callback();

	    	iwcfg_set_essid (IPCFG_default_enet_device, (char*)pNetparams->ssid);

	    	strcpy((char*)original_ssid,(char*)pNetparams->ssid);

			if(pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WPA2){

		        ath_param.cmd_id = ATH_SET_CIPHER;
		    	ath_param.data = &pNetparams->cipher;
		    	ath_param.length = 8;

		        if(ENET_OK != ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&ath_param)){
		        	break;
		        }
		        strcpy(ver,"wpa2");
		        iwcfg_set_sec_type (IPCFG_default_enet_device,ver);

		        if(strlen((char_ptr)pNetparams->u.passphrase) != 64)
				{
					iwcfg_set_passphrase (IPCFG_default_enet_device,(char_ptr)pNetparams->u.passphrase);
				}
				else
				{
					if(SHELL_EXIT_SUCCESS != handle_pmk((char_ptr)pNetparams->u.passphrase))
						break;
				}
			}else if (pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WPA){



		        ath_param.cmd_id = ATH_SET_CIPHER;
		    	ath_param.data = &pNetparams->cipher;
		    	ath_param.length = 8;

		        if(ENET_OK != ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&ath_param)){
		        	break;
		        }
		        strcpy(ver,"wpa");
		        iwcfg_set_sec_type (IPCFG_default_enet_device,ver);
		        if(strlen((char_ptr)pNetparams->u.passphrase) != 64)
				{
					iwcfg_set_passphrase (IPCFG_default_enet_device,(char_ptr)pNetparams->u.passphrase);
				}
				else
				{
					if(SHELL_EXIT_SUCCESS != handle_pmk((char_ptr)pNetparams->u.passphrase))
						break;
				}
			}else if (pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WEP){

				iwcfg_set_sec_type (IPCFG_default_enet_device,"wep");
				set_wep_keys (IPCFG_default_enet_device,(char_ptr)pNetparams->u.wepkey,
		      	NULL,NULL,NULL,strlen((char_ptr)pNetparams->u.wepkey),pNetparams->key_index);

			}else if (pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_NONE){
			//	iwcfg_set_sec_type (IPCFG_default_enet_device,"none");
			}

			//iwcfg_set_mode (IPCFG_default_enet_device,"managed");
		    iwcfg_commit(IPCFG_default_enet_device);
		    status = 0;

		}

	}while(0);

	return status;
}


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : wps_query()
* Returned Value  : 0 - success, 1 - failure
* Comments	: Queries WPS status
*
*END*-----------------------------------------------------------------*/
int_32 wps_query(A_UINT8 block)
{
	ATH_NETPARAMS netparams;
	int_32 status = 1;

	if(wps_context.wps_in_progress){
		if(CompleteWPS(&netparams, block)){
			status = 0;
			wps_context.wps_in_progress = 0;

			if(wps_context.connect_flag){
				if(NetConnect(&netparams)){
					printf("connection failed\n");
				}
			}
		}
	}

	return status;
}

static int_32 connect_loop(int_32 argc, char_ptr argv[])
{
	int i=0;
	int count;
	uint_32 bad_count,good_count;
	char_ptr my_argv[7];
	my_argv[0]="wmiconfig";
	my_argv[1]="--connect";
	my_argv[2]="atheros_demo";
	my_argv[3]="--wpa";
	my_argv[4]="2";
	my_argv[5]="CCMP";
	my_argv[6]="CCMP";

	count = atoi(argv[0]);

	printf("connect loop start count= %d passphrase= %s\n", count, argv[1]);

	bad_count=good_count=0;

	for(i=0;i<count;i++){
		my_argv[0]="wmiconfig";
		my_argv[1]="--connect";

		disconnect_handler();
		printf("wait for disconnect\n");

		while(wifi_state){};

		/* clear driver */
		my_argv[0]="0";
		ath_driver_state(1, my_argv);
		_time_delay(10);
		/* re-init driver */
		my_argv[0]="1";
		ath_driver_state(1, my_argv);

		printf("set passphrase\n");
		set_passphrase(argv[1]);
		printf("call connect\n");
		connect_handler(2, 7, my_argv);

		while(wifi_state==0){};

#if 0
		// CA - Sh_ping seems to call RTCS_ping. TBD is to use wmi_ping
		if(wifi_state){
			_time_delay(50);
			my_argv[0]="ping";
			my_argv[1]="192.168.1.2";

			Shell_ping(2, my_argv );
			good_count++;
		}else{
			bad_count++;
		}
#endif

	}


	printf("results: bad = %d good = %d\n", bad_count, good_count);
	return SHELL_EXIT_SUCCESS;
}

static int_32 allow_aggr(int_32 argc, char_ptr argv[])
{

	ATH_SET_AGGREGATION_PARAM param;
	ATH_IOCTL_PARAM_STRUCT ath_param;

	do{
		if(argc != 2){
			break;
		}

		param.txTIDMask = mystrtoul(argv[0], NULL, 16);
		param.rxTIDMask = mystrtoul(argv[1], NULL, 16);

		if(param.txTIDMask > 0xff || param.rxTIDMask > 0xff){
			break;
		}

		ath_param.cmd_id = ATH_SET_AGGREGATION;
		ath_param.data = &param;
		ath_param.length = sizeof(ATH_SET_AGGREGATION_PARAM);

	    if(ENET_OK != ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&ath_param)){
	    	printf("ERROR: driver command failed\n");
	    }

    	return SHELL_EXIT_SUCCESS;
    }while(0);

   	printf ("wmiconfig --allow_aggr <tx_tid_mask> <rx_tid_mask> Enables aggregation based on the provided bit mask where\n");
    printf ("each bit represents a TID valid TID's are 0-7\n");

    return SHELL_EXIT_SUCCESS;
}

void application_frame_cb(A_VOID *ptr);
void application_frame_cb(A_VOID *ptr)
{
	PCB_PTR frame = (PCB_PTR)ptr;
	A_UINT16 i,print_length,j=0;

	print_length = 32;
	printf("frame (%d):\n", frame->FRAG[0].LENGTH);
	/* only print the first 64 bytes of each frame */
	if(frame->FRAG[0].LENGTH < print_length)
		print_length = frame->FRAG[0].LENGTH;

	for(i=0 ; i<print_length ; i++){
		printf("0x%02x, ", frame->FRAG[0].FRAGMENT[i]);
		if(j++==7){
			j=0;
			printf("\n");
		}
	}

	if(j){
		printf("\n");
	}

	frame->FREE(frame);

}


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : test_promiscuous()
* Returned Value  : SHELL_EXIT_SUCCESS - success
* Comments	: Tests promiscuous mode
*
*END*-----------------------------------------------------------------*/
static int_32 test_promiscuous(int_32 argc, char_ptr argv[])
{
	ATH_IOCTL_PARAM_STRUCT param;
	ATH_PROMISCUOUS_MODE prom_mode_details;
	param.cmd_id = ATH_SET_PROMISCUOUS_MODE;
    param.data = &prom_mode_details;
    param.length = sizeof(prom_mode_details);

    A_MEMZERO(&prom_mode_details, sizeof(ATH_PROMISCUOUS_MODE));

   	prom_mode_details.enable = atoi(argv[0]);

   	if(argc > 1){
	   	prom_mode_details.filter_flags = atoi(argv[1]);

	   	if(argc > 2){
		   	if(mystrtomac(argv[2], &(prom_mode_details.src_mac[0])))
		   	{
		   		printf("ERROR: MAC address translation failed.\n");
		   		return SHELL_EXIT_SUCCESS;
		   	}

		   	if(argc > 3){
			   	if(mystrtomac(argv[3], &(prom_mode_details.dst_mac[0])))
			   	{
			   		printf("ERROR: MAC address translation failed.\n");
			   		return SHELL_EXIT_SUCCESS;
			   	}
			}
		}
	}

	prom_mode_details.cb = application_frame_cb;
    /* this starts WPS on the Aheros wifi */
    if (ENET_OK != ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&param))
    {
        printf("ERROR: promiscuous command failed\n");
    }

    return SHELL_EXIT_SUCCESS;
}

/* test_raw -- makes use of special API's to send a raw MAC frame irregardless of connection state
 */
static int_32 test_raw(int_32 argc, char_ptr argv[])
{
	A_UINT8 rate_index = 0;
	A_UINT8 access_cat;
	A_UINT8 tries = 1;
	A_UINT8 header_type;
	uint_32 i,error,dev_status;
	uint_32 chan, size;
    ATH_IOCTL_PARAM_STRUCT param;
    uint_32 status = 0;
    ATH_MAC_TX_RAW_S details;
    A_UINT8 *buffer = NULL;
    typedef struct{
    	A_UINT8 fc[2];
    	A_UINT8 duration[2];
    	A_UINT8 addr[3][6];
    	A_UINT8 seq[2];
    }WIFI_HDR;

    typedef struct{
    	A_UINT8 fc[2];
    	A_UINT8 duration[2];
    	A_UINT8 addr[3][6];
    	A_UINT8 seq[2];
    	A_UINT8 addr_3[6];
    	A_UINT8 pad[2]; // pad required by firmware
    }WIFI_4_HDR;

    typedef struct{
    	A_UINT8 fc[2];
    	A_UINT8 duration[2];
    	A_UINT8 addr[3][6];
    	A_UINT8 seq[2];
    	A_UINT8 qos[2];
    	A_UINT8 pad[2]; // pad required by firmware
    }WIFI_QOS_HDR;
#define WIFI_HDR_SZ (24)
#define WIFI_4_HDR_SZ (32)
#define WIFI_QOS_HDR_SZ (28)
	A_UINT8 header_sz[3]={WIFI_HDR_SZ, WIFI_QOS_HDR_SZ, WIFI_4_HDR_SZ};
	WIFI_4_HDR* p4Hdr;
    WIFI_HDR* pHdr;
    WIFI_QOS_HDR *pQosHdr;


    do{
    	/* collect user inputs if any */

    	if(argc != 5){
    		goto RAW_USAGE;
    	}


    	rate_index = atoi(argv[0]);
    	tries = atoi(argv[1]);
    	size = atoi(argv[2]);
    	chan = atoi(argv[3]);
    	header_type = atoi(argv[4]);


    	if(rate_index > 19 ||
    		tries > 14 ||
    		size > 1400 ||
    		chan < 1 || chan > 13 ||
    		header_type > 2){
    		goto RAW_USAGE;
    	}


    	size += header_sz[header_type];

    	if(NULL == (buffer = _mem_alloc_system_zero(size))){
          printf("malloc failure\n");
    		break;
    	}

        error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
        if (ENET_OK != error || dev_status == FALSE)
        {
            break;
        }



    	/* convert channel from number to freq */
    	chan = ATH_IOCTL_FREQ_1 + (chan-1)*5;
        param.cmd_id = ATH_SET_CHANNEL;
        param.data = &chan;
        param.length = sizeof(uint_32);


        error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&param);
        if (ENET_OK != error)
        {
            break;
        }



    	pHdr = (WIFI_HDR*)&buffer[0];

    	memset(pHdr, 0, WIFI_HDR_SZ);

        pHdr->fc[0] = 0x80; //beacon

        pHdr->addr[0][0] = 0xff;
        pHdr->addr[0][1] = 0xff;
        pHdr->addr[0][2] = 0xff;
        pHdr->addr[0][3] = 0xff;
        pHdr->addr[0][4] = 0xff;
        pHdr->addr[0][5] = 0xff;

        pHdr->addr[1][0] = 0x00;
        pHdr->addr[1][1] = 0x03;
        pHdr->addr[1][2] = 0x7f;
        pHdr->addr[1][3] = 0xdd;
        pHdr->addr[1][4] = 0xdd;
        pHdr->addr[1][5] = 0xdd;

        pHdr->addr[2][0] = 0x00;
        pHdr->addr[2][1] = 0x03;
        pHdr->addr[2][2] = 0x7f;
        pHdr->addr[2][3] = 0xdd;
        pHdr->addr[2][4] = 0xdd;
        pHdr->addr[2][5] = 0xdd;

        access_cat = ATH_ACCESS_CAT_BE;

        if(header_type == 1){
        	//change frame control to qos data
        	pHdr->fc[0] = 0x88;
        	pHdr->fc[1] = 0x01; //to-ds
        	pQosHdr = (WIFI_QOS_HDR*)pHdr;
        	pQosHdr->qos[0] = 0x03;
        	pQosHdr->qos[1] = 0x00;
        	//bssid == addr[0]
        	memcpy(&(pHdr->addr[0][0]), &(pHdr->addr[1][0]), 6);
        	//change destination address
        	pHdr->addr[2][3] = 0xaa;
        	pHdr->addr[2][4] = 0xaa;
        	pHdr->addr[2][5] = 0xaa;
        	access_cat = ATH_ACCESS_CAT_VI;
        }else if(header_type == 2){
        	pHdr->fc[0] = 0x08;
        	pHdr->fc[1] = 0x03; //to-ds | from-ds
        	p4Hdr = (WIFI_4_HDR*)pHdr;
        	p4Hdr->addr_3[0] = 0x00;
        	p4Hdr->addr_3[1] = 0x03;
        	p4Hdr->addr_3[2] = 0x7f;
        	p4Hdr->addr_3[3] = 0xee;
        	p4Hdr->addr_3[4] = 0xee;
        	p4Hdr->addr_3[5] = 0xee;
        }

        /* now setup the frame for transmission */
    	for(i=header_sz[header_type];i<size;i++)
    		buffer[i] = (i-header_sz[header_type])&0xff;

        memset(&details, 0, sizeof(details));

        details.buffer = buffer;
        details.length = size;
        details.params.pktID = 1;
        details.params.rateSched.accessCategory = access_cat;
        /* we set the ACK bit simply to force the desired number of retries */
        details.params.rateSched.flags = ATH_MAC_TX_FLAG_ACK;
        details.params.rateSched.trySeries[0] = tries;
        details.params.rateSched.rateSeries[0] = rate_index;
        details.params.rateSched.rateSeries[1] = 0xff;//terminate series

        param.cmd_id = ATH_MAC_TX_RAW;
        param.data = &details;
        param.length = sizeof(details);

        /* this starts WPS on the Aheros wifi */
        error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&param);
        if (ENET_OK != error)
        {
            break;
        }

        status = SHELL_EXIT_SUCCESS;
        break;
 RAW_USAGE:
		printf("raw input error\n");
    	printf("usage = wmiconfig --raw rate num_tries num_bytes channel header_type\n");
   	 	printf("rate = rate index where 0==1mbps; 1==2mbps; 2==5.5mbps etc\n");
   		printf("num_tries = number of transmits 1 - 14\n");
   		printf("num_bytes = payload size 0 to 1400\n");
   		printf("channel = 1 - 11\n");
   		printf("header_type = 0==beacon frame; 1==QOS data frame; 2==4 address data frame\n");
    }while(0);



    if(buffer)
    	_mem_free(buffer);

    return status;
}

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : wps_handler()
* Returned Value  : 1 - successful completion or
*					0 - failed.
* Comments		  : Handles WPS related functionality
*
*END*-----------------------------------------------------------------*/
static int_32 wps_handler( int_32 index, int_32 argc, char_ptr argv[])
{
	uint_32 error= SHELL_EXIT_SUCCESS,i;
	A_UINT8 val;
    A_UINT8 wps_mode = 0;
    ATH_NETPARAMS netparams;
	wps_scan_params wpsScan, *wpsScan_p;

	int j;

	//init context
 	wps_context.wps_in_progress = 0;
 	wps_context.connect_flag = 0;


	if(index+1 < argc)
	{
		i = index;

		wps_context.connect_flag = atoi(argv[i]);

		i++;

		if(strcmp (argv[i],"push") == 0)
		{
			wps_mode = PUSH_BUTTON;
		}
		else if (strcmp (argv[i],"pin") == 0)
		{
			wps_mode = PIN;
		    if( argc < 4)  //User did not enter parameter
		    {
		    	printf("Missing parameters: please enter pin\n");
		    	return SHELL_EXIT_ERROR;
		    }

		    i++;

		    strcpy(wpsPin,argv[i]);
		}

		i++;

		if(argc > i)
		{
			if((argc-i) < 3)
			{
				printf("missing parameters\n");
				return SHELL_EXIT_ERROR;
			}
			else
			{
				strcpy((char*)(wpsScan.ssid),argv[i]);
				i++;

				if(strlen(argv[i]) != 12)
				{
					printf("Invalid MAC address length\n");
					return SHELL_EXIT_ERROR;
				}

				for(j=0;j<6;j++)
				{
					wpsScan.macaddress[j] = 0;
				}
				for(j=0;j<strlen(argv[i]);j++)
				{
					val = ascii_to_hex(argv[i][j]);
					if(val == 0xff)
					{
						printf("Invalid character\n");
						return SHELL_EXIT_ERROR;
					}
					else
					{
						if((j&1) == 0)
							val <<= 4;

						wpsScan.macaddress[j>>1] |= val;
					}
				}

				i++;
				wpsScan.channel = atoi(argv[i]);

			}
			wpsScan_p = &wpsScan;
		}
		else
		{
			wpsScan_p = NULL;
		}
	}

	if(wps_mode == PUSH_BUTTON)
	{
		if(StartWPS(&netparams, 0,wpsScan_p) == 0)
		{
			printf("WPS connect error\n");
			return SHELL_EXIT_ERROR;
		}
	}
	else if(wps_mode == PIN)
	{
		if(StartWPS(&netparams, 1,wpsScan_p) == 0)
		{
			printf("WPS connect error\n");
			return SHELL_EXIT_ERROR;
		}
	}

    wps_context.wps_in_progress = 1;
    return SHELL_EXIT_SUCCESS;
}



extern A_UINT32 ar4XXX_boot_param;

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : ath_driver_state()
* Returned Value  : A_OK - successful completion or
*		   A_ERROR - failed.
* Comments		  : Handles sriver reset functionality
*
*END*-----------------------------------------------------------------*/

static int_32 ath_driver_state( int_32 argc, char_ptr argv[])
{
    uint_32 error;
    ATH_IOCTL_PARAM_STRUCT ath_param;
    uint_32 value;

    do{
        if(argc != 1) break;

        ath_param.cmd_id = ATH_CHIP_STATE;
        ath_param.length = sizeof(value);
        //value = atoi(argv[0]);

        if(strcmp(argv[0],"up") == 0)
        {
            value = 1;

            ath_param.data = &value;
            error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

            if(error == A_OK){
                    return A_OK;
            }else{
                    printf("driver ioctl error\n");
                    return A_ERROR;
            }
        }
        else if(strcmp(argv[0],"down") == 0)
        {
            value = 0;
            ath_param.data = &value;
            error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

            if(error == A_OK){
                    return A_OK;
            }else{
                    printf("driver ioctl error\n");
                    return A_ERROR;
            }
        }
        else if(strcmp(argv[0],"flushdown") == 0)
        {
            /*Check if no packets are queued, if TX is pending, then wait*/
            while(get_tx_status() != ATH_TX_STATUS_IDLE){
            _time_delay(500);
            }

            value = 0;
            ath_param.data = &value;
            error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

            if(error == A_OK){
                    return A_OK;
            }else{
                    printf("driver ioctl error\n");
                    return A_ERROR;
            }
        }
        else if(strcmp(argv[0],"raw") == 0)
        {
            ar4XXX_boot_param = AR4XXX_PARAM_RAWMODE_BOOT;
            value = 0;
            ath_param.data = &value;
            if((error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param)) != A_OK)
                    return A_ERROR;

            value = 1;
            ath_param.data = &value;
            if((error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param)) != A_OK)
                    return A_ERROR;
        }
        else if(strcmp(argv[0],"rawq") == 0)
        {
            printf("Not supported on MQX platform\n");
            return A_OK;
        }
        else if(strcmp(argv[0],"quad") == 0)
        {
            printf("Not supported on MQX platform\n");
            return A_OK;
        }
        else if(strcmp(argv[0],"normal") == 0)
        {
            ar4XXX_boot_param = AR4XXX_PARAM_MODE_NORMAL;
            value = 0;
            ath_param.data = &value;
            if((error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param)) != A_OK)
                    return A_ERROR;

            value = 1;
            ath_param.data = &value;
            if((error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param)) != A_OK)
                    return A_ERROR;
        }
        else if(strcmp(argv[0],"pmu") == 0)
        {
            ar4XXX_boot_param = (0x00000020 | AR4XXX_PARAM_MODE_NORMAL);
            value = 0;
            ath_param.data = &value;
            if((error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param)) != A_OK)
                    return A_ERROR;

            value = 1;
            ath_param.data = &value;
            if((error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param)) != A_OK)
                    return A_ERROR;
        }
        else
        {
            printf("Invalid parameter: try wmiconfig --help\n");
        }

        return A_OK;

    }while(0);

    printf("param error: driver state\n");
    return A_ERROR;
}

/*FUNCTION*--------------------------------------------------------------------
*
* Function Name   : ath_assert_dump()
* Returned Value  : SHELL_EXIT_SUCCESS - on successful completion
*					SHELL_EXIT_ERROR - on any failure.
* Comments        : causes assert information to be collected from chip and
*				    printed to stdout.
*
*END*------------------------------------------------------------------------*/
static int_32 ath_assert_dump( int_32 argc, char_ptr argv[])
{
	ATH_IOCTL_PARAM_STRUCT ath_param;
	uint_32 error;

	ath_param.cmd_id = ATH_ASSERT_DUMP;
    ath_param.data = NULL;
    ath_param.length = 0;

	error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

	return SHELL_EXIT_SUCCESS;
}


/*FUNCTION*--------------------------------------------------------------------
*
* Function Name   : ath_reg_query()
* Returned Value  : SHELL_EXIT_SUCCESS - on successful completion
*					SHELL_EXIT_ERROR - on any failure.
* Comments        : reads / writes AR600X registers.
*
*END*------------------------------------------------------------------------*/
static int_32 ath_reg_query( int_32 argc, char_ptr argv[])
{
	uint_32 error;
	A_UINT8 print_result=0;
	ATH_IOCTL_PARAM_STRUCT ath_param;
	ATH_REGQUERY ath_regquery;

	do{
		if(argc < 2 || argc > 4) break;

		ath_regquery.operation = atoi(argv[0]);
		ath_regquery.address = mystrtoul(argv[1], NULL, 16);

		if(ath_regquery.operation == ATH_REG_OP_READ){
			if(argc != 2) break;

			print_result = 1;
		}else if(ath_regquery.operation == ATH_REG_OP_WRITE){
			if(argc != 3) break;

			ath_regquery.value = mystrtoul(argv[2], NULL, 16);
		}else if(ath_regquery.operation == ATH_REG_OP_RMW){
			if(argc != 4) break;

			ath_regquery.value = mystrtoul(argv[2], NULL, 16);
			ath_regquery.mask = mystrtoul(argv[3], NULL, 16);
		}else{
			break;
		}

		ath_param.cmd_id = ATH_REG_QUERY;
	    ath_param.data = &ath_regquery;
	    ath_param.length = sizeof(ath_regquery);

		error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

		if(error == ENET_OK){
			if(print_result){
				printf("value=%08x\n", ath_regquery.value);
			}
			return SHELL_EXIT_SUCCESS;
		}else{
			printf("driver ioctl error\n");
			return SHELL_EXIT_ERROR;
		}
	}while(0);

	printf("param error: register query\n");
	return SHELL_EXIT_ERROR;
}

/*FUNCTION*--------------------------------------------------------------------
*
* Function Name   : ath_mem_query()
* Returned Value  : SHELL_EXIT_SUCCESS - on successful completion
*					SHELL_EXIT_ERROR - on any failure.
* Comments        : reads / writes AR600X registers.
*
*END*------------------------------------------------------------------------*/
static int_32 ath_mem_query( int_32 argc, char_ptr argv[])
{
	uint_32 error;
	uint_8 print_result=0;
	ATH_IOCTL_PARAM_STRUCT ath_param;
	ATH_MEMQUERY ath_memquery;

	do{
		ath_param.cmd_id = ATH_MEM_QUERY;
	    ath_param.data = &ath_memquery;
	    ath_param.length = sizeof(ath_memquery);

		error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

		printf("firmware memory stats:\n");

		if(error == ENET_OK){

			printf("literals = %d\n", ath_memquery.literals);

			printf("rodata = %d\n", ath_memquery.rodata);

			printf("data = %d\n", ath_memquery.data);

			printf("bss = %d\n", ath_memquery.bss);

			printf("text = %d\n", ath_memquery.text);

			printf("heap remaining = %d\n", ath_memquery.heap);

			return SHELL_EXIT_SUCCESS;
		}else{
			printf("driver ioctl error\n");
			return SHELL_EXIT_ERROR;
		}
	}while(0);

	printf("param error: register query\n");
	return SHELL_EXIT_ERROR;
}

/*FUNCTION*--------------------------------------------------------------------
*
* Function Name   : scan_control()
* Returned Value  : SHELL_EXIT_SUCCESS - on successful completion
*					SHELL_EXIT_ERROR - on any failure.
* Comments        : Disables/Enables foreground and background scan operations
*					in the Atheros device.  Both params must be provided where
*					foreground param is first followed by background param. a
*					'0' param disables the scan type while a '1' enables the
*					scan type.  Background scan -- firmware occasionally scans
*					while connected to a network. Foreground scan -- firmware
*					occasionally scans while disconnected to a network.
*
*END*------------------------------------------------------------------------*/
static int_32 scan_control( int_32 argc, char_ptr argv[])
{
	uint_32 error;
	ATH_IOCTL_PARAM_STRUCT ath_param;
	int_32 fg, bg;
	ATH_SCANPARAMS ath_scan;

	do{
		if(argc != 2) break;

		fg = atoi(argv[0]);

		if(fg != 0 && fg != 1) break;

		bg = atoi(argv[1]);

		if(bg != 0 && bg != 1) break;

		ath_param.cmd_id = ATH_SCAN_CTRL;
	    ath_param.data = &ath_scan;
	    ath_param.length = sizeof(ath_scan);

		ath_scan.flags = ((fg)? 0:ATH_DISABLE_FG_SCAN) | ((bg)? 0:ATH_DISABLE_BG_SCAN);

		error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

		if(error == ENET_OK){
			return SHELL_EXIT_SUCCESS;
		}else{
			printf("driver ioctl error\n");
			return SHELL_EXIT_ERROR;
		}
	}while(0);

	printf("param error: scan control requires 2 inputs [0|1] [0|1]\n");
	return SHELL_EXIT_ERROR;
}



/*FUNCTION*--------------------------------------------------------------------
*
* Function Name   : get_tx_status()
* Returned Value  : A_OK - on successful completion
*					A_ERROR - on any failure.
* Comments        : gets TX status from driver
*
*END*------------------------------------------------------------------------*/
static uint_32 get_tx_status( )
{
    ATH_IOCTL_PARAM_STRUCT ath_param;
    A_UINT32 error;
    ATH_TX_STATUS result;
	 A_UINT32 dev_status;
	error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (ENET_OK != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    ath_param.cmd_id = ATH_GET_TX_STATUS;
    ath_param.data = &result;
    ath_param.length = 4;


    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

    if(error == A_OK){

      switch (result.status)
      {
        case ATH_TX_STATUS_IDLE:
          printf("TX Idle\n");
          break;
        case ATH_TX_STATUS_WIFI_PENDING:
          printf("TX status WIFI Pending \n");
          break;
        case ATH_TX_STATUS_HOST_PENDING:
          printf("TX status Host Pending\n");
          break;
        default:
          printf("Invalid result\n");
          break;
      }
      return result.status;
    }
    else
    {
    	printf("Command Failed\n");
    	return ENET_ERROR;
    }
}



/*FUNCTION*--------------------------------------------------------------------
*
* Function Name   : get_reg_domain()
* Returned Value  : A_OK - on successful completion
*		    A_ERROR - on any failure.
* Comments        : gets Regulatory domain from driver
*
*END*------------------------------------------------------------------------*/
static A_INT32 get_reg_domain( )
{
    ATH_IOCTL_PARAM_STRUCT ath_param;
    A_UINT32 error = A_ERROR;
    ATH_REG_DOMAIN result;
	A_UINT32 dev_status;

	error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (ENET_OK != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    ath_param.cmd_id = ATH_GET_REG_DOMAIN;
    ath_param.data = &result;
    ath_param.length = 0;

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

    if(error == A_OK){
      printf("Regulatory Domain 0x%x\n",result.domain);
    }
    else{
      printf("Command Failed\n");
      return error;
    }
    return A_OK;
}



/*FUNCTION*--------------------------------------------------------------------
*
* Function Name   : set_wep_key()
* Returned Value  : ENET_OK if mode set successfull else ERROR CODE
* Comments        : Sets the Mode for Wifi Device.
*
*END*------------------------------------------------------------------------*/

uint_32 set_wep_keys
    (
        uint_32 dev_num,
        char_ptr wep_key1,
        char_ptr wep_key2,
        char_ptr wep_key3,
        char_ptr wep_key4,
        uint_32 key_len,
        uint_32 key_index

    )
{

    uint_32 error,dev_status;
    //ENET_MEDIACTL_PARAM  inout_param;
    ENET_WEPKEYS wep_param;

   if (dev_num < IPCFG_DEVICE_COUNT)
    {

        error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
        if (ENET_OK != error)
        {
            return error;
        }
        if (dev_status == FALSE)
        {
            return ENET_ERROR;
        }

        wep_param.defKeyIndex = key_index;
        wep_param.numKeys = 0;
        wep_param.keyLength = key_len;

        do{
	        if(wep_key1 == NULL){
	        	break;
	        }

	        wep_param.keys[0] = wep_key1;
	        wep_param.numKeys++;

	        if(wep_key2 == NULL){
	        	break;
	        }

	        wep_param.keys[1] = wep_key2;
	        wep_param.numKeys++;

	        if(wep_key3 == NULL){
	        	break;
	        }

	        wep_param.keys[2] = wep_key3;
	        wep_param.numKeys++;

	        if(wep_key4 == NULL){
	        	break;
	        }

	        wep_param.keys[3] = wep_key4;
	        wep_param.numKeys++;

		}while(0);

        error = ENET_mediactl (whistle_handle,ENET_SET_MEDIACTL_ENCODE,&wep_param);
        if (ENET_OK != error)
        {
            return error;
        }
        return ENET_OK;
    }
    return ENETERR_INVALID_DEVICE;
}














/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : set_cipher()
* Returned Value  : SHELL_EXIT_SUCCESS if ESSID set successfully else ERROR CODE
* Comments        : Sets the Unicast and multicast cipher
*
*END*-----------------------------------------------------------------*/

static uint_32 set_cipher
    (
        uint_32 dev_num,
       char_ptr cipher
    )
{

     ATH_IOCTL_PARAM_STRUCT  inout_param;
    uint_32 error,dev_status;




    inout_param.cmd_id = ATH_SET_CIPHER;
    inout_param.data = cipher;
    inout_param.length = 8;
    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    return SHELL_EXIT_SUCCESS;
}





/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : set_sec_mode()
* Returned Value  : SHELL_EXIT_SUCCESS if mode set successfully else ERROR CODE
* Comments        : Sets the mode.
*
*END*-----------------------------------------------------------------*/

static uint_32 set_sec_mode
    (
        uint_32 dev_num,
        char_ptr mode
    )
{

    uint_32 error,dev_status;
    ATH_IOCTL_PARAM_STRUCT param;

    if (dev_num < IPCFG_DEVICE_COUNT)
    {

        param.cmd_id = ATH_SET_SEC_MODE;
        param.data = mode;
        param.length = strlen(mode);
        error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
        if (SHELL_EXIT_SUCCESS != error)
        {
            return error;
        }
        if (dev_status == FALSE)
        {
            return ENET_ERROR;
        }

        error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&param);
        if (SHELL_EXIT_SUCCESS != error)
        {
            return error;
        }
        return SHELL_EXIT_SUCCESS;
    }
    printf("IWCONFIG_ERROR: Invalid Device number\n");
    return ENETERR_INVALID_DEVICE;
}





static int_32 disconnect_handler()
{
	uint_32 error;

    error = iwcfg_set_essid (DEMOCFG_DEFAULT_DEVICE,"");
    if (error != 0)
    {
        //printf ("Error during setting of ssid %s error=%08x!\n",ssid, error);
        return SHELL_EXIT_ERROR;
    }

    /*Reset stored SSID*/
    memset(original_ssid,'\0',33);

    error = iwcfg_commit(DEMOCFG_DEFAULT_DEVICE);

	hidden_flag = 0;

    return SHELL_EXIT_SUCCESS;
}





static int_32 set_tx_power(char_ptr pwr)
{

     ATH_IOCTL_PARAM_STRUCT  inout_param;
    uint_32 error,dev_status;
    A_UINT8 dBm;

    dBm = atoi(pwr);
    inout_param.cmd_id = ATH_SET_TXPWR;
    inout_param.data = &dBm;
    inout_param.length = 1;//strlen(dBm);
    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    return SHELL_EXIT_SUCCESS;
}





static int_32 set_channel(char_ptr channel)
{
    ATH_IOCTL_PARAM_STRUCT  inout_param;
    uint_32 error,dev_status, chnl;
    char* p = 0;
    chnl = strtoul(channel, &p,0);

    if(chnl < 1 || chnl > 14)
    {
      printf("invalid channel: should range between 1-14\n");
      return A_ERROR;
    }
    /* convert channel from number to freq */
    chnl = ATH_IOCTL_FREQ_1 + (chnl-1)*5;

    inout_param.data = &chnl;
    inout_param.length = sizeof (uint_32);
    inout_param.cmd_id = ATH_SET_CHANNEL;
    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    return SHELL_EXIT_SUCCESS;
}






static int_32 set_pmparams(int_32 index, int_32 argc, char_ptr argv[])
{
	uint_32 i;
	ATH_WMI_POWER_PARAMS_CMD pm;
	A_UINT8  iflag = 0, npflag = 0, dpflag = 0, txwpflag = 0, ntxwflag = 0, pspflag = 0;
     ATH_IOCTL_PARAM_STRUCT  inout_param;
    uint_32 error,dev_status;


	for(i=index; i<argc;i++)
	{
		if (strcmp (argv[i],"--idle") == 0)
		{
		    if(i+1 == argc)  //User did not enter parameter
		    {
		    	printf("Missing parameter\n");
		    	return SHELL_EXIT_ERROR;
		    }
		    pm.idle_period = atoi(argv[i+1]);
		    i++;
			iflag = 1;
		}
		else if(strcmp (argv[i], "--np") == 0)
		{
		     if(i+1 == argc)  //User did not enter parameter
		    {
		    	printf("Missing parameter\n");
		    	return SHELL_EXIT_ERROR;
		    }

			npflag = 1;
			pm.pspoll_number = atoi(argv[i+1]);
			i++;
		}
		else if(strcmp (argv[i], "--dp") == 0)
		{
		     if(i+1 == argc)  //User did not enter parameter
		    {
		    	printf("Missing parameter\n");
		    	return SHELL_EXIT_ERROR;
		    }

			dpflag = 1;
			pm.dtim_policy = atoi(argv[i+1]);
			i++;
		}
		else if(strcmp (argv[i], "--txwp") == 0)
		{
		     if(i+1 == argc)  //User did not enter parameter
		    {
		    	printf("Missing parameter\n");
		    	return SHELL_EXIT_ERROR;
		    }

			txwpflag = 1;
			pm.tx_wakeup_policy = atoi(argv[i+1]);
			i++;
		}
		else if(strcmp (argv[i], "--ntxw") == 0)
		{
		     if(i+1 == argc)  //User did not enter parameter
		    {
		    	printf("Missing parameter\n");
		    	return SHELL_EXIT_ERROR;
		    }

			ntxwflag = 1;
			pm.num_tx_to_wakeup = atoi(argv[i+1]);
			i++;
		}
		else if(strcmp (argv[i], "--psfp") == 0)
		{
		     if(i+1 == argc)  //User did not enter parameter
		    {
		    	printf("Missing parameter\n");
		    	return SHELL_EXIT_ERROR;
		    }

			pspflag = 1;
			pm.ps_fail_event_policy = atoi(argv[i+1]);
			i++;
		}
		else
		{
			printf("Invalid parameter\n");
			return SHELL_EXIT_ERROR;
		}
	}


    /*Fill in default values if needed*/
	if(!iflag)
	{
		pm.idle_period = 0;
	}

	if(!npflag)
	{
		pm.pspoll_number = 10;
	}

	if(!dpflag)
	{
		pm.dtim_policy = 3;
	}

	if(!txwpflag)
	{
		pm.tx_wakeup_policy = 2;
	}

	if(!ntxwflag)
	{
		pm.num_tx_to_wakeup = 1;
	}

	if(!pspflag)
	{
		pm.ps_fail_event_policy = 2;
	}

    inout_param.cmd_id = ATH_SET_PMPARAMS;
    inout_param.data = &pm;
    inout_param.length = 12;
    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return SHELL_EXIT_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    return SHELL_EXIT_SUCCESS;
    //return(iwcfg_set_pm_params(DEMOCFG_DEFAULT_DEVICE, (char*)&pm));
}





/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : set_listen_interval
* Returned Value  : SHELL_EXIT_SUCCESS if success else ERROR
*
* Comments        : Sets listen interval
*
*END*-----------------------------------------------------------------*/
static int_32 set_listen_interval(char_ptr listen_int)
{

    ATH_IOCTL_PARAM_STRUCT  inout_param;
    uint_32 error,dev_status,listen_interval;

    inout_param.cmd_id = ATH_SET_LISTEN_INT;
    listen_interval = atoi(listen_int);
    inout_param.data = &listen_interval;
    inout_param.length = 4;
    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return SHELL_EXIT_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    return SHELL_EXIT_SUCCESS;
}



/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : resetTarget
* Returned Value  :   none
*
* Comments        : Resets target
*
*END*-----------------------------------------------------------------*/
void resetTarget()
{
	uint_32 error;
	ATH_IOCTL_PARAM_STRUCT ath_param;
	uint_32 value;

    printf("Target reset\n");

	ath_param.cmd_id = ATH_CHIP_STATE;
	ath_param.length = sizeof(value);

    //Driver down
	value = 0;

	ath_param.data = &value;
	error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

	if(error != A_OK){

		printf("driver ioctl error\n");
		return;
	}
    //Driver up
	value = 1;
	ath_param.data = &value;
	error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);

	if(error != A_OK){

		printf("driver ioctl error\n");
		return;
	}

}


static int_32 set_host_mode(char_ptr host_mode)
{
    printf("Command not supported\n");
//	return (iwcfg_set_host_mode(DEMOCFG_DEFAULT_DEVICE,host_mode));
    return 0;
}



/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : set_phy_mode
* Returned Value  : SHELL_EXIT_SUCCESS if success else ERROR
*
* Comments        : Sets current phy mode
*
*END*-----------------------------------------------------------------*/
static int_32 set_phy_mode(char_ptr phy_mode)
{
    ATH_IOCTL_PARAM_STRUCT  inout_param;
    uint_32 error,dev_status;

	if ((strcmp (phy_mode,"n") != 0) && (strcmp (phy_mode,"b") != 0) && (strcmp (phy_mode,"g") != 0))
	{
		printf("Invalid Phy mode\n");
		return SHELL_EXIT_ERROR;
	}

    inout_param.cmd_id = ATH_SET_PHY_MODE;
    inout_param.data = phy_mode;
    inout_param.length = sizeof(phy_mode);
    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    return SHELL_EXIT_SUCCESS;
}




/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : set_power_mode
* Returned Value  : SHELL_EXIT_SUCCESS if success else ERROR
*
* Comments        : Sets current power mode to Power-Save or Max-Perf
*
*END*-----------------------------------------------------------------*/
static int_32 set_power_mode(char_ptr pwr_mode)
{
	int mode;

	mode = atoi(pwr_mode);

	if (mode != 0 && mode != 1)
	{
		printf("Invalid power mode\n");
		return SHELL_EXIT_ERROR;
	}

	return(iwcfg_set_power(DEMOCFG_DEFAULT_DEVICE,mode,0));
}




/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : get_power_mode
* Returned Value  : SHELL_EXIT_SUCCESS if success else ERROR
*
* Comments        : Gets current power mode
*
*END*-----------------------------------------------------------------*/
static int_32 get_power_mode(char_ptr pwr_mode)
{
	ENET_MEDIACTL_PARAM inout_param;
    uint_32 error = SHELL_EXIT_SUCCESS,dev_status;


    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_GET_MEDIACTL_POWER,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }

    if(inout_param.value == 1)
    {
    	strcpy(pwr_mode,"Power Save");
    }
    else if(inout_param.value == 0)
    {
    	strcpy(pwr_mode,"Max Perf");
    }
    else
    {
    	strcpy(pwr_mode,"Invalid");
    }
    return error;
}

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : get_phy_mode()
* Returned Value  : SHELL_EXIT_SUCCESS if phy_mode set successfully else ERROR CODE
* Comments        : Gets the phy_mode.
*
*END*-----------------------------------------------------------------*/

uint_32 get_phy_mode
    (
        uint_32 dev_num,
       char_ptr phy_mode
    )
{
    ATH_IOCTL_PARAM_STRUCT inout_param;
    uint_32 error,dev_status;
    uint_32 value = 0;

    inout_param.cmd_id = ATH_GET_PHY_MODE;
    inout_param.data = &value;

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }

    if(*(uint_32*)(inout_param.data) == 0x01)
    {
    	strcpy(phy_mode,"a");
    }
    else if(*(uint_32*)(inout_param.data) == 0x02)
    {
    	strcpy(phy_mode,"g");
    }
    else if(*(uint_32*)(inout_param.data) == 0x04)
    {
    	strcpy(phy_mode,"b");
    }
    else
    {
    	strcpy(phy_mode,"mixed");
    }
    return SHELL_EXIT_SUCCESS;
}


#if !FLASH_APP
/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : get_version()
* Returned Value  : SHELL_EXIT_SUCCESS version is retrieved successfully else ERROR CODE
* Comments        : gets driver,firmware version.
*
*END*-----------------------------------------------------------------*/
uint_32 get_version()
{
	ATH_IOCTL_PARAM_STRUCT inout_param;
    uint_32 error,dev_status;
    version_t version;

    inout_param.cmd_id = ATH_GET_VERSION;
    inout_param.data = &version;
    inout_param.length = 16;

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }

    //printf("Host version     :  %d\n",version.host_ver);
    printf("Host version :  %d.%d.%d.%d.%d\n",(version.host_ver&0xF0000000)>>28,(version.host_ver&0x0F000000)>>24
                                              ,(version.host_ver&0x00FC0000)>>18,(version.host_ver&0x0003FF00)>>8,(version.host_ver&0x000000FF));

    printf("Target version   :  0x%x\n",version.target_ver);
    printf("Firmware version :  %d.%d.%d.%d.%d\n",(version.wlan_ver&0xF0000000)>>28,(version.wlan_ver&0x0F000000)>>24
                                              ,(version.wlan_ver&0x00FC0000)>>18,(version.wlan_ver&0x0003FF00)>>8,(version.wlan_ver&0x000000FF));
    printf("Interface version:  %d\n",version.abi_ver);
    return SHELL_EXIT_SUCCESS;
}
#endif

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : get_mac_addr
* Returned Value  : SHELL_EXIT_SUCCESS if success else ERROR
*
* Comments        : Gets current firmware mac addr
*
*END*-----------------------------------------------------------------*/
static int_32 get_mac_addr(char_ptr mac_addr)
{

    ATH_IOCTL_PARAM_STRUCT inout_param;
    uint_32 error,dev_status;

    inout_param.cmd_id = ATH_GET_MACADDR;
    inout_param.data = mac_addr;

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }

    return SHELL_EXIT_SUCCESS;
}


static int_32 get_channel_val(char_ptr channel_val)
{
    ATH_IOCTL_PARAM_STRUCT inout_param;
    uint_32 error,dev_status;

    inout_param.cmd_id = ATH_GET_CHANNEL;
    inout_param.data = channel_val;

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }

    return SHELL_EXIT_SUCCESS;
}

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : get_last_error()
* Returned Value  : SHELL_EXIT_SUCCESS if phy_mode set successfully else ERROR CODE
* Comments        : Gets the last error in the host driver
*
*END*-----------------------------------------------------------------*/

uint_32 get_last_error()
{
    ATH_IOCTL_PARAM_STRUCT inout_param;
    uint_32 error,dev_status;

    inout_param.cmd_id = ATH_GET_LAST_ERROR;

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }

    printf("Last Driver error %d\n",(*(uint_32*)(inout_param.data)));
    return SHELL_EXIT_SUCCESS;
}


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : dev_susp_enable()
* Returned Value  : SHELL_EXIT_SUCCESS if device suspend is enabled
*                   successfully else ERROR CODE
* Comments        : Sets the device suspend mode.
*
*END*-----------------------------------------------------------------*/
uint_32 dev_susp_enable()
{
    ATH_IOCTL_PARAM_STRUCT inout_param;
    uint_32 error,dev_status;

    inout_param.cmd_id = ATH_DEVICE_SUSPEND_ENABLE;
    inout_param.data = NULL;
    inout_param.length = 0;

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    return SHELL_EXIT_SUCCESS;
}



/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : dev_susp_start()
* Returned Value  : SHELL_EXIT_SUCCESS if device suspend is started
*                   successfully else ERROR CODE
* Comments        : Suspends device for requested time period
*
*END*-----------------------------------------------------------------*/
uint_32 dev_susp_start(char_ptr susp_time)
{
	int suspend_time;
    ATH_IOCTL_PARAM_STRUCT inout_param;
    uint_32 error,dev_status;

	suspend_time = atoi(susp_time);

    inout_param.cmd_id = ATH_DEVICE_SUSPEND_START;
    inout_param.data = &suspend_time;
    inout_param.length = 4;

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);

    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }

    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    /*Check if no packets are queued, if TX is pending, then wait*/
    while(get_tx_status() != ATH_TX_STATUS_IDLE){
      _time_delay(500);
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);

    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    return SHELL_EXIT_SUCCESS;
}



typedef struct{
#define SENTINAL 0xa55a1234
	uint_32 sentinal;
	int_8 ssid[33];
	A_UINT8 ipaddr[4];
	int_8 passphrase[128];
}FLASH_CONTENTS;

static int_32 test_flash(void)
{
	FLASHX_BLOCK_INFO_STRUCT_PTR info_ptr;
    MQX_FILE_PTR   flash_hdl;
    _mqx_uint      error_code;
   _mem_size      base_addr;
 //  _mem_size      sector_size;
   _mqx_uint      num_sectors;
 //  _mqx_uint      test_val;
   _mqx_uint      num_blocks;
   _mqx_uint      width;
   _mem_size      total_size = 0;
 //  uchar_ptr       write_buffer;
 //  uchar_ptr       read_buffer;
//   uchar_ptr       old_buffer;
   _mqx_int       i;//, test_block;
 //  _mqx_uint      j, k, n;
   _mem_size      seek_location;
   _mem_size      read_write_size = 0;
   _mqx_uint      ident_arr[3];
   boolean        test_completed = FALSE;
   FLASH_CONTENTS flash_buffer;
   /* Open the flash device */
    flash_hdl = fopen("flashx:", NULL);
    _io_ioctl(flash_hdl, IO_IOCTL_DEVICE_IDENTIFY, &ident_arr[0]);
    printf("\nFlash Device Identity: %#010x, %#010x, %#010x",
             ident_arr[0], ident_arr[1], ident_arr[2]);

   error_code = ioctl(flash_hdl, FLASH_IOCTL_GET_BASE_ADDRESS, &base_addr);
   if (error_code != MQX_OK) {
      printf("\nFLASH_IOCTL_GET_BASE_ADDRESS failed.");
      _task_block();
   } else {
      printf("\nThe BASE_ADDRESS: 0x%x", base_addr);
   } /* Endif */

   error_code = ioctl(flash_hdl, FLASH_IOCTL_GET_NUM_SECTORS, &num_sectors);
   if (error_code != MQX_OK) {
      printf("\nFLASH_IOCTL_GET_NUM_SECTORS failed.");
      _task_block();
   } else {
      printf("\nNumber of sectors: %d", num_sectors);
   } /* Endif */

   error_code = ioctl(flash_hdl, FLASH_IOCTL_GET_WIDTH, &width);
   if (error_code != MQX_OK) {
      printf("\nFLASH_IOCTL_GET_WIDTH failed.");
      _task_block();
   } else {
      printf("\nThe WIDTH: %d", width);
   } /* Endif */

   error_code = ioctl(flash_hdl, FLASH_IOCTL_GET_BLOCK_MAP,
      (uint_32 _PTR_)&info_ptr);
   if (error_code != MQX_OK) {
      printf("\nFLASH_IOCTL_GET_BLOCK_MAP failed.");
      _task_block();
   } /* Endif */

   error_code = ioctl(flash_hdl, FLASH_IOCTL_GET_BLOCK_GROUPS, &num_blocks);
   if (error_code != MQX_OK) {
      printf("\nFLASH_IOCTL_GET_NUM_BLOCKS failed");
      _task_block();
   } /* Endif */

   for ( i = 0; i < num_blocks; i++ ) {
       printf("\nThere are %d sectors in Block %d", info_ptr[i].NUM_SECTORS,
          (i + 1));
#if MQX_VERSION == 380
       printf("\nBlock %d Sector Size: %d (0x%x)", (i + 1),
                 info_ptr[i].SECTOR_SIZE, info_ptr[i].SECTOR_SIZE);
       total_size += (info_ptr[i].SECTOR_SIZE * info_ptr[i].NUM_SECTORS);
#else
       printf("\nBlock %d Sector Size: %d (0x%x)", (i + 1),
          info_ptr[i].SECT_SIZE, info_ptr[i].SECT_SIZE);
       total_size += (info_ptr[i].SECT_SIZE * info_ptr[i].NUM_SECTORS);
#endif

   } /* Endfor */

   /* init seek_location to the first write-able address */
   seek_location = info_ptr[0].START_ADDR;


   printf("\nTotal size of the Flash device is: %d (0x%x)", total_size,
      total_size);
   fseek(flash_hdl, seek_location, IO_SEEK_SET);
   i = read(flash_hdl, &flash_buffer, sizeof(FLASH_CONTENTS));

   if(i==sizeof(FLASH_CONTENTS) && flash_buffer.sentinal == SENTINAL){
   		printf("\nsuccessfully read contents from flash\n");
   		printf("ipadddr=%d.%d.%d.%d\n", flash_buffer.ipaddr[0],
   			flash_buffer.ipaddr[1],
   			flash_buffer.ipaddr[2],
   			flash_buffer.ipaddr[3]);
   		printf("ssid=%s\n", flash_buffer.ssid);
   		printf("passphrase=%s\n", flash_buffer.passphrase);
   }else{
   		printf("\nfailed reading contents from flash--attempting write\n");

		memset(&flash_buffer, 0,sizeof(FLASH_CONTENTS));
   		flash_buffer.sentinal = SENTINAL;
   		flash_buffer.ipaddr[0] =  192;
   		flash_buffer.ipaddr[1] =  168;
   		flash_buffer.ipaddr[2] =  1;
   		flash_buffer.ipaddr[3] =  90;
   		memcpy(flash_buffer.ssid, "atheros_demo", 12);
   		memcpy(flash_buffer.passphrase, "my security passphrase", strlen("my security passphrase"));
   		fseek(flash_hdl, seek_location, IO_SEEK_SET);
   		i = write(flash_hdl, &flash_buffer, sizeof(FLASH_CONTENTS) );

   		printf("write result = %d\n", i);

   		printf("try read now\n");
   		memset(&flash_buffer, 0,sizeof(FLASH_CONTENTS));
   		fseek(flash_hdl, seek_location, IO_SEEK_SET);
   		i = read(flash_hdl, &flash_buffer, sizeof(FLASH_CONTENTS));
   		if(i==sizeof(FLASH_CONTENTS)){
	   		printf("\nsuccessfully read contents from flash\n");
	   		printf("ipadddr=%d.%d.%d.%d\n", flash_buffer.ipaddr[0],
	   			flash_buffer.ipaddr[1],
	   			flash_buffer.ipaddr[2],
	   			flash_buffer.ipaddr[3]);
	   		printf("ssid=%s\n", flash_buffer.ssid);
	   		printf("passphrase=%s\n", flash_buffer.passphrase);
	    }else{
	   		printf("\nfailed reading contents from flash--aborting!!!\n");
   		}
   }

   fclose(flash_hdl);
	return SHELL_EXIT_SUCCESS;
}

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : get_rssi()
* Returned Value  : SHELL_EXIT_SUCCESS if rssi is retrieved
*                   successfully else ERROR CODE
* Comments        : Gets RSSI value from driver.
*
*END*-----------------------------------------------------------------*/

uint_32 get_rssi()
{
	uint_32 error,dev_status,i=0;
	ATH_IOCTL_PARAM_STRUCT inout_param;
	uint_32 rssi=0;

	if (DEMOCFG_DEFAULT_DEVICE < IPCFG_DEVICE_COUNT)
	{

		error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
		if (SHELL_EXIT_SUCCESS != error)
		{
			return error;
		}
		if (dev_status == FALSE)
		{
			return SHELL_EXIT_ERROR;
		}

		inout_param.cmd_id = ATH_GET_RX_RSSI;
		inout_param.data = &rssi;
		error = iwcfg_get_essid (DEMOCFG_DEFAULT_DEVICE,ssid);


		if(ssid[0] == '\0')
		{
			printf("Not associated\n");
			return SHELL_EXIT_SUCCESS;
		}


		error = ENET_mediactl(whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &inout_param);

		if (SHELL_EXIT_SUCCESS != error)
		{
			return error;
		}

		/*error = ENET_mediactl (handle, ENET_GET_MEDIACTL_SCAN, );
		if (SHELL_EXIT_SUCCESS != error)
		{
			return error;
		}*/

		printf("\nindicator = %d dB",rssi);
			printf("\n\r");


		return SHELL_EXIT_SUCCESS;
	}
	printf("IWCONFIG_ERROR: Invalid Device number\n");
	return ENETERR_INVALID_DEVICE;
}

uint_32 ext_scan()
{
	uint_32 error,dev_status,i=0;
	ATH_IOCTL_PARAM_STRUCT inout_param;
	ATH_GET_SCAN param;
	A_UINT8 temp_ssid[33];

	if (DEMOCFG_DEFAULT_DEVICE < IPCFG_DEVICE_COUNT)
	{

		error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
		if (SHELL_EXIT_SUCCESS != error)
		{
			return error;
		}
		if (dev_status == FALSE)
		{
			return SHELL_EXIT_ERROR;
		}

		inout_param.cmd_id = ATH_START_SCAN_EXT;
		inout_param.data = NULL;


		error = ENET_mediactl(whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &inout_param);

		if (SHELL_EXIT_SUCCESS != error)
		{
			return error;
		}

		inout_param.cmd_id = ATH_GET_SCAN_EXT;
		inout_param.data = (A_VOID*)&param;

		error = ENET_mediactl(whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &inout_param);

		if (SHELL_EXIT_SUCCESS != error)
		{
			return error;
		}


		if(param.num_entries){
			for(i=0 ; i<param.num_entries ; i++){
				A_UINT8 j = MAX_RSSI_TABLE_SZ-1;

				memcpy(temp_ssid,param.scan_list[i].ssid,param.scan_list[i].ssid_len);

				temp_ssid[param.scan_list[i].ssid_len] = '\0';

	            if (param.scan_list[i].ssid_len == 0)
	            {
	               printf("ssid = SSID Not available\n");
	            }
	            else
	               printf ("ssid = %s\n",temp_ssid);

	            printf ("bssid = %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",param.scan_list[i].bssid[0],param.scan_list[i].bssid[1],param.scan_list[i].bssid[2],param.scan_list[i].bssid[3],param.scan_list[i].bssid[4],param.scan_list[i].bssid[5]);
	            printf ("channel = %d\n",param.scan_list[i].channel);

	            printf("indicator = %d\n",param.scan_list[i].rssi);
	            printf("security = ");

	            if(param.scan_list[i].security_enabled){
	            	if(param.scan_list[i].rsn_auth || param.scan_list[i].rsn_cipher){
	            		printf("\n\r");
	            		printf("RSN/WPA2= ");
	            	}

	            	if(param.scan_list[i].rsn_auth){
	            		printf(" {");
	            		if(param.scan_list[i].rsn_auth & SECURITY_AUTH_1X){
	            			printf("802.1X ");
	            		}

	            		if(param.scan_list[i].rsn_auth & SECURITY_AUTH_PSK){
	            			printf("PSK ");
	            		}
	            		printf("}");
	            	}

	            	if(param.scan_list[i].rsn_cipher){
	            		printf(" {");
		            	/* AP security can support multiple options hence
		            	 * we check each one separately. Note rsn == wpa2 */
		            	if(param.scan_list[i].rsn_cipher & ATH_CIPHER_TYPE_WEP){
			            	printf("WEP ");
			            }

			            if(param.scan_list[i].rsn_cipher & ATH_CIPHER_TYPE_TKIP){
			            	printf("TKIP ");
			            }

			            if(param.scan_list[i].rsn_cipher & ATH_CIPHER_TYPE_CCMP){
			            	printf("AES ");
			            }
			            printf("}");
			        }

			        if(param.scan_list[i].wpa_auth || param.scan_list[i].wpa_cipher){
	            		printf("\n\r");
	            		printf("WPA= ");
	            	}

			        if(param.scan_list[i].wpa_auth){
			        	printf(" {");
	            		if(param.scan_list[i].wpa_auth & SECURITY_AUTH_1X){
	            			printf("802.1X ");
	            		}

	            		if(param.scan_list[i].wpa_auth & SECURITY_AUTH_PSK){
	            			printf("PSK ");
	            		}
	            		printf("}");
	            	}

	            	if(param.scan_list[i].wpa_cipher){
	            		printf(" {");
			            if(param.scan_list[i].wpa_cipher & ATH_CIPHER_TYPE_WEP){
			            	printf("WEP ");
			            }

			            if(param.scan_list[i].wpa_cipher & ATH_CIPHER_TYPE_TKIP){
			            	printf("TKIP ");
			            }

			            if(param.scan_list[i].wpa_cipher & ATH_CIPHER_TYPE_CCMP){
			            	printf("AES ");
			            }
			            printf("}");
			        }
		            /* it may be old-fashioned WEP this is identified by
		             * absent wpa and rsn ciphers */
		            if(param.scan_list[i].rsn_cipher == 0 &&
		             	param.scan_list[i].wpa_cipher == 0){
		             	printf("WEP ");
		            }
		        }else{
		        	printf("NONE! ");
		        }

	            printf("\n");

	            printf("\n\r");
			}
		}else{
			printf("no scan results found\n");
		}

		return SHELL_EXIT_SUCCESS;
	}
	printf("IWCONFIG_ERROR: Invalid Device number\n");
	return ENETERR_INVALID_DEVICE;
}






/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : wmi_iwconfig()
* Returned Value  : SHELL_EXIT_SUCCESS if success else ERROR
*
* Comments        : Setup for scan command
*
*END*-----------------------------------------------------------------*/
int_32 wmi_iwconfig(int_32 argc, char_ptr argv[])
{
	A_UINT8 scan_ssid_flag = 0;			//Flag to decide whether to scan all ssid/channels
	A_UINT8 scan_ssid[MAX_SSID_LENGTH];
	uint_32 error = ENET_OK;

	memset(scan_ssid,0,MAX_SSID_LENGTH);

	if(argc < 2)
	{
		printf("Missing Parameters\n");
		return SHELL_EXIT_ERROR;
	}

	else if(argc > 1)
	{
		if(strcmp(argv[1],"scan")!=0)
		{
			printf("Incorrect Parameter\n");
			return SHELL_EXIT_ERROR;
		}

		if(argc == 3)
		{
			if(strlen(argv[2]) > MAX_SSID_LENGTH)
            {
                printf("SSID length exceeds Maximum value\n");
                return A_ERROR;
            }
			/*Scan specified SSID*/
			scan_ssid_flag = 1;
			strcpy((char*)scan_ssid,argv[2]);
		}
	}
	/*Set SSID for scan*/
	if(scan_ssid_flag)
	{
		error = iwcfg_set_essid (DEMOCFG_DEFAULT_DEVICE,(char*)scan_ssid);
		if(error != ENET_OK)
		{
			printf("Unable to set SSID\n");
			return error;
		}
	}
	else
	{
		error = iwcfg_set_essid (DEMOCFG_DEFAULT_DEVICE,"");
		if(error != ENET_OK)
		{
			printf("Unable to set SSID\n");
			return error;
		}
	}

    if (MODE_AP != mode_flag)
    {
        /*Do the actual scan*/
        wmi_set_scan(DEMOCFG_DEFAULT_DEVICE);
    }

	/*Revert to original SSID*/

	error = iwcfg_set_essid (DEMOCFG_DEFAULT_DEVICE,(char*)original_ssid);
	if(error != ENET_OK)
	{
		printf("Unable to set SSID\n");
		return error;
	}

	return error;
}

#if ENABLE_AP_MODE
static int_32 ap_handler( int_32 index, int_32 argc, char_ptr argv[])
{
    uint_32  error= SHELL_EXIT_SUCCESS, i, ret_val;
    char* p = 0;

    if (MODE_AP != mode_flag)
    {
      printf ("Err:Set AP Mode to apply AP settings\n");
      return SHELL_EXIT_ERROR;
    }

    if (index+1 < argc)
    {
        i = index;

        if (strcmp (argv[i],"country") == 0)
        {


            ap_config_param.u.wmi_ap_set_country_cmd.countryCode[0] = argv[i+1][0];
            ap_config_param.u.wmi_ap_set_country_cmd.countryCode[1] = argv[i+1][1];
            ap_config_param.u.wmi_ap_set_country_cmd.countryCode[2] = 0x20;

            if ( ('F' == ap_config_param.u.wmi_ap_set_country_cmd.countryCode[0] && 'F' == ap_config_param.u.wmi_ap_set_country_cmd.countryCode[1]) ||
                 ('f' == ap_config_param.u.wmi_ap_set_country_cmd.countryCode[0] && 'f' == ap_config_param.u.wmi_ap_set_country_cmd.countryCode[1]))
            {
              printf ("AP in WWR Mode\n");
            }

            ret_val = set_ap_params (AP_SUB_CMD_SET_COUNTRY, (A_UINT8 *)ap_config_param.u.wmi_ap_set_country_cmd.countryCode);

            if (ret_val != SHELL_EXIT_SUCCESS)
            {
      			printf ("Err:set country code \n");
      			return SHELL_EXIT_ERROR;
			}
        }
        else
        if(strcmp (argv[i],"inact") == 0)
        {
            ap_config_param.u.wmi_ap_conn_inact_cmd.period = strtoul(argv[i+1], &p,0);

            if(ap_config_param.u.wmi_ap_conn_inact_cmd.period == 0)
            {
     			printf ("incorrect inactive timeout \n");
      			return SHELL_EXIT_ERROR;
			}

            ret_val = set_ap_params(AP_SUB_CMD_INACT_TIME, (A_UINT8 *)&ap_config_param.u.wmi_ap_conn_inact_cmd.period);

	        if (ret_val != SHELL_EXIT_SUCCESS)
            {
      			printf ("Err:set inactive_time\n");
      			return SHELL_EXIT_ERROR;
			}
        }
        else
        if(strcmp (argv[i],"bconint") == 0)
        {
            ap_config_param.u.wmi_beacon_int_cmd.beaconInterval = strtoul(argv[i+1], &p,0);

            if((ap_config_param.u.wmi_beacon_int_cmd.beaconInterval < 100) || (ap_config_param.u.wmi_beacon_int_cmd.beaconInterval > 1000))
            {
      			printf ("beacon interval has to be within 100-1000 in units of 100 \n");
      			return SHELL_EXIT_ERROR;
			}

            ret_val = set_ap_params(AP_SUB_CMD_BCON_INT, (A_UINT8 *)&ap_config_param.u.wmi_beacon_int_cmd.beaconInterval);

            if (ret_val != SHELL_EXIT_SUCCESS)
            {
      			printf ("Err:set beacon interval \n");
      			return SHELL_EXIT_ERROR;
			}
        }
        else
        {
            printf ("usage:--ap [country <country code>]\n"
                    "      --ap [bconint<intvl>]\n"
                    "      --ap [inact <minutes>]>\n");
        }
    }
    else
    {
            printf ("usage:--ap [country <country code>]\n"
                    "      --ap [bconint<intvl>]\n"
                    "      --ap [inact <minutes>]>\n");
    }

    return error;
}

static int_32 set_ap_params(A_UINT16 param_cmd, A_UINT8 *data_val)
{
	ATH_IOCTL_PARAM_STRUCT inout_param;
	ATH_AP_PARAM_STRUCT ap_param;
	uint_32 error,dev_status;

	inout_param.cmd_id = ATH_CONFIG_AP;
	ap_param.cmd_subset = param_cmd;
	ap_param.data		= data_val;
	inout_param.data	= &ap_param;

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }
    if (dev_status == FALSE)
    {
        return ENET_ERROR;
    }

    error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&inout_param);
    if (SHELL_EXIT_SUCCESS != error)
    {
        return error;
    }

    return SHELL_EXIT_SUCCESS;
}
#endif

static uint_32 wmi_set_scan(uint_32 dev_num)
{
    uint_32 error,dev_status,i=0;
    ENET_SCAN_LIST  param;
    A_UINT8 temp_ssid[33];

    if (dev_num < IPCFG_DEVICE_COUNT)
    {

        error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
        if (ENET_OK != error)
        {
            return error;
        }
        if (dev_status == FALSE)
        {
            return ENET_ERROR;
        }

        error = ENET_mediactl (whistle_handle,ENET_SET_MEDIACTL_SCAN,NULL);
        if (ENET_OK != error)
        {
            return error;
        }

        error = ENET_mediactl (whistle_handle,ENET_GET_MEDIACTL_SCAN,&param);
        if (ENET_OK != error)
        {
            return error;
        }

        for (i = 0;i<param.num_scan_entries;i++)
        {
            A_UINT8 j = MAX_RSSI_TABLE_SZ-1;

			memcpy(temp_ssid,param.scan_info_list[i].ssid,param.scan_info_list[i].ssid_len);

			temp_ssid[param.scan_info_list[i].ssid_len] = '\0';

            if (param.scan_info_list[i].ssid_len == 0)
            {
               printf("ssid = SSID Not available\n");
            }
            else
               printf ("ssid = %s\n",temp_ssid);

            printf ("bssid = %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",param.scan_info_list[i].bssid[0],param.scan_info_list[i].bssid[1],param.scan_info_list[i].bssid[2],param.scan_info_list[i].bssid[3],param.scan_info_list[i].bssid[4],param.scan_info_list[i].bssid[5]);
            printf ("channel = %d\n",param.scan_info_list[i].channel);

            printf("indicator = %d\n",param.scan_info_list[i].rssi);
            printf("\n\r");
        }
        return ENET_OK;
    }
    printf("IWCONFIG_ERROR: Invalid Device number\n");
    return ENETERR_INVALID_DEVICE;
}










#if ENABLE_STACK_OFFLOAD


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : wmi_ping()
* Returned Value  : None
*
* Comments        : Sample function to depict IPv4 ping functionality
*                   Only used with On-Chip IP stack
*END*-----------------------------------------------------------------*/
A_INT32 wmi_ping(A_INT32 argc, char_ptr argv[] )
{
  	unsigned long       hostaddr;
  	A_INT32 	    error;
    char            ip_str[16];
    A_UINT32 		count, index, size;

  	if(argc < 2)
  	{
  		 printf("Incomplete parameters\n");
         printf("Usage: %s <host> -c <count> -s <size>\n", argv[0]);
         printf("or %s <host> \n", argv[0]);
         printf("   <host>   = host ip address or name\n");
         printf("   <count>  = number of pings\n");
         printf("   <size>   = size of ping packet\n");
         return SHELL_EXIT_ERROR;

  	}


	error=ath_inet_aton(argv[1], &hostaddr);
	if(error != -1)
	{
		printf("Invalid IP address\n");
		return error;
	}
	if (argc == 2)
	{
		count = 1;
		size  = 64;
	}
	else if (argc == 6)
	{
		count = atoi(argv[3]);
		size  = atoi(argv[5]);
	}
	else
	{
         printf("Usage: %s <host> -c <count> -s <size>\n", argv[0]);
         printf("or %s <host> \n", argv[0]);
         return SHELL_EXIT_ERROR;

	}

	if(size > CFG_PACKET_SIZE_MAX_TX)
	{
		printf("Error: Invalid Parameter %s \n", argv[5]);
		printf("Enter size <= %d \n",CFG_PACKET_SIZE_MAX_TX);
		return SHELL_EXIT_ERROR;
	}

  	for (index = 0; index < count; index++)
  	{
	  	error =t_ping((void*) whistle_handle, hostaddr, size);
  		if(error == 0)
  			printf("Ping reply from %s: time<1ms\n", inet_ntoa(A_CPU2BE32(hostaddr),ip_str));
  		else
  			printf("Request timed out\n");

  	}

  	return A_OK;
}


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : wmi_ping6()
* Returned Value  : None
*
* Comments        : Sample function to depict IPv6 ping functionality
*                   Only used with On-Chip IP stack
*END*-----------------------------------------------------------------*/
A_INT32 wmi_ping6(A_INT32 argc, char_ptr argv[] )
{
  	A_INT32 	    error = -1;
    A_UINT8 	    ip6addr[16];
    char    	    ip6_str [48];
    A_UINT32 		count, index, size;

  	if(argc < 2)
  	{
  		 printf("Incomplete parameters\n");
         printf("Usage: %s <host> -c [count] -s [size]\n", argv[0]);
         printf("or %s <host> \n", argv[0]);
         printf("   <host>   = host ip address or name\n");
         printf("   <count>  = number of pings\n");
         printf("   <size>   = size of ping packet\n");
         return SHELL_EXIT_ERROR;

  	}

	error=Inet6Pton(argv[1],ip6addr);
	if(error != 0)
	{
		printf("Invalid IP address\n");
		return error;
	}
	if (argc == 2)
	{
		count = 1;
		size  = 64;
	}
	else if (argc == 6)
	{
		count = atoi(argv[3]);
		size  = atoi(argv[5]);
	}
	else
	{
         printf("Usage: %s <host> -c <count> -s <size>\n", argv[0]);
         printf("or %s <host> \n", argv[0]);
         return SHELL_EXIT_ERROR;

	}

	if(size > CFG_PACKET_SIZE_MAX_TX)
	{
		printf("Error: Invalid Parameter %s \n", argv[5]);
		printf("Enter size <= %d \n",CFG_PACKET_SIZE_MAX_TX);
		return SHELL_EXIT_ERROR;
	}

	for (index = 0; index < count; index++)
	{
  		error =t_ping6((void*) whistle_handle,ip6addr, size);
  		if(error == 0)
  			printf("Ping reply from %s: time<1ms\n", inet6_ntoa((char *)ip6addr, ip6_str));
  		else
  			printf("Request timed out\n");
	}

  	return A_OK;
}

#else

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : wmi_ping()
* Returned Value  : None
*
* Comments        : Sample function to depict IPv4 ping functionality
*                   Only used with RTCS IP stack
*END*-----------------------------------------------------------------*/
int_32 wmi_ping(int_32 argc, char_ptr argv[] )
{
 _ip_address          hostaddr;
   char                 hostname[MAX_HOSTNAMESIZE];
   uint_32              i;
   int_32               time, error;
   uint_16              pingid = 0;
   boolean              print_usage, shorthelp = FALSE;
   int_32               return_code = SHELL_EXIT_SUCCESS;
   A_UINT8               num_dots = 0,param_miss = 0;
   A_UINT8               index = 0;
   uint_32              num_pings = 4;

   print_usage = Shell_check_help_request(argc, argv, &shorthelp );

   if (!print_usage)  {
      if (argc > 1)  {
         memset (hostname, 0, sizeof (hostname));

         index = 1;

         if(strcmp(argv[index],"-c")==0)
         {
         	if(argc != 4)
         	{
         		printf("missing parameters\n");
         		return SHELL_EXIT_ERROR;
         	}
         	else
         	{
         		index++;
         		num_pings = atoi(argv[index]);
         	}

	        index++;
         }
         for (i=0;i<strlen(argv[index]);i++)
         {
         	if(argv[index][i] == '.')
         	 num_dots++;

         	if(num_dots == 3)
         	{
         		if(i == strlen(argv[index])-1)
         		{
         			param_miss = 1;
         		}
         		else
         		{
         			param_miss = 0;
         			break;
         		}
         	}
         }
         if(num_dots != 3 || param_miss != 0)
         {
         	printf("Invalid IP address\n");
         	return SHELL_EXIT_ERROR;
         }

         RTCS_resolve_ip_address( argv[index], &hostaddr, hostname, MAX_HOSTNAMESIZE );

         if (!hostaddr)  {
#if RTCSCFG_ENABLE_DNS | RTCSCFG_ENABLE_LWDNS
            printf("Unable to resolve host\n");
#else
            printf("Unable to resolve host - dns service disabled\n");
#endif
            return_code = SHELL_EXIT_ERROR;

         } else  {
            printf("Pinging %s [%ld.%ld.%ld.%ld]:\n", hostname, IPBYTES(hostaddr));

            for (i=0; i<num_pings ; i++) {
               time = 5000; /* 5 seconds */
               error = RTCS_ping(hostaddr, (uint_32_ptr)&time, ++pingid);
               if (error == RTCSERR_ICMP_ECHO_TIMEOUT) {
                  printf("Request timed out\n");
               } else if (error) {
                  if(error == 0x1000515)
                  {
                     printf("Error 0x%04lX - illegal ip address\n", error);
                  }
                  else if( error == 0x1000510)
                  {
                     printf("Error 0x%04lX - no route to host\n", error);
                  }
                  else
                  {
                     printf("Error 0x%04lX\n", error);
                  }
               } else {
                  if (time < 1)
                  {
                     printf("Reply from %ld.%ld.%ld.%ld: time<1ms\n", IPBYTES(hostaddr));
                  }
                  else
                  {
                     printf("Reply from %ld.%ld.%ld.%ld: time=%ldms\n", IPBYTES(hostaddr), time);
                  }
                  if ((time<1000) && (i<num_pings - 1))
                  {
                     RTCS_time_delay(1000-time);
                  }
               }
            } /* Endfor */
         }
      } else  {
         printf("Error, %s invoked with incorrect number of arguments\n", argv[0]);
         print_usage = TRUE;
      }
   }

   if (print_usage)  {
      if (shorthelp)  {
         printf("%s <host>\n", argv[0]);
         printf("Usage: %s -c <count> <host>\n", argv[0]);
      } else  {
         printf("Usage: %s <host>\n", argv[0]);
         printf("Usage: %s -c <count> <host>\n", argv[0]);
         printf("   <host>   = host ip address or name\n");
         printf("   <count>  = number of pings\n");
      }
   }
   return return_code;


}

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : wmi_ping6()
* Returned Value  : None
*
* Comments        : Sample function to depict IPv6 ping functionality
*                   Not supported on RTCS
*END*-----------------------------------------------------------------*/
A_INT32 wmi_ping6(A_INT32 argc, char_ptr argv[] )
{
   printf("ping6 command not supported ...\n");
   return A_OK;
}
#endif


extern void __boot(void);


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : host_reset()
* Returned Value  : None
*
* Comments        : Sample function to depict how to safely reset the host
*
*END*-----------------------------------------------------------------*/
void host_reset()
{
	uint_32 error;
	ATH_IOCTL_PARAM_STRUCT ath_param;
	uint_32 value, dev_status;
#if PSP_MQX_CPU_IS_KINETIS
        uint_32 read_value = SCB_AIRCR;
#endif
	ath_param.cmd_id = ATH_CHIP_STATE;
	ath_param.length = sizeof(value);
        A_MDELAY(100);

        while((error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status))!=ENET_OK)
        {
            /*Wait for driver to be ready before proceeding*/
        }
	value = 0;
	ath_param.data = &value;
	error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);
	if(error != A_OK){
          printf("IOCTL error\n");
        }
        /*Wait for a short period before reset*/
        A_MDELAY(100);

#if PSP_MQX_CPU_IS_KINETIS
        read_value &= ~SCB_AIRCR_VECTKEY_MASK;
        read_value |= SCB_AIRCR_VECTKEY(0x05FA);
        read_value |= SCB_AIRCR_SYSRESETREQ_MASK;

        _int_disable();
        SCB_AIRCR = read_value;
        while(1)
        	;
#else
        __boot();
#endif
}


#if ENABLE_STACK_OFFLOAD




#define getByte(n, data) ((data>>(8*n))&0xFF)

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : ipconfig_query()
* Returned Value  : A_OK if success else A_ERROR
*
* Comments        : Sample function to depict ipconfig functionality.
*                   Queries IP parameters from target.
*
*END*-----------------------------------------------------------------*/
int_32 ipconfig_query(int_32 argc, char_ptr argv[])
{
	A_UINT32 addr, mask, gw;
	IP6_ADDR_T v6Global;
        IP6_ADDR_T v6GlobalExtd;
	IP6_ADDR_T v6LinkLocal;
	IP6_ADDR_T v6DefGw;
	char ip6buf[48], mac_data[6];
	char *ip6ptr =  NULL;
	A_INT32 LinkPrefix = 0;
	A_INT32 GlobalPrefix = 0;
	A_INT32 DefGwPrefix = 0;
        A_INT32 GlobalPrefixExtd = 0;
    get_mac_addr((char_ptr)mac_data);
    printf("\n mac addr    =   %x:%x:%x:%x:%x:%x \n", mac_data[0], mac_data[1], mac_data[2],
                                                      mac_data[3], mac_data[4], mac_data[5]);

	t_ipconfig((void*)whistle_handle,0, &addr, &mask, &gw);
	printf("IP:%x Mask:%x, Gateway:%x\n", addr, mask, gw);
	printf("IP:%d.%d.%d.%d Mask:%d.%d.%d.%d, Gateway:%d.%d.%d.%d\n",
				getByte(3, addr), getByte(2, addr), getByte(1, addr), getByte(0, addr),
				getByte(3, mask), getByte(2, mask), getByte(1, mask), getByte(0, mask),
				getByte(3, gw), getByte(2, gw), getByte(1, gw), getByte(0, gw));

	memset(&v6LinkLocal,0, sizeof(IP6_ADDR_T));
        memset(&v6Global,0, sizeof(IP6_ADDR_T));
        memset(&v6DefGw,0, sizeof(IP6_ADDR_T));
	memset(&v6GlobalExtd,0, sizeof(IP6_ADDR_T));
	t_ip6config((void *)whistle_handle, 0, &v6Global,&v6LinkLocal,&v6DefGw,&v6GlobalExtd,&LinkPrefix,&GlobalPrefix,&DefGwPrefix,&GlobalPrefixExtd);

        if(v6LinkLocal.addr[0] )
	{
           ip6ptr = print_ip6(&v6LinkLocal,ip6buf);
	   if(ip6ptr)
           {
             if(LinkPrefix)
             printf("Link-local IPv6 Address ..... : %s/%d \n", ip6ptr,LinkPrefix);
	     else
             printf("Link-local IPv6 Address ..... : %s \n", ip6ptr);
	   }
           ip6ptr = NULL;
           ip6ptr = print_ip6(&v6Global,ip6buf);
           if(ip6ptr)
           {
              if(GlobalPrefix)
	      printf("Global IPv6 Address ......... : %s/%d \n", ip6ptr,GlobalPrefix);
	      else
	      printf("Global IPv6 Address ......... : %s \n", ip6ptr);
	    }
	    ip6ptr = NULL;
            ip6ptr = print_ip6(&v6DefGw, ip6buf);
	    if(ip6ptr)
	    {
	      if(DefGwPrefix)
	      printf("Default Gateway  ............. : %s/%d \n", ip6ptr,DefGwPrefix);
	      else
	      printf("Default Gateway  ............. : %s \n", ip6ptr);

	    }
            ip6ptr = NULL;
            ip6ptr = print_ip6(&v6GlobalExtd,ip6buf);
            if(ip6ptr)
            {
              if(GlobalPrefixExtd)
	      printf("Global IPv6 Address 2 ......... : %s/%d \n", ip6ptr,GlobalPrefixExtd);
	      else
	      printf("Global IPv6 Address 2 ......... : %s \n", ip6ptr);
	    }

	}

	return 0;
}


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : ipconfig_static()
* Returned Value  : A_OK if success else A_ERROR
*
* Comments        : Sample function to depict ipconfig functionality.
*                   Sets static IPv4 parameters.
*
*END*-----------------------------------------------------------------*/
int_32 ipconfig_static(int_32 argc, char_ptr argv[])
{
	A_UINT32 addr, mask, gw;
	char *ip6ptr =  NULL;
	A_INT32 LinkPrefix = 0;
	A_INT32 GlobalPrefix = 0;
	A_INT32 DefGwPrefix = 0;


	int_32 error = ENET_OK;
	char* ip_addr_string;
	char* mask_string;
	char* gw_string;
	unsigned int sbits;

	if(argc < 5)
	{
		printf("incomplete params\n");
		return ENET_ERROR;
	}
	ip_addr_string = argv[2];
	mask_string = argv[3];
	gw_string = argv[4];

	error=parse_ipv4_ad(&addr, &sbits, ip_addr_string);
	if(error!=0)
	{
		printf("Invalid IP address\n");
		return error;
	}
	addr = A_BE2CPU32(addr);
	error=parse_ipv4_ad(&mask, &sbits, mask_string);
	if(error!=0)
	{
		printf("Invalid Mask address\n");
		return error;
	}
	mask = A_BE2CPU32(mask);
	error=parse_ipv4_ad(&gw, &sbits, gw_string);
	if(error!=0)
	{
		printf("Invalid Gateway address\n");
		return error;
	}
	gw = A_BE2CPU32(gw);
	t_ipconfig((void*)whistle_handle,1, &addr, &mask, &gw);
	printf("IP:%x Mask:%x, Gateway:%x\n", addr, mask, gw);
	printf("IP:%d.%d.%d.%d Mask:%d.%d.%d.%d, Gateway:%d.%d.%d.%d\n",
				getByte(3, addr), getByte(2, addr), getByte(1, addr), getByte(0, addr),
				getByte(3, mask), getByte(2, mask), getByte(1, mask), getByte(0, mask),
				getByte(3, gw), getByte(2, gw), getByte(1, gw), getByte(0, gw));


	return 0;
}



/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : ipconfig_dhcp()
* Returned Value  : A_OK if success else A_ERROR
*
* Comments        : Sample function to depict ipconfig functionality.
*                   Runs DHCP on target (only IPv4).
*
*END*-----------------------------------------------------------------*/
int_32 ipconfig_dhcp(int_32 argc, char_ptr argv[])
{
	A_UINT32 addr, mask, gw;
	char *ip6ptr =  NULL;
	A_INT32 LinkPrefix = 0;
	A_INT32 GlobalPrefix = 0;
	A_INT32 DefGwPrefix = 0;

	t_ipconfig((void*)whistle_handle,2, &addr, &mask, &gw);
	return 0;
}




#endif
