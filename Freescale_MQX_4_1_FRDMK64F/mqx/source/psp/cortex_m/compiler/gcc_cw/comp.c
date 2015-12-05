
/*HEADER**********************************************************************
*
* Copyright 2008-2012 Freescale Semiconductor, Inc.
*
* This software is owned or controlled by Freescale Semiconductor.
* Use of this software is governed by the Freescale MQX RTOS License
* distributed with this Material.
* See the MQX_RTOS_LICENSE file distributed for more details.
*
* Brief License Summary:
* This software is provided in source form for you to use free of charge,
* but it is not open source software. You are allowed to use this software
* but you cannot redistribute it or derivative works of it in source form.
* The software may be used only in connection with a product containing
* a Freescale microprocessor, microcontroller, or digital signal processor.
* See license agreement file for full license terms including other restrictions.
*****************************************************************************
*
* Comments:
*
*   This file contains runtime support for the CodeWarrior GCC.
*
*
*END************************************************************************/

#include "mqx.h"


/* CodeWarrior init function prototypes */
void __init_hardware(void);
void __init_registers(void);


void *malloc(_mem_size);
void *calloc(_mem_size, _mem_size);
void free(void *);
void __thumb_startup(void);
void _exit(int);


extern void init_hardware(void);

#if MQX_ENABLE_CPP || BSPCFG_ENABLE_CPP
extern void __fini_cpp(void);
extern void __destroy_global_chain(void);
#endif /* MQX_ENABLE_CPP || BSPCFG_ENABLE_CPP */


/*!
 * \brief Function __init_hardware() overloads its default instance in EWL Runtime 
 *  library. It calls function init_hardware() which initializes CPU registers 
 *  .
 */
void __init_hardware(void)
{
    /* Initialize device. */
    init_hardware();
}

/*!
 * \brief Empty function __init_registers() overloads its default instance in EWL 
 *  Runtime library. Registers are already initialized in __boot
 */
void __init_registers(void)
{
    /* this is an empty function */
}

/*!
 * \brief Override C/C++ runtime heap allocation function
 *
 * \param bytes
 *
 * \return pointer
 */
void *malloc(_mem_size bytes)
{
    return _mem_alloc_system(bytes);
}

/*!
 * \brief Override C/C++ runtime heap deallocation
 *
 * \param n
 * \param z
 *
 * \return pointer
 */
void *calloc(_mem_size n, _mem_size z)
{
    return _mem_alloc_system_zero(n*z);
}

/*!
 * \brief Override C/C++ runtime heap deallocation function
 * 
 * \param p 
 */
void free(void *p)
{
    _mem_free(p);
}


/*!
 * \brief Replacement for Codewarrior's exit function
 * 
 * \param status 
 *
 * \return should not return
 */
#if MQX_EXIT_ENABLED
void _exit(int status)
{
#if MQX_ENABLE_CPP || BSPCFG_ENABLE_CPP
    /* Semi-normal CodeWarrior shutdown sequence */
    __destroy_global_chain();
    __fini_cpp();
#endif /* MQX_ENABLE_CPP || BSPCFG_ENABLE_CPP */

    /*
    ** Change for whatever is appropriate for your board
    ** and application.  Perhaps a software reset or
    ** some kind of trap/call to the kernel soft re-boot.
    */
    while (TRUE) {
    }
}

#else  /* MQX_EXIT_ENABLED */

void _exit(int status)
{
    ;
}

#endif /* MQX_EXIT_ENABLED */

/*!
 * \brief Perform necessary toolchain startup routines before main()
 */
void toolchain_startup(void)
{
    __thumb_startup();
}


/* EOF */
