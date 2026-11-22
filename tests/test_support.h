/* Copyright (c) 2022-2026, HelloAlpha */
#ifndef PROTO_TEST_SUPPORT_H
#define PROTO_TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern int g_checks;
extern int g_failures;

#define CHECK(expr, desc)                                                  \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(expr)) {                                                     \
            g_failures++;                                                  \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, (desc));      \
        }                                                                  \
    } while (0)

#define TEST_RESULT(name)                                                  \
    do {                                                                   \
        printf("=== %s: %d checks, %d failure(s) ===\n",                   \
               (name), g_checks, g_failures);                              \
    } while (0)

uint16_t test_crc16(const uint8_t *data, size_t size);
size_t test_build_frame(uint8_t *dst, uint16_t data_len,
                        const uint8_t *payload, size_t payload_size);

int test_finish(void);

#endif
