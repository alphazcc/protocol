# Protocol

一个简洁实用的基于字节的自定义通信协议。

## 协议帧定义

```c
/* Frame format definition */
typedef struct msg_frame
{
    uint32_t head;                   /* Frame header */
    uint8_t type;                    /* Device  ID */
    uint8_t cmd;                     /* Order code */
    uint16_t code;                   /* Function code */
    uint16_t datalen;                /* Data length */
    uint8_t data[MSG_FRAME_MAX_LEN]; /* Data storage area */
    uint16_t chkval;                 /* Check value */
    uint16_t tail;                   /* Frame end */
} msg_frame_t;
```

## 帧格式说明

|    说明    | 帧头 | 设备 ID | 指令码 | 功能码 | 数据长度 | 数据存储区 | 校验值 | 帧尾 |
| :--------: | :--: | :-----: | :----: | :----: | :------: | :--------: | :----: | :--: |
|   变量名   | head |  type   |  cmd   |  code  | datalen  |    data    | chkval | tail |
| 长度(Byte) |  4   |    1    |   1    |   2    |    2     |     n      |   2    |  2   |

数据存储区最大长度，单位为`字节`。

```c
#define MSG_FRAME_MAX_LEN   64
```

可处理有效数据最大长度，单位为`字节`。

```c
#define MSG_BUF_MAX_LEN     96
```

帧头：有效数据的开始。

```c
#define MSG_FRAME_HEAD0     0xED
#define MSG_FRAME_HEAD1     0xB9
#define MSG_FRAME_HEAD2     0x55
#define MSG_FRAME_HEAD3     0xAA
```

设备 ID：设备种类、地址或者设备类型

```c
uint8_t type;
```

指令码：对设备的造作命令，如 open、close、read、write 等。

```c
uint8_t cmd;
```

功能码：对指令码的进一步说明。

```c
uint16_t code;
```

数据长度：数据存储区中`数据`的长度，单位为`字节`。

```c
uint16_t datalen;
```

数据存储区：用来存储要传输的数据。

```c
uint8_t data[MSG_FRAME_MAX_LEN];
```

校验值：基于 CRC 校验(可自定义)。

```c
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
```

帧尾：有效数据的结束。

```c
#define MSG_FRAME_TAil0     0x5A
#define MSG_FRAME_TAil1     0xA5
```

## API 说明

数据解包

```c
/* Packet decoding */
msg_pkg_t *unpkg_frame(const uint8_t *_msg_buf, const uint8_t size)
```

数据打包

```c
/* Packet packaging */
msg_buf_t *pkg_frame(const msg_frame_t *_msg_pkg)
```

## 测试验证

测试环境：Visual Studio 2022 IDE

测试代码：

```c
void pkg_test_cmd(void)
{
    kprintf("\r\n----- Packing and unpacking test begin -----\r\n");

    msg_frame_t my_frame = { 0x01, 0x02, 0xabcd, 0x8,
            {0x12, 0x34, 0x56,0x78, 0xab, 0xcd, 0xef, 0x5a} };

    uint8_t my_buf[] = { 0xed, 0xb9, 0x55, 0xaa, 0x0a, 0x2c, 0x80, 0x00,
            0x00, 0x04, 0x12, 0x34, 0xab, 0xcd, 0xe5, 0x50, 0x5a, 0xa5 };

    msg_buf_t* msg_buf = pkg_frame(&my_frame);
    msg_buf_print("frame to buffer test", msg_buf);

    msg_pkg_t* msg_pkg = unpkg_frame(msg_buf->buf_ptr, msg_buf->buf_size);
    msg_pkg_print("buffer to frame test", msg_pkg);

    msg_pkg = unpkg_frame(my_buf, sizeof(my_buf) / sizeof(uint8_t));
    msg_pkg_print("buffer to frame test", msg_pkg);

    kprintf("\r\n---- Packing and unpacking test end ---- \r\n");
}
```

运行结果：

```
----- Packing and unpacking test begin -----

frame to buffer test
size   (DEC):  22
data   (HEX):  ed b9 55 aa 01 02 ab cd 00 08 12 34 56 78 ab cd ef 5a 47 14 5a a5
state  (DEC):  0

buffer to frame test
type   (HEX):  01
cmd    (HEX):  02
code   (HEX):  ab cd
datalen(DEC):  8
data:  (HEX):  12 34 56 78 ab cd ef 5a
state: (DEC):  0

buffer to frame test
type   (HEX):  0a
cmd    (HEX):  2c
code   (HEX):  80 00
datalen(DEC):  4
data:  (HEX):  12 34 ab cd
state: (DEC):  0

---- Packing and unpacking test end ----
请按任意键继续. . .
```
