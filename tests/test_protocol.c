/* Copyright (c) 2022-2026, HelloAlpha */
#include "test_support.h"

#include "proto/protocol.h"

#include <string.h>

static const uint8_t known_frame[] = {
    UINT8_C(0xED), UINT8_C(0xB9), UINT8_C(0x55), UINT8_C(0xAA),
    UINT8_C(0x01), UINT8_C(0x02), UINT8_C(0xAB), UINT8_C(0xCD),
    UINT8_C(0x00), UINT8_C(0x08), UINT8_C(0x12), UINT8_C(0x34),
    UINT8_C(0x56), UINT8_C(0x78), UINT8_C(0xAB), UINT8_C(0xCD),
    UINT8_C(0xEF), UINT8_C(0x5A), UINT8_C(0x14), UINT8_C(0x47),
    UINT8_C(0x5A), UINT8_C(0xA5),
};

static const uint8_t known_data[8] = {
    UINT8_C(0x12), UINT8_C(0x34), UINT8_C(0x56), UINT8_C(0x78),
    UINT8_C(0xAB), UINT8_C(0xCD), UINT8_C(0xEF), UINT8_C(0x5A),
};

static void test_known_vector(void)
{
    proto_decoded_t out;

    memset(&out, 0, sizeof(out));
    CHECK(proto_decode(known_frame, sizeof(known_frame), &out) == PROTO_OK,
          "known vector decodes");
    CHECK(out.status == PROTO_OK, "known vector status");
    CHECK(out.frame.type == UINT8_C(0x01), "known vector type");
    CHECK(out.frame.cmd == UINT8_C(0x02), "known vector cmd");
    CHECK(out.frame.code == UINT16_C(0xABCD), "known vector code");
    CHECK(out.frame.data_len == 8U, "known vector data length");
    CHECK(memcmp(out.frame.data, known_data, sizeof(known_data)) == 0,
          "known vector payload");
}

static void test_roundtrip(void)
{
    proto_frame_t frame;
    proto_encoded_t encoded;
    proto_decoded_t decoded;

    memset(&frame, 0, sizeof(frame));
    memset(&encoded, 0, sizeof(encoded));
    memset(&decoded, 0, sizeof(decoded));

    frame.type = UINT8_C(0x07);
    frame.cmd = UINT8_C(0x13);
    frame.code = UINT16_C(0x1234);
    frame.data_len = 8U;
    memcpy(frame.data, known_data, sizeof(known_data));

    CHECK(proto_encode(&frame, &encoded) == PROTO_OK, "roundtrip encode");
    CHECK(encoded.status == PROTO_OK, "roundtrip encode status");
    CHECK(encoded.size == PROTO_OVERHEAD + 8U, "roundtrip encoded size");
    CHECK(proto_decode(encoded.bytes, encoded.size, &decoded) == PROTO_OK,
          "roundtrip decode");
    CHECK(decoded.frame.type == frame.type, "roundtrip type");
    CHECK(decoded.frame.cmd == frame.cmd, "roundtrip cmd");
    CHECK(decoded.frame.code == frame.code, "roundtrip code");
    CHECK(decoded.frame.data_len == frame.data_len, "roundtrip data length");
    CHECK(memcmp(decoded.frame.data, frame.data, sizeof(known_data)) == 0,
          "roundtrip payload");
}

static void test_null_arguments(void)
{
    proto_frame_t frame;
    proto_encoded_t encoded;
    proto_decoded_t decoded;
    proto_span_t span;
    uint8_t bytes[PROTO_MIN_LEN];
    size_t written;

    memset(&frame, 0, sizeof(frame));
    memset(&encoded, 0, sizeof(encoded));
    memset(&decoded, 0, sizeof(decoded));
    memset(&span, 0, sizeof(span));
    memset(bytes, 0, sizeof(bytes));

    CHECK(proto_decode(NULL, sizeof(bytes), &decoded) == PROTO_E_NULL,
          "decode rejects NULL bytes");
    CHECK(decoded.status == PROTO_E_NULL, "decode NULL reports status");
    CHECK(proto_decode(bytes, sizeof(bytes), NULL) == PROTO_E_NULL,
          "decode rejects NULL output without crash");

    CHECK(proto_encode(NULL, &encoded) == PROTO_E_NULL, "encode rejects NULL frame");
    CHECK(encoded.status == PROTO_E_NULL, "encode NULL reports status");
    CHECK(proto_encode(&frame, NULL) == PROTO_E_NULL,
          "encode rejects NULL output without crash");

    CHECK(proto_encode_to(NULL, bytes, sizeof(bytes), &written) == PROTO_E_NULL,
          "encode_to rejects NULL frame");
    CHECK(proto_encode_to(&frame, NULL, sizeof(bytes), &written) == PROTO_E_NULL,
          "encode_to rejects NULL destination");
    CHECK(proto_encode_to(&frame, bytes, sizeof(bytes), NULL) == PROTO_E_NULL,
          "encode_to rejects NULL size output");

    CHECK(proto_find(NULL, sizeof(bytes), &span, &decoded) == PROTO_E_NULL,
          "find rejects NULL stream");
    CHECK(proto_find(bytes, sizeof(bytes), NULL, &decoded) == PROTO_E_NULL,
          "find rejects NULL span");
    CHECK(proto_find(bytes, sizeof(bytes), &span, NULL) == PROTO_E_NULL,
          "find rejects NULL decoded output");
}

