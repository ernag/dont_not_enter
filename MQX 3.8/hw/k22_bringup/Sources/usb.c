/*
 * usb.c
 *
 *  Created on: Jan 23, 2013
 *      Author: Ernie Aguilar
 */

#include "usb.h"
#include "k22_irq.h"
#include "USB_Corona.h"
#include "PE_LDD.h"

static bool CallbackCalled = FALSE;

void Callback(LDD_TDeviceData *pDevData, LDD_USB_Device_TTD* pTD)
{
	printf("This idiotic callback was called.\n");
	CallbackCalled = TRUE;
}

k2x_error_t usb_test(void)
{
	LDD_TDeviceData *MyUSBPtr;
	LDD_TError Error;
	LDD_USB_Device_TTD TrDescr;
	LDD_USB_Device_TTD DataTD;
	uint8_t DataBuffer[256];
	
	printf("Corona USB Test\n");
	k22_enable_irq(IRQ_USB_OTG, VECTOR_USB_OTG, &USB_Corona_USB_Interrupt);
	MyUSBPtr = USB_Corona_Init((LDD_TUserData *)NULL);        /* Initialize the device */
	USB_Corona_Enable(MyUSBPtr);
	
	  /* Wait until the Device is configured by the Host */
	//while(1){}

	  DataTD.Head.EpNum      = 2;
	  DataTD.Head.BufferPtr  = &DataBuffer;
	  DataTD.Head.BufferSize = sizeof(DataBuffer);
	  DataTD.Head.Flags      = LDD_USB_DEVICE_TRANSFER_FLAG_EXT_PARAM;
	  DataTD.CallbackFnPtr   = &Callback;
	  DataTD.ParamPtr        = (uint8_t*)0;

	  Error = USB_Corona_DeviceSendData(MyUSBPtr, &DataTD);
	  printf("Error: 0x%x\n", Error);

	  /* Do other things */

	  while (!CallbackCalled) {}; /* Wait for callback */
	  if (DataTD.TransferState != LDD_USB_TRANSFER_DONE) {
	    /* Transfer error */

	  }

#if 0
	/* Wait until the Device is configured by the Host */
	
	DataTD.Head.EpNum = 2;
	DataTD.Head.BufferPtr = &DataBuffer;
	DataTD.Head.Flags = LDD_USB_DEVICE_TRANSFER_FLAG_EXT_PARAM;
	DataTD.Head.BufferSize = sizeof(DataBuffer);
	
	Error = USB_Corona_DeviceRecvData(MyUSBPtr, &DataTD);
	if(Error)
	{
		printf("USB Error!!  0x%x\n", Error);
		return ERR_USB_RCV;
	}
	
	while (DataTD.TransferState >= LDD_USB_TRANSFER_QUEUED) {}; /* Wait for transfer done */
	if (DataTD.TransferState != LDD_USB_TRANSFER_DONE) {
	/* Transfer error */
	
		printf("USB Transfer Error!\n");
	} else {
	/* Transfer OK */
	
		printf("USB Transfer OK!\n");
	}
#endif
	return SUCCESS;
}
