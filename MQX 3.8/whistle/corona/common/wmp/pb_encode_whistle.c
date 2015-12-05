
#include <string.h>
#include "pb.h"
#include "pb_encode.h"
#include "pb_encode_whistle.h"


bool pb_ostream_buffer_shift_contents(pb_ostream_t *stream, uint8_t *src_ptr, size_t payload_length, int8_t offset)
{
    uint8_t *dest_ptr = src_ptr + offset * sizeof(uint8_t);
    
    if (stream->bytes_written + offset > stream->max_size)
        offset = stream->max_size - stream->bytes_written;
    
    memmove(dest_ptr, src_ptr, payload_length);
    
    stream->bytes_written += offset;
    (uint8_t*) stream->state += offset * sizeof(uint8_t);
    
    return true;
}