static void test_undersized_frames(void)
{
    const size_t sizes[] = {0U, 1U, 3U, 13U};
    uint8_t bytes[PROTO_BUF_MAX];
    proto_decoded_t decoded;
    size_t i;

    CHECK(test_build_frame(bytes, 0U, NULL, 0U) == PROTO_MIN_LEN,
          "empty frame is minimum size");

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        memset(&decoded, 0, sizeof(decoded));
        CHECK(proto_decode(bytes, sizes[i], &decoded) == PROTO_E_LENGTH,
              "undersized frame rejected");
        CHECK(decoded.status == PROTO_E_LENGTH, "undersized reports length error");
    }
}

static void test_oversized_data(void)
{
    uint8_t bytes[PROTO_OVERHEAD + 300U];
    uint8_t payload[300U];
    proto_frame_t frame;
    proto_encoded_t encoded;
    proto_decoded_t decoded;
    size_t n;

    memset(payload, 0x5A, sizeof(payload));

    n = test_build_frame(bytes, (uint16_t)(PROTO_DATA_MAX + 1U), payload,
                    PROTO_DATA_MAX + 1U);
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_decode(bytes, n, &decoded) == PROTO_E_DATASIZE,
          "decode rejects data length above maximum");

    n = test_build_frame(bytes, 300U, payload, 300U);
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_decode(bytes, n, &decoded) == PROTO_E_DATASIZE,
          "decode rejects data length 300 without wraparound");

    memset(&frame, 0, sizeof(frame));
    memset(&encoded, 0, sizeof(encoded));
    frame.data_len = (uint16_t)(PROTO_DATA_MAX + 1U);
    CHECK(proto_encode(&frame, &encoded) == PROTO_E_DATASIZE,
          "encode rejects data length above maximum");
    CHECK(encoded.status == PROTO_E_DATASIZE, "encode oversized reports status");
    CHECK(encoded.size == 0U, "encode oversized leaves size untouched");
}

static void test_crc_error(void)
{
    uint8_t bytes[PROTO_MIN_LEN + 8U];
    const uint8_t payload[8] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
    proto_decoded_t decoded;
    size_t n;

    n = test_build_frame(bytes, 8U, payload, sizeof(payload));
    bytes[PROTO_CRC_COVER_BASE] ^= UINT8_C(0xFF);

    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_decode(bytes, n, &decoded) == PROTO_E_CRC, "CRC mismatch reported");
    CHECK(decoded.status == PROTO_E_CRC, "CRC mismatch status");
}

static void test_length_mismatch(void)
{
    uint8_t bytes[PROTO_BUF_MAX];
    const uint8_t payload[8] = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
    proto_decoded_t decoded;
    size_t n;

    n = test_build_frame(bytes, 8U, payload, sizeof(payload));

    bytes[PROTO_DATALEN_OFS + 1U] = 16U;
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_decode(bytes, n, &decoded) == PROTO_E_LENGTH,
          "declared length above payload rejected");
    bytes[PROTO_DATALEN_OFS + 1U] = 8U;

    bytes[n - 2U] = UINT8_C(0x00);
    bytes[n - 1U] = PROTO_TAIL0;
    bytes[n] = PROTO_TAIL1;
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_decode(bytes, n + 1U, &decoded) == PROTO_E_LENGTH,
          "extra trailing byte rejected");

    bytes[n - 3U] = PROTO_TAIL0;
    bytes[n - 2U] = PROTO_TAIL1;
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_decode(bytes, n - 1U, &decoded) == PROTO_E_LENGTH,
          "truncated frame rejected");
}

static void test_magic_errors(void)
{
    uint8_t bytes[PROTO_MIN_LEN];
    proto_decoded_t decoded;
    size_t n;

    n = test_build_frame(bytes, 0U, NULL, 0U);

    bytes[0] = UINT8_C(0x00);
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_decode(bytes, n, &decoded) == PROTO_E_HEAD,
          "bad header reports header error");

    bytes[0] = PROTO_HEAD0;
    bytes[n - 1U] = UINT8_C(0x00);
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_decode(bytes, n, &decoded) == PROTO_E_TAIL,
          "bad trailer reports trailer error");
}

