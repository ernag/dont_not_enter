#include <main.h>
#include <throughput.h>

A_STATUS is_driver_initialized()
{
  A_INT32 error,dev_status; 
  
  error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
  if (ENET_OK != error || dev_status == FALSE)
  {
      return A_ERROR;
  }   
  
  return A_OK;
}


A_STATUS handle_ioctl(ATH_IOCTL_PARAM_STRUCT* inout_param)
{ 
    A_STATUS status = A_OK;

    uint_32 error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,inout_param);

    if (ENET_OK != error)
    {
        status = (error == ENETERR_INPROGRESS)?A_EBUSY:A_ERROR;
    }

    return status;
}

A_STATUS ath_commit(A_UINT32 device)
{
   return(iwcfg_commit(device)); 
}


A_STATUS ath_set_mode(A_UINT32 device,char* mode)
{
    uint_32 error;
    uint_32 inout_param,dev_status;
 
    if (!strcmp(mode,"managed")) 
    {
        inout_param = ENET_MEDIACTL_MODE_INFRA;
    }
    else if (!strcmp(mode,"adhoc")) 
    {
        inout_param = ENET_MEDIACTL_MODE_ADHOC;
    }
    else if (!strcmp(mode,"softap"))
    {
        inout_param = ENET_MEDIACTL_MODE_MASTER;
    }
    else
    {
        printf("Invalid Mode parameter %s. \n",mode);
        return ENET_ERROR;
    }
        
       
        error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status);
        if (ENET_OK != error)
        {
            return error;
        }
        if (dev_status == FALSE)
        {
            return ENET_ERROR;
        }
        
        error = ENET_mediactl (whistle_handle,ENET_SET_MEDIACTL_MODE,&inout_param);
        if (ENET_OK != error)
        {
            return error;
        }
        return ENET_OK;
   
    
    return ENETERR_INVALID_DEVICE;
}

A_STATUS ath_set_essid(A_UINT32 device, char* essid){
  return(iwcfg_set_essid(device,essid)); 
}


A_STATUS ath_set_sec_type(A_UINT32 device, char*  sec_type){
  return(iwcfg_set_sec_type (device,sec_type)); 
}

A_STATUS ath_set_passphrase(A_UINT32 device,char* passphrase){
  return(iwcfg_set_passphrase (device,passphrase)); 
}

A_STATUS ath_send_wep_param(ENET_WEPKEYS* wep_param){
  return(ENET_mediactl (whistle_handle,ENET_SET_MEDIACTL_ENCODE,wep_param));
}


