/* Copyright (c) 2022-2026, HelloAlpha */
#include "test_support.h"

#include "proto/protocol.h"

uint16_t test_crc16(const uint8_t *data, size_t size)
{
    uint16_t crc = UINT16_C(0xFFFF);
    size_t i;

    for (i = 0; i < size; i++)
    {
        uint8_t bit;
        crc = (uint16_t)(crc ^ (uint16_t)data[i]);
        for (bit = 0; bit < 8U; bit++)
        {
            uint8_t lsb = (uint8_t)(crc & UINT16_C(0x0001));
            crc = (uint16_t)(crc >> 1);
            if (lsb != 0U)
            {
                crc = (uint16_t)(crc ^ UINT16_C(0xA001));
            }
        }
    }
    return crc;
}

size_t test_build_frame(uint8_t *dst, uint16_t data_len,
                        const uint8_t *payload, size_t payload_size)
{
    size_t n = 0;
    size_t i;
    uint16_t crc;

    dst[n++] = PROTO_HEAD0;
    dst[n++] = PROTO_HEAD1;
    dst[n++] = PROTO_HEAD2;
    dst[n++] = PROTO_HEAD3;
    dst[n++] = UINT8_C(0x01);
    dst[n++] = UINT8_C(0x02);
    dst[n++] = UINT8_C(0xAB);
    dst[n++] = UINT8_C(0xCD);
    dst[n++] = (uint8_t)(data_len >> 8);
    dst[n++] = (uint8_t)(data_len & UINT16_C(0x00FF));

    for (i = 0; i < payload_size; i++)
    {
        dst[n++] = payload[i];
    }

    crc = test_crc16(dst, n);
    dst[n++] = (uint8_t)(crc >> 8);
    dst[n++] = (uint8_t)(crc & UINT16_C(0x00FF));
    dst[n++] = PROTO_TAIL0;
    dst[n++] = PROTO_TAIL1;
    return n;
}

int g_checks;
int g_failures;

int test_finish(void)
{
    return g_failures == 0 ? 0 : 1;
}
