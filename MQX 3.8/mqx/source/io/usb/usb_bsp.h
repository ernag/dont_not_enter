/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved
*
* Copyright (c) 2004-2008 Embedded Access Inc.;
* All Rights Reserved
*
* Copyright (c) 1989-2008 ARC International;
* All Rights Reserved
*
*************************************************************************** 
*
* THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR 
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
* IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
* THE POSSIBILITY OF SUCH DAMAGE.
*
**************************************************************************
*
* $FileName: usb_bsp.h$
* $Version : 3.8.13.0$
* $Date    : Aug-30-2011$
*
* Comments:
*
*   Include this header file to use BSP-specific USB initialization code
*
*END************************************************************************/

#ifndef __usb_bsp_h__
#define __usb_bsp_h__ 1

#ifdef __cplusplus
extern "C" {
#endif

_mqx_int _bsp_usb_dev_init (pointer);
_mqx_int _bsp_usb_host_init (pointer);
pointer _bsp_get_usb_base(uint_8 dev_num);
pointer _bsp_get_usb_capability_register_base(uint_8 dev_num);
pointer _bsp_get_usb_timer_register_base(uint_8 dev_num);
pointer _bsp_get_usb_otg_csr(uint_8 dev_num);
uint_8 _bsp_get_usb_vector(uint_8 dev_num);
#if MCF51MM_REV2_USB_PATCH || MCF51JE_REV2_USB_PATCH
uint_8 _bsp_get_usb_shadow_vector(uint_8 dev_num);
#endif

#ifdef __cplusplus
}
#endif

#endif

