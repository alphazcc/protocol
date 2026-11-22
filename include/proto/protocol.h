/* Copyright (c) 2022-2026, HelloAlpha */
#ifndef PROTO_PROTOCOL_H
#define PROTO_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "port/compiler.h"

#define PROTO_HEAD0 UINT8_C(0xED)
#define PROTO_HEAD1 UINT8_C(0xB9)
#define PROTO_HEAD2 UINT8_C(0x55)
#define PROTO_HEAD3 UINT8_C(0xAA)
#define PROTO_TAIL0 UINT8_C(0x5A)
#define PROTO_TAIL1 UINT8_C(0xA5)

#define PROTO_HEAD_LEN    4U
#define PROTO_TYPE_LEN    1U
#define PROTO_CMD_LEN     1U
#define PROTO_CODE_LEN    2U
#define PROTO_DATALEN_LEN 2U
#define PROTO_CRC_LEN     2U
#define PROTO_TAIL_LEN    2U

#define PROTO_HEAD_OFS    ((size_t)0)
#define PROTO_TYPE_OFS    (PROTO_HEAD_OFS + PROTO_HEAD_LEN)
#define PROTO_CMD_OFS     (PROTO_TYPE_OFS + PROTO_TYPE_LEN)
#define PROTO_CODE_OFS    (PROTO_CMD_OFS + PROTO_CMD_LEN)
#define PROTO_DATALEN_OFS (PROTO_CODE_OFS + PROTO_CODE_LEN)
#define PROTO_DATA_OFS    (PROTO_DATALEN_OFS + PROTO_DATALEN_LEN)
#define PROTO_CRC_OFS(n)  (PROTO_DATA_OFS + (size_t)(n))
#define PROTO_TAIL_OFS(n) (PROTO_CRC_OFS(n) + PROTO_CRC_LEN)

#define PROTO_OVERHEAD       (PROTO_DATA_OFS + PROTO_CRC_LEN + PROTO_TAIL_LEN)
#define PROTO_MIN_LEN        PROTO_OVERHEAD
#define PROTO_CRC_COVER_BASE PROTO_DATA_OFS

#define PROTO_DATA_MAX ((size_t)96)
#define PROTO_BUF_MAX  (PROTO_OVERHEAD + PROTO_DATA_MAX)

PROTO_STATIC_ASSERT(PROTO_DATA_MAX == (size_t)96,
                    "maximum payload is fixed at 96 bytes by the wire protocol");
PROTO_STATIC_ASSERT(PROTO_OVERHEAD == (size_t)14, "wire overhead changed");
PROTO_STATIC_ASSERT(PROTO_DATA_OFS == (size_t)10, "CRC coverage base changed");
PROTO_STATIC_ASSERT(PROTO_BUF_MAX == PROTO_OVERHEAD + PROTO_DATA_MAX,
                    "encoded buffer must match maximum frame exactly");

typedef enum proto_status
{
    PROTO_OK         =  0,
    PROTO_E_NULL     = -1,
    PROTO_E_HEAD     = -2,
    PROTO_E_TAIL     = -3,
    PROTO_E_LENGTH   = -4,
    PROTO_E_DATASIZE = -5,
    PROTO_E_CRC      = -6,
    PROTO_E_NOSPACE  = -7,
    PROTO_E_NOFRAME  = -8,
    PROTO_E_INCOMPLETE = -9,
} proto_status_t;

PROTO_STATIC_ASSERT(sizeof(proto_status_t) == sizeof(int32_t),
                    "proto_status_t ABI is 4 bytes; do not use -fshort-enums");

typedef struct proto_frame
{
    uint8_t type;
    uint8_t cmd;
    uint16_t code;
    uint16_t data_len;
    uint8_t data[PROTO_DATA_MAX];
} proto_frame_t;

typedef struct proto_decoded
{
    proto_frame_t frame;
    proto_status_t status;
} proto_decoded_t;

typedef struct proto_encoded
{
    uint8_t bytes[PROTO_BUF_MAX];
    size_t size;
    proto_status_t status;
} proto_encoded_t;

typedef struct proto_span
{
    size_t offset;
    size_t size;
} proto_span_t;

PROTO_EXTERN_C_BEGIN

PROTO_API proto_status_t proto_decode(const uint8_t *bytes, size_t size,
                                      proto_decoded_t *out);

PROTO_API proto_status_t proto_encode(const proto_frame_t *frame,
                                      proto_encoded_t *out);

PROTO_API proto_status_t proto_encode_to(const proto_frame_t *frame,
                                         uint8_t *dst, size_t capacity,
                                         size_t *out_size);

PROTO_API proto_status_t proto_find(const uint8_t *stream, size_t size,
                                    proto_span_t *span, proto_decoded_t *out);

PROTO_API proto_status_t proto_find_bounded(const uint8_t *stream, size_t size,
                                            size_t window,
                                            proto_span_t *span,
                                            proto_decoded_t *out);

PROTO_API const char *proto_status_str(int status);

PROTO_EXTERN_C_END

#endif