static void test_in_place_decode(void)
{
    union {
        uint8_t raw[sizeof(proto_decoded_t) + 16U];
        proto_decoded_t decoded;
    } storage;
    proto_frame_t frame;
    proto_encoded_t encoded;
    int rc;

    memset(&storage, 0, sizeof(storage));
    memset(&frame, 0, sizeof(frame));
    memset(&encoded, 0, sizeof(encoded));

    frame.type = UINT8_C(0x7E);
    frame.cmd = UINT8_C(0x5A);
    frame.code = UINT16_C(0x1234);
    frame.data_len = 8U;
    memcpy(frame.data, "ABCDEFGH", 8U);

    CHECK(proto_encode(&frame, &encoded) == PROTO_OK, "in-place source encodes");
    CHECK(encoded.size <= sizeof(storage.raw), "in-place frame fits storage");

    memcpy(storage.raw, encoded.bytes, encoded.size);
    rc = proto_decode(storage.raw, encoded.size, &storage.decoded);

    CHECK(rc == PROTO_OK, "in-place decode succeeds");
    CHECK(storage.decoded.frame.type == frame.type, "in-place type preserved");
    CHECK(storage.decoded.frame.cmd == frame.cmd, "in-place cmd preserved");
    CHECK(storage.decoded.frame.code == frame.code, "in-place code preserved");
    CHECK(storage.decoded.frame.data_len == frame.data_len,
          "in-place length preserved");
    CHECK(memcmp(storage.decoded.frame.data, frame.data, 8U) == 0,
          "in-place payload preserved");
}

static void test_failure_preserves_output(void)
{
    uint8_t bytes[sizeof(known_frame)];
    proto_decoded_t decoded;
    proto_decoded_t before;

    memcpy(bytes, known_frame, sizeof(known_frame));
    bytes[PROTO_CRC_COVER_BASE] ^= UINT8_C(0xFF);

    memset(&decoded, 0xA5, sizeof(decoded));
    memcpy(&before, &decoded, sizeof(decoded));

    CHECK(proto_decode(bytes, sizeof(bytes), &decoded) == PROTO_E_CRC,
          "corrupt frame fails CRC");
    before.status = PROTO_E_CRC;
    CHECK(memcmp(&decoded, &before, sizeof(decoded)) == 0,
          "failed decode leaves data fields intact");
}

static void test_boundaries(void)
{
    proto_frame_t frame;
    proto_encoded_t encoded;
    proto_decoded_t decoded;
    uint16_t i;

    memset(&frame, 0, sizeof(frame));
    memset(&encoded, 0, sizeof(encoded));
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_encode(&frame, &encoded) == PROTO_OK, "empty payload encodes");
    CHECK(encoded.size == PROTO_MIN_LEN, "empty frame is minimum size");
    CHECK(proto_decode(encoded.bytes, encoded.size, &decoded) == PROTO_OK,
          "empty payload decodes");
    CHECK(decoded.frame.data_len == 0U, "empty payload length preserved");

    memset(&frame, 0, sizeof(frame));
    memset(&encoded, 0, sizeof(encoded));
    memset(&decoded, 0, sizeof(decoded));
    frame.data_len = (uint16_t)PROTO_DATA_MAX;
    for (i = 0; i < PROTO_DATA_MAX; i++)
    {
        frame.data[i] = (uint8_t)i;
    }
    CHECK(proto_encode(&frame, &encoded) == PROTO_OK, "maximum payload encodes");
    CHECK(encoded.size == PROTO_OVERHEAD + PROTO_DATA_MAX,
          "maximum frame size");
    CHECK(encoded.size <= PROTO_BUF_MAX, "maximum frame fits buffer");
    CHECK(proto_decode(encoded.bytes, encoded.size, &decoded) == PROTO_OK,
          "maximum payload decodes");
    CHECK(memcmp(decoded.frame.data, frame.data, PROTO_DATA_MAX) == 0,
          "maximum payload preserved");
}

static void test_encode_to(void)
{
    proto_frame_t frame;
    uint8_t dst[PROTO_BUF_MAX];
    uint8_t guard[PROTO_MIN_LEN + 4U];
    size_t written;
    size_t i;
    int pristine;

    memset(&frame, 0, sizeof(frame));
    frame.type = UINT8_C(0x09);
    frame.cmd = UINT8_C(0x0A);
    frame.code = UINT16_C(0xBEEF);
    frame.data_len = 3U;
    frame.data[0] = UINT8_C(0x11);
    frame.data[1] = UINT8_C(0x22);
    frame.data[2] = UINT8_C(0x33);

    memset(dst, 0, sizeof(dst));
    written = 0U;
    CHECK(proto_encode_to(&frame, dst, sizeof(dst), &written) == PROTO_OK,
          "encode_to writes frame");
    CHECK(written == PROTO_OVERHEAD + 3U, "encode_to reports exact size");

    memset(guard, 0x5A, sizeof(guard));
    written = 12345U;
    CHECK(proto_encode_to(&frame, guard, PROTO_OVERHEAD + 2U, &written) ==
              PROTO_E_NOSPACE,
          "encode_to reports insufficient capacity");
    CHECK(written == 12345U, "encode_to leaves size untouched on failure");
    pristine = 1;
    for (i = 0; i < sizeof(guard); i++)
    {
        if (guard[i] != UINT8_C(0x5A))
        {
            pristine = 0;
        }
    }
    CHECK(pristine, "encode_to leaves destination untouched on failure");
}

static void test_find(void)
{
    uint8_t stream[PROTO_BUF_MAX * 4];
    proto_frame_t frames[2];
    proto_encoded_t encoded[2];
    proto_decoded_t decoded;
    proto_span_t span;
    size_t sizes[2];
    size_t total;
    size_t i;

    memset(frames, 0, sizeof(frames));
    memset(encoded, 0, sizeof(encoded));

    frames[0].type = UINT8_C(0x01);
    frames[0].data_len = 4U;
    memcpy(frames[0].data, "WXYZ", 4U);
    frames[1].type = UINT8_C(0x03);
    frames[1].data_len = 8U;
    memcpy(frames[1].data, "ABCDEFGH", 8U);

    for (i = 0; i < 2U; i++)
    {
        CHECK(proto_encode(&frames[i], &encoded[i]) == PROTO_OK,
              "find source frame encodes");
        sizes[i] = encoded[i].size;
    }

    memset(stream, 0, sizeof(stream));
    memcpy(stream, encoded[0].bytes, sizes[0]);
    memset(&span, 0, sizeof(span));
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_find(stream, sizes[0], &span, &decoded) == PROTO_OK,
          "find lone frame");
    CHECK(span.offset == 0U, "lone frame offset");
    CHECK(span.size == sizes[0], "lone frame size");
    CHECK(decoded.frame.type == frames[0].type, "find decodes output");

    memset(stream, 0xA7, sizeof(stream));
    memcpy(stream + 7U, encoded[0].bytes, sizes[0]);
    memset(&span, 0, sizeof(span));
    CHECK(proto_find(stream, 7U + sizes[0], &span, &decoded) == PROTO_OK,
          "find with leading noise");
    CHECK(span.offset == 7U, "leading noise skipped");

    memcpy(stream, encoded[0].bytes, sizes[0]);
    memcpy(stream + sizes[0], encoded[1].bytes, sizes[1]);
    total = sizes[0] + sizes[1];
    memset(&span, 0, sizeof(span));
    CHECK(proto_find(stream, total, &span, &decoded) == PROTO_OK,
          "find first concatenated frame");
    CHECK(span.offset == 0U && span.size == sizes[0], "first frame span");
    memset(&span, 0, sizeof(span));
    CHECK(proto_find(stream + sizes[0], sizes[1], &span, &decoded) == PROTO_OK,
          "find second concatenated frame");
    CHECK(span.offset == 0U && span.size == sizes[1], "second frame span");

    {
        proto_frame_t spoof;
        proto_encoded_t spoof_encoded;

        memset(&spoof, 0, sizeof(spoof));
        memset(&spoof_encoded, 0, sizeof(spoof_encoded));
        spoof.data_len = 8U;
        spoof.data[0] = PROTO_HEAD0;
        spoof.data[1] = PROTO_HEAD1;
        spoof.data[2] = PROTO_HEAD2;
        spoof.data[3] = PROTO_HEAD3;
        spoof.data[4] = PROTO_TAIL0;
        spoof.data[5] = PROTO_TAIL1;
        CHECK(proto_encode(&spoof, &spoof_encoded) == PROTO_OK,
              "find spoof frame encodes");
        memset(stream, 0, sizeof(stream));
        memcpy(stream, spoof_encoded.bytes, spoof_encoded.size);
        memset(&span, 0, sizeof(span));
        memset(&decoded, 0, sizeof(decoded));
        CHECK(proto_find(stream, spoof_encoded.size, &span, &decoded) == PROTO_OK,
              "find ignores header magic inside payload");
        CHECK(span.offset == 0U, "spoof offset is real frame");
        CHECK(decoded.frame.data[0] == PROTO_HEAD0, "spoof bytes preserved");
    }

    memcpy(stream, encoded[1].bytes, sizes[1]);
    memset(&span, 0, sizeof(span));
    CHECK(proto_find(stream, sizes[1] - 3U, &span, &decoded) == PROTO_E_INCOMPLETE,
          "incomplete frame reports incomplete status");
    CHECK(span.offset == 0U, "incomplete frame offset");
    CHECK(span.size == sizes[1], "incomplete frame expected size");

    memset(stream, 0, sizeof(stream));
    stream[0] = PROTO_HEAD0;
    stream[1] = PROTO_HEAD1;
    stream[2] = PROTO_HEAD2;
    stream[3] = PROTO_HEAD3;
    stream[PROTO_DATALEN_OFS + 1U] = 60U;
    memcpy(stream + 12U, encoded[0].bytes, sizes[0]);
    memset(&span, 0, sizeof(span));
    CHECK(proto_find(stream, 12U + sizes[0], &span, &decoded) == PROTO_OK,
          "false incomplete header does not mask real frame");
    CHECK(span.offset == 12U, "real frame found after false header");

    memset(stream, 0x33, sizeof(stream));
    memset(&span, 0, sizeof(span));
    CHECK(proto_find(stream, sizeof(stream), &span, &decoded) == PROTO_E_NOFRAME,
          "no frame reports no frame");
    CHECK(proto_find(stream, 3U, &span, &decoded) == PROTO_E_NOFRAME,
          "stream below minimum reports no frame");
}