A_STATUS ath_get_essid(A_UINT32 device, char*  essid){
  return(iwcfg_get_essid(device,essid));
}

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : driver_updown_indicator()
* Returned Value  : A_OK - success, A_ERROR - failure
* Comments	: handles the state of driver either up/down
*
*END*-----------------------------------------------------------------*/
A_UINT8 driver_status(A_UINT8 val)
{
    static A_UINT8 driver_down_flag = DRIVER_UP;
    /* if the driver is up set this val to driver_up*/
    if(val == DRIVER_UP)
    {
        driver_down_flag = DRIVER_UP;
    }
    /* if the driver is down set this val to driver_down*/
    else if(val == DRIVER_DOWN)
    {
       driver_down_flag = DRIVER_DOWN;
    }
    /* we will read the driver status from reset fn
       if the driver status indicates down we will
       not allow reset
     */
    else if(val == READ_STATUS)
    {
       return driver_down_flag;
    }
    return driver_down_flag;
}

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : NetConnect()
* Returned Value  : 0 - success, 1 - failure
* Comments	: Handles connection to AP after WPS
*
*END*-----------------------------------------------------------------*/
A_INT32 NetConnect(ATH_NETPARAMS *pNetparams)
{
	A_INT32 status = 1;
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
				printf("Passphrase %s\n",(char*)pNetparams->u.passphrase);
			}
			else if(pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WPA)
		    {
		    	printf("Security type is WPA\n");
				printf("Passphrase %s\n",(char*)pNetparams->u.passphrase);
			}
			else if(pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WEP)
		    {
		    	printf("Security type is WEP\n");
				printf("WEP key %s\n",(char*)pNetparams->u.wepkey);
				printf("Key index %d\n",pNetparams->key_index);
			}
			else if(pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_NONE)
		    {
		    	printf("Security type is None\n");
		    }
		    
		        	    
	    	set_callback();
	    
	    	ATH_SET_ESSID (IPCFG_default_enet_device, (char*)pNetparams->ssid);
	    	
	    	strcpy((char*)original_ssid,(char*)pNetparams->ssid);
	    	
			if(pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WPA2){
					
		        ath_param.cmd_id = ATH_SET_CIPHER;
		    	ath_param.data = &pNetparams->cipher;
		    	ath_param.length = 8;        
		        
		        if(A_OK != ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&ath_param)){
		        	break;
		        }
		        strcpy(ver,"wpa2");
		        ATH_SET_SEC_TYPE (IPCFG_default_enet_device,ver); 
				
		        if(strlen((char*)pNetparams->u.passphrase) != 64)
				{								
					ATH_SET_PASSPHRASE (IPCFG_default_enet_device,(char*)pNetparams->u.passphrase);
				}
				else
				{
					if(A_OK != handle_pmk((char*)pNetparams->u.passphrase))
						break;
				}
			}else if (pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WPA){
			
											
				
		        ath_param.cmd_id = ATH_SET_CIPHER;
		    	ath_param.data = &pNetparams->cipher;
		    	ath_param.length = 8;        
		        
		        if(A_OK != ENET_mediactl (whistle_handle,ENET_MEDIACTL_VENDOR_SPECIFIC,&ath_param)){
		        	break;
		        }
		        strcpy(ver,"wpa");
		        ATH_SET_SEC_TYPE (IPCFG_default_enet_device,ver);
		        if(strlen((char*)pNetparams->u.passphrase) != 64)
				{								
					ATH_SET_PASSPHRASE (IPCFG_default_enet_device,(char*)pNetparams->u.passphrase);
				}
				else
				{
					if(A_OK != handle_pmk((char*)pNetparams->u.passphrase))
						break;
				}
			}else if (pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WEP){
			
				ATH_SET_SEC_TYPE (IPCFG_default_enet_device,"wep");
				set_wep_keys (IPCFG_default_enet_device,(char*)pNetparams->u.wepkey,
		      	NULL,NULL,NULL,strlen((char*)pNetparams->u.wepkey),pNetparams->key_index);
			
			}else if (pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_NONE){
			//	ATH_SET_SEC_TYPE (IPCFG_default_enet_device,"none");
			}
				
			//iwcfg_set_mode (IPCFG_default_enet_device,"managed");
		    ATH_COMMIT(IPCFG_default_enet_device);
		    status = 0;
		    
		}
	
	}while(0);
	
	return status;
}

A_INT32 CompleteWPS(ATH_NETPARAMS *pNetparams, A_UINT8 block, mode_t mode_flag)
{
	A_UINT32 status = 0;
	ATH_IOCTL_PARAM_STRUCT param;
    A_STATUS error;
	
	pNetparams->error = 0;
    
    pNetparams->dont_block = (block)? 0:1;
    
    param.cmd_id = ATH_AWAIT_WPS_COMPLETION;  
    param.data = pNetparams;
    param.length = sizeof(*pNetparams);
     
    /* this will block until the WPS operation completes or times out */
       error = HANDLE_IOCTL (&param);
      if (MODE_STATION == mode_flag) {
           pNetparams->error = (pNetparams->error)?pNetparams->error:A_EBUSY;
       }
       
    do{

        if (A_EBUSY == error)
	    {
	        break;
	    }
	    
	    status =1;
	    
	    if(A_OK != error){
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

                case ATH_WPS_ERROR_PWD_AUTH_FAIL:
                printf("WPS error: AUTH FAIL\n");
                break;

                case ATH_WPS_ERROR_CANCELLED:
                printf("WPS error: WPS CANCEL\n");
                break;

                case ATH_WPS_ERROR_INVALID_PIN:
                printf("WPS error: INVALID PIN\n");
                break;

	        	default:
	        	//printf("WPS error: unknown %d\n",pNetparams->error);
	        	break;
	        }

	    }
    }while(0);
    
    if (MODE_AP == mode_flag) {

        if (0x00 == pNetparams->error && A_OK == error) {
            printf("\n***** WPS PROFILE ****\n %s\n", pNetparams->ssid);
            printf("SSID received %s\n", pNetparams->ssid);
            if (pNetparams->sec_type == ENET_MEDIACTL_SECURITY_TYPE_WPA2)
            {
                printf ("Security type is WPA2, Encryption Type is AES\n");
                printf ("Passphrase %s\n",(char*)pNetparams->u.passphrase);
            }
            else
            {
                printf ("Security type is Open\n");
            }
            status = 0;
        }
        else
        {
            
            pNetparams->error = (pNetparams->error)?pNetparams->error:A_EBUSY;
            status = 0;
        }

    }

    return status;
}



