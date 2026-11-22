/* Copyright (c) 2022-2026, HelloAlpha */
#ifndef PROTO_CRC16_H
#define PROTO_CRC16_H

#include "proto/port/compiler.h"

#include <stddef.h>
#include <stdint.h>

PROTO_INTERNAL uint16_t proto_crc16_modbus(const uint8_t *data, size_t size);

#endif