static void test_find_noframe_preserves_span(void)
{
    uint8_t stream[PROTO_BUF_MAX];
    proto_span_t span;
    proto_decoded_t decoded;

    memset(stream, 0x33, sizeof(stream));
    memset(&span, 0, sizeof(span));
    memset(&decoded, 0, sizeof(decoded));

    CHECK(proto_find(stream, sizeof(stream), &span, &decoded) == PROTO_E_NOFRAME,
          "no-frame search returns no-frame status");
    CHECK(span.offset == 0U && span.size == 0U,
          "no-frame search leaves span untouched");
}

static void test_find_bounded_null_arguments(void)
{
    uint8_t stream[PROTO_MIN_LEN];
    proto_span_t span;
    proto_decoded_t decoded;

    memset(stream, 0, sizeof(stream));
    memset(&span, 0, sizeof(span));
    memset(&decoded, 0, sizeof(decoded));

    CHECK(proto_find_bounded(NULL, sizeof(stream), sizeof(stream),
                             &span, &decoded) == PROTO_E_NULL,
          "bounded find rejects NULL stream");
    CHECK(proto_find_bounded(stream, sizeof(stream), sizeof(stream),
                             NULL, &decoded) == PROTO_E_NULL,
          "bounded find rejects NULL span");
    CHECK(decoded.status == PROTO_E_NULL,
          "bounded find NULL span sets status");
    CHECK(proto_find_bounded(stream, sizeof(stream), sizeof(stream),
                             &span, NULL) == PROTO_E_NULL,
          "bounded find rejects NULL decoded output");
}

static void test_find_partial_and_status(void)
{
    proto_span_t span;
    proto_decoded_t decoded;
    const size_t short_sizes[] = {4U, 8U};
    const size_t sized_partials[] = {12U, 13U};
    size_t i;

    for (i = 0U; i < sizeof(short_sizes) / sizeof(short_sizes[0]); i++)
    {
        memset(&span, 0, sizeof(span));
        memset(&decoded, 0, sizeof(decoded));
        decoded.status = PROTO_OK;

        CHECK(proto_find(known_frame, short_sizes[i], &span, &decoded) ==
                  PROTO_E_INCOMPLETE,
              "short header reports incomplete status");
        CHECK(decoded.status == PROTO_E_INCOMPLETE, "short header sets status");
        CHECK(span.offset == 0U, "short header offset");
        CHECK(span.size == 0U, "short header size unknown");
    }

    for (i = 0U; i < sizeof(sized_partials) / sizeof(sized_partials[0]); i++)
    {
        memset(&span, 0, sizeof(span));
        memset(&decoded, 0, sizeof(decoded));
        decoded.status = PROTO_OK;

        CHECK(proto_find(known_frame, sized_partials[i], &span, &decoded) ==
                  PROTO_E_INCOMPLETE,
              "partial frame reports incomplete status");
        CHECK(decoded.status == PROTO_E_INCOMPLETE, "partial frame sets status");
        CHECK(span.offset == 0U, "partial frame offset");
        CHECK(span.size == sizeof(known_frame), "partial frame expected size");
    }

    memset(&span, 0, sizeof(span));
    memset(&decoded, 0, sizeof(decoded));
    decoded.status = PROTO_OK;
    CHECK(proto_find(known_frame, sizeof(known_frame) - 1U, &span, &decoded) ==
              PROTO_E_INCOMPLETE,
          "truncated frame reports incomplete status");
    CHECK(decoded.status == PROTO_E_INCOMPLETE, "truncated frame sets status");

    memset(&span, 0, sizeof(span));
    memset(&decoded, 0, sizeof(decoded));
    decoded.status = PROTO_OK;
    CHECK(proto_find(known_frame, sizeof(known_frame), &span, &decoded) ==
              PROTO_OK,
          "complete frame still decodes");
    CHECK(decoded.status == PROTO_OK, "complete frame sets OK status");

    memset(&span, 0, sizeof(span));
    memset(&decoded, 0, sizeof(decoded));
    decoded.status = PROTO_OK;
    CHECK(proto_find(NULL, sizeof(known_frame), &span, &decoded) == PROTO_E_NULL,
          "find rejects NULL stream");
    CHECK(decoded.status == PROTO_E_NULL, "NULL stream sets status");
}

static void test_find_bounded(void)
{
    uint8_t stream[sizeof(known_frame) + 4U];
    proto_span_t span;
    proto_decoded_t decoded;

    memcpy(stream, "xxxx", 4U);
    memcpy(stream + 4U, known_frame, sizeof(known_frame));

    memset(&span, 0, sizeof(span));
    decoded.status = PROTO_OK;
    CHECK(proto_find_bounded(stream, sizeof(stream), 4U, &span, &decoded) ==
              PROTO_E_NOFRAME,
          "window before frame reports no frame");
    CHECK(decoded.status == PROTO_E_NOFRAME,
          "window before frame sets status");

    memset(&span, 0, sizeof(span));
    decoded.status = PROTO_OK;
    CHECK(proto_find_bounded(stream, sizeof(stream), 8U, &span, &decoded) ==
              PROTO_OK,
          "window covering frame head finds frame");
    CHECK(span.offset == 4U, "bounded frame offset");
    CHECK(span.size == sizeof(known_frame), "bounded frame size");
    CHECK(decoded.status == PROTO_OK, "bounded frame sets status");

    memset(&span, 0, sizeof(span));
    decoded.status = PROTO_OK;
    CHECK(proto_find_bounded(known_frame, sizeof(known_frame),
                             sizeof(known_frame) + 16U, &span, &decoded) ==
              PROTO_OK,
          "window larger than size behaves as find");
    CHECK(decoded.status == PROTO_OK, "oversized window sets status");

    memset(&span, 0, sizeof(span));
    decoded.status = PROTO_OK;
    CHECK(proto_find_bounded(known_frame, sizeof(known_frame), 3U,
                             &span, &decoded) == PROTO_E_NOFRAME,
          "window shorter than header reports no frame");
    CHECK(decoded.status == PROTO_E_NOFRAME, "short window sets status");

    memset(&span, 0, sizeof(span));
    decoded.status = PROTO_OK;
    CHECK(proto_find_bounded(known_frame, 12U, 12U, &span, &decoded) ==
              PROTO_E_INCOMPLETE,
          "bounded window reports partial frame");
    CHECK(decoded.status == PROTO_E_INCOMPLETE, "bounded partial sets status");
    CHECK(span.size == sizeof(known_frame), "bounded partial reports frame size");

    memset(&span, 0, sizeof(span));
    decoded.status = PROTO_OK;
    CHECK(proto_find_bounded(NULL, sizeof(known_frame),
                             sizeof(known_frame), &span, &decoded) ==
              PROTO_E_NULL,
          "bounded rejects NULL stream");
    CHECK(decoded.status == PROTO_E_NULL, "bounded NULL sets status");

    memset(&span, 0, sizeof(span));
    decoded.status = PROTO_OK;
    CHECK(proto_find(stream, sizeof(stream), &span, &decoded) == PROTO_OK,
          "unbounded find still scans whole stream");
    CHECK(span.offset == 4U, "unbounded find offset unchanged");
}

static void check_status_text(int code, const char *expected)
{
    CHECK(strcmp(proto_status_str(code), expected) == 0,
          "status text matches contract");
}

