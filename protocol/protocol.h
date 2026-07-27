/**
 * Copyright (c) 2022-2026, HelloAlpha
 *
 * Change Logs:
 * Date           Author       Notes
 */
#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define MSG_FRAME_HEAD0 0xED /* Frame Header */
#define MSG_FRAME_HEAD1 0xB9 /* Frame Header */
#define MSG_FRAME_HEAD2 0x55 /* Frame Header */
#define MSG_FRAME_HEAD3 0xAA /* Frame Header */
#define MSG_FRAME_TAil0 0x5A /* Frame End */
#define MSG_FRAME_TAil1 0xA5 /* Frame End */

#define MSG_DATA_MAX_LEN 64
#define MSG_BUF_MAX_LEN 96

/* Error type Definition */
typedef enum
{
    MSG_OK = 0,          /* Normal */
    MSG_NULL = -1,       /* Data packet empty */
    MSG_FORMAT_ERR = -2, /* Frame format error */
    MSG_CRC_ERR = -3,    /* CRC check error */
    MSG_TIMEOUT = -4,    /* Communication timeout */
} MSG_RESULT;

/* Frame format definition */
typedef struct msg_frame
{
    uint32_t head;                  /* Frame header */
    uint8_t type;                   /* Device  ID */
    uint8_t cmd;                    /* Order code */
    uint16_t code;                  /* Function code */
    uint16_t datalen;               /* Data length */
    uint8_t data[MSG_DATA_MAX_LEN]; /* Data storage area */
    uint16_t chkval;                /* Check value */
    uint16_t tail;                  /* Frame end */
} msg_frame_t;

typedef struct msg_pkg
{
    msg_frame_t pkg;
    int8_t pkg_state;
} msg_pkg_t;

typedef struct msg_buf
{
    uint8_t buf[MSG_BUF_MAX_LEN];
    uint16_t buf_size;
    int8_t buf_state;
} msg_buf_t;

int unpkg_frame(const uint8_t *msg_buf, const uint8_t size, msg_pkg_t *msg_pkg);
int pkg_frame(const msg_frame_t *msg_frame, msg_buf_t *msg_buf);

#endif
