/* Copyright (c) 2022-2026, HelloAlpha */
#include "test_support.h"

#include "proto/protocol.h"

#include <string.h>

#define PROP_ITERATIONS 512U
#define PROP_STREAM_MAX 256U

static uint32_t prop_next(uint32_t *seed)
{
    *seed = (*seed * UINT32_C(1103515245)) + UINT32_C(12345);
    return *seed;
}

static void test_roundtrip_properties(void)
{
    uint32_t seed = UINT32_C(0x12345678);
    proto_frame_t frame;
    proto_encoded_t encoded;
    proto_decoded_t decoded;
    uint16_t data_len;
    unsigned int i;
    unsigned int j;

    for (i = 0U; i < PROP_ITERATIONS; i++)
    {
        memset(&frame, 0, sizeof(frame));
        memset(&encoded, 0, sizeof(encoded));
        memset(&decoded, 0, sizeof(decoded));

        frame.type = (uint8_t)prop_next(&seed);
        frame.cmd = (uint8_t)prop_next(&seed);
        frame.code = (uint16_t)prop_next(&seed);
        data_len = (uint16_t)(prop_next(&seed) % (uint32_t)(PROTO_DATA_MAX + 1U));
        frame.data_len = data_len;
        for (j = 0U; j < (unsigned int)data_len; j++)
        {
            frame.data[j] = (uint8_t)prop_next(&seed);
        }

        CHECK(proto_encode(&frame, &encoded) == PROTO_OK,
              "property: encode succeeds for generated frame");
        CHECK(encoded.size == PROTO_OVERHEAD + (size_t)data_len,
              "property: encoded size matches declared length");
        CHECK(encoded.status == PROTO_OK, "property: encode sets OK status");
        CHECK(proto_decode(encoded.bytes, encoded.size, &decoded) == PROTO_OK,
              "property: decode succeeds for encoded frame");

        CHECK(decoded.frame.type == frame.type, "property: type roundtrips");
        CHECK(decoded.frame.cmd == frame.cmd, "property: cmd roundtrips");
        CHECK(decoded.frame.code == frame.code, "property: code roundtrips");
        CHECK(decoded.frame.data_len == frame.data_len,
              "property: data length roundtrips");
        CHECK(memcmp(decoded.frame.data, frame.data, data_len) == 0,
              "property: payload roundtrips");
    }
}

static void test_find_span_bounds(void)
{
    uint32_t seed = UINT32_C(0x87654321);
    uint8_t stream[PROP_STREAM_MAX];
    proto_frame_t frame;
    proto_encoded_t encoded;
    proto_decoded_t decoded;
    proto_span_t span;
    size_t prefix;
    size_t stream_size;
    size_t i;
    unsigned int iter;

    for (iter = 0U; iter < PROP_ITERATIONS; iter++)
    {
        prefix = (size_t)(prop_next(&seed) % 32U);
        for (i = 0U; i < PROP_STREAM_MAX; i++)
        {
            stream[i] = (uint8_t)prop_next(&seed);
        }

        memset(&frame, 0, sizeof(frame));
        frame.type = (uint8_t)prop_next(&seed);
        frame.data_len = (uint16_t)(prop_next(&seed) % 17U);
        for (i = 0U; i < (size_t)frame.data_len; i++)
        {
            frame.data[i] = (uint8_t)prop_next(&seed);
        }
        memset(&encoded, 0, sizeof(encoded));
        CHECK(proto_encode(&frame, &encoded) == PROTO_OK,
              "property: embedded frame encodes");
        CHECK(prefix + encoded.size <= PROP_STREAM_MAX,
              "property: embedded frame fits stream");
        memcpy(stream + prefix, encoded.bytes, encoded.size);
        stream_size = prefix + encoded.size;

        memset(&span, 0, sizeof(span));
        memset(&decoded, 0, sizeof(decoded));
        CHECK(proto_find(stream, stream_size, &span, &decoded) == PROTO_OK,
              "property: embedded frame located");
        CHECK(span.offset <= stream_size,
              "property: span offset within stream");
        CHECK(span.size <= stream_size - span.offset,
              "property: span end within stream");
        CHECK(span.offset + span.size <= stream_size,
              "property: span end does not wrap");
        CHECK(decoded.status == PROTO_OK, "property: found frame status OK");
    }
}

static void test_failure_preserves_payload(void)
{
    uint32_t seed = UINT32_C(0x0F0F0F0F);
    uint8_t stream[PROTO_BUF_MAX];
    proto_frame_t frame;
    proto_encoded_t encoded;
    proto_decoded_t decoded;
    proto_decoded_t before;
    size_t flip;
    unsigned int iter;

    for (iter = 0U; iter < PROP_ITERATIONS; iter++)
    {
        memset(&frame, 0, sizeof(frame));
        frame.type = (uint8_t)prop_next(&seed);
        frame.cmd = (uint8_t)prop_next(&seed);
        frame.code = (uint16_t)prop_next(&seed);
        frame.data_len = (uint16_t)(prop_next(&seed) % (uint32_t)(PROTO_DATA_MAX + 1U));
        for (flip = 0U; flip < (size_t)frame.data_len; flip++)
        {
            frame.data[flip] = (uint8_t)prop_next(&seed);
        }

        memset(&encoded, 0, sizeof(encoded));
        CHECK(proto_encode(&frame, &encoded) == PROTO_OK,
              "property: frame encodes before corruption");
        memcpy(stream, encoded.bytes, encoded.size);

        if (frame.data_len > 0U)
        {
            flip = PROTO_DATA_OFS +
                   (size_t)(prop_next(&seed) % (uint32_t)frame.data_len);
            stream[flip] ^= UINT8_C(0x01);

            memset(&decoded, 0xA5, sizeof(decoded));
            memcpy(&before, &decoded, sizeof(decoded));
            CHECK(proto_decode(stream, encoded.size, &decoded) != PROTO_OK,
                  "property: corrupted frame is rejected");
            before.status = decoded.status;
            CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0,
                  "property: failed decode never modifies data fields");
        }
    }
}

static void test_encode_to_capacity_boundary(void)
{
    proto_frame_t frame;
    uint8_t dst[PROTO_BUF_MAX + 1U];
    size_t written;
    size_t required;
    uint16_t data_len;

    memset(&frame, 0, sizeof(frame));
    for (data_len = 0U; data_len <= PROTO_DATA_MAX; data_len++)
    {
        frame.data_len = data_len;
        required = PROTO_OVERHEAD + (size_t)data_len;

        memset(dst, 0x5A, sizeof(dst));
        written = SIZE_MAX;
        CHECK(proto_encode_to(&frame, dst, required - 1U, &written) ==
                  PROTO_E_NOSPACE,
              "property: capacity one byte short is rejected");
        CHECK(written == SIZE_MAX,
              "property: short capacity leaves size untouched");

        written = 0U;
        CHECK(proto_encode_to(&frame, dst, required, &written) == PROTO_OK,
              "property: exact capacity is accepted");
        CHECK(written == required, "property: exact capacity writes full frame");
        CHECK(dst[required] == UINT8_C(0x5A),
              "property: exact capacity writes no trailing byte");
    }
}

int main(void)
{
    test_roundtrip_properties();
    test_find_span_bounds();
    test_failure_preserves_payload();
    test_encode_to_capacity_boundary();

    TEST_RESULT("properties");
    return test_finish();
}
