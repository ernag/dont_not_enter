/*
 * wdns.c
 *
 *  Created on: May 15, 2013
 *      Author: SpottedLabs
 */


#include <mqx.h>
#include <rtcs.h>
#include "cormem.h"
#include "wdns.h"
#include "wifi_mgr.h"
#include "wassert.h"

/*
 *  From dnsutil.c
 */


/* Global variable used for gethostbyaddr and gethostbyname calls */
static INTERNAL_HOSTENT_STRUCT RTCS_HOST;
static uchar RTCS_HOST_NAMES[DNS_MAX_NAMES][DNS_MAX_CHARS_IN_NAME];
static local_cache_entry_t DNS_CACHE[DNS_MAX_NAMES];
static int DNS_CACHE_count = -1;
static _ip_address RTCS_HOST_ADDRS[DNS_MAX_ADDRS];
//static const HOST_ENTRY_STRUCT RTCS_Hosts_list[] = {{0,0,0}};
static char DNS_Local_network_name[] = {"."};

#define DNS_SEND_TIMEOUT_WD		(60*1000)
#define DNS_RECEIVE_TIMEOUT_WD 	(60*1000)
#define DNS_DEFAULT_WD			(60*1000)


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : DNS_append_local_name()
* Returned Value  : char _PTR_
* Comments        :
*
*END*-----------------------------------------------------------------*/

static uint_32 DNS_append_local_name (uchar   _PTR_ qname_buffer, uint_32       total_length)
{  /* Body */

   uint_32       local_name_length = 0;
   uint_32       error;
   uchar   _PTR_ label_length_ptr;
   uchar         label_length = 0;


   if ( DNS_Local_network_name[local_name_length] == '\0' ) {
      return( RTCSERR_DNS_INVALID_LOCAL_NAME );
   }/* Endif */

   /* If the Local Network is the ROOT Domain Name */
   if ( ( DNS_Local_network_name[local_name_length] == '.' )
        && ( DNS_Local_network_name[local_name_length + 1] == '\0' )) {
      htonc((qname_buffer + total_length), '\0');
      return( DNS_OK );
   }/* Endif */

   label_length_ptr = qname_buffer + total_length;
   error            = DNS_OK;

   while ( DNS_Local_network_name[local_name_length] ) {

      /*
      ** RFC 1035 says labels must start with a letter, but in practice
      ** some aren't
      */
      if ( isalnum(DNS_Local_network_name[local_name_length]) ) {
         htonc((qname_buffer + total_length + 1),
               DNS_Local_network_name[local_name_length]);
         total_length++;
         local_name_length++;
         label_length++;
      } else {
         error = RTCSERR_DNS_INVALID_LOCAL_NAME;
         break;
      }  /* Endif */

      while ( ( DNS_Local_network_name[local_name_length] != '.' ) &&
              ( DNS_Local_network_name[local_name_length] != '\0' ) ) {

         /* Domain Name characters can only be letters, hyphens, or numbers */
         if ( (isalnum(DNS_Local_network_name[local_name_length]) ||
               DNS_Local_network_name[local_name_length] == '-'  ) &&
              total_length < DNS_MAX_CHARS_IN_NAME  &&
              label_length <= DNS_MAX_LABEL_LENGTH ) {
            htonc((qname_buffer + total_length + 1),
                  DNS_Local_network_name[local_name_length]);
            total_length++;
            local_name_length++;
            label_length++;
         } else {
            error = RTCSERR_DNS_INVALID_LOCAL_NAME;
            break;
         } /* Endif */
      } /* Endwhile */

      if ( error == RTCSERR_DNS_INVALID_LOCAL_NAME ) {
         break;
      }/* Endif */

      htonc(label_length_ptr, label_length);
      label_length = 0;
      total_length++;
      label_length_ptr = qname_buffer + total_length;
      local_name_length++;
   } /* Endwhile */

   if ( error == DNS_OK ) {
      /*
      ** Need to append '\0' to terminate domain name properly.
      ** This signifies the ROOT domain
      */
      htonc(label_length_ptr, '\0');
   }/* Endif */

   return( error );

}  /* Endbody */

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : DNS_is_dotted_IP_addr()
* Returned Value  : bool
* Comments        :
*
*END*-----------------------------------------------------------------*/

