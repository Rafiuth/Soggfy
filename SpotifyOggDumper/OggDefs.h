#pragma once
#include <cstdint>

//https://github.com/xiph/ogg/blob/master/include/ogg/ogg.h

typedef struct {
    void* iov_base;
    size_t iov_len;
} ogg_iovec_t;

typedef struct {
    long endbyte;
    int  endbit;

    unsigned char* buffer;
    unsigned char* ptr;
    long storage;
} oggpack_buffer;

/* ogg_page is used to encapsulate the data in one Ogg bitstream page *****/

typedef struct {
    unsigned char* header;
    long header_len;
    unsigned char* body;
    long body_len;
} ogg_page;

/* ogg_stream_state contains the current encode/decode state of a logical
   Ogg bitstream **********************************************************/

typedef struct {
    unsigned char* body_data;    /* bytes from packet bodies */
    long    body_storage;          /* storage elements allocated */
    long    body_fill;             /* elements stored; fill mark */
    long    body_returned;         /* elements of fill returned */


    int* lacing_vals;      /* The values that will go to the segment table */
    int64_t* granule_vals; /* granulepos values for headers. Not compact
                                  this way, but it is simple coupled to the
                                  lacing fifo */
    long    lacing_storage;
    long    lacing_fill;
    long    lacing_packet;
    long    lacing_returned;

    unsigned char    header[282];      /* working space for header encode */
    int              header_fill;

    int     e_o_s;          /* set when we have buffered the last packet in the
                               logical bitstream */
    int     b_o_s;          /* set after we've written the initial page
                               of a logical bitstream */
    long    serialno;
    long    pageno;
    int64_t  packetno;      /* sequence number for decode; the framing
                               knows where there's a hole in the data,
                               but we need coupling so that the codec
                               (which is in a separate abstraction
                               layer) also knows about the gap */
    int64_t   granulepos;

} ogg_stream_state;

/* ogg_packet is used to encapsulate the data and metadata belonging
   to a single raw Ogg/Vorbis packet *************************************/

typedef struct {
    unsigned char* packet;
    long  bytes;
    long  b_o_s;
    long  e_o_s;

    int64_t  granulepos;

    int64_t  packetno;         /* sequence number for decode; the framing
                                  knows where there's a hole in the data,
                                  but we need coupling so that the codec
                                  (which is in a separate abstraction
                                  layer) also knows about the gap */
} ogg_packet;

typedef struct {
    unsigned char* data;
    int storage;
    int fill;
    int returned;

    int unsynced;
    int headerbytes;
    int bodybytes;
} ogg_sync_state;