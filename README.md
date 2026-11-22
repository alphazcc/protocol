# Proto

`proto` 是一个面向字节流通信的 C11 协议库，提供固定线格式的帧编解码与分帧能力。库版本为 **1.0.0**（由 `CMakeLists.txt` 定义）。

## 1. 设计目标

- 固定线格式：帧长 14–110 字节，有效载荷 0–96 字节。
- 确定性校验：CRC16/Modbus，多字节字段采用大端序。
- 精确解码：`proto_decode()` 要求输入恰好为一帧；`proto_find()` 系列负责从连续流中定位帧。
- 资源受限友好：无动态内存分配、无浮点运算、无操作系统依赖、无全局可变状态，函数可重入。
- 可移植：线格式与主机字节序、字长和对齐方式无关；公共头文件兼容 C++11 及以上。
- 可验证：提供功能、CRC 向量与差分、边界、属性和确定性模糊测试，并由 CI 覆盖多种工具链与编译配置。

## 2. 非目标

本库不处理以下事项，调用方需在更上层实现：

- 传输介质、链路建立与关闭；
- 重传、确认、超时与会话管理；
- 身份认证、加密与防重放；
- 超出本协议线格式的其他帧结构。

## 3. 快速开始

以下示例中的 `send()`、`read_more()` 与 `handle()` 由应用层实现。

### 3.1 编码到内置缓冲区

```c
#include <proto/protocol.h>

proto_frame_t frame = {0};
proto_encoded_t out = {0};

frame.type = 0x07;
frame.cmd = 0x13;
frame.code = 0x1234;
frame.data_len = 4;
frame.data[0] = 0xDE;
frame.data[1] = 0xAD;
frame.data[2] = 0xBE;
frame.data[3] = 0xEF;

if (proto_encode(&frame, &out) == PROTO_OK)
{
    send(out.bytes, out.size);
}
```

`proto_encode_to()` 可写入调用方提供的缓冲区；仅当容量足够时才写入，且要求 `frame` 存储与 `dst` 不重叠、`out_size` 不指向 `dst` 存储。状态仅通过返回值传递，成功时写 `*out_size`。

### 3.2 解码单帧

```c
proto_decoded_t decoded = {0};

if (proto_decode(rx_bytes, rx_size, &decoded) == PROTO_OK)
{
    handle(decoded.frame.type, decoded.frame.cmd,
           decoded.frame.code, decoded.frame.data, decoded.frame.data_len);
}
```

`rx_size` 必须等于 `14 + decoded.frame.data_len`。`proto_decode()` 支持原地解码：输入缓冲区与输出结构存储重叠时，库先完成全部输入读取，再写入输出。

### 3.3 字节流分帧

```c
#include <proto/protocol.h>
#include <string.h>

uint8_t stream[512];
size_t used = 0;
proto_span_t span;
proto_decoded_t decoded;

for (;;)
{
    proto_status_t s = proto_find(stream, used, &span, &decoded);

    if (s == PROTO_E_INCOMPLETE)
    {
        if (used == sizeof(stream))
        {
            if (span.offset > 0U)
            {
                memmove(stream, stream + span.offset, used - span.offset);
                used -= span.offset;
            }
            else
            {
                used = 0U; /* 缓冲区满：按超时策略丢弃当前内容 */
            }
        }
        size_t n = read_more(stream + used, sizeof(stream) - used);
        if (n == 0U)
        {
            break;
        }
        used += n;
        continue;
    }

    if (s == PROTO_E_NOFRAME)
    {
        if (used == 0U)
        {
            size_t n = read_more(stream, sizeof(stream));
            if (n == 0U)
            {
                break;
            }
            used = n;
        }
        else
        {
            memmove(stream, stream + 1, used - 1); /* 丢弃 1 字节噪声 */
            used--;
        }
        continue;
    }

    if (s != PROTO_OK)
    {
        break;
    }

    handle(decoded.frame.type, decoded.frame.cmd,
           decoded.frame.code, decoded.frame.data, decoded.frame.data_len);

    memmove(stream, stream + span.offset + span.size,
            used - span.offset - span.size);
    used -= span.offset + span.size;
}
```

`span.offset + span.size` 是已消费字节数。持续未收全时调用方必须设置上层超时；实时场景应使用 `proto_find_bounded()` 限制候选帧头搜索范围。

## 4. 构建与测试

要求 CMake 3.10 或更高版本。

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

启用 AddressSanitizer 与 UndefinedBehaviorSanitizer：

```bash
cmake -S . -B build-asan -DPROTO_ENABLE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

测试套件包含六组：核心编解码、CRC 向量、CRC 差分、边界哨兵、属性与确定性模糊。CI 覆盖默认/查表/`memchr`/双开四种算法配置，以及 GCC、MSVC、MinGW、sanitizer、静态分析、覆盖率、导出符号与 C++ 兼容性检查。

## 5. 构建选项

| CMake 选项 | 默认值 | 说明 |
| :-- | :--: | :-- |
| `PROTO_BUILD_TESTS` | `ON` | 构建测试套件 |
| `PROTO_ENABLE_SANITIZERS` | `OFF` | 启用 ASan 与 UBSan（非 MSVC） |
| `PROTO_ENABLE_HARDENING` | `ON` | 启用编译器加固选项 |
| `PROTO_CRC_TABLE` | `OFF` | 使用 256 项 CRC 查表实现 |
| `PROTO_FIND_MEMCHR` | `OFF` | 使用 `memchr` 预筛候选帧头 |

加固选项仅在 GCC/Clang、非 MinGW、非 Debug 且未启用 sanitizer 时生效，对应 `-D_FORTIFY_SOURCE=2 -fstack-protector-strong`。

算法选项通过编译定义切换：

| 选项 | 编译定义 | 关闭时行为 |
| :-- | :-- | :-- |
| `PROTO_CRC_TABLE` | `PROTO_CRC_USE_TABLE=1` | CRC 逐位计算 |
| `PROTO_FIND_MEMCHR` | `PROTO_FIND_USE_MEMCHR=1` | 逐字节扫描候选帧头 |

非 CMake 构建可自行定义上述宏；未选中的实现不会进入最终产物。

## 6. 公共 API

| 函数 | 用途 |
| :-- | :-- |
| `proto_decode()` | 解码恰好一帧，支持原地解码 |
| `proto_encode()` | 编码到内置固定缓冲区 |
| `proto_encode_to()` | 编码到调用方缓冲区 |
| `proto_find()` | 在字节流中定位首个通过校验的完整帧 |
| `proto_find_bounded()` | 限定候选帧头搜索范围的帧定位 |
| `proto_status_str()` | 返回状态码的只读文本 |

完整契约见 [API 参考](docs/API_REFERENCE.md)，状态语义见 [状态码](docs/ERROR_CODES.md)。

## 7. 可移植性与兼容性

- 库要求 C11；公共头文件兼容 C++11/C++17，并通过 `extern "C"` 链接声明。
- `proto_status_t` 通过静态断言固定为 4 字节；`-fshort-enums` 构建会被拒绝。
- 不依赖主机字节序、字长或对齐方式；多字节字段由代码显式按大端序读写。
- 无动态内存、无递归、无全局可变状态；栈占用为固定大小的结构体。
- 内部符号不通过公共头暴露；GCC/Clang 非 Windows 构建中其可见性为隐藏，MSVC 与 Windows MinGW 下 `PROTO_INTERNAL` 为空宏，静态库中仍具外部链接。调用方不得依赖内部符号。

## 8. 安全与资源约束

- CRC16 仅用于检测随机传输错误，不具备抗恶意篡改能力；需要认证时应在上层增加消息认证码。
- 库不限制累计接收字节数；调用方必须约束接收缓冲区容量，并设置持续未收全的超时策略。
- `proto_encode()` 与 `proto_encode_to()` 要求输入帧存储与输出缓冲区不重叠；`proto_encode_to()` 另要求 `out_size` 不指向 `dst` 存储。上述条件不由库检测，违反时行为未定义。
- 有界窗口仅限制候选帧头起点，帧体可延伸到窗口之外；窗口应覆盖最坏噪声前缀长度。
- 错误码与处置建议见 [状态码](docs/ERROR_CODES.md)。

## 9. 仓库结构

| 路径 | 职责 |
| :-- | :-- |
| `include/proto/protocol.h` | 公共类型、常量与 API 声明 |
| `include/proto/port/compiler.h` | 编译器与平台宏隔离 |
| `src/protocol.c` | 编解码、分帧与候选帧扫描 |
| `src/crc16.h`、`src/crc16.c` | CRC16/Modbus 内部实现 |
| `tests/` | 六组自动化测试及公共测试辅助 |
| `docs/` | 协议规范、API 参考与状态码文档 |
| `.github/workflows/ci.yml` | 持续集成配置 |
| `LICENSE` | MIT 许可条款 |

## 10. 许可

MIT 许可，版权归 `HelloAlpha` 所有（2022–2026）。详见 [LICENSE](LICENSE)。

## 11. 文档

- [协议规范](docs/PROTOCOL_SPEC.md)
- [API 参考](docs/API_REFERENCE.md)
- [状态码](docs/ERROR_CODES.md)
