/**
 * Copyright (c) 2022-2026, HelloAlpha
 *
 * Change Logs:
 * Date           Author       Notes
 */
#include "protocol.h"

/* CRC16 verification */
static uint16_t mc_check_crc16(const uint8_t *data, uint8_t len)
{
    uint16_t crc16 = 0xffff;
    uint8_t state, i, j;
    for (i = 0; i < len; i++)
    {
        crc16 ^= data[i];
        for (j = 0; j < 8; j++)
        {
            state = crc16 & 0x01;
            crc16 >>= 1;
            if (state)
            {
                crc16 ^= 0xa001;
            }
        }
    }
    return crc16;
}

/* Packet decoding */
int unpkg_frame(const uint8_t *msg_buf, const uint8_t size, msg_pkg_t *msg_pkg)
{
    uint8_t cnt = 0;
    uint16_t rxchkval = 0;  /* Received verification code */
    uint16_t calchkval = 0; /* Calculated check code */

    if (NULL == msg_buf)
    {
        msg_pkg->pkg_state = MSG_NULL;
        return MSG_NULL;
    }

    if (msg_buf[cnt++] != MSG_FRAME_HEAD0)
    {
        msg_pkg->pkg_state = MSG_FORMAT_ERR;
        return MSG_FORMAT_ERR;
    }
    if (msg_buf[cnt++] != MSG_FRAME_HEAD1)
    {
        msg_pkg->pkg_state = MSG_FORMAT_ERR;
        return MSG_FORMAT_ERR;
    }
    if (msg_buf[cnt++] != MSG_FRAME_HEAD2)
    {
        msg_pkg->pkg_state = MSG_FORMAT_ERR;
        return MSG_FORMAT_ERR;
    }
    if (msg_buf[cnt++] != MSG_FRAME_HEAD3)
    {
        msg_pkg->pkg_state = MSG_FORMAT_ERR;
        return MSG_FORMAT_ERR;
    }

    if (msg_buf[size - 2] != MSG_FRAME_TAil0)
    {
        msg_pkg->pkg_state = MSG_FORMAT_ERR;
        return MSG_FORMAT_ERR;
    }
    if (msg_buf[size - 1] != MSG_FRAME_TAil1)
    {
        msg_pkg->pkg_state = MSG_FORMAT_ERR;
        return MSG_FORMAT_ERR;
    }

    /* Calculate the check code */
    calchkval = mc_check_crc16(msg_buf, size - 4);

    /* Received verification code */
    rxchkval = ((uint16_t)msg_buf[size - 4] << 8) + msg_buf[size - 3];

    if (calchkval == rxchkval)
    {
        /* Parse data into structs */
        msg_pkg->pkg.type = msg_buf[cnt++];
        msg_pkg->pkg.cmd = msg_buf[cnt++];
        msg_pkg->pkg.code = (uint16_t)msg_buf[cnt++] << 8;
        msg_pkg->pkg.code += (uint16_t)msg_buf[cnt++];
        msg_pkg->pkg.datalen = (uint16_t)msg_buf[cnt++] << 8;
        msg_pkg->pkg.datalen += (uint16_t)msg_buf[cnt++];

        if (msg_pkg->pkg.datalen)
        {
            for (uint8_t i = 0; i < msg_pkg->pkg.datalen; i++)
            {
                msg_pkg->pkg.data[i] = msg_buf[cnt++];
            }
        }
        msg_pkg->pkg_state = MSG_OK;
        return MSG_OK;
    }
    else
    {
        msg_pkg->pkg_state = MSG_FORMAT_ERR;
        return MSG_FORMAT_ERR;
    }

    return MSG_OK;
}

/* Packet packaging */
int pkg_frame(const msg_frame_t *msg_frame, msg_buf_t *msg_buf)
{
    uint8_t cnt = 0;

    if (NULL == msg_frame)
    {
        msg_buf->buf_state = MSG_NULL;
        return MSG_NULL;
    }

    /* Add frame header */
    msg_buf->buf[cnt++] = MSG_FRAME_HEAD0;
    msg_buf->buf[cnt++] = MSG_FRAME_HEAD1;
    msg_buf->buf[cnt++] = MSG_FRAME_HEAD2;
    msg_buf->buf[cnt++] = MSG_FRAME_HEAD3;

    /* Writes instructions to the store */
    msg_buf->buf[cnt++] = msg_frame->type;
    msg_buf->buf[cnt++] = msg_frame->cmd;
    msg_buf->buf[cnt++] = (msg_frame->code & 0xff00) >> 8;
    msg_buf->buf[cnt++] = msg_frame->code & 0xff;

    /* Writes data to the store */
    msg_buf->buf[cnt++] = (msg_frame->datalen & 0xff00) >> 8;
    msg_buf->buf[cnt++] = msg_frame->datalen & 0xff;

    if (msg_frame->datalen)
    {
        for (uint8_t i = 0; i < msg_frame->datalen; i++)
        {
            msg_buf->buf[cnt++] = msg_frame->data[i];
        }
    }

    /* Calculate the check code */
    uint16_t calchkval = mc_check_crc16(msg_buf->buf, cnt);

    /* Add the check code */
    msg_buf->buf[cnt++] = calchkval >> 8;
    msg_buf->buf[cnt++] = calchkval & 0xff;

    /* Add frame end */
    msg_buf->buf[cnt++] = MSG_FRAME_TAil0;
    msg_buf->buf[cnt++] = MSG_FRAME_TAil1;

    msg_buf->buf_size = cnt;
    msg_buf->buf_state = MSG_OK;

    return MSG_OK;
}
