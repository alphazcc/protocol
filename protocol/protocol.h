/**
 * Copyright (c) 2022-2023，HelloAlpha
 * 
 * Change Logs:
 * Date           Author       Notes
 */
#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "stdint.h"

#ifndef NULL
#define NULL 	((void *)0)
#endif

/* 帧头、帧尾定义 */
#define MSG_FRAME_HEAD0     0xED  /* 帧头 */
#define MSG_FRAME_HEAD1     0xB9  /* 帧头 */
#define MSG_FRAME_HEAD2     0x55  /* 帧头 */
#define MSG_FRAME_HEAD3     0xAA  /* 帧头 */
#define MSG_FRAME_TAil0     0x5A  /* 帧尾 */
#define MSG_FRAME_TAil1     0xA5  /* 帧尾 */

#define MSG_FRAME_MAX_LEN   64
#define MSG_BUF_MAX_LEN     128

/* 通信错误类型定义 */
typedef enum
{
    MSG_OK = 0,                     /* 正常 */
    MSG_FRAME_FORMAT_ERR = 1,       /* 帧格式错误 */
    MSG_FRAME_CHECK_ERR = 2,        /* 校验值错误 */
    MSG_TIMEOUT = 3,                /* 通信超时 */
    MSG_PKG_NULL = 4,               /* 数据包空 */
} MSG_RESULT;

/* 帧格式定义 */
struct msg_frame_
{
//  uint32_t head;                      /* 帧头 */
    uint8_t type;                       /* 设备 ID */
    uint8_t cmd;                        /* 指令码 */
    uint16_t code;                      /* 功能码 */
    uint16_t datalen;                   /* 数据长度 */
    uint8_t data[MSG_FRAME_MAX_LEN];    /* 数据存储区 */
//  uint16_t chkval;                    /* 校验值 */
//  uint16_t tail;                      /* 帧尾 */
} ;

typedef struct msg_frame_ msg_frame_t;

struct msg_pkg_
{
    struct msg_frame_ frame;
    struct msg_frame_ *pkg;
    int8_t pkg_state; 
} ;

typedef struct msg_pkg_ msg_pkg_t;

struct msg_buf_
{
    uint8_t msg_buf[MSG_BUF_MAX_LEN];   /* 发送缓冲区 */
    uint8_t *buf_ptr;
    uint16_t buf_size;
    int8_t buf_state; 
} ;

typedef struct msg_buf_ msg_buf_t;

msg_pkg_t msg_pkg;
msg_buf_t msg_buf;

msg_pkg_t *unpkg_frame(const uint8_t* msg_buf, const uint8_t size);
msg_buf_t *pkg_frame(const msg_frame_t *msg_pkg);

#endif
