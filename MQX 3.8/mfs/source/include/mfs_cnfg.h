#ifndef __mfs_cnfg_h__
#define __mfs_cnfg_h__
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
* $FileName: mfs_cnfg.h$
* $Version : 3.8.10.0$
* $Date    : Aug-1-2011$
*
* Comments:
*
*   This file contains the structure definitions and constants for a
*   user who will be compiling programs that will use the Embedded MS-DOS
*   File System (MFS)
*
*END************************************************************************/

/* 
** Can be modified to customize MFS 
*/

/***************************************
**
** Code and Data configuration options
**
** Define MFSCFG_MINIMUM_FOOTPRINT to reducte MFS memory requirements 
**
*/


#if MFSCFG_MINIMUM_FOOTPRINT
   #define MFSCFG_FEATURE_DEFAULT 0
#else
   #define MFSCFG_FEATURE_DEFAULT 1
#endif

#ifndef MFSCFG_READ_ONLY
   #define MFSCFG_READ_ONLY 0
#endif

#ifndef MFSCFG_READ_ONLY_CHECK
   #define MFSCFG_READ_ONLY_CHECK 1
#endif

#ifndef MFSCFG_READ_ONLY_CHECK_ALWAYS
   #define MFSCFG_READ_ONLY_CHECK_ALWAYS 0
#endif

#ifndef MFSCFG_CALCULATE_FREE_SPACE_ON_OPEN
   #define MFSCFG_CALCULATE_FREE_SPACE_ON_OPEN 0 
#endif



/* Only used if formatting. Value must be 1 or 2. 2 is the standard */
#ifndef MFSCFG_NUM_OF_FATS
   #define MFSCFG_NUM_OF_FATS 1 
#endif

/* # of fat sectors in FAT cache. Minimum value of 2 required. */           
#ifndef MFSCFG_FAT_CACHE_SIZE
   #if MFSCFG_MINIMUM_FOOTPRINT
      #define MFSCFG_FAT_CACHE_SIZE 2 
   #else
      #define MFSCFG_FAT_CACHE_SIZE 2 // use the same value; though we don't need to save RAM space, we need to save read/write cycles
   #endif
#endif
 
/* 
** The amount of times MFS will attempt to read or write to the device 
** unsuccessfully before reporting an error
*/
#ifndef MFSCFG_MAX_WRITE_RETRIES
   #define MFSCFG_MAX_WRITE_RETRIES    1
#endif

#ifndef MFSCFG_MAX_READ_RETRIES
   #define MFSCFG_MAX_READ_RETRIES  1
#endif


#ifndef MFSCFG_HANDLE_INITIAL
   #if MFSCFG_MINIMUM_FOOTPRINT
      #define MFSCFG_HANDLE_INITIAL 4
   #else
      #define MFSCFG_HANDLE_INITIAL 16
   #endif
#endif

#ifndef MFSCFG_HANDLE_GROW
   #if MFSCFG_MINIMUM_FOOTPRINT
      #define MFSCFG_HANDLE_GROW 4 
   #else
      #define MFSCFG_HANDLE_GROW 16
   #endif
#endif

#ifndef MFSCFG_HANDLE_MAX
   #if MFSCFG_MINIMUM_FOOTPRINT
      #define MFSCFG_HANDLE_MAX  0 
   #else
      #define MFSCFG_HANDLE_MAX  0
   #endif
#endif

/*
** Maximum # of times to try to rewrite a bad cluster.
*/
#ifndef MFSCFG_MAX_CLUSTER_REMAP_ATTEMPTS
   #define MFSCFG_MAX_CLUSTER_REMAP_ATTEMPTS       0
#endif

#ifndef MFSCFG_FIND_TEMP_TRIALS
   #define MFSCFG_FIND_TEMP_TRIALS      300
#endif


#ifndef MFSCFG_ENABLE_FORMAT
   #define MFSCFG_ENABLE_FORMAT         MFSCFG_FEATURE_DEFAULT
#endif

#ifndef MFSCFG_DEPRICATED_UNINSTALL
   #define MFSCFG_DEPRICATED_UNINSTALL         1
#endif


#endif
