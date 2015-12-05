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
* $FileName: io_dopr.c$
* $Version : 3.8.12.3$
* $Date    : Nov-28-2011$
*
* Comments:
*
*   This file contains the functions for the formatting of
*   output.
*   $Header:io_dopr.c, 7, 2/23/2004 2:36:23 PM, $
*   $NoKeywords$
*
*END************************************************************************/


#include "mqx.h"
#include "fio.h"
#include "fio_prv.h"
#include "io_prv.h"

extern void _io_doprint_prt(int_32, uchar _PTR_, int_32, boolean);
extern void _io_doprint_prt64(int_64, uchar _PTR_, int_32, boolean);

#if MQX_INCLUDE_FLOATING_POINT_IO
extern _mqx_int _io_dtoe(char _PTR_, double arg, char, char, char, char, int_32, char);
extern _mqx_int _io_dtof(char _PTR_, double arg, char, char, char, char, int_32);
extern _mqx_int _io_dtog(char _PTR_, double arg, char, char, char, char, int_32, char);
#endif

// Keil: suppress ellipsis warning in va_arg usage below
#if defined(__CC_ARM)
#pragma diag_suppress 1256 
#endif


// Utility function prototypes
static _mqx_int _io_mputc(MQX_FILE_PTR, _mqx_int (_CODE_PTR_ func_ptr)(_mqx_int, MQX_FILE_PTR), char c, _mqx_int count);
static _mqx_int _io_putstr(MQX_FILE_PTR, _mqx_int (_CODE_PTR_ func_ptr)(_mqx_int, MQX_FILE_PTR), char_ptr str);


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_mputc
* Returned Value   : _mqx_int number_of_character_printed_out
* Comments         :
*    This function outputs given character count times.
*
* NOTE: I/O is performed by calling given function pointer using following
*         (*func_ptr)(c,farg);
*
*END*----------------------------------------------------------------------*/
static _mqx_int _io_mputc(
/* [IN] Argument to func_ptr             */
MQX_FILE_PTR farg,

/* [IN] Function to put character out */
_mqx_int (_CODE_PTR_ func_ptr)(_mqx_int, MQX_FILE_PTR),

/* [IN] The character to put out */
char c,

/* Number of character prints out */
_mqx_int count
)
{
   int_32   cnt = 0;
   
   if (count < 0) {
      count = 0;
   }
   
   for (; (cnt < count) ; cnt++) {
      if (c != func_ptr((_mqx_int) c, farg)) {
          break;
      }
   }

   return cnt;
}