uint_32 check_DNS_is_dotted_IP_addr(char *name)
{  /* Body */

   uint_32     i;
   uint_32     j = 0;
   uint_32     digit_count = 0;
   uint_32     dot_count = 0;
   int_32      byte_num;
   uint_32     addr_number = 0;
   uchar       number_chars[4];
   uchar       addr[4][3];

   /*
   ** first check to see if name is written in correct dotted decimal
   ** IP address format
   */
   for (i=0; name[i] != '\0'; ++i ) {
      if ( name[i] == '.' ) {
         dot_count++;

         if ( dot_count > 3 ) {
            /* invalid IP address */
            return( RTCSERR_DNS_INVALID_IP_ADDR );
         }/* Endif */

         if ( digit_count == 0 ) {
            /* there are no digits before the '.' */
            return( RTCSERR_DNS_INVALID_IP_ADDR );
         }/* Endif */

         byte_num = byte_num / 10; /* shift back */

         if ( (byte_num < 0 ) || (byte_num > 255) ) {
         /* if the number does fall within this range it's invalid */
            return( RTCSERR_DNS_INVALID_IP_ADDR );
         } else {
            addr_number++;
            number_chars[j] = digit_count;
            j++;
            digit_count = 0;
         } /* Endif */
      } else { /* a digit */

         if ( digit_count == 0 ) {
            byte_num = 0;
         }/* Endif */
         ++digit_count;

         if ( digit_count > 3 ) {
            /* too many digits between the '.' */
            return( RTCSERR_DNS_INVALID_IP_ADDR );
         }/* Endif */

         if ( isdigit(name[i]) ) {
            /* number is in range */
            addr[addr_number][digit_count - 1] = name[i];
            byte_num = byte_num + name[i] - '0';
            byte_num = byte_num * 10;
         } else {
            /* if the characters are not decimal digits it's invalid */
            return( RTCSERR_DNS_INVALID_IP_ADDR );
         }/* Endif */
      }/* Endif */
   } /* Endfor */

   if ( digit_count == 0 ) {
      /* there are no digits after the last '.' */
      return( RTCSERR_DNS_INVALID_IP_ADDR );
   } /* Endif */

   byte_num = byte_num / 10;
   if ( (byte_num < 0 ) || (byte_num > 255) ) {
      /* if the number does fall within this range it's invalid */
      return( RTCSERR_DNS_INVALID_IP_ADDR );
   } else {
      number_chars[j] = digit_count;
   }  /* Endif */

   if ( dot_count != 3 ) {
      /* the wrong number of dots were found */
      return( RTCSERR_DNS_INVALID_IP_ADDR );
   }/* Endif */

   return( DNS_OK );

}  /* Endbody */

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : DNS_is_dotted_domain_name()
* Returned Value  : uint_32
* Comments        : Checks the DNS Domain Name passed to it to see if the
*                   proper naming conventions have been followed.
*
*END*-----------------------------------------------------------------*/

