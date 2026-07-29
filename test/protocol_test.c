/**
 * Copyright (c) 2022-2026, HelloAlpha
 *
 * Change Logs:
 * Date           Author       Notes
 */

/**
 * Environment: Visual Studio 2022 IDE
 */

#include "protocol.h"
#include <stdio.h>
#include <string.h>

#define kprintf printf

void msg_frame_print(msg_frame_t *msg_frame)
{
    kprintf("type   (HEX):  %02X \r\n", msg_frame->type);
    kprintf("cmd    (HEX):  %02X \r\n", msg_frame->cmd);
    kprintf("code   (HEX):  %02X %02X \r\n",
            ((msg_frame->code & 0xff00) >> 8), (msg_frame->code & 0xff));
    kprintf("datalen(DEC):  %d \r\n", msg_frame->datalen);
    if (msg_frame->datalen)
    {
        kprintf("data:  (HEX):  ");
        for (uint8_t i = 0; i < msg_frame->datalen; i++)
        {
            kprintf("%02X ", msg_frame->data[i]);
        }
        kprintf("\r\n");
    }
}

void msg_pkg_print(char *name, msg_pkg_t *msg_pkg)
{
    if (name != NULL)
    {
        kprintf("\r\n%s\r\n", name);
    }
    msg_frame_print(&msg_pkg->pkg);
    kprintf("state: (DEC):  %d \r\n", msg_pkg->pkg_state);
}

void msg_buf_print(char *name, msg_buf_t *msg_buf)
{
    if (name != NULL)
    {
        kprintf("\r\n%s\r\n", name);
    }
    kprintf("size   (DEC):  %d \r\n", msg_buf->buf_size);
    kprintf("data   (HEX):  ");
    for (uint8_t i = 0; i < msg_buf->buf_size; i++)
    {
        kprintf("%02X ", msg_buf->buf[i]);
    }
    kprintf("\r\n");
    kprintf("state  (DEC):  %d \r\n", msg_buf->buf_state);
}

void pkg_test_cmd(void)
{
    kprintf("\r\n----- Packing and unpacking test begin -----\r\n");

    msg_frame_t my_frame = {0x00,
                            0x01,
                            0x02,
                            0xabcd,
                            0x0008,
                            {0x12, 0x34, 0x56, 0x78, 0xab, 0xcd, 0xef, 0x5a},
                            0x00,
                            0x00};

    uint8_t my_buf[] = {0xed, 0xb9, 0x55, 0xaa,
                        0x01,
                        0x02,
                        0xab, 0xcd,
                        0x00, 0x08,
                        0x12, 0x34, 0x56, 0x78, 0xab, 0xcd, 0xef, 0x5a,
                        0x14, 0x47,
                        0x5a, 0xa5};

    msg_buf_t msg_buf = {0};
    msg_pkg_t msg_pkg = {0};

    int res = 0;

    res = pkg_frame(&my_frame, &msg_buf);
    kprintf("\r\nPacking test %s!\r\n", res == 0 ? "Succeed" : "Failed");
    msg_buf_print("Buffer:", &msg_buf);

    res = unpkg_frame(my_buf, sizeof(my_buf), &msg_pkg);
    kprintf("\r\nUnpacking test %s!\r\n", res == 0 ? "Succeed" : "Failed");
    msg_pkg_print("Frame:", &msg_pkg);

    kprintf("\r\n---- Packing and unpacking test end ---- \r\n");
}

int main(void)
{
    pkg_test_cmd();

    system("Pause");

    return 0;
}
