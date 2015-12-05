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
* $FileName: Command_Lists.c$
* $Version : 3.5.16.0$
* $Date    : Dec-2-2009$
*
* Comments:
*
*   
*
*END************************************************************************/

#include "main.h"
#include "throughput.h"
#include "atheros_stack_offload.h"

const SHELL_COMMAND_STRUCT Shell_commands[] = {
   { "exit",      Shell_exit },      
   { "help",      Shell_help }, 
   { "benchtx",   worker_cmd_handler }, //fapp_benchtx_cmd},   
   { "benchrx",   worker_cmd_handler }, //fapp_benchrx_cmd},
   { "benchquit", worker_cmd_quit },
   { "benchmode", worker_cmd_handler }, 
//#if DEMOCFG_ENABLE_RTCS
#if ENABLE_STACK_OFFLOAD
   { "ipconfig",  ipconfig_query },
#else
   { "netstat",   Shell_netstat },     
   { "ipconfig",  Shell_ipconfig },
#endif   
#if DEMOCFG_USE_WIFI   
   { "iwconfig",  worker_cmd_handler }, //wmi_iwconfig },
   { "wmiconfig", worker_cmd_handler }, //wmiconfig_handler },
#endif      
#if RTCSCFG_ENABLE_ICMP
   { "ping",      wmi_ping},      
   { "ping6",     wmi_ping6},
#endif   
//#endif
   { "?",         Shell_command_list },     
   { NULL,        NULL } 
};

const SHELL_COMMAND_STRUCT Telnet_commands[] = {
   { "exit",      Shell_exit },      
   { "help",      Shell_help }, 
#if DEMOCFG_ENABLE_USB_FILESYSTEM
   { "log",       Shell_log },
#endif


#if DEMOCFG_ENABLE_RTCS
#if RTCSCFG_ENABLE_ICMP
   { "ping",      Shell_ping },      
#endif   
#endif   

   { "?",         Shell_command_list },     
   
   { NULL,        NULL } 
};


#if DEMOCFG_ENABLE_RTCS

// ftp root dir
char FTPd_rootdir[] = {"c:\\"}; 

//ftp commands
const FTPd_COMMAND_STRUCT FTPd_commands[] = {
#if FTPDCFG_USES_MFS		
   { "abor", FTPd_unimplemented },
   { "acct", FTPd_unimplemented },
   { "cdup", FTPd_cdup },        
   { "cwd",  FTPd_cd   },        
   { "feat", FTPd_feat },       
   { "help", FTPd_help },       
   { "dele", FTPd_dele },       
   { "list", FTPd_list },       
   { "mkd",  FTPd_mkdir},       
   { "noop", FTPd_noop },
   { "nlst", FTPd_nlst },       
   { "opts", FTPd_opts },
   { "pass", FTPd_pass },
   { "pasv", FTPd_pasv },
   { "port", FTPd_port },
   { "pwd",  FTPd_pwd  },       
   { "quit", FTPd_quit },
   { "rnfr", FTPd_rnfr },
   { "rnto", FTPd_rnto },
   { "retr", FTPd_retr },
   { "stor", FTPd_stor },
   { "rmd",  FTPd_rmdir},       
   { "site", FTPd_site },
   { "size", FTPd_size },
   { "syst", FTPd_syst },
   { "type", FTPd_type },
   { "user", FTPd_user },
   { "xcwd", FTPd_cd    },        
   { "xmkd", FTPd_mkdir },       
   { "xpwd", FTPd_pwd   },       
   { "xrmd", FTPd_rmdir },     
#endif
   { NULL,   NULL } 
};


#endif
/* EOF */
