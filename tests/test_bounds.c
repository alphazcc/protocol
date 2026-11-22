/* Copyright (c) 2022-2026, HelloAlpha */
#include "test_support.h"

#include "proto/protocol.h"

#include <string.h>

typedef struct guarded_decoded {
    uint8_t pre[32];
    proto_decoded_t decoded;
    uint8_t post[32];
} guarded_decoded_t;

typedef struct guarded_encoded {
    uint8_t pre[32];
    proto_encoded_t encoded;
    uint8_t post[32];
} guarded_encoded_t;

typedef struct guarded_stream {
    uint8_t pre[64];
    uint8_t data[512];
    uint8_t post[64];
} guarded_stream_t;

static void check_guard(const uint8_t *guard, size_t size, const char *label)
{
    size_t i;

    for (i = 0; i < size; i++)
    {
        if (guard[i] != UINT8_C(0xCD))
        {
            CHECK(false, label);
            return;
        }
    }
    g_checks++;
}

static void sweep_decode(void)
{
    uint8_t bytes[512];
    uint8_t payload[400];
    guarded_decoded_t guarded;
    uint16_t data_len;
    size_t i;

    for (i = 0; i < sizeof(payload); i++)
    {
        payload[i] = (uint8_t)(UINT8_C(0x30) + (uint8_t)(i & 15U));
    }

    for (data_len = 0; data_len <= 300U; data_len++)
    {
        size_t n = test_build_frame(bytes, data_len, payload, (size_t)data_len);

        memset(&guarded, 0xCD, sizeof(guarded));
        (void)proto_decode(bytes, n, &guarded.decoded);
        check_guard(guarded.pre, sizeof(guarded.pre), "decode pre-guard intact");
        check_guard(guarded.post, sizeof(guarded.post), "decode post-guard intact");
    }
}

static void sweep_encode(void)
{
    guarded_encoded_t guarded;
    proto_frame_t frame;
    uint16_t data_len;

    for (data_len = 0; data_len <= 300U; data_len++)
    {
        memset(&guarded, 0xCD, sizeof(guarded));
        memset(&frame, 0, sizeof(frame));
        frame.data_len = data_len;

        (void)proto_encode(&frame, &guarded.encoded);
        check_guard(guarded.pre, sizeof(guarded.pre), "encode pre-guard intact");
        check_guard(guarded.post, sizeof(guarded.post), "encode post-guard intact");
    }
}

static void sweep_find(void)
{
    guarded_stream_t guarded;
    proto_span_t span;
    proto_decoded_t decoded;
    size_t prefix;
    size_t i;
    uint32_t seed = 12345U;

    memset(&guarded, 0xCD, sizeof(guarded));
    for (i = 0; i < sizeof(guarded.data); i++)
    {
        seed = seed * UINT32_C(1103515245) + UINT32_C(12345);
        guarded.data[i] = (uint8_t)((seed >> 16) & UINT32_C(0xFF));
    }
    guarded.data[0] = PROTO_HEAD0;
    guarded.data[1] = PROTO_HEAD1;
    guarded.data[2] = PROTO_HEAD2;
    guarded.data[3] = PROTO_HEAD3;
    guarded.data[500] = PROTO_HEAD0;
    guarded.data[501] = PROTO_HEAD1;
    guarded.data[502] = PROTO_HEAD2;
    guarded.data[503] = PROTO_HEAD3;
    guarded.data[510] = PROTO_HEAD0;
    guarded.data[511] = PROTO_HEAD1;

    for (prefix = 0; prefix <= sizeof(guarded.data); prefix++)
    {
        memset(&span, 0, sizeof(span));
        memset(&decoded, 0, sizeof(decoded));
        (void)proto_find(guarded.data, prefix, &span, &decoded);
        check_guard(guarded.pre, sizeof(guarded.pre), "find pre-guard intact");
        check_guard(guarded.post, sizeof(guarded.post), "find post-guard intact");
    }
}

int main(void)
{
    sweep_decode();
    sweep_encode();
    sweep_find();

    TEST_RESULT("bounds");
    return test_finish();
}
