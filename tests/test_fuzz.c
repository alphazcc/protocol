/* Copyright (c) 2022-2026, HelloAlpha */
#include "proto/protocol.h"

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FUZZ_ITERATIONS UINT32_C(20000)
#define FUZZ_STREAM_MAX (PROTO_BUF_MAX * 2U)

typedef struct fuzz_counters
{
    unsigned invalid_span;
    unsigned bad_status;
    unsigned decode_mismatch;
    unsigned injected_found;
} fuzz_counters_t;

static uint32_t next_random(uint32_t *state)
{
    uint32_t x = *state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void verify_result(proto_status_t status, const uint8_t *stream,
                          const proto_span_t *span,
                          const proto_span_t *pristine,
                          size_t size, uint32_t injected_offset,
                          bool injected, fuzz_counters_t *counters)
{
    proto_decoded_t decoded;

    if (status == PROTO_OK)
    {
        if (span->offset > size || span->size > size - span->offset)
        {
            counters->invalid_span++;
        }
        memset(&decoded, 0, sizeof(decoded));
        if (proto_decode(stream + span->offset, span->size, &decoded) !=
            PROTO_OK)
        {
            counters->decode_mismatch++;
        }
        if (injected && span->offset == (size_t)injected_offset)
        {
            counters->injected_found++;
        }
        return;
    }

    if (status == PROTO_E_INCOMPLETE)
    {
        if (span->offset > size || span->size > PROTO_BUF_MAX)
        {
            counters->invalid_span++;
        }
        return;
    }

    if (status == PROTO_E_NOFRAME)
    {
        if (memcmp(span, pristine, sizeof(*span)) != 0)
        {
            counters->invalid_span++;
        }
        return;
    }

    counters->bad_status++;
}

int main(void)
{
    uint8_t stream[FUZZ_STREAM_MAX];
    proto_frame_t frame;
    proto_encoded_t encoded;
    proto_span_t span;
    proto_span_t pristine;
    proto_decoded_t decoded;
    fuzz_counters_t counters;
    uint32_t rng = UINT32_C(0x12345678);
    uint32_t i;
    size_t j;

    memset(&counters, 0, sizeof(counters));
    memset(&frame, 0, sizeof(frame));
    frame.type = UINT8_C(0x2A);
    frame.data_len = 4U;
    memcpy(frame.data, "FUZZ", 4U);

    if (proto_encode(&frame, &encoded) != PROTO_OK)
    {
        CHECK(0, "fuzz source frame encodes");
        TEST_RESULT("fuzz");
        return test_finish();
    }

    for (i = 0U; i < FUZZ_ITERATIONS; i++)
    {
        size_t size = (size_t)(next_random(&rng) % (uint32_t)FUZZ_STREAM_MAX);
        size_t window = (size_t)(next_random(&rng) % (uint32_t)FUZZ_STREAM_MAX);
        uint32_t inject = next_random(&rng) % (uint32_t)FUZZ_STREAM_MAX;
        bool injected = false;

        for (j = 0U; j < size; j++)
        {
            stream[j] = (uint8_t)next_random(&rng);
        }
        if ((size_t)inject + encoded.size <= size)
        {
            memcpy(stream + inject, encoded.bytes, encoded.size);
            injected = true;
        }

        memset(&span, 0xA5, sizeof(span));
        pristine = span;
        memset(&decoded, 0, sizeof(decoded));
        verify_result(proto_find(stream, size, &span, &decoded),
                      stream, &span, &pristine, size, inject, injected,
                      &counters);

        memset(&span, 0xA5, sizeof(span));
        pristine = span;
        memset(&decoded, 0, sizeof(decoded));
        verify_result(proto_find_bounded(stream, size, window, &span, &decoded),
                      stream, &span, &pristine, size, inject, injected,
                      &counters);
    }

    CHECK(counters.invalid_span == 0U, "fuzz span invariants");
    CHECK(counters.bad_status == 0U, "fuzz status range");
    CHECK(counters.decode_mismatch == 0U, "fuzz find/decode agreement");
    CHECK(counters.injected_found > 0U, "fuzz exercised full frames");
    TEST_RESULT("fuzz");
    return test_finish();
}