void application_frame_cb(A_VOID *ptr);
void application_frame_cb(A_VOID *ptr)
{	
	A_UINT16 i,print_length,j=0;
	A_UINT8 *pData;
	
	
	print_length = 32;
	printf("frame (%d):\n", A_NETBUF_LEN(ptr));
	/* only print the first 64 bytes of each frame */
	if(A_NETBUF_LEN(ptr) < print_length) 
		print_length = A_NETBUF_LEN(ptr);
	
	
	pData = A_NETBUF_DATA(ptr);
	
	for(i=0 ; i<print_length ; i++){
		printf("0x%02x, ", pData[i]);
		if(j++==7){
			j=0;
			printf("\n");
		}
	}
	
	if(j){
		printf("\n");
	}


	A_NETBUF_FREE(ptr);
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

A_INT32 ath_driver_state( A_INT32 argc, char* argv[])
{
    A_INT32 error;
    ATH_IOCTL_PARAM_STRUCT ath_param;
    A_UINT32 value;
    
    do{
        if(argc != 1) break;
        
        ath_param.cmd_id = ATH_CHIP_STATE;
        ath_param.length = sizeof(value);
        //value = atoi(argv[0]);
        
        if(strcmp(argv[0],"up") == 0)
        {
            value = 1;
            
            ath_param.data = &value;		   
            error = HANDLE_IOCTL (&ath_param);
            
            if(error == A_OK){			
                    driver_status(DRIVER_UP);
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
            error = HANDLE_IOCTL (&ath_param);
            
            if(error == A_OK){			
                    driver_status(DRIVER_DOWN);
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
            error = HANDLE_IOCTL ( &ath_param);
            
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
            if((error = HANDLE_IOCTL (&ath_param)) != A_OK)
                    return A_ERROR;	
            
            value = 1;
            ath_param.data = &value;
            if((error = HANDLE_IOCTL (&ath_param)) != A_OK)
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
            if((error = HANDLE_IOCTL (&ath_param)) != A_OK)
                    return A_ERROR;	
            
            value = 1;
            ath_param.data = &value;
            if((error = HANDLE_IOCTL (&ath_param)) != A_OK)
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
* Function Name   : ath_reg_query()
* Returned Value  : A_OK - on successful completion
*					A_ERROR - on any failure. 
* Comments        : reads / writes AR600X registers.
*
*END*------------------------------------------------------------------------*/
A_INT32 ath_reg_query( A_INT32 argc, char* argv[])
{
	A_INT32 error;
	A_UINT8 print_result=0;
	ATH_IOCTL_PARAM_STRUCT ath_param;
	ATH_REGQUERY ath_regquery;

        /*Check if driver is loaded*/
        if(IS_DRIVER_READY != A_OK){
            return A_ERROR;
        } 
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
		
		if(error == A_OK){
			if(print_result){
				printf("value=%08x\n", ath_regquery.value);
			}
			return A_OK;
		}else{
			printf("driver ioctl error\n");
			return A_ERROR;
		}
	}while(0);
	
	printf("param error: register query\n");
	return A_ERROR; 
}

/*FUNCTION*--------------------------------------------------------------------
*
* Function Name   : ath_mem_query()
* Returned Value  : A_OK - on successful completion
*					A_ERROR - on any failure. 
* Comments        : reads / writes AR600X registers.
*
*END*------------------------------------------------------------------------*/
A_INT32 ath_mem_query( A_INT32 argc, char* argv[])
{
	A_INT32 error;
	uint_8 print_result=0;
	ATH_IOCTL_PARAM_STRUCT ath_param;
	ATH_MEMQUERY ath_memquery;

	do{									
		ath_param.cmd_id = ATH_MEM_QUERY;
	    ath_param.data = &ath_memquery;
	    ath_param.length = sizeof(ath_memquery);
	    
		error = ENET_mediactl (whistle_handle, ENET_MEDIACTL_VENDOR_SPECIFIC, &ath_param);
		
		printf("firmware memory stats:\n");
		
		if(error == A_OK){
			
			printf("literals = %d\n", ath_memquery.literals);
			
			printf("rodata = %d\n", ath_memquery.rodata);
			
			printf("data = %d\n", ath_memquery.data);
		
			printf("bss = %d\n", ath_memquery.bss);
			
			printf("text = %d\n", ath_memquery.text);
			
			printf("heap remaining = %d\n", ath_memquery.heap);
														
			return A_OK;
		}else{
			printf("driver ioctl error\n");
			return A_ERROR;
		}
	}while(0);
	
	printf("param error: register query\n");
	return A_ERROR; 
}



/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : set_power_mode
* Returned Value  : A_OK if success else ERROR 
*                   
* Comments        : Sets current power mode to Power-Save or Max-Perf
*
*END*-----------------------------------------------------------------*/
A_INT32 set_power_mode(char* pwr_mode)
{
	int mode;
	
	mode = atoi(pwr_mode);
	
	if (mode != 0 && mode != 1)
	{
		printf("Invalid power mode\n");
		return A_ERROR;
	}	
	
	return(iwcfg_set_power(DEMOCFG_DEFAULT_DEVICE,mode,0));
}


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : get_power_mode
* Returned Value  : A_OK if success else ERROR 
*                   
* Comments        : Gets current power mode
*
*END*-----------------------------------------------------------------*/
A_INT32 get_power_mode(char* pwr_mode)
{	
	ENET_MEDIACTL_PARAM inout_param;
    A_INT32 error = A_OK;
 
   /*Check if driver is loaded*/
    if(IS_DRIVER_READY != A_OK){
        return A_ERROR;
    }     

    error = ENET_mediactl (whistle_handle,ENET_GET_MEDIACTL_POWER,&inout_param);
    if (A_OK != error)
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


A_INT32 wmi_set_scan(A_UINT32 dev_num)
{
    A_INT32 error,i=0;
    ENET_SCAN_LIST  param;
    A_UINT8 temp_ssid[33];

    /*Check if driver is loaded*/
    if(IS_DRIVER_READY != A_OK){
        return A_ERROR;
    }     
    if (dev_num < IPCFG_DEVICE_COUNT)
    {
      
        error = ENET_mediactl (whistle_handle,ENET_SET_MEDIACTL_SCAN,NULL);
        if (A_OK != error)
        {
            return error;
        }

        error = ENET_mediactl (whistle_handle,ENET_GET_MEDIACTL_SCAN,&param);
        if (A_OK != error)
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
        return A_OK;
    }
    printf("IWCONFIG_ERROR: Invalid Device number\n");
    return ENETERR_INVALID_DEVICE;
}


typedef struct{
#define SENTINAL 0xa55a1234
	A_UINT32 sentinal;
	int_8 ssid[33];
	A_UINT8 ipaddr[4];
	int_8 passphrase[128];
}FLASH_CONTENTS;

A_INT32 test_flash(void)
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
 //  uchar*       write_buffer;
 //  uchar*       read_buffer;
//   uchar*       old_buffer;
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
      (A_UINT32 _PTR_)&info_ptr);      
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
	return A_OK;
}


extern void __boot(void);
extern A_UINT8 bench_quit;


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : SystemReset()
* Returned Value  : None
*
* Comments        : Sample function to depict how to safely reset the host 
*
*END*-----------------------------------------------------------------*/
void SystemReset()
{
	A_INT32 error;
	ATH_IOCTL_PARAM_STRUCT ath_param;
	A_UINT32 value, dev_status;
#if PSP_MQX_CPU_IS_KINETIS        
        A_UINT32 read_value = SCB_AIRCR;
#endif 
	/* Fix for Ev 115606
	 * If benchrx is already started and reset is given,
	 * stop the benchrx thread by making bench_quit = 1 */        
	bench_quit = 1;
	ath_param.cmd_id = ATH_CHIP_STATE;
	ath_param.length = sizeof(value);
        A_MDELAY(100);
        if(driver_status(READ_STATUS) != DRIVER_UP)
        {
            printf("driver is down reset issued only in driver up state \n");
            return;
        }

        while((error = ENET_mediactl (whistle_handle,ENET_MEDIACTL_IS_INITIALIZED,&dev_status))!=A_OK)
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
        while(1);
#else
        __boot();
#endif   
}





A_UINT32 ext_scan()
{
	A_INT32 error,i=0;
	ATH_IOCTL_PARAM_STRUCT inout_param;   
	ATH_GET_SCAN param;
	A_UINT8 temp_ssid[33];
	
    /*Check if driver is loaded*/
    if(IS_DRIVER_READY != A_OK){
        return A_ERROR;
    } 
		inout_param.cmd_id = ATH_START_SCAN_EXT;
		inout_param.data = NULL;
			   
		 
		error = HANDLE_IOCTL( &inout_param);
		
		if (A_OK != error)
		{
			return error;
		}
		
		inout_param.cmd_id = ATH_GET_SCAN_EXT;
		inout_param.data = (A_VOID*)&param;
			
		error = HANDLE_IOCTL(&inout_param);
		
		if (A_OK != error)
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
	   
		return A_OK;	
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
A_INT32 wmi_ping(A_INT32 argc, char* argv[] )
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
         return A_ERROR;

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
         return A_ERROR;

	}

	if(size > CFG_PACKET_SIZE_MAX_TX)
	{
		printf("Error: Invalid Parameter %s \n", argv[5]);
		printf("Enter size <= %d \n",CFG_PACKET_SIZE_MAX_TX);
		return A_ERROR;
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
A_INT32 wmi_ping6(A_INT32 argc, char* argv[] )
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
         return A_ERROR;

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
         return A_ERROR;

	}

	if(size > CFG_PACKET_SIZE_MAX_TX)
	{
		printf("Error: Invalid Parameter %s \n", argv[5]);
		printf("Enter size <= %d \n",CFG_PACKET_SIZE_MAX_TX);
		return A_ERROR;
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
A_INT32 wmi_ping(A_INT32 argc, char* argv[] )
{
 _ip_address          hostaddr;
   char                 hostname[MAX_HOSTNAMESIZE];
   A_UINT32              i;
   A_INT32               time, error;
   A_UINT16              pingid = 0;
   boolean              print_usage, shorthelp = FALSE;
   A_INT32               return_code = A_OK;
   A_UINT8               num_dots = 0,param_miss = 0;
   A_UINT8               index = 0;
   A_UINT32              num_pings = 4;
     
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
         		return A_ERROR;
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
         	return A_ERROR;
         }
         
         RTCS_resolve_ip_address( argv[index], &hostaddr, hostname, MAX_HOSTNAMESIZE ); 

         if (!hostaddr)  {
#if RTCSCFG_ENABLE_DNS | RTCSCFG_ENABLE_LWDNS
            printf("Unable to resolve host\n");
#else
            printf("Unable to resolve host - dns service disabled\n");
#endif            
            return_code = A_ERROR;

         } else  {
            printf("Pinging %s [%ld.%ld.%ld.%ld]:\n", hostname, IPBYTES(hostaddr));

            for (i=0; i<num_pings ; i++) {
               time = 5000; /* 5 seconds */
               error = RTCS_ping(hostaddr, (A_UINT32*)&time, ++pingid);
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
A_INT32 wmi_ping6(A_INT32 argc, char* argv[] )
{
   printf("ping6 command not supported ...\n");
   return A_OK;
}
#endif
