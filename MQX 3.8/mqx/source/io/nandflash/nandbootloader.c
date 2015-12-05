/**HEADER********************************************************************
* 
* Copyright (c) 2010 Freescale Semiconductor;
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
* $FileName: nandbootloader.c$
* $Version : 3.8.2.1$
* $Date    : Dec-7-2011$
*
* Comments:
*
*   
*
*END************************************************************************/

#include "mqx.h"
#include "bsp.h"
#include "fio.h"
#include "fio_prv.h"
#include "io.h"
#include "nandbootloader.h"
#include "bootstrap.h"


static int_32 store_table_to_nand_flash(MQX_FILE_PTR, uchar_ptr );
static int_32 check_sum_image
(
   MQX_FILE_PTR   nandflash_hdl,
   IMAGE_INFO_PTR image_ptr
);
uchar_ptr _image_data = (pointer)(__START_IMAGE_DATA + sizeof(IMAGE_HEADER));

#pragma section ".image_data"
__declspec(section ".image_data")  uchar_ptr _image_data; 

/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : allocate_buffer
* Returned Value   : 
* Comments         :
*    
*
*END*----------------------------------------------------------------------*/
uchar_ptr allocate_buffer(_mem_size buffer_size, char_ptr string) 
{
    uchar_ptr buffer_ptr;
    
    buffer_ptr = _mem_alloc_zero(buffer_size);
    
    if (buffer_ptr == NULL) {
        printf("\nFailed to get %s buffer", string);
    } 
    return buffer_ptr;          
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _bootloader_init_table
* Returned Value   : 
* Comments         :
*    
*
*END*----------------------------------------------------------------------*/
int_32 _bootloader_init_table(void)
{
   IMAGE_INFO_PTR      image;
   MQX_FILE_PTR        nandflash_hdl;
   uchar_ptr            read_buffer;
   int_32              result = -1;
   uint_32             i,check_sum = 0;
   boolean             image_found;
   
   /* Open the flash device */
   nandflash_hdl = fopen("nandflash:", NULL);
   if (nandflash_hdl == NULL) {
        printf("\nUnable to open NAND flash device");
        return -1;
   }
   read_buffer = allocate_buffer(IMAGE_TABLE_SIZE, "read");
   image = (IMAGE_INFO_PTR)read_buffer;
   image_found = FALSE;
   
   if(NANDFLASHERR_BLOCK_BAD == ioctl(nandflash_hdl, NANDFLASH_IOCTL_CHECK_BLOCK, (void*)IMAGE_TABLE_PAGE_LOCATION)) {
      printf("\nBlock %d is bad, please use another bock as table images!",IMAGE_TABLE_PAGE_LOCATION);
   }
   
   fseek(nandflash_hdl, IMAGE_TABLE_PAGE_LOCATION, IO_SEEK_SET);
   read(nandflash_hdl, read_buffer,IMAGE_TABLE_SIZE/NAND_FLASH_PAGE_SIZE);
   
   while(image->IS_IMAGE == IMAGE_IN_TABLE_MASK) {
         image_found = TRUE;
         result = MQX_OK;
         image++;
   }
   
   image = (IMAGE_INFO_PTR)read_buffer;
   while(image->IS_IMAGE == IMAGE_IN_TABLE_MASK) {
      if(image->INDEX != IMAGE_MASK_AS_DELETE){
         if(CHECK_SUM_FAIL == check_sum_image(nandflash_hdl,image)) {
            printf("\nImage %d is corrupted !",image->INDEX);
         }
      }
         image++;
   }
   
   
   image = (IMAGE_INFO_PTR)read_buffer;
   if(!image_found){
      image->UNUSED = IMAGE_MASK_AS_DELETE;
      image->INDEX = IMAGE_MASK_AS_DELETE;
      image->ADDR = 0;
      image->SIZE = 0;
      image->BLOCK = 0;
      image->BLOCK_NUM = 0;
      image->IS_IMAGE = IMAGE_IN_TABLE_UNMASK;
      
      if(MQX_OK == store_table_to_nand_flash(nandflash_hdl,read_buffer)) {
         result = MQX_OK;
      }
      else{
         result = IO_ERROR;
      }
   }
   
   fclose(nandflash_hdl);
   free(read_buffer);
   return result;
}

/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : store_table_to_nand_flash
* Returned Value   : 
* Comments         :
*    
*
*END*----------------------------------------------------------------------*/
static int_32 store_table_to_nand_flash
(
   MQX_FILE_PTR nandflash_hdl,
   uchar_ptr    table_ptr
)
{
   if(nandflash_hdl == NULL) {
      printf("\nNAND flash device not opened");
      return IO_ERROR; 
   }
   
   if(MQX_OK == ioctl(nandflash_hdl, NANDFLASH_IOCTL_ERASE_BLOCK, (void*)IMAGE_TABLE_BLOCK_LOCATION)) {
      /* Write table to Nand Flash */
      fseek(nandflash_hdl, IMAGE_TABLE_PAGE_LOCATION, IO_SEEK_SET);
      if(IMAGE_TABLE_SIZE/NAND_FLASH_PAGE_SIZE == write(nandflash_hdl, table_ptr,IMAGE_TABLE_SIZE/NAND_FLASH_PAGE_SIZE)) {
         return MQX_OK;
      }
      else{
         printf("\nWrite table to Nand Flash Failed ");
         return IO_ERROR;
      }
    }
    printf("\nErase Block:%d Failed ",IMAGE_TABLE_BLOCK_LOCATION);
    return IO_ERROR;
}

/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _bootloader_init_table
* Returned Value   : 
* Comments         :
*    
*
*END*----------------------------------------------------------------------*/
static int_32 check_sum_image
(
   MQX_FILE_PTR   nandflash_hdl,
   IMAGE_INFO_PTR image_ptr
)
{
   uint_32           i,check_sum = 0;
   IMAGE_HEADER      img_hdr;
   
   if(nandflash_hdl == NULL) {
      printf("\nNAND flash device not opened");
      return IO_ERROR; 
   }
   
   fseek(nandflash_hdl, image_ptr->BLOCK*256, IO_SEEK_SET);
   read(nandflash_hdl, _image_data,1);
   
   img_hdr = *((IMAGE_HEADER_PTR)_image_data);
   
   fseek(nandflash_hdl, image_ptr->BLOCK*256, IO_SEEK_SET);
   read(nandflash_hdl, _image_data,(image_ptr->BLOCK_NUM*256));
   
   _image_data += sizeof(IMAGE_HEADER);
   
   for(i = 0; i < img_hdr.LEN; i++) {
      check_sum += _image_data[i];
   }
   
   if(check_sum == img_hdr.CHECKSUM) {
      return CHECK_SUM_OK;
   }
   return CHECK_SUM_FAIL;
}
/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _bootloader_init_table
* Returned Value   : 
* Comments         :
*    
*
*END*----------------------------------------------------------------------*/
int_32 _bootloader_set_default(uint_32 index)
{
   IMAGE_INFO_PTR      image;
   MQX_FILE_PTR        nandflash_hdl;
   uchar_ptr            read_buffer;
   int_32              result = -1;
   uint_32             i;
   boolean             image_found;
   
   /* Open the flash device */
   nandflash_hdl = fopen("nandflash:", NULL);
   if (nandflash_hdl == NULL) {
        printf("\nUnable to open NAND flash device");
        return -1;
   }
   read_buffer = allocate_buffer(IMAGE_TABLE_SIZE, "read");
   image = (IMAGE_INFO_PTR)read_buffer;
   image_found = FALSE;
   fseek(nandflash_hdl, IMAGE_TABLE_PAGE_LOCATION, IO_SEEK_SET);
   read(nandflash_hdl, read_buffer,IMAGE_TABLE_SIZE/NAND_FLASH_PAGE_SIZE);
   
   while(image->IS_IMAGE == IMAGE_IN_TABLE_MASK) {
      if(index == image->INDEX){
        image->BOOT_DEFAULT = TRUE;
      }
      else {
         image->BOOT_DEFAULT = FALSE;
      }
      image_found = TRUE;
      image++;
   }
   
 
   if(!image_found){
      printf("\n No image found !");
      return MQX_OK;
   }
   image = (IMAGE_INFO_PTR)read_buffer;

   if(MQX_OK == store_table_to_nand_flash(nandflash_hdl,read_buffer)) {
      result = MQX_OK;
   }
   else{
      result = IO_ERROR;
   }
   fclose(nandflash_hdl);
   free(read_buffer);
   return result;
}
/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _bootloader_list_image
* Returned Value   : 
* Comments         :
*    
*
*END*----------------------------------------------------------------------*/
boolean _bootloader_check_image(boolean autoboot, uint_32 index)
{
   uchar_ptr           read_buffer;
   MQX_FILE_PTR        nandflash_hdl;
   IMAGE_INFO_PTR      image;
   uint_32             i,image_num = 0;
   
   /* Open the flash device */
   read_buffer = allocate_buffer(IMAGE_TABLE_SIZE, "read");
   nandflash_hdl = fopen("nandflash:", NULL);
   if (nandflash_hdl == NULL) {
        printf("\nUnable to open NAND flash device");
        return FALSE;
   }
   image = (IMAGE_INFO_PTR)read_buffer;
   
   fseek(nandflash_hdl, IMAGE_TABLE_PAGE_LOCATION, IO_SEEK_SET);
   read(nandflash_hdl, read_buffer,IMAGE_TABLE_SIZE/NAND_FLASH_PAGE_SIZE);
   while(image->IS_IMAGE == IMAGE_IN_TABLE_MASK) {
      if(autoboot&&image->BOOT_DEFAULT) {
         return TRUE;
      }
      else
      if(index == image->INDEX) {
         return TRUE;
      }
      image++;
   }
   
   fclose(nandflash_hdl);
   free(read_buffer);
   return FALSE;
}
/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _bootloader_list_image
* Returned Value   : 
* Comments         :
*    
*
*END*----------------------------------------------------------------------*/
void _bootloader_list_image(void)
{
   uchar_ptr            read_buffer;
   MQX_FILE_PTR        nandflash_hdl;
   IMAGE_INFO_PTR      image;
   uint_32             i,image_num = 0;
   
   /* Open the flash device */
   read_buffer = allocate_buffer(IMAGE_TABLE_SIZE, "read");
   nandflash_hdl = fopen("nandflash:", NULL);
   if (nandflash_hdl == NULL) {
        printf("\nUnable to open NAND flash device");
        return;
   }
   image = (IMAGE_INFO_PTR)read_buffer;
   
   fseek(nandflash_hdl, IMAGE_TABLE_PAGE_LOCATION, IO_SEEK_SET);
   read(nandflash_hdl, read_buffer,IMAGE_TABLE_SIZE/NAND_FLASH_PAGE_SIZE);
   while(image->IS_IMAGE == IMAGE_IN_TABLE_MASK) {
      image_num++;
      image++;
   }
   printf("Table of stored images:\n");
   printf("====================================================================================\n");
   printf("| Index |      Name      |  Start address  |    Size    |  Flash Addr  | Auto Boot |\n");
   printf("====================================================================================\n");
   
   for(i = 1; i <= image_num; i++) {
      image = (IMAGE_INFO_PTR)read_buffer;
      while(image->IS_IMAGE == IMAGE_IN_TABLE_MASK) {
         if(image->INDEX == i){
            printf("|  %3d  |%15s |    0x%08x   | 0x%08x |  0x%08x  |     %1d     |\n",
            image->INDEX,image->NAME,image->ADDR,image->SIZE,
            (image->BLOCK*NAND_FLASH_BLOCK_SIZE),image->BOOT_DEFAULT);
         }
         image++;
      }
   }
   printf("|       |                |                 |            |              |           |\n");
   printf("====================================================================================\n");
   
   fclose(nandflash_hdl);
   free(read_buffer);
   
}
/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _bootloader_del_image
* Returned Value   : 
* Comments         :
*    
*
*END*----------------------------------------------------------------------*/
uint_32 _bootloader_del_image(uint_32 index) 
{
   
   boolean             print_usage, shorthelp = FALSE;
   uint_32             block;
   uchar_ptr           read_buffer;
   IMAGE_INFO_PTR      image;
   MQX_FILE_PTR        nandflash_hdl;
   uint_32             result;
   boolean             image_found;
   /* Open the flash device */
   nandflash_hdl = fopen("nandflash:", NULL);
   if (nandflash_hdl == NULL) {
        printf("\nUnable to open NAND flash device");
        return -1;
   }
   read_buffer = allocate_buffer(IMAGE_TABLE_SIZE, "read");
   
   image = (IMAGE_INFO_PTR)read_buffer;
   
   fseek(nandflash_hdl, IMAGE_TABLE_PAGE_LOCATION, IO_SEEK_SET);
   read(nandflash_hdl, read_buffer,IMAGE_TABLE_SIZE/NAND_FLASH_PAGE_SIZE);
   
   image_found = FALSE;
   
   while(image->IS_IMAGE == IMAGE_IN_TABLE_MASK) {
      /* Delete table index and block */
      if(index == image->INDEX) {
         image_found = TRUE;
         image->INDEX = IMAGE_MASK_AS_DELETE;
         image->BOOT_DEFAULT = FALSE;
       /* Check if deleted image in end of table */
       if((image->BOOT_DEFAULT == TRUE)&&(image+1)->IS_IMAGE == IMAGE_IN_TABLE_UNMASK) {
         image->UNUSED = IMAGE_MASK_AS_DELETE;
         image->INDEX = IMAGE_MASK_AS_DELETE;
         image->ADDR = 0;
         image->SIZE = 0;
         image->BLOCK = 0;
         image->BLOCK_NUM = 0;
         image->IS_IMAGE = IMAGE_IN_TABLE_UNMASK;
         (image-1)->BOOT_DEFAULT = TRUE;
         
       }
      }
      image++;
   }
   
   image = (IMAGE_INFO_PTR)read_buffer;
   while(image->IS_IMAGE == IMAGE_IN_TABLE_MASK) {
      if((index < image->INDEX)&&(image->INDEX != IMAGE_MASK_AS_DELETE)){
         image->INDEX --;
      }
      image++;
   }
   
   if(image_found){
      if(MQX_OK == store_table_to_nand_flash(nandflash_hdl,read_buffer)) {
         printf("Delete image: %d success\n",index);
         result = MQX_OK;
      }
      else{
         result = IO_ERROR;
      }
   }
   else{
      printf("\n No image found !\n");   
   }
   
   fclose(nandflash_hdl);
   free(read_buffer);
   return result;
}

/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _bootloader_del_image
* Returned Value   : 
* Comments         :
*    
*
*END*----------------------------------------------------------------------*/
int_32 _bootloader_del_table(void)
{
   uchar_ptr            buffer;
   IMAGE_INFO_PTR       image;
   MQX_FILE_PTR         nandflash_hdl;
   int_32               result;
   
   nandflash_hdl = fopen("nandflash:", NULL);
   if (nandflash_hdl == NULL) {
        printf("\nUnable to open NAND flash device");
        return -1;
   }
   
   buffer = allocate_buffer(IMAGE_TABLE_SIZE, "read");
   
   image = (IMAGE_INFO_PTR)buffer;
   while(image->IS_IMAGE == IMAGE_IN_TABLE_MASK) {
      image->UNUSED = IMAGE_MASK_AS_DELETE;
      image->INDEX = IMAGE_MASK_AS_DELETE;
      image->ADDR = 0;
      image->SIZE = 0;
      image->BLOCK = 0;
      image->BLOCK_NUM = 0;
      image->IS_IMAGE = IMAGE_IN_TABLE_UNMASK;
   }
   if(MQX_OK == store_table_to_nand_flash(nandflash_hdl,buffer)) {
      printf("Delete table success \n");
      result = MQX_OK;
   }
   else{
      result = IO_ERROR;
   }
   free(buffer);
   fclose(nandflash_hdl);
   return result;
}
/*FUNCTION*------------------------------------------------------------
*
* Function Name  : store_image_to_nand_flash
* Returned Value : void
* Comments       :
*
*END------------------------------------------------------------------*/
int_32 store_image_to_nand_flash
(
   uint_32         block,
   uint_32         block_size,
   uchar_ptr       buff,
   MQX_FILE_PTR    nandflash_hdl
) 
{  
   static uint_32   i, page_number, size;
   
   page_number = block * (NAND_FLASH_BLOCK_SIZE/NAND_FLASH_PAGE_SIZE);
   
   for(i = block; i < (block + block_size); i++) {
      
      if(MQX_OK != ioctl(nandflash_hdl, NANDFLASH_IOCTL_ERASE_BLOCK, (void*)(i))) {
         printf("Erasing block %d failed.\n",(i));
         return IO_ERROR;
      }
   }
   fseek(nandflash_hdl, page_number, IO_SEEK_SET);
   size = block_size * (NAND_FLASH_BLOCK_SIZE/NAND_FLASH_PAGE_SIZE);
   if(size != write(nandflash_hdl, buff, size)) {
      return IO_ERROR;
   }
   
   return MQX_OK;
}
/*TASK*-----------------------------------------------------------------
*
* Function Name  : _bootloader_store_image
* Returned Value : void
* Comments       :
*
*END------------------------------------------------------------------*/

int_32  _bootloader_store_image
(
   uint_32   addr,
   uint_32   size,
   uchar_ptr buff,
   uchar_ptr name
)
{ /* Body */

   uint_32             block, block_num;
   uint_32             i,j,image_num;
   uchar_ptr           read_buffer;
   uchar_ptr           write_buffer;
   IMAGE_INFO_PTR      image;
   IMAGE_INFO          img; 
   MQX_FILE_PTR        nandflash_hdl;
   int_32              result;
   uint_32             check_sum = 0;
   IMAGE_HEADER        *p_img_hdr;
   
   /* Open the flash device */
   nandflash_hdl = fopen("nandflash:", NULL);
   if (nandflash_hdl == NULL) {
        printf("\nUnable to open NAND flash device");
        return IO_ERROR;
   }
   image_num = 0;
   block_num = 0;
   img.INDEX = 0;
   /* Initialize image header */
   for(i = 0; i < size; i++) {
      check_sum += *(buff+i);
   }
      
   buff -= sizeof(IMAGE_HEADER);
   p_img_hdr = (IMAGE_HEADER_PTR)buff;
   p_img_hdr->IMAGIC = IMAGE_IMAGIC_NUMBER;
   p_img_hdr->HEADERFLAG = IMAGE_HEADER_FLAG;
   p_img_hdr->LEN = size;
   p_img_hdr->CHECKSUM = check_sum;
   
   size += sizeof(IMAGE_HEADER);
   /* Read image table */
   read_buffer = allocate_buffer(IMAGE_TABLE_SIZE, "read");
   
   fseek(nandflash_hdl, IMAGE_TABLE_PAGE_LOCATION, IO_SEEK_SET);
   read(nandflash_hdl, read_buffer,IMAGE_TABLE_SIZE/NAND_FLASH_PAGE_SIZE);
   
   image = (IMAGE_INFO_PTR)read_buffer;
   
   while(image->IS_IMAGE == IMAGE_IN_TABLE_MASK) {
      if(image->INDEX != IMAGE_MASK_AS_DELETE){
         img.INDEX++;
      }
      image->BOOT_DEFAULT = FALSE;
      block_num += image->BLOCK_NUM;
      image_num++;
      image++;
   }
   
   img.UNUSED = IMAGE_MASK_AS_DELETE;
   img.ADDR = addr;
   img.INDEX += 1;
   img.SIZE = size;
   img.BLOCK = IMAGE_START_BLOCK;
   img.BLOCK_NUM = (size + NAND_FLASH_BLOCK_SIZE)/NAND_FLASH_BLOCK_SIZE;
   img.IS_IMAGE = IMAGE_IN_TABLE_MASK;
   img.BOOT_DEFAULT = TRUE;
   
   for(i = 0; i < 15; i++){
      img.NAME[i]  = *(name++);
   }
   
   image = (IMAGE_INFO_PTR)read_buffer;
   
   /* Empty table store it in position 0 */
   if(image_num == 0) {
      *(image) = img;
   }
   else{
      
      /* Finding location where enough memory */
      for(i = 0; i < image_num; i ++) {
         if((image->INDEX == IMAGE_MASK_AS_DELETE) && (img.BLOCK_NUM <= image->BLOCK_NUM)) {
            image->INDEX = img.INDEX;
            image->SIZE = img.SIZE;
            image->ADDR = img.ADDR;
            for(j = 0; j < 15; j++){
               image->NAME[j] = img.NAME[j];
            }
            image->BOOT_DEFAULT = TRUE;
            img = *(image);
            break;
         }
         image++;
      }
      /* Unable to find, store in end of table */
      if( i == image_num) {
         img.BLOCK = IMAGE_START_BLOCK + block_num;
         *(image) = img;
      }
   }
   
   image = (IMAGE_INFO_PTR)read_buffer;
   
   if(MQX_OK == store_image_to_nand_flash(img.BLOCK,img.BLOCK_NUM,buff,nandflash_hdl)) {
      if(MQX_OK == ioctl(nandflash_hdl, NANDFLASH_IOCTL_ERASE_BLOCK, (void*)IMAGE_TABLE_BLOCK_LOCATION)) {
         /* Write table to Nand Flash */
         fseek(nandflash_hdl, IMAGE_TABLE_PAGE_LOCATION, IO_SEEK_SET);
         if(IMAGE_TABLE_SIZE/NAND_FLASH_PAGE_SIZE == write(nandflash_hdl, read_buffer,IMAGE_TABLE_SIZE/NAND_FLASH_PAGE_SIZE)) {
            result = MQX_OK;
         }
         else{
            result = IO_ERROR;
         }
      }
      else{
         result = IO_ERROR;
      }
   }
   else{
      result = IO_ERROR;
   }
   fclose(nandflash_hdl);
   free(read_buffer);
   return result;
}

/*TASK*-----------------------------------------------------------------
*
* Function Name  : _bootloader_reset
* Returned Value : void
* Comments       :
*
*END------------------------------------------------------------------*/
void _bootloader_reset(void)
{
   MPC5125_RESET_STRUCT_PTR reset = (&((VMPC5125_STRUCT_PTR) MPC5125_GET_IMMR() )->RESET);
   
   uint_32 t;
   printf ("Resetting the board.\n");
   _PSP_SYNC();
   _mmu_disable();
   _PSP_MSR_GET(t);
   t &= ~(PSP_MSR_IR | PSP_MSR_EE | PSP_MSR_DR); // Disable MSR[IR,EE,DR]
   _PSP_MSR_SET(t);
   
   reset->RPR = 0x52535445;
   
   /* Verify Reset Control Reg is enabled */
	while (!((reset->RCER) & 0x00000001))
	   reset->RPR = 0x52535445;

	//
   //_time_delay(200);

	/* Perform reset */
	reset->RCR = 0x02;
	
}
/* EOF */