uint_32 DNS_is_dotted_domain_name
   (
   uchar *domain_name,
   uchar *qname_buffer
   )
{  /* Body */
   uint_32     total_length = 0;
   uint_32     error;
   uchar *label_length_ptr;
   uchar       label_length = 0;


   if ( domain_name[total_length] == '\0' ) {
      return( DNS_INVALID_NAME );
   }/* Endif */

   /* If the request is for the ROOT Domain Name */
   if ( ( domain_name[total_length] == '.' )
        && ( domain_name[total_length + 1] == '\0' )) {
      htonc(qname_buffer, '\0');
      return( DNS_OK );
   }/* Endif */

   label_length_ptr = qname_buffer;
   error            = DNS_OK;

   while ( domain_name[total_length] != '\0' ) {

      /*
      ** RFC 1035 says labels must start with a letter, but in practice
      ** some aren't
      */
      if ( isalnum(domain_name[total_length]) ) {
         htonc((qname_buffer + total_length + 1), domain_name[total_length]);
         total_length++;
         label_length++;
      } else {
         error = DNS_INVALID_NAME;
         break;
      }  /* Endif */

      while ( ( domain_name[total_length] != '.') &&
              ( domain_name[total_length] != '\0' ) ) {

         /* Domain Name characters can only be letters, hyphens, or numbers */
         if ( (isalnum(domain_name[total_length]) ||
               domain_name[total_length] == '-'  ) &&
              total_length < DNS_MAX_CHARS_IN_NAME &&
              label_length <= DNS_MAX_LABEL_LENGTH ) {
            htonc((qname_buffer + total_length + 1), domain_name[total_length]);
            total_length++;
            label_length++;
         } else {
            error = DNS_INVALID_NAME;
            break;
         } /* Endif */
      } /* Endwhile */

      if ( error == DNS_INVALID_NAME ) {
         break;
      }/* Endif */

      htonc(label_length_ptr, label_length);
      label_length = 0;


      if ( domain_name[total_length] != '\0' ) {
         label_length_ptr = qname_buffer + total_length + 1;
      } else {
         /* It's NULL, append the local network name */
         error = DNS_append_local_name( qname_buffer, total_length + 1 );
         return( error );
//          return(DNS_INVALID_NAME);
      } /* Endif */
      total_length++;
   } /* Endwhile */

   if ( error == DNS_OK ) {
      /*
      ** Need to append '\0' to terminate domain name properly.
      ** This denotes the ROOT domain
      */
      htonc(label_length_ptr, '\0');
   }/* Endif */

   return( error );

}  /* Endbody */


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : DNS_parse_answer_name_to_dotted_form()
* Returned Value  : uchar *
* Comments        :
*
*END*-----------------------------------------------------------------*/

static uchar * DNS_parse_answer_name_to_dotted_form
   (
   uchar * buffer_ptr,
   uchar * name,
   uint_32     loc
   )

{  /* Body */

   uchar   * compressed_name;
   uchar   * fill_ptr;
   uchar         label_size;
   uint_32       i;
   uint_16       new_location;

   fill_ptr = RTCS_HOST_NAMES[loc];
   _mem_zero(fill_ptr, DNS_MAX_CHARS_IN_NAME);

   compressed_name = name;

   while ( ntohc(compressed_name) != '\0' ) {

      if ( ntohc(compressed_name) & DNS_COMPRESSED_NAME_MASK ) {
         new_location = ntohs(compressed_name)
                        & DNS_COMPRESSED_LOCATION_MASK;
         compressed_name = buffer_ptr + new_location;
      }/* Endif */

      label_size  = ntohc(compressed_name);
      compressed_name++;

      for ( i = 0; i < label_size; i++ ) {
           *fill_ptr++ = ntohc(compressed_name);
            compressed_name++;
      } /* Endfor */

      if ( ntohc(compressed_name) != '\0' ) {
         *fill_ptr++ = '.';
      }/* Endif */
   } /* Endwhile */

   *fill_ptr = '\0';

   return( RTCS_HOST_NAMES[loc] );

}  /* Endbody */



/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : DNS_free_HOSTENT_STRUCT()
* Returned Value  : void
* Comments        :
*
*END*-----------------------------------------------------------------*/

void DNS_free_HOSTENT_STRUCT
   (
   HOSTENT_STRUCT * host_ptr
   )

{  /* Body */
   /*
   ** This function is no longer needed, but the wrapper is left for
   ** backwards compatibility.
   */
}  /* Endbody */


/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : DNS_parse_UDP_response()
* Returned Value  : HOSTENT_STRUCT_PTR
* Comments        : If answers were returned, this function will
*                   parse them into a HOSTENT_STRUCT
*
*END*-----------------------------------------------------------------*/

HOSTENT_STRUCT * DNS_parse_UDP_response(uchar *buffer_ptr, uchar *name_ptr, uint_16 query_type)
{  /* Body */

   DNS_MESSAGE_HEADER_STRUCT     *   message_head_ptr = NULL;
   DNS_RESPONSE_RR_MIDDLE_STRUCT *   answer_middle;
   INTERNAL_HOSTENT_STRUCT       *   host_ptr = &RTCS_HOST;
   uchar                         *   answer_ptr;
   uchar                         *   answer_tail;
   uchar                         *   temp_ptr;
   uint_16                               response_length, answer_type,
      name_size, number_of_answers, num_queries;
   uint_32                               i, name_index = 0;
   uint_32                               j = 0;
   uint_32                               k = 0;
   uint_32                               buffer_size;
   uint_32                       *   addr_ptr;
   boolean                               unknown_answer_type = FALSE;

   message_head_ptr  = (DNS_MESSAGE_HEADER_STRUCT *)buffer_ptr;
   buffer_size       = sizeof(DNS_MESSAGE_HEADER_STRUCT);
   temp_ptr = buffer_ptr + sizeof( DNS_MESSAGE_HEADER_STRUCT );

   /* Zero the global HOSTENT_STRUCT */
   _mem_zero((char_ptr)host_ptr, sizeof(INTERNAL_HOSTENT_STRUCT));

   /* Get the number of queries. */
   num_queries = ntohs(message_head_ptr->QDCOUNT);
   for (i = 0; i < num_queries; i++) {
      name_size = 0;
      while( (ntohc(temp_ptr) != '\0') &&
         name_size < DNS_MAX_CHARS_IN_NAME ) {
         name_size = name_size + ntohc(temp_ptr) + 1;
         temp_ptr  = temp_ptr  + ntohc(temp_ptr) + 1;
      } /* Endwhile */
      /* To include the terminating NULL char */
      name_size++;
      buffer_size += (name_size + sizeof(DNS_MESSAGE_TAIL_STRUCT));
      temp_ptr    += (1 + sizeof(DNS_MESSAGE_TAIL_STRUCT));
   } /* Endfor */

   number_of_answers = ntohs(message_head_ptr->ANCOUNT);
   if (number_of_answers > DNS_MAX_NAMES ) {
      number_of_answers = DNS_MAX_NAMES;
   } /* Endif */

   host_ptr->HOSTENT.h_aliases   = &host_ptr->ALIASES[0];
   host_ptr->HOSTENT.h_addr_list = (char_ptr *)&host_ptr->ADDRESSES[0];
   host_ptr->ADDRESSES[0]        = NULL;
   host_ptr->HOSTENT.h_name      = NULL;
   host_ptr->HOSTENT.h_length    = sizeof( _ip_address );

   for (i = 0; (i < number_of_answers) && (j < DNS_MAX_ADDRS) &&
       (k < DNS_MAX_NAMES); i++ )
   {
      answer_ptr = temp_ptr;
      name_size  = 0;

      while( (ntohc(temp_ptr) != '\0') &&
             name_size < DNS_MAX_CHARS_IN_NAME &&
             !(ntohc(temp_ptr) & DNS_COMPRESSED_NAME_MASK)) {
         name_size += ntohc(temp_ptr);
         temp_ptr += ntohc(temp_ptr) + 1;
      } /* Endwhile */

      if ( ntohc(temp_ptr) & DNS_COMPRESSED_NAME_MASK ) {
         temp_ptr++;
      }/* Endif */

      temp_ptr++;
      answer_middle   = (DNS_RESPONSE_RR_MIDDLE_STRUCT *)temp_ptr;
      response_length = ntohs(answer_middle->RDLENGTH);
      answer_type     = ntohs(answer_middle->TYPE);
      temp_ptr       += sizeof(DNS_RESPONSE_RR_MIDDLE_STRUCT);
      answer_tail     = temp_ptr;
      temp_ptr       += response_length;

      switch ( answer_type ) {

         case DNS_A:
//             corona_print("DNS: rx'd A resp\r\n");
            if ( host_ptr->HOSTENT.h_name == NULL ) {
               host_ptr->HOSTENT.h_name =
                  (char *)DNS_parse_answer_name_to_dotted_form(
                  buffer_ptr, answer_ptr, name_index );
               name_index++;
            } /* Endif */

            RTCS_HOST_ADDRS[j] = ntohl((uchar_ptr)answer_tail);
            /*
            ** j is used in case BOTH CNAME and A data is received.  If CNAME
            ** answer is first, will write into wrong address space if using
            ** i.
            */
            host_ptr->ADDRESSES[j] = &RTCS_HOST_ADDRS[j];
            j++;
            /*
            ** This is to assure that the first IP address used is the first
            ** one that was given
            */
            host_ptr->IP_address = *host_ptr->ADDRESSES[0];
            break;

         case DNS_PTR:
//             corona_print("DNS: rx'd DNS_PTR resp\r\n");
            if (query_type == DNS_PTR) {
               if (host_ptr->HOSTENT.h_name != NULL) {
                  host_ptr->ALIASES[k] = host_ptr->HOSTENT.h_name;
                  k++;
               } /* Endif */
               host_ptr->HOSTENT.h_name =
                  (char *)DNS_parse_answer_name_to_dotted_form(
                  buffer_ptr, answer_tail, name_index );
               name_index++;
               addr_ptr = walloc( sizeof( _ip_address ));  //KL: is there a memory leak here?

               *addr_ptr = *((_ip_address *)name_ptr);
               host_ptr->ADDRESSES[j] = addr_ptr;
               j++;
               host_ptr->IP_address = *host_ptr->ADDRESSES[0];
            } else {
               host_ptr->ALIASES[k] = (char *)
                     DNS_parse_answer_name_to_dotted_form( buffer_ptr,
                     answer_tail, name_index );
               name_index++;
               k++;
            } /* Endif */
            break;

         case DNS_CNAME:
//             corona_print("DNS: rx'd CNAME resp\r\n");
            /* the k is used for ALIASES as the j is used for ADDRESSES */
            host_ptr->ALIASES[k] = (char *)
               DNS_parse_answer_name_to_dotted_form(
               buffer_ptr, answer_tail, name_index );
            name_index++;
            k++;
            break;

         default:
            unknown_answer_type = TRUE;
      } /* Endswitch */

      if ( unknown_answer_type == TRUE ) {
         break;
      }/* Endif */

      host_ptr->ADDRESSES[j]       = NULL;
      host_ptr->ALIASES[k]         = NULL;
      host_ptr->HOSTENT.h_addrtype = ntohs(answer_middle->CLASS);

   } /* Endfor */

   if ( number_of_answers == 0 ) {
      return( NULL );
   } /* Endif */

   return( &RTCS_HOST.HOSTENT );

} /* Endbody */

