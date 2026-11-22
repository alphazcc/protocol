/* Copyright (c) 2022-2026, HelloAlpha */
#include "proto/protocol.h"

#include "crc16.h"

#include <stdbool.h>
#include <string.h>

static uint16_t load_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void store_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & UINT16_C(0x00FF));
}

static bool head_matches(const uint8_t *p)
{
    return p[PROTO_HEAD_OFS] == PROTO_HEAD0 &&
           p[PROTO_HEAD_OFS + 1U] == PROTO_HEAD1 &&
           p[PROTO_HEAD_OFS + 2U] == PROTO_HEAD2 &&
           p[PROTO_HEAD_OFS + 3U] == PROTO_HEAD3;
}

static bool tail_matches(const uint8_t *p, uint16_t data_len)
{
    return p[PROTO_TAIL_OFS(data_len)] == PROTO_TAIL0 &&
           p[PROTO_TAIL_OFS(data_len) + 1U] == PROTO_TAIL1;
}

static proto_status_t fail_decoded(proto_decoded_t *out, proto_status_t code)
{
    if (out != NULL)
    {
        out->status = code;
    }
    return code;
}

static proto_status_t fail_encoded(proto_encoded_t *out, proto_status_t code)
{
    if (out != NULL)
    {
        out->status = code;
    }
    return code;
}

proto_status_t proto_decode(const uint8_t *bytes, size_t size,
                            proto_decoded_t *out)
{
    proto_frame_t tmp;
    uint16_t data_len;
    size_t expected;
    size_t i;

    if (out == NULL)
    {
        return PROTO_E_NULL;
    }
    if (bytes == NULL)
    {
        return fail_decoded(out, PROTO_E_NULL);
    }
    if (size < PROTO_MIN_LEN)
    {
        return fail_decoded(out, PROTO_E_LENGTH);
    }
    if (!head_matches(bytes))
    {
        return fail_decoded(out, PROTO_E_HEAD);
    }

    data_len = load_be16(&bytes[PROTO_DATALEN_OFS]);
    if ((size_t)data_len > PROTO_DATA_MAX)
    {
        return fail_decoded(out, PROTO_E_DATASIZE);
    }

    expected = PROTO_OVERHEAD + (size_t)data_len;
    if (size != expected)
    {
        return fail_decoded(out, PROTO_E_LENGTH);
    }
    if (!tail_matches(bytes, data_len))
    {
        return fail_decoded(out, PROTO_E_TAIL);
    }

    if (proto_crc16_modbus(bytes, PROTO_CRC_COVER_BASE + (size_t)data_len) !=
        load_be16(&bytes[PROTO_CRC_OFS(data_len)]))
    {
        return fail_decoded(out, PROTO_E_CRC);
    }

    memset(&tmp, 0, sizeof(tmp));
    tmp.type = bytes[PROTO_TYPE_OFS];
    tmp.cmd = bytes[PROTO_CMD_OFS];
    tmp.code = load_be16(&bytes[PROTO_CODE_OFS]);
    tmp.data_len = data_len;
    for (i = 0; i < (size_t)data_len; i++)
    {
        tmp.data[i] = bytes[PROTO_DATA_OFS + i];
    }

    out->frame = tmp;
    out->status = PROTO_OK;
    return PROTO_OK;
}

proto_status_t proto_encode_to(const proto_frame_t *frame, uint8_t *dst,
                               size_t capacity, size_t *out_size)
{
    size_t required;
    size_t pos;
    uint16_t i;

    if (frame == NULL || dst == NULL || out_size == NULL)
    {
        return PROTO_E_NULL;
    }
    if ((size_t)frame->data_len > PROTO_DATA_MAX)
    {
        return PROTO_E_DATASIZE;
    }

    required = PROTO_OVERHEAD + (size_t)frame->data_len;
    if (required > capacity)
    {
        return PROTO_E_NOSPACE;
    }

    dst[PROTO_HEAD_OFS] = PROTO_HEAD0;
    dst[PROTO_HEAD_OFS + 1U] = PROTO_HEAD1;
    dst[PROTO_HEAD_OFS + 2U] = PROTO_HEAD2;
    dst[PROTO_HEAD_OFS + 3U] = PROTO_HEAD3;
    dst[PROTO_TYPE_OFS] = frame->type;
    dst[PROTO_CMD_OFS] = frame->cmd;
    store_be16(&dst[PROTO_CODE_OFS], frame->code);
    store_be16(&dst[PROTO_DATALEN_OFS], frame->data_len);

    for (i = 0; i < frame->data_len; i++)
    {
        dst[PROTO_DATA_OFS + (size_t)i] = frame->data[i];
    }

    pos = PROTO_CRC_OFS(frame->data_len);
    store_be16(&dst[pos], proto_crc16_modbus(dst, pos));
    pos += PROTO_CRC_LEN;
    dst[pos++] = PROTO_TAIL0;
    dst[pos++] = PROTO_TAIL1;

    *out_size = pos;
    return PROTO_OK;
}

