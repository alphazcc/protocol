/**
 * Copyright (c) 2022-2023，HelloAlpha
 * 
 * Change Logs:
 * Date           Author       Notes
 */
#include "protocol.h"

/* CRC16 校验 */
static uint16_t mc_check_crc16(const uint8_t *data, uint8_t len)
{
    uint16_t crc16 = 0xffff;
    uint8_t state, i, j;
    for(i = 0; i < len; i++ )
    {
        crc16 ^= data[i];
        for( j = 0; j < 8; j++)
        {
            state = crc16 & 0x01;
            crc16 >>= 1;
            if(state)
            {
                crc16 ^= 0xa001;
            }
        }
    }
    return crc16;
}

/* 数据包解码 */
msg_pkg_t *unpkg_frame(const uint8_t* msg_buf, const uint8_t size)
{
    msg_pkg_t *_msg_pkg = &msg_pkg;
    uint8_t cnt = 0;
    uint16_t rxchkval = 0;                  /* 接收到的校验值 */
    uint16_t calchkval = 0;                 /* 计算得到的校验值 */

    if(NULL == msg_buf) {
    	_msg_pkg->pkg_state = MSG_PKG_NULL;
        goto msg_err;
    }

    if(msg_buf[cnt++] != MSG_FRAME_HEAD0) {
    	_msg_pkg->pkg_state = MSG_FRAME_FORMAT_ERR;
    	goto msg_err;
    }
    if(msg_buf[cnt++] != MSG_FRAME_HEAD1) {
    	_msg_pkg->pkg_state = MSG_FRAME_FORMAT_ERR;
    	goto msg_err;
    }
    if(msg_buf[cnt++] != MSG_FRAME_HEAD2) {
    	_msg_pkg->pkg_state = MSG_FRAME_FORMAT_ERR;
    	goto msg_err;
    }
    if(msg_buf[cnt++] != MSG_FRAME_HEAD3) {
    	_msg_pkg->pkg_state = MSG_FRAME_FORMAT_ERR;
    	goto msg_err;
    }

    /* CRC16 校验 */
    calchkval = mc_check_crc16(msg_buf, size - 4);

    /* 接收到的校验位 */
    rxchkval = ((uint16_t)msg_buf[size-3]<<8) + msg_buf[size-4];

    if(calchkval == rxchkval)
    {
        /* 解析数据到结构体中 */
        _msg_pkg->pkg = &_msg_pkg->frame;
        _msg_pkg->pkg->type = msg_buf[cnt++];
        _msg_pkg->pkg->cmd = msg_buf[cnt++];
        _msg_pkg->pkg->code = (uint16_t)msg_buf[cnt++] << 8;
        _msg_pkg->pkg->code += (uint16_t)msg_buf[cnt++];
        _msg_pkg->pkg->datalen = (uint16_t)msg_buf[cnt++] << 8;
        _msg_pkg->pkg->datalen += (uint16_t)msg_buf[cnt++];

        if(_msg_pkg->pkg->datalen)
        {
            for(uint8_t i = 0; i < _msg_pkg->pkg->datalen; i++)
            {
                _msg_pkg->pkg->data[i] = msg_buf[cnt++];
            }
        }
        _msg_pkg->pkg_state = MSG_OK;
    }
    else 
    {
    	_msg_pkg->pkg_state = MSG_FRAME_CHECK_ERR;
    	goto msg_err;
    }
    return _msg_pkg;

msg_err:
    return _msg_pkg;
}

/* 数据包打包 */
msg_buf_t *pkg_frame(const msg_frame_t *_msg_pkg)
{
	msg_buf_t *_msg_buf = &msg_buf;
    uint8_t cnt = 0;

    if(NULL == _msg_pkg)
    {
    	_msg_buf->buf_state = MSG_PKG_NULL;
        goto msg_err;
    }

    /* 添加帧头 */
    _msg_buf->msg_buf[cnt++] = MSG_FRAME_HEAD0;
    _msg_buf->msg_buf[cnt++] = MSG_FRAME_HEAD1;
    _msg_buf->msg_buf[cnt++] = MSG_FRAME_HEAD2;
    _msg_buf->msg_buf[cnt++] = MSG_FRAME_HEAD3;

    /* 写入要发送的指令 */
    _msg_buf->msg_buf[cnt++] = _msg_pkg->type;
    _msg_buf->msg_buf[cnt++] = _msg_pkg->cmd;
    _msg_buf->msg_buf[cnt++] = (_msg_pkg->code & 0xff00) >> 8;
    _msg_buf->msg_buf[cnt++] = _msg_pkg->code & 0xff;

    /* 写入要发送的数据 */
    _msg_buf->msg_buf[cnt++] = (_msg_pkg->datalen & 0xff00) >> 8;
    _msg_buf->msg_buf[cnt++] = _msg_pkg->datalen & 0xff;

    if(_msg_pkg->datalen)
    {
        for(uint8_t i = 0; i < _msg_pkg->datalen; i++)
        {
            _msg_buf->msg_buf[cnt++] = _msg_pkg->data[i];
        }
    }

    /* 计算校验值 */
    uint16_t calchkval = mc_check_crc16(_msg_buf->msg_buf, cnt);
    
    /* 添加校验位 */
    _msg_buf->msg_buf[cnt++] = calchkval & 0xff;
    _msg_buf->msg_buf[cnt++] = calchkval >> 8;

    /* 添加帧尾 */
    _msg_buf->msg_buf[cnt++] = MSG_FRAME_TAil0;
    _msg_buf->msg_buf[cnt++] = MSG_FRAME_TAil1;

    _msg_buf->buf_ptr = _msg_buf->msg_buf;
    _msg_buf->buf_size = cnt;
    _msg_buf->buf_state = MSG_OK;

    return _msg_buf;

msg_err:
    return _msg_buf;
}