/*FUNCTION*-------------------------------------------------------------
*
* Function Name   : DNS_query_resolver_task()
* Returned Value  : HOSTENT_STRUCT *
* Comments        :
*
*END*-----------------------------------------------------------------*/

HOSTENT_STRUCT * DNS_query_resolver_task(uchar * name, uint_16 query_type)

{  /* Body */

   DNS_MESSAGE_HEADER_STRUCT *   message_head_ptr;
   DNS_MESSAGE_TAIL_STRUCT   *   message_tail_ptr;
   HOSTENT_STRUCT            *   host_ptr = NULL;
   sockaddr_in                   foreign_addr, local_addr;
   uint_32                       local_sock;
   uint_32                       qname_size;
   uint_32                       buffer_size;
   uint_16                       rlen;
   int_32                        temp_size;
   int_32                        error;
   uchar                     *   temp_ptr;
   uchar                     *   qname_ptr;
   uint_8                    *   buffer_ptr;
   int_32                        send_result, received;
   int                           udp_socket;
   
   /*
   ** If the size of this buffer is changed, also change the buffer size
   ** in the recvfrom() call near the bottom of this function
   */
   buffer_ptr = walloc( DNS_MAX_UDP_MESSAGE_SIZE );
   
//   _mem_set_type(buffer_ptr, MEM_TYPE_DNS_UDP_MESSAGE);
   qname_ptr = buffer_ptr + sizeof( DNS_MESSAGE_HEADER_STRUCT );

   if ( query_type == DNS_A ) {
       error = DNS_is_dotted_domain_name( name, qname_ptr );
       if ( error != DNS_OK ) {
           corona_print("DNS bad dom\n");
           wfree(buffer_ptr);
           return( NULL );
       }/* Endif */
   } else {
       corona_print("DNS rec QRY only\n");
       wfree(buffer_ptr);
       return( NULL );
   } /* Endif */

   /* set up buffer for sending query.   */
   message_head_ptr = (DNS_MESSAGE_HEADER_STRUCT *)buffer_ptr;

   htons(message_head_ptr->ID, 0);
//   htons(message_head_ptr->CONTROL, DNS_STANDARD_QUERY); // Original value, changed by KL
   htons(message_head_ptr->CONTROL, DNS_RECURSION_DESIRED);
   htons(message_head_ptr->QDCOUNT, DNS_SINGLE_QUERY);
   htons(message_head_ptr->NSCOUNT, 0);
   htons(message_head_ptr->ARCOUNT, 0);
   htons(message_head_ptr->ANCOUNT, 0);

   qname_size = strlen((char *)qname_ptr );
   /* Need to include the last '\0' character as well */
   qname_size++;

   temp_ptr = buffer_ptr + sizeof( DNS_MESSAGE_HEADER_STRUCT )
                          + qname_size;

   message_tail_ptr = (DNS_MESSAGE_TAIL_STRUCT *)temp_ptr;

   htons(message_tail_ptr->QTYPE, query_type);
   htons(message_tail_ptr->QCLASS, DNS_IN);

   buffer_size = sizeof(DNS_MESSAGE_HEADER_STRUCT) + qname_size
                 + sizeof(DNS_MESSAGE_TAIL_STRUCT);
   
   wassert(buffer_size <= DNS_MAX_UDP_MESSAGE_SIZE);

   error = WIFIMGR_open("8.8.8.8", 53, &udp_socket, SOCK_DGRAM_TYPE);
   if (ERR_OK != error)
   {
       WPRINT_ERR("%d open", error);
       wfree(buffer_ptr);
       return NULL;
   }
//   _time_delay(200);
//   corona_print("DNS: sending data\n");
   
   /* Send the buffer to the resolver for making a query */
   
   _watchdog_start(DNS_SEND_TIMEOUT_WD);
   error = WIFIMGR_send(udp_socket, (uint8_t *)buffer_ptr, buffer_size, &send_result);
   if (send_result != buffer_size) 
   {
      WIFIMGR_close(udp_socket);
      WPRINT_ERR("DNS tx");
      wfree(buffer_ptr);
      return( NULL );
   }/* Endif */
   corona_print("Qry,wait resp\n");

   memset(buffer_ptr, 0, DNS_MAX_UDP_MESSAGE_SIZE);

   // reuse buffer_ptr to store response   
   _watchdog_start(DNS_RECEIVE_TIMEOUT_WD);
   error = WIFIMGR_receive(udp_socket, (void *) buffer_ptr, DNS_MAX_UDP_MESSAGE_SIZE, DNS_QUERY_TIMEOUT, &received, FALSE);
   _watchdog_start(DNS_DEFAULT_WD);

   if ( received <= 0 || error == ERR_CONMGR_ERROR) 
   {
	   WPRINT_ERR("DNS rx");
	   WIFIMGR_close(udp_socket);
	   wfree(buffer_ptr);
	   return( NULL );
   }/* Endif */

   host_ptr = DNS_parse_UDP_response(buffer_ptr, name, query_type);
   WIFIMGR_close(udp_socket);

   if (host_ptr != NULL)
   {
	   WDNS_update_local_cache((char*) name, ((_ip_address *) (host_ptr->h_addr_list[0]))[0]);
   }
   
   wfree(buffer_ptr);

   return( host_ptr );
} /* Endbody */