static void test_status_str(void)
{
    const int codes[] = {
        PROTO_OK, PROTO_E_NULL, PROTO_E_HEAD, PROTO_E_TAIL,
        PROTO_E_LENGTH, PROTO_E_DATASIZE, PROTO_E_CRC,
        PROTO_E_NOSPACE, PROTO_E_NOFRAME, PROTO_E_INCOMPLETE, 999,
    };
    size_t i;
    size_t j;

    for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
    {
        CHECK(proto_status_str(codes[i]) != NULL, "status text is non-NULL");
    }
    for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
    {
        for (j = i + 1U; j < sizeof(codes) / sizeof(codes[0]); j++)
        {
            CHECK(strcmp(proto_status_str(codes[i]), proto_status_str(codes[j])) != 0,
                  "status texts are distinct");
        }
    }

    check_status_text(PROTO_OK, "OK");
    check_status_text(PROTO_E_NULL, "null argument");
    check_status_text(PROTO_E_HEAD, "header mismatch");
    check_status_text(PROTO_E_TAIL, "trailer mismatch");
    check_status_text(PROTO_E_LENGTH, "length error");
    check_status_text(PROTO_E_DATASIZE, "data length out of range");
    check_status_text(PROTO_E_CRC, "CRC mismatch");
    check_status_text(PROTO_E_NOSPACE, "insufficient output capacity");
    check_status_text(PROTO_E_NOFRAME, "no valid frame in stream");
    check_status_text(PROTO_E_INCOMPLETE, "frame incomplete");
    check_status_text(999, "unknown status");
}

static void test_find_bounded_extremes(void)
{
    uint8_t stream[sizeof(known_frame)];
    proto_span_t span;
    proto_decoded_t decoded;

    memcpy(stream, known_frame, sizeof(stream));

    memset(&span, 0, sizeof(span));
    decoded.status = PROTO_OK;
    CHECK(proto_find_bounded(stream, sizeof(stream), 0U, &span, &decoded) ==
              PROTO_E_NOFRAME,
          "zero window reports no frame");
    CHECK(decoded.status == PROTO_E_NOFRAME, "zero window sets status");

    memset(&span, 0, sizeof(span));
    decoded.status = PROTO_OK;
    CHECK(proto_find_bounded(stream, sizeof(stream), SIZE_MAX, &span, &decoded) ==
              PROTO_OK,
          "SIZE_MAX window behaves as find");
    CHECK(span.offset == 0U && span.size == sizeof(known_frame),
          "SIZE_MAX window span");
}

static void test_find_illegal_length_candidate(void)
{
    uint8_t stream[sizeof(known_frame) * 2U];
    proto_span_t span;
    proto_decoded_t decoded;

    memset(stream, 0x33, sizeof(stream));
    memcpy(stream, known_frame, PROTO_DATALEN_OFS + PROTO_DATALEN_LEN);
    stream[PROTO_DATALEN_OFS] = 0x01U;
    stream[PROTO_DATALEN_OFS + 1U] = 0x00U;
    memcpy(stream + sizeof(known_frame), known_frame, sizeof(known_frame));

    memset(&span, 0, sizeof(span));
    decoded.status = PROTO_OK;
    CHECK(proto_find(stream, sizeof(stream), &span, &decoded) == PROTO_OK,
          "illegal length candidate does not mask later frame");
    CHECK(span.offset == sizeof(known_frame),
          "real frame located after illegal candidate");
}

static void test_crc_bit_flips(void)
{
    uint8_t frame[sizeof(known_frame)];
    proto_decoded_t decoded;
    size_t i;
    unsigned int bit;

    for (i = PROTO_DATA_OFS; i < PROTO_CRC_OFS(8U); i++)
    {
        for (bit = 0U; bit < 8U; bit++)
        {
            memcpy(frame, known_frame, sizeof(frame));
            frame[i] ^= (uint8_t)(1U << bit);
            memset(&decoded, 0, sizeof(decoded));
            CHECK(proto_decode(frame, sizeof(frame), &decoded) == PROTO_E_CRC,
                  "single data bit flip detected by CRC");
        }
    }
}

static void test_status_exclusivity(void)
{
    uint8_t frame[sizeof(known_frame)];
    proto_decoded_t decoded;
    proto_span_t span;

    memcpy(frame, known_frame, sizeof(frame));

    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_decode(frame, sizeof(frame) - 1U, &decoded) == PROTO_E_LENGTH,
          "decode truncated frame returns length error");

    memset(&span, 0, sizeof(span));
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_find(frame, sizeof(frame) - 1U, &span, &decoded) ==
              PROTO_E_INCOMPLETE,
          "find truncated frame returns incomplete");
}