proto_status_t proto_encode(const proto_frame_t *frame, proto_encoded_t *out)
{
    size_t written = 0;
    proto_status_t rc;

    if (out == NULL)
    {
        return PROTO_E_NULL;
    }

    rc = proto_encode_to(frame, out->bytes, PROTO_BUF_MAX, &written);
    if (rc == PROTO_OK)
    {
        out->size = written;
    }
    return fail_encoded(out, rc);
}

static proto_status_t check_frame_candidate(const uint8_t *stream, size_t size,
                                            size_t i,
                                            proto_span_t *span,
                                            proto_decoded_t *out,
                                            bool *has_partial,
                                            size_t *partial_offset,
                                            size_t *partial_size)
{
    proto_decoded_t candidate;
    uint16_t data_len;
    size_t expected;

    if (!head_matches(&stream[i]))
    {
        return PROTO_E_NOFRAME;
    }

    if (size - i < PROTO_DATALEN_OFS + PROTO_DATALEN_LEN)
    {
        if (!*has_partial)
        {
            *has_partial = true;
            *partial_offset = i;
            *partial_size = 0U;
        }
        return PROTO_E_INCOMPLETE;
    }

    data_len = load_be16(&stream[i + PROTO_DATALEN_OFS]);
    if ((size_t)data_len > PROTO_DATA_MAX)
    {
        return PROTO_E_NOFRAME;
    }

    expected = PROTO_OVERHEAD + (size_t)data_len;
    if (size - i < expected)
    {
        if (!*has_partial)
        {
            *has_partial = true;
            *partial_offset = i;
            *partial_size = expected;
        }
        return PROTO_E_INCOMPLETE;
    }

    if (proto_decode(&stream[i], expected, &candidate) == PROTO_OK)
    {
        span->offset = i;
        span->size = expected;
        *out = candidate;
        return PROTO_OK;
    }
    return PROTO_E_NOFRAME;
}

static proto_status_t find_in_window(const uint8_t *stream, size_t size,
                                     size_t window,
                                     proto_span_t *span,
                                     proto_decoded_t *out)
{
    size_t i;
    size_t limit;
    size_t scan_end;
    size_t partial_offset = 0U;
    size_t partial_size = 0U;
    bool has_partial = false;

    if (stream == NULL || span == NULL || out == NULL)
    {
        if (out != NULL)
        {
            out->status = PROTO_E_NULL;
        }
        return PROTO_E_NULL;
    }
    if (size < PROTO_HEAD_LEN)
    {
        out->status = PROTO_E_NOFRAME;
        return PROTO_E_NOFRAME;
    }

    scan_end = (window < size) ? window : size;
    if (scan_end < PROTO_HEAD_LEN)
    {
        out->status = PROTO_E_NOFRAME;
        return PROTO_E_NOFRAME;
    }

    limit = scan_end - PROTO_HEAD_LEN;
#ifdef PROTO_FIND_USE_MEMCHR
    i = 0U;
    while (i <= limit)
    {
        const uint8_t *hit = (const uint8_t *)memchr(&stream[i], PROTO_HEAD0,
                                                     limit - i + 1U);
        if (hit == NULL)
        {
            break;
        }
        i = (size_t)(hit - stream);
        if (check_frame_candidate(stream, size, i, span, out,
                                  &has_partial, &partial_offset,
                                  &partial_size) == PROTO_OK)
        {
            return PROTO_OK;
        }
        i++;
    }
#else
    for (i = 0U; i <= limit; i++)
    {
        if (check_frame_candidate(stream, size, i, span, out,
                                  &has_partial, &partial_offset,
                                  &partial_size) == PROTO_OK)
        {
            return PROTO_OK;
        }
    }
#endif

    if (has_partial)
    {
        out->status = PROTO_E_INCOMPLETE;
        span->offset = partial_offset;
        span->size = partial_size;
        return PROTO_E_INCOMPLETE;
    }
    out->status = PROTO_E_NOFRAME;
    return PROTO_E_NOFRAME;
}

proto_status_t proto_find(const uint8_t *stream, size_t size,
                          proto_span_t *span, proto_decoded_t *out)
{
    return find_in_window(stream, size, size, span, out);
}

proto_status_t proto_find_bounded(const uint8_t *stream, size_t size,
                                  size_t window,
                                  proto_span_t *span,
                                  proto_decoded_t *out)
{
    return find_in_window(stream, size, window, span, out);
}

const char *proto_status_str(int status)
{
    switch (status)
    {
    case PROTO_OK:         return "OK";
    case PROTO_E_NULL:     return "null argument";
    case PROTO_E_HEAD:     return "header mismatch";
    case PROTO_E_TAIL:     return "trailer mismatch";
    case PROTO_E_LENGTH:   return "length error";
    case PROTO_E_DATASIZE: return "data length out of range";
    case PROTO_E_CRC:      return "CRC mismatch";
    case PROTO_E_NOSPACE:  return "insufficient output capacity";
    case PROTO_E_NOFRAME:  return "no valid frame in stream";
    case PROTO_E_INCOMPLETE: return "frame incomplete";
    default:               return "unknown status";
    }
}
