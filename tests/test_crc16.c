/* Copyright (c) 2022-2026, HelloAlpha */
#include "test_support.h"

#include "crc16.h"

static const uint8_t vector[18] = {
    UINT8_C(0xED), UINT8_C(0xB9), UINT8_C(0x55), UINT8_C(0xAA),
    UINT8_C(0x01), UINT8_C(0x02), UINT8_C(0xAB), UINT8_C(0xCD),
    UINT8_C(0x00), UINT8_C(0x08), UINT8_C(0x12), UINT8_C(0x34),
    UINT8_C(0x56), UINT8_C(0x78), UINT8_C(0xAB), UINT8_C(0xCD),
    UINT8_C(0xEF), UINT8_C(0x5A),
};

int main(void)
{
    uint16_t crc;

    crc = proto_crc16_modbus(vector, sizeof(vector));
    CHECK(crc == UINT16_C(0x1447), "CRC16/Modbus known vector matches 0x1447");

    crc = proto_crc16_modbus(vector, 0U);
    CHECK(crc == UINT16_C(0xFFFF), "CRC16/Modbus empty input returns seed");

    crc = proto_crc16_modbus(NULL, 0U);
    CHECK(crc == UINT16_C(0xFFFF), "CRC16/Modbus NULL with zero size returns seed");

    {
        const uint8_t empty_frame[10] = {
            UINT8_C(0xED), UINT8_C(0xB9), UINT8_C(0x55), UINT8_C(0xAA),
            UINT8_C(0x00), UINT8_C(0x00), UINT8_C(0x00), UINT8_C(0x00),
            UINT8_C(0x00), UINT8_C(0x00),
        };

        crc = proto_crc16_modbus(empty_frame, sizeof(empty_frame));
        CHECK(crc == UINT16_C(0x3575), "CRC16/Modbus empty-frame vector matches spec");
    }

    crc = proto_crc16_modbus(vector, 4U);
    CHECK(crc != UINT16_C(0x1447), "CRC16/Modbus prefix differs from full vector");

    TEST_RESULT("crc16");
    return test_finish();
}
