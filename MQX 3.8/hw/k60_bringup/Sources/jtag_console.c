/*
 * jtag_console.c
 *
 *  Created on: Feb 26, 2013
 *      Author: Ernie Aguilar
 *      Used to bring the Corona Console to JTAG.
 */

#include "jtag_console.h"
#include "corona_console.h"
#include "PE_Types.h"
#include <stdio.h>


#define STDIN_CONSOLE_BUF_SZ 	128

void jtag_console(void)
{
	cc_handle_t handle;
	char buf[STDIN_CONSOLE_BUF_SZ];

	printf("\n\r\n*** Corona Console (ChEOS)***\n\r\n");
	cc_init(&handle, NULL, CC_TYPE_JTAG);

	while( 1 )
	{
		memset(buf, 0, STDIN_CONSOLE_BUF_SZ);
		// printf("]");
		gets((char *)buf);
		if(buf[0])
		{
			// printf("%s", buf);
		}
		cc_process_cmd(&handle, buf);
	}
}

