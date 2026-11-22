/* Copyright (c) 2022-2026, HelloAlpha */
#include "test_support.h"

#include "crc16.h"

#define DIFF_LEN_MAX     200U
#define REF_CRC16_POLY   UINT16_C(0xA001)
#define REF_CRC16_SEED   UINT16_C(0xFFFF)

static uint16_t ref_crc16_bitwise(const uint8_t *data, size_t size)
{
    uint16_t crc = REF_CRC16_SEED;
    size_t i;

    for (i = 0U; i < size; i++)
    {
        uint8_t bit;
        crc = (uint16_t)(crc ^ (uint16_t)data[i]);
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & UINT16_C(0x0001)) != UINT16_C(0))
            {
                crc = (uint16_t)((crc >> 1) ^ REF_CRC16_POLY);
            }
            else
            {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

static void ref_crc16_build_table(uint16_t *table)
{
    unsigned int i;

    for (i = 0U; i < 256U; i++)
    {
        uint16_t crc = (uint16_t)i;
        unsigned int bit;

        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & UINT16_C(0x0001)) != UINT16_C(0))
            {
                crc = (uint16_t)((crc >> 1) ^ REF_CRC16_POLY);
            }
            else
            {
                crc = (uint16_t)(crc >> 1);
            }
        }
        table[i] = crc;
    }
}

static uint16_t ref_crc16_table(const uint16_t *table,
                                const uint8_t *data, size_t size)
{
    uint16_t crc = REF_CRC16_SEED;
    size_t i;

    for (i = 0U; i < size; i++)
    {
        uint16_t index =
            (uint16_t)((crc ^ (uint16_t)data[i]) & UINT16_C(0x00FF));
        crc = (uint16_t)((crc >> 8) ^ table[index]);
    }
    return crc;
}

int main(void)
{
    uint8_t data[DIFF_LEN_MAX];
    uint16_t table[256];
    size_t n;
    size_t i;

    ref_crc16_build_table(table);

    CHECK(ref_crc16_bitwise(NULL, 0U) == REF_CRC16_SEED,
          "differential: reference bitwise empty input returns seed");
    CHECK(ref_crc16_table(table, NULL, 0U) == REF_CRC16_SEED,
          "differential: reference table empty input returns seed");

    for (n = 0U; n <= DIFF_LEN_MAX; n++)
    {
        for (i = 0U; i < n; i++)
        {
            data[i] = (uint8_t)((i * 37U + n) & 0xFFU);
        }

        CHECK(ref_crc16_bitwise(data, n) == ref_crc16_table(table, data, n),
              "differential: reference bitwise equals reference table");
        CHECK(proto_crc16_modbus(data, n) == ref_crc16_bitwise(data, n),
              "differential: library equals reference bitwise");
        CHECK(proto_crc16_modbus(data, n) == ref_crc16_table(table, data, n),
              "differential: library equals reference table");
    }

    TEST_RESULT("crc16_diff");
    return test_finish();
}