/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_putstr
* Returned Value   : _mqx_int number_of_character_printed_out
* Comments         :
*    This function outputs given string.
*
* NOTE: I/O is performed by calling given function pointer using following
*         (*func_ptr)(c,farg);
*
*END*----------------------------------------------------------------------*/
static _mqx_int _io_putstr(
/* [IN] Argument to func_ptr             */
MQX_FILE_PTR farg,

/* [IN] Function to put character out */
_mqx_int (_CODE_PTR_ func_ptr)(_mqx_int, MQX_FILE_PTR),

/* [IN] The string to put out */
char_ptr str
)
{
   _mqx_int res = 0;
   for ( ; *str && ((_mqx_int) *str == func_ptr((_mqx_int) *str, farg)); res++, str++) {}
   
   if (*str) {
       res--;
   }
   return res;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_doprint
* Returned Value   : _mqx_int number_of_character_printed_out
* Comments         :
*    This function outputs its parameters according to a formatted string.
*
* NOTE: I/O is performed by calling;
*         (*func_ptr)(c,farg);
*
*END*----------------------------------------------------------------------*/

_mqx_int _io_doprint
   (
      /* [IN] Argument to func_ptr             */
      MQX_FILE_PTR farg,

      /* [IN] Function to put a character called as (*func_ptr)(c,farg)  */
      _mqx_int (_CODE_PTR_ func_ptr)(_mqx_int, MQX_FILE_PTR),

      /* [IN] Format string for printf         */
      register char _PTR_ fmt_ptr,

      /* [IN] Arguments to printf               */
      va_list  args_ptr
   )
{ /* Body */
   uchar    string[32];   /* The string str points to this output   */
   char     c;
   int_32   i;
   int_64   i64;
   _mqx_int f;            /* the format character (comes after %)   */
   uchar _PTR_ str_ptr;  /* running pointer in string              */
   /*    from number conversion              */
   _mqx_int length;       /* Length of string "str"                 */
   char     fill;         /* Fill character ( ' ' or '0' )          */
   _mqx_int leftjust;     /* 0 = right-justified, else left-just.   */
   _mqx_int prec,width;   /* Field specifications % width . precision */
   _mqx_int leading;      /* No. of leading/trailing fill chars.    */
   uchar    sign;         /* Set to '-' for negative decimals       */
   int      digit1;       /* Offset to add to first numeric digit.  */
   char     use_caps;     /* Hex output to use capital letters.     */
   char     unsig;        /* Indicates if specifier is of unsigned type */
   char     use_signs;    /* Always output a sign for %d            */
   char     use_space;    /* If positive and not sign, print space for %d */
   char     use_prep;     /* Prepend 0 for octal, 0x , 0X for hex   */
   char     use_int_prec; /* Precision usage with d, i, o, u, x, X  */
   char     zero_value;   /* indicates 0 output value */
   char     prepend;      /* What prepend type 3 octal 2 0x, 1 0X   */
   char     arg_length;   /* optional argument length               */
   char     done;
   char     is_integer;
   _mqx_int number_of_characters;  /* # chars printed out */
#if MQX_INCLUDE_FLOATING_POINT_IO
   char     prec_set;
   double   fnumber;
   char     fstring[324]; /* Floating point output string           */
#endif
   uint_32_ptr temp_ptr;
   _mqx_int    func_result = MQX_OK;

   number_of_characters = 0;
   for ( ;; ) {
      /* Echo characters until '%' or end of fmt_ptr string */
      while (  (c = *fmt_ptr++ ) != '%'  ) {
         if (  c == '\0'  ) {
            if (func_result == IO_EOF) {
               return(IO_EOF);
            } else {
               return number_of_characters;
            } /* Endif */
         } /* Endif */
         func_result = (*func_ptr)((_mqx_int)c, farg);
         number_of_characters++;
      } /* Endwhile */

      /* Echo "...%%..." as '%' */
      if (  *fmt_ptr == '%'  ) {
         func_result = (*func_ptr)((_mqx_int)*fmt_ptr++, farg);
         number_of_characters++;
         continue;
      } /* Endif */

      /* Set default flag */
      fill         = ' ';
      sign         = '\0';
      arg_length   = sizeof(int_32);
      leftjust     = 0;
      use_caps     = 0;
      use_space    = 0;
      use_prep     = 0;
      use_signs    = 0;
      use_int_prec = 0;
      prepend      = 0;
      unsig        = 0;
      zero_value   = 0;
#if MQX_INCLUDE_FLOATING_POINT_IO
      prec_set   = 0;
#endif

      /* Collect Valid Flags */
      done = FALSE;
      while ( *fmt_ptr && ! done ) {
         switch ( *fmt_ptr ) {

            case '-':
               /* Check for "%-..." EQ Left-justified output */
               leftjust = 1;
               fmt_ptr++;
               break;

            case '0':
               fill = '0';
               fmt_ptr++;
               break;

            case '+':
               use_signs = 1;
               fmt_ptr++;
               break;
               
            case ' ':
               use_space = 1;
               fmt_ptr++;
               break;

            case '#':
               use_prep = 1;
               fmt_ptr++;
               break;

            default:
               done = TRUE;
            break;
         } /* Endswitch */
      } /* Endwhile */

      /* Allow for minimum field width specifier for %d,u,x,o,c,s */
      /* Also allow %* for variable width ( %0* as well)           */
      width = 0;
      if (  *fmt_ptr == '*'  ) {
         width = va_arg(args_ptr, uint_32);
         ++fmt_ptr;
      } else {
         while (  ('0' <= *fmt_ptr) && (*fmt_ptr <= '9')  ) {
            width = width * 10 + *fmt_ptr++ - '0';
         } /* Endwhile */
         /* Allow for maximum string width for %s */
      } /* Endif */

      prec = 0;
      if (  *fmt_ptr == '.'  ) {
         if (  *(++fmt_ptr) == '*'  ) {
            prec = va_arg(args_ptr, uint_32);
#if MQX_INCLUDE_FLOATING_POINT_IO
            prec_set = 1;
#endif
            ++fmt_ptr;
         } else {
            while (  ('0' <= *fmt_ptr) && (*fmt_ptr <= '9')  ) {
               prec = prec * 10 + *fmt_ptr++ - '0';
#if MQX_INCLUDE_FLOATING_POINT_IO
               prec_set = 1;
#endif
            } /* Endwhile */
         } /* Endif */
      } /* Endif */

      str_ptr = string;
      if (  (f = *fmt_ptr++) == '\0'  ) {
         func_result = (*func_ptr)((_mqx_int)'%', farg);
         number_of_characters++;
         if (func_result == IO_EOF) {
            return(IO_EOF);
         } else {
            return number_of_characters;
         } /* Endif */
      } /* Endif */
      
      arg_length = 'm';
      if ( (f == 'h') || (f == 'l') || (f == 'L') || (f == 'm')) {
         arg_length = f;
         f = *fmt_ptr++;
      } /* Endif */

      use_caps = FALSE;
      is_integer = FALSE;
      length = 0;               /* CR1281 */
      switch (  f  ) {
         case 'c':
         case 'C':
            string[0] = (char)va_arg(args_ptr, _mqx_int);
            if (string[0] == 0) {
               length += 1;     /* CR1281 */
            } /* Endif */
            string[1] = '\0';
            prec = 0;
            fill = ' ';
            use_prep = 0;
            break;
            
         case 's':
         case 'S':
            str_ptr = (uchar _PTR_)va_arg(args_ptr, pointer);
            fill = ' ';
            use_prep = 0;
            break;
            
         case 'i':
         case 'I':
         case 'd':
         case 'D':
            is_integer = TRUE;
         case 'u':
         case 'U':
            if (f == 'u' || f == 'U') {
               unsig = 1;
            }
            if (prec > 0) {
                use_int_prec = 1;
            }
            if (is_integer) {
               if (arg_length == 'L') {
                  i64 = va_arg(args_ptr, int_64);
                  zero_value = i64 == 0L;
                  if (i64 < 0) {
                     sign = '-';
                     i64 = -i64;
                  } else if ( use_signs ) {  /* Always a sign */
                     sign = '+';
                  } else if ( use_space ) {  /* use spaces */
                     sign = ' ';
                  } /* Endif */
               } else {
                  if (arg_length == 'h') {
                     i = va_arg(args_ptr, int_16);
                  } else if (arg_length == 'l') {
                     i = va_arg(args_ptr, int_32);
                  } else {
                     i = va_arg(args_ptr, _mqx_int);
                  }
                  zero_value = i == 0;
                  if (i < 0) {
                     sign = '-';
                     i = -i;
                  } else if ( use_signs ) {  /* Always a sign */
                     sign = '+';
                  } else if ( use_space ) {  /* use spaces */
                     sign = ' ';
                  } /* Endif */
               }
            } else {
               if (arg_length == 'h') {
                  i = va_arg(args_ptr, uint_16);
                  zero_value = i == 0;
               } else if (arg_length == 'l') {
                  i = va_arg(args_ptr, uint_32);
                  zero_value = i == 0;
               } else if (arg_length == 'L') {
                  i64 = va_arg(args_ptr, uint_64);
                  zero_value = i64 == 0L;
               } else {
                  i = va_arg(args_ptr, _mqx_uint);
                  zero_value = i == 0;
               }/* Endif */
            } /* Endif */
            digit1 = '\0';
            /* "negative" longs in unsigned format       */
            /* can't be computed with long division      */
            /* convert *args_ptr to "positive", digit1   */
            /* = how much to add back afterwards         */
            if (arg_length == 'L') {
               while (i64 < 0) {
                  i64 -= _MQX_IO_DIVISION_ADJUST_64;
                  ++digit1;
               } /* Endwhile */
               _io_doprint_prt64(i64, str_ptr + 1, PRINT_DECIMAL_64, (boolean)use_caps);
               str_ptr[1] += digit1;
               if (str_ptr[1] > '9') {
                  str_ptr[1] -= 10;
                  str_ptr[0] = '1';
               } else {
                  str_ptr += 1;
               }
            } else {
               while (i < 0) {
                  i -= _MQX_IO_DIVISION_ADJUST;
                  ++digit1;
               } /* Endwhile */
               _io_doprint_prt(i, str_ptr, PRINT_DECIMAL, (boolean)use_caps);
               str_ptr[0] += digit1;
            }
            use_prep = 0;
            break;

         case 'o':
         case 'O':
            unsig = 1;
            if (prec > 0) {
               use_int_prec = 1;
            }
            if (arg_length == 'L') {
               i64 = va_arg(args_ptr, int_64);
               zero_value = i64 == 0L;
               _io_doprint_prt64(i64, str_ptr, PRINT_OCTAL_64, (boolean)use_caps);
            } else {
               if (arg_length == 'h') {
                  i = va_arg(args_ptr, int_16);
               } else if (arg_length == 'l') {
                  i = va_arg(args_ptr, int_32);
               } else {
                  i = va_arg(args_ptr, _mqx_int);
               }/* Endif */
               zero_value = i == 0;
               _io_doprint_prt(i, str_ptr, PRINT_OCTAL, (boolean)use_caps);
            }
            prepend = 3;
            break;
            
         case 'P':
            use_caps = TRUE;
         case 'p':
            leading = 0;
            temp_ptr = (uint_32_ptr) va_arg(args_ptr, pointer);
            _io_doprint_prt((_mqx_max_type)temp_ptr, str_ptr, PRINT_ADDRESS,
               (boolean)use_caps);
         break;

         case 'X':
            use_caps = TRUE;
            prepend  = 1;
         case 'x':
            unsig = 1;
            if (prec > 0) {
                use_int_prec = 1;
            }
            prepend++;
            if (arg_length == 'L') {
               i64 = va_arg(args_ptr, uint_64);
               zero_value = i64 == 0L;
               _io_doprint_prt64(i64, str_ptr, PRINT_HEX_64, (boolean)use_caps);
            } else {
               if (arg_length == 'h') {
                  i = va_arg(args_ptr, uint_16);
               } else if (arg_length == 'l') {
                  i = va_arg(args_ptr, uint_32);
               } else {
                  i = va_arg(args_ptr, _mqx_uint);
               }/* Endif */
               zero_value = i == 0;
               _io_doprint_prt(i, str_ptr, PRINT_HEX, (boolean)use_caps);
            }
            break;
            
         case 'n':   /* Defaults to size of number on stack */
         case 'N':   /* Defaults to size of number on stack */
            str_ptr = (uchar _PTR_)va_arg(args_ptr, pointer);
            *(_mqx_int _PTR_)(str_ptr) = (int_32)number_of_characters;
            continue;
            
#if MQX_INCLUDE_FLOATING_POINT_IO
            
         case 'g':
         case 'G':
             
             if (! prec_set) {
                prec = 6;
             } /* Endif */
             if (prec == 0) {
                prec = 1;
             }
             if (arg_length == 'L') {
                fnumber = va_arg(args_ptr, long double);
             } else {
                fnumber = va_arg(args_ptr, double);
             }/* Endif */
             i = _io_dtog(fstring, fnumber, fill, use_signs, use_space, use_prep, 
                prec, (char)f);
             str_ptr = (uchar_ptr)fstring;
             if (*str_ptr == '+') {
                sign = '+';
                str_ptr++;
             } else if (*str_ptr == '-') {
                sign = '-';
                str_ptr++;
             }
             use_prep = 0;
             prec = 0;
          break;             

         case 'f':
         case 'F':
            if (! prec_set) {
               prec = 6;
            } /* Endif */
            if (arg_length == 'L') {
               fnumber = va_arg(args_ptr, long double);
            } else {
               fnumber = va_arg(args_ptr, double);
            }/* Endif */
            i = _io_dtof(fstring, fnumber, fill, use_signs, use_space, use_prep, 
               prec);
            str_ptr = (uchar_ptr)fstring;
            if (*str_ptr == '+') {
               sign = '+';
               str_ptr++;
            } else if (*str_ptr == '-') {
               sign = '-';
               str_ptr++;
            }
            use_prep = 0;
            prec = 0;
         break;

         case 'e':
         case 'E':
            if (! prec_set) {
               prec = 6;
            }
            if (arg_length == 'L') {
               fnumber = va_arg(args_ptr, long double);
            } else {
               fnumber = va_arg(args_ptr, double);
            }/* Endif */
            i = _io_dtoe(fstring, fnumber, fill, use_signs, use_space, use_prep, 
               prec, (char)f);
            str_ptr = (uchar_ptr)fstring;
            if (*str_ptr == '+') {
               sign = '+';
               str_ptr++;
            } else if (*str_ptr == '-') {
               sign = '-';
               str_ptr++;
            }
            use_prep = 0;
            prec = 0;
         break;
#endif

         default:
            func_result = (*func_ptr)((_mqx_int)f, farg);
            number_of_characters++;
            if (func_result == IO_EOF) {
               return(IO_EOF);
            } else {
               return number_of_characters;
            } /* Endif */
      } /* End Switch */
      
      for (; str_ptr[length] != '\0'; length++ ) {
         ;
      } /* Endfor */
      
      if ( (width < 0) ) {
         width = 0;
      } /* Endif */
      if ( (prec < 0) ) {
         prec = 0;
      } /* Endif */
      leading = 0;

      if ( (prec != 0) || (width != 0) ) {
         /*
          ** prec = precision
          **    For a string, the maximum number of characters to be printed
          **    from the string.
          **    for f,e,E the number of digits after the decimal point
          **    for g,G the number of significant digits
          **    for integer, the minimum number of digits to be printed
          **       leading 0's will precede
          */
         if ( !use_int_prec && (prec != 0) ) {
            if ( length > prec ) {
               if ( str_ptr == string ) {
                  /* this was a numeric input */
                  /* thus truncate to the left */
                  str_ptr += (length - prec);
               } /* Endif */
               length = prec;
            } /* Endif */
         } /* Endif */

         /* width = minimum field width
          **    argument will be printed in a field at least this wide,
          **    and wider if necessary.  If argument has fewer characters
          **    than the field width, is will be padded on left or right
          **    to make up the width.  Normally pad ' ' unless 0 specified.
          */
         if (use_int_prec) {
            if (width > prec) {
               leading = (prec > length) ? width - prec : width - length;
               if ( !zero_value && use_prep && (prepend == 3) && (prec > length)) {
                  leading++; //correction for octal because one prepend 0 is pad as well as prepend character
               }
            } else {
               leading = 0;
            }
         } else if ( width != 0 ) {
            leading = width - length;
         } /* Endif */

         if ( sign ) {
            --leading;
         } /* Endif */
         if ( use_prep && !zero_value ) {
            --leading;
            if ( prepend < 3 ) { /* HEX */
               --leading;
            } /* Endif */
         } /* Endif */

      } /* Endif */

      if (leading < 0) {
          leading = 0;
      }
      
      if (use_space && !unsig && (sign != '+') && (sign != '-')) {
         number_of_characters += _io_mputc(farg, func_ptr, ' ', 1);
      }

      if ( leftjust == 0 ) { /* main flow for right justification */
         if (use_int_prec) {
            //print out leading spaces first
            number_of_characters += _io_mputc(farg, func_ptr, ' ', leading);

            //print out sign if there's any
            if ( ((sign == '+') || (sign == '-')) ) { /* Leading '-', '+' */
               number_of_characters += _io_mputc(farg, func_ptr, sign, 1);
            }

            //handle leading '0', '0x' or '0X'
            if ( use_prep && !zero_value ) {
               switch (prepend) {
               case 1:
                  number_of_characters += _io_putstr(farg, func_ptr, "0x");
                  break;
               case 2:
                  number_of_characters += _io_putstr(farg, func_ptr, "0X");
                  break;
               default:
                  if (!((prec - length) > 0)) {
                     number_of_characters += _io_mputc(farg, func_ptr, '0', 1);
                  }
               }
            } /* Endif */
            
            //print out leading 0's given by precision
            number_of_characters += _io_mputc(farg, func_ptr, '0', prec - length);
         } else { // right justification without precision for integer types
            if (fill == '0') { // flow for leading zeroes
               //print out sign if there's any
               if ( ((sign == '+') || (sign == '-')) ) { /* Leading '-', '+' */
                  number_of_characters += _io_mputc(farg, func_ptr, sign, 1);
               }

               //handle leading '0', '0x' or '0X'
               if ( use_prep && !zero_value ) {
                  switch (prepend) {
                  case 1:
                     number_of_characters += _io_putstr(farg, func_ptr, "0x");
                     break;
                  case 2:
                     number_of_characters += _io_putstr(farg, func_ptr, "0X");
                     break;
                  default:
                     if ( leading <= 0 ) {
                        number_of_characters += _io_mputc(farg, func_ptr, '0', 1);
                     }
                  }
               } /* Endif */

               //print out leading 0's
               number_of_characters += _io_mputc(farg, func_ptr, fill, leading);
            } else { // flow for leading spaces
               //print out leading spaces
               number_of_characters += _io_mputc(farg, func_ptr, fill, leading);
               
               //print out sign if there's any
               if ( ((sign == '+') || (sign == '-')) ) { /* Leading '-', '+' */
                  number_of_characters += _io_mputc(farg, func_ptr, sign, 1);
               }

               //handle leading '0', '0x' or '0X'
               if ( use_prep && !zero_value ) {
                  switch (prepend) {
                  case 1:
                     number_of_characters += _io_putstr(farg, func_ptr, "0x");
                     break;
                  case 2:
                     number_of_characters += _io_putstr(farg, func_ptr, "0X");
                     break;
                  default:
                      number_of_characters += _io_mputc(farg, func_ptr, '0', 1);
                  }
               } /* Endif */
            }
         }
      } else { // left justification has different flow
         //print out sign if there's any
         if ( ((sign == '+') || (sign == '-')) ) { /* Leading '-', '+' */
            number_of_characters += _io_mputc(farg, func_ptr, sign, 1);
         }

         //handle leading '0', '0x' or '0X'
         if ( use_prep && !zero_value ) {
            switch (prepend) {
            case 1:
               number_of_characters += _io_putstr(farg, func_ptr, "0x");
               break;
            case 2:
               number_of_characters += _io_putstr(farg, func_ptr, "0X");
               break;
            default:
               if ((use_int_prec) && !((prec - length) > 0)) {
                   number_of_characters += _io_mputc(farg, func_ptr, '0', 1);
               } else if (!use_int_prec) {
                   if (fill == '0') {
                       if ( leading <= 0 ) {
                           number_of_characters += _io_mputc(farg, func_ptr, '0', 1);
                       }
                   } else {
                       number_of_characters += _io_mputc(farg, func_ptr, '0', 1);
                   }
               }
            }
         } /* Endif */

         if (use_int_prec) {
            //print out leading 0's given by precision
            number_of_characters += _io_mputc(farg, func_ptr, '0', prec - length);
         }
      }

      // print out actual characters
      number_of_characters += _io_putstr(farg, func_ptr, (char_ptr) str_ptr);

      if (leftjust != 0) { /* Print trailing pad characters */
         //print out leading 0's
         number_of_characters += _io_mputc(farg, func_ptr, ' ', leading);
      } /* Endif */

   } /* Endfor */
} /* Endbody */


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_doprint_prt
* Returned Value   : 
* Comments         :
*   This function converts a number into a printable string.  
*
*END*----------------------------------------------------------------------*/

static const char upper_hex_string[] = "0123456789ABCDEF";
static const char lower_hex_string[] = "0123456789abcdef";

void _io_doprint_prt
   (
      /* [IN] The number to convert */
      int_32         num,

      /* [IN/OUT] the string to generate */
      uchar _PTR_    str_ptr,

      /* [IN] what base to use */
      int_32         type,

      /* [IN] in hex, use CAPITALS? */
      boolean        use_caps
   )             
{ /* Body */
   register int_32 i;
   char     temp[16];
   
   temp[0] = '\0';
   if (type == PRINT_OCTAL) {
      for (i = 1; i <= type; i++) {
         temp[i] = (char)((num & 0x7) + '0');
         num >>= 3;
      } /* Endfor */
      temp[PRINT_OCTAL] &= '3';
   } else if (type == PRINT_DECIMAL) {
      for (i = 1; i <= type; i++) {
         temp[i] = (char)(num % 10 + '0');
         num /= 10;
      } /* Endfor */
   } else {
      for (i = 1; i <= type; i++) {
         temp[i] = use_caps ? upper_hex_string[num & 0xF] : lower_hex_string[num & 0xF];
         num >>= 4;
      } /* Endfor */
   } /* Endif */
   for (i = type; temp[i] == '0'; i--) {
      ; 
   } /* Endfor */
   if (i == 0) {
      i++; 
   } /* Endif */
   while (i >= 0) {
      *str_ptr++ = temp[i--]; 
   } /* Endwhile */

} /* Endbody */


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_doprint_prt64
* Returned Value   : 
* Comments         :
*   This function converts a 64bit number into a printable string.  
*
*END*----------------------------------------------------------------------*/

void _io_doprint_prt64
   (
      /* [IN] The number to convert */
      int_64         num,

      /* [IN/OUT] the string to generate */
      uchar _PTR_    str_ptr,

      /* [IN] what base to use */
      int_32         type,

      /* [IN] in hex, use CAPITALS? */
      boolean        use_caps
   )             
{ /* Body */
   register int_32 i;
   char     temp[32];
   
   temp[0] = '\0';
   if (type == PRINT_OCTAL_64) {
      for (i = 1; i <= type; i++) {
         temp[i] = (char)((num & 0x7) + '0');
         num >>= 3;
      } /* Endfor */
      temp[PRINT_OCTAL_64] &= '1';
   } else if (type == PRINT_DECIMAL_64) {
      for (i = 1; i <= type; i++) {
         temp[i] = (char)(num % 10 + '0');
         num /= 10;
      } /* Endfor */
   } else {
      for (i = 1; i <= type; i++) {
         temp[i] = use_caps ? upper_hex_string[num & 0xF] : lower_hex_string[num & 0xF];
         num >>= 4;
      } /* Endfor */
   } /* Endif */
   for (i = type; temp[i] == '0'; i--) {
      ; 
   } /* Endfor */
   if (i == 0) {
      i++; 
   } /* Endif */
   while (i >= 0) {
      *str_ptr++ = temp[i--]; 
   } /* Endwhile */

} /* Endbody */

/* EOF */