_ip_address WDNS_search_local_cache(char *name)
{
	int i = 0;
	while (i < DNS_CACHE_count)
	{
        if (strcmp(DNS_CACHE[i].name, name) == 0)
            return DNS_CACHE[i].addr;
        i++;
    }
    return 0xFFFFFFFF;
}

int _WDNS_local_cache_index(char *name)
{
    int i = 0;
    while (i < DNS_CACHE_count)
    {
        if (strcmp(DNS_CACHE[i].name, name) == 0)
            return i;
        i++;
    }
    return -1;
}

void WDNS_init_local_cache()
{
    DNS_CACHE_count = 0;
}

void WDNS_clean_local_cache()
{
    while (DNS_CACHE_count > 0)
    {
        wfree(DNS_CACHE[--DNS_CACHE_count].name);
    }
    DNS_CACHE_count = 0; // incase DNS_CACHE_count was some how set negative;
}

void WDNS_update_local_cache(char *name, _ip_address addr)
{
    int local_index = -1;
    local_index = _WDNS_local_cache_index(name);
    
    if (local_index >= 0)
    {
        DNS_CACHE[local_index].addr = addr;
    }
    else
    {
        DNS_CACHE[DNS_CACHE_count].name = walloc(strlen(name) + 1);
        strcpy(DNS_CACHE[DNS_CACHE_count].name, name);
        DNS_CACHE[DNS_CACHE_count].addr = addr;
        DNS_CACHE_count++;
    }
}