static void test_find_skips_crc_failure(void)
{
    proto_frame_t bad;
    proto_frame_t good;
    proto_encoded_t bad_encoded;
    proto_encoded_t good_encoded;
    uint8_t stream[PROTO_BUF_MAX * 2];
    proto_decoded_t decoded;
    proto_span_t span;

    memset(&bad, 0, sizeof(bad));
    memset(&good, 0, sizeof(good));
    memset(&bad_encoded, 0, sizeof(bad_encoded));
    memset(&good_encoded, 0, sizeof(good_encoded));

    bad.type = UINT8_C(0x11);
    bad.data_len = 4U;
    memcpy(bad.data, "BAD!", 4U);
    good.type = UINT8_C(0x22);
    good.data_len = 8U;
    memcpy(good.data, "GOODDATA", 8U);

    CHECK(proto_encode(&bad, &bad_encoded) == PROTO_OK,
          "invalid-CRC source frame encodes");
    CHECK(proto_encode(&good, &good_encoded) == PROTO_OK,
          "valid source frame encodes");

    bad_encoded.bytes[PROTO_CRC_OFS(bad.data_len)] ^= UINT8_C(0xFF);

    memset(stream, 0xA7, sizeof(stream));
    memcpy(stream, bad_encoded.bytes, bad_encoded.size);
    memcpy(stream + bad_encoded.size, good_encoded.bytes, good_encoded.size);

    memset(&span, 0, sizeof(span));
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_find(stream, bad_encoded.size + good_encoded.size,
                     &span, &decoded) == PROTO_OK,
          "find skips frame with invalid CRC");
    CHECK(span.offset == bad_encoded.size,
          "valid frame offset after CRC failure");
    CHECK(span.size == good_encoded.size,
          "valid frame size after CRC failure");
    CHECK(decoded.frame.type == good.type,
          "decoded frame is the valid one");
    CHECK(decoded.status == PROTO_OK, "valid frame sets OK status");
}

static void test_find_first_partial_wins(void)
{
    uint8_t stream[24];
    uint8_t short_stream[9];
    proto_decoded_t decoded;
    proto_span_t span;

    memset(stream, 0x33, sizeof(stream));
    stream[PROTO_HEAD_OFS] = PROTO_HEAD0;
    stream[PROTO_HEAD_OFS + 1U] = PROTO_HEAD1;
    stream[PROTO_HEAD_OFS + 2U] = PROTO_HEAD2;
    stream[PROTO_HEAD_OFS + 3U] = PROTO_HEAD3;
    stream[PROTO_DATALEN_OFS] = UINT8_C(0x00);
    stream[PROTO_DATALEN_OFS + 1U] = UINT8_C(0x28);

    stream[12U] = PROTO_HEAD0;
    stream[13U] = PROTO_HEAD1;
    stream[14U] = PROTO_HEAD2;
    stream[15U] = PROTO_HEAD3;
    stream[20U] = UINT8_C(0x00);
    stream[21U] = UINT8_C(0x1E);

    memset(&span, 0, sizeof(span));
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_find(stream, sizeof(stream), &span, &decoded) ==
              PROTO_E_INCOMPLETE,
          "multiple partial candidates report incomplete");
    CHECK(span.offset == 0U, "first partial candidate wins");
    CHECK(span.size == PROTO_OVERHEAD + (size_t)40,
          "first partial size preserved");
    CHECK(decoded.status == PROTO_E_INCOMPLETE,
          "multiple partials set incomplete status");

    memset(short_stream, 0x33, sizeof(short_stream));
    short_stream[0U] = PROTO_HEAD0;
    short_stream[1U] = PROTO_HEAD1;
    short_stream[2U] = PROTO_HEAD2;
    short_stream[3U] = PROTO_HEAD3;
    short_stream[4U] = PROTO_HEAD0;
    short_stream[5U] = PROTO_HEAD1;
    short_stream[6U] = PROTO_HEAD2;
    short_stream[7U] = PROTO_HEAD3;

    memset(&span, 0, sizeof(span));
    memset(&decoded, 0, sizeof(decoded));
    CHECK(proto_find(short_stream, sizeof(short_stream), &span, &decoded) ==
              PROTO_E_INCOMPLETE,
          "multiple header-only partials report incomplete");
    CHECK(span.offset == 0U, "first header-only partial wins");
    CHECK(span.size == 0U, "header-only partial size unknown");
}

int main(void)
{
    test_known_vector();
    test_roundtrip();
    test_null_arguments();
    test_undersized_frames();
    test_oversized_data();
    test_crc_error();
    test_length_mismatch();
    test_magic_errors();
    test_in_place_decode();
    test_failure_preserves_output();
    test_boundaries();
    test_encode_to();
    test_find();
    test_find_noframe_preserves_span();
    test_find_bounded_null_arguments();
    test_find_partial_and_status();
    test_find_bounded();
    test_status_str();
    test_find_bounded_extremes();
    test_find_illegal_length_candidate();
    test_find_skips_crc_failure();
    test_find_first_partial_wins();
    test_crc_bit_flips();
    test_status_exclusivity();

    TEST_RESULT("protocol");
    return test_finish();
}
