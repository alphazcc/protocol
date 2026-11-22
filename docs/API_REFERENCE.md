# API 参考

公共接口声明位于 `<proto/protocol.h>`。本文描述每个接口的前置条件、后置条件、返回值与副作用。内部模块见第 7 节，不属于公共 API。

## 1. 通用约定

- 除 `proto_status_str()` 返回 `const char *` 外，公共函数均返回 `proto_status_t`；内部 CRC 函数返回 `uint16_t`。
- `PROTO_OK` 表示成功，负值表示失败。状态码数值与文本见 [状态码](ERROR_CODES.md)。
- 所有指针参数必须非 NULL，除非接口明确允许。传入 NULL 时返回 `PROTO_E_NULL`；对于带 `out` 参数的接口，若 `out` 非 NULL，则同时写入 `out->status`。
- 对于带 `out` 参数的接口，失败时只写输出对象的 `status` 字段，不修改数据字段。唯一例外是 `proto_find()` 与 `proto_find_bounded()` 在 `PROTO_E_INCOMPLETE` 时按第 4.4、4.5 节写入 `span`。`proto_encode_to()` 不带 `out` 参数，状态仅通过返回值传递，失败时不写 `dst` 与 `*out_size`。
- 库不分配或释放内存，不返回指向内部缓冲区的指针；所有输出均为调用方存储中的值拷贝。
- 库不使用 `errno`，不依赖第三方库，不持有全局可变状态；函数可重入。
- 状态码文本由 `proto_status_str()` 提供，调用方 MUST NOT 依赖文本进行逻辑判断。
- 公共头通过 `extern "C"` 声明链接符号，兼容 C++11 及以上。
- `proto_decode()` 支持输入与输出存储重叠；`proto_encode()` 与 `proto_encode_to()` 要求 `frame` 存储与输出缓冲区不重叠；`proto_encode_to()` 另要求 `out_size` 不指向 `dst` 存储。库不检测上述重叠条件，重叠行为未定义。

## 2. 常量与类型

### 2.1 公共常量

| 常量 | 值 | 含义 |
| :-- | :--: | :-- |
| `PROTO_HEAD_LEN` | 4 | 帧头字节数 |
| `PROTO_OVERHEAD` | 14 | 除数据区外的固定开销 |
| `PROTO_MIN_LEN` | 14 | 最小帧长 |
| `PROTO_DATA_MAX` | 96 | 最大数据长度 |
| `PROTO_BUF_MAX` | 110 | 编码缓冲区容量 |

### 2.2 `proto_status_t`

枚举类型，取值范围见 [状态码](ERROR_CODES.md)。公共头通过静态断言将其固定为 4 字节；使用 `-fshort-enums` 的构建会被拒绝。

### 2.3 `proto_frame_t`

应用层的单帧语义表示。

| 字段 | 类型 | 约束 |
| :-- | :-- | :-- |
| `type` | `uint8_t` | 无 |
| `cmd` | `uint8_t` | 无 |
| `code` | `uint16_t` | 无 |
| `data_len` | `uint16_t` | 编码时必须 `<= PROTO_DATA_MAX` |
| `data` | `uint8_t[PROTO_DATA_MAX]` | 仅前 `data_len` 字节有效 |

### 2.4 `proto_decoded_t`

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `frame` | `proto_frame_t` | 成功时有效 |
| `status` | `proto_status_t` | 本次操作的返回状态 |

### 2.5 `proto_encoded_t`

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `bytes` | `uint8_t[PROTO_BUF_MAX]` | 成功时前 `size` 字节为完整帧 |
| `size` | `size_t` | 成功时写入，失败时保持不变 |
| `status` | `proto_status_t` | 本次操作的返回状态 |

### 2.6 `proto_span_t`

| 字段 | 类型 | 说明 |
| :-- | :-- | :-- |
| `offset` | `size_t` | 候选帧首字节在流中的下标 |
| `size` | `size_t` | 帧总字节数；长度字段尚不可读时为 0 |

## 3. 接口一览

| 接口 | 用途 |
| :-- | :-- |
| `proto_decode()` | 解码恰好一帧 |
| `proto_encode()` | 编码到内置固定缓冲区 |
| `proto_encode_to()` | 编码到调用方缓冲区 |
| `proto_find()` | 在字节流中定位首个通过校验的帧 |
| `proto_find_bounded()` | 限定候选帧头搜索范围 |
| `proto_status_str()` | 返回状态码的只读文本 |

## 4. 函数契约

### 4.1 `proto_decode`

```c
proto_status_t proto_decode(const uint8_t *bytes, size_t size,
                            proto_decoded_t *out);
```

- **目的**：解码恰好一帧，并校验帧头、长度、帧尾与 CRC。
- **前置条件**：`bytes`、`out` 非 NULL；`size == 14 + 数据长度字段值`。输入与 `out` 存储可重叠。
- **校验顺序**：NULL 检查 → 最小长度 → 帧头 → 数据长度上界 → 长度一致性 → 帧尾 → CRC。
- **成功后置条件**：`out->frame` 被写入完整帧字段，`out->status == PROTO_OK`。
- **失败后置条件**：仅 `out->status` 被修改，`out->frame` 保持不变。
- **返回值**：`PROTO_OK`、`PROTO_E_NULL`、`PROTO_E_LENGTH`、`PROTO_E_HEAD`、`PROTO_E_DATASIZE`、`PROTO_E_TAIL`、`PROTO_E_CRC`。

线格式要求见 [协议规范](PROTOCOL_SPEC.md)。

### 4.2 `proto_encode`

```c
proto_status_t proto_encode(const proto_frame_t *frame,
                            proto_encoded_t *out);
```

- **目的**：编码到 `out->bytes` 内置缓冲区。
- **前置条件**：`frame`、`out` 非 NULL；`frame->data_len <= PROTO_DATA_MAX`；`frame` 存储与 `out->bytes` 不重叠。
- **成功后置条件**：`out->bytes[0..out->size)` 为完整帧，`out->size` 为写入字节数，`out->status == PROTO_OK`。
- **失败后置条件**：仅 `out->status` 被修改，`out->size` 保持不变。
- **返回值**：`PROTO_OK`、`PROTO_E_NULL`、`PROTO_E_DATASIZE`、`PROTO_E_NOSPACE`。最后一项在固定容量下正常不可达，保留以与 `proto_encode_to()` 保持一致。

### 4.3 `proto_encode_to`

```c
proto_status_t proto_encode_to(const proto_frame_t *frame,
                               uint8_t *dst, size_t capacity,
                               size_t *out_size);
```

- **目的**：编码到调用方缓冲区。
- **前置条件**：`frame`、`dst`、`out_size` 非 NULL；`frame->data_len <= PROTO_DATA_MAX`；`frame` 存储与 `dst` 不重叠；`out_size` MUST NOT 指向 `dst` 存储。
- **容量规则**：所需容量为 `14 + frame->data_len`。超过 `capacity` 时返回 `PROTO_E_NOSPACE`，且不写 `dst` 与 `*out_size`。
- **成功后置条件**：`dst[0..*out_size)` 为完整帧，`*out_size` 为写入字节数。
- **状态传递**：本接口不带 `out` 参数，状态仅通过返回值传递；失败时不写 `dst` 与 `*out_size`，成功时写 `*out_size`。库不检测存储重叠，违反上述重叠约束的行为未定义。
- **返回值**：`PROTO_OK`、`PROTO_E_NULL`、`PROTO_E_DATASIZE`、`PROTO_E_NOSPACE`。

### 4.4 `proto_find`

```c
proto_status_t proto_find(const uint8_t *stream, size_t size,
                          proto_span_t *span, proto_decoded_t *out);
```

- **目的**：在字节流中定位下标最小的、通过全部校验的完整帧。
- **前置条件**：`stream`、`span`、`out` 非 NULL；`size` 允许为 0。
- **扫描规则**：候选起点下标为 `[0, size - 4]`；校验顺序为帧头、长度字段可读性、数据长度上界、可用字节数、帧尾与 CRC。
- **`PROTO_OK`**：`span` 写入帧起点与总长度，`out` 写入解码结果；`span->offset + span->size` 为已消费字节数。
- **`PROTO_E_INCOMPLETE`**：发现候选帧头但未收全。`span->offset` 指向该候选；长度字段尚不可读时 `span->size == 0`，否则为 `14 + n`。`out->frame` 不修改。若更靠后存在完整通过校验的帧，则返回 `PROTO_OK` 而非此状态。
- **`PROTO_E_NOFRAME`**：未找到通过全部校验的候选帧，包含无有效帧头、流长度不足、数据长度越界、帧尾或 CRC 失败。`span` 不修改，`out->status` 写入该状态。
- **`PROTO_E_NULL`**：必填指针为 NULL；若 `out` 非 NULL，则写入 `out->status`。

### 4.5 `proto_find_bounded`

```c
proto_status_t proto_find_bounded(const uint8_t *stream, size_t size,
                                  size_t window,
                                  proto_span_t *span,
                                  proto_decoded_t *out);
```

- **目的**：与 `proto_find()` 行为一致，但限制候选帧头的搜索范围。
- **窗口语义**：候选起点下标落在 `[0, min(window, size) - 4]`；帧体可延伸到窗口之外，但必须完全位于 `size` 范围内。`window >= size` 时等同于 `proto_find()`；`window < 4` 时返回 `PROTO_E_NOFRAME` 并写入 `out->status`。
- **复杂度**：候选数量与窗口长度成正比；每个完整候选的 CRC 处理另需 `O(96 × 8)`。实时场景 MUST 分别估算两部分最坏时间。
- **返回值与输出**：与 `proto_find()` 一致；`PROTO_E_INCOMPLETE` 时写 `span`，其余失败路径不修改 `span`。

调用方 MUST 按最坏噪声前缀长度设定窗口，否则窗口前的有效帧不会被发现。

### 4.6 `proto_status_str`

```c
const char *proto_status_str(int status);
```

- **目的**：返回状态码的英文文本。
- **参数**：`status` 为任意 `int`；未知值返回固定文本 `"unknown status"`。
- **返回值**：指向静态只读字符串的指针，调用方 MUST NOT 修改该内存。
- **使用限制**：文本仅供日志与人工排查，MUST NOT 用于逻辑判断。

## 5. 状态码

状态码的数值、文本与处置建议见 [状态码](ERROR_CODES.md)。`proto_decode()`、`proto_encode()`、`proto_find()`、`proto_find_bounded()` 通过返回值与 `out->status` 双通道传递状态，两者在成功路径上一致；`proto_encode_to()` 不带 `out` 参数，仅通过返回值传递状态。

## 6. 调用方职责

- 约束接收缓冲区容量，并对持续 `PROTO_E_INCOMPLETE` 设置超时。
- 对 `PROTO_E_NOFRAME` 执行噪声丢弃或坏帧丢弃策略，避免无限等待。
- 在需要抗篡改的场景中增加消息认证码。
- 避免编码接口的存储重叠；如需原地解码，使用 `proto_decode()`。
- 实时场景使用 `proto_find_bounded()`，并按最坏噪声长度设定窗口。
- 不解析 `proto_status_str()` 返回的文本。

## 7. 内部模块（非公共 API）

以下模块位于 `src/`，仅供库内部使用，不属于公共 API，不享受公共接口稳定性保证。

### 7.1 `proto_crc16_modbus`

```c
uint16_t proto_crc16_modbus(const uint8_t *data, size_t size);
```

- 计算 CRC16/Modbus，算法与覆盖范围见 [协议规范](PROTOCOL_SPEC.md) 第 4 节。
- `size == 0` 时 `data` 允许为 NULL，返回初值 `0xFFFF`。
- `size > 0` 时 `data` 必须指向至少 `size` 个可读字节且不得为 NULL；调试构建通过断言校验该前置条件。
- 函数不修改输入、不分配内存、无全局可变状态。
- 实现由 `PROTO_CRC_TABLE`（编译定义 `PROTO_CRC_USE_TABLE=1`）切换，默认逐位计算，启用时使用 256 项只读查表，二者结果一致。

该声明位于 `src/crc16.h`，使用 `PROTO_INTERNAL` 标注。GCC/Clang 非 Windows 构建中该宏展开为隐藏可见性属性；MSVC 与 Windows MinGW 下该宏为空，符号在静态库中仍具有外部链接。调用方 MUST NOT 依赖这些符号。

## 8. 版本与兼容性

- 线上帧格式与状态码数值在 1.x 内保持稳定。
- 公共头中的静态断言约束关键尺寸；跨版本混用目标文件或静态库不在保证范围内。
- 构建选项与算法实现的对应关系见 README 的“构建选项”章节。
