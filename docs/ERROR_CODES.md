# 状态码

## 1. 编码约定

- 状态码类型为 `proto_status_t`。`PROTO_OK` 为 0，失败码为连续负整数 `-1..-9`。
- 状态码数值在 1.x 内保持稳定；新增错误码追加在现有区间之后，不重排既有取值。
- 文本描述由 `proto_status_str()` 提供，仅用于日志与人工排查；调用方 MUST NOT 依据文本进行逻辑判断。
- 编码、解码与分帧函数均通过返回值传递状态。`proto_decode()`、`proto_encode()`、`proto_find()`、`proto_find_bounded()` 另通过 `out->status` 传递同一状态；`proto_encode_to()` 不带 `out` 参数，仅通过返回值传递状态，成功时写 `*out_size`。带 `out` 参数的接口失败时仅写 `status`，例外见第 4 节。
- 库不使用 `errno`。

## 2. 状态码表

| 状态码 | 数值 | 文本 | 含义 | 典型触发接口 | 建议处置 |
| :-- | :--: | :-- | :-- | :-- | :-- |
| `PROTO_OK` | 0 | `OK` | 操作成功 | 全部 | 读取输出数据 |
| `PROTO_E_NULL` | -1 | `null argument` | 必填指针参数为 NULL | 所有带指针参数的公共函数 | 检查调用点参数传递，属程序缺陷 |
| `PROTO_E_HEAD` | -2 | `header mismatch` | 帧头四字节不匹配 | `proto_decode()` | 丢弃当前输入，重新搜索帧头 |
| `PROTO_E_TAIL` | -3 | `trailer mismatch` | 帧尾两字节不匹配 | `proto_decode()` | 丢弃该帧，检查链路误码 |
| `PROTO_E_LENGTH` | -4 | `length error` | 输入长度小于 14，或与声明长度不一致 | `proto_decode()` | 丢弃该帧，检查对端实现 |
| `PROTO_E_DATASIZE` | -5 | `data length out of range` | 数据长度超过 96 字节 | `proto_decode()`、`proto_encode()`、`proto_encode_to()` | 丢弃/拒绝该帧；编码场景检查调用方构造的 `data_len` |
| `PROTO_E_CRC` | -6 | `CRC mismatch` | CRC 校验不通过 | `proto_decode()` | 丢弃该帧，统计误码率 |
| `PROTO_E_NOSPACE` | -7 | `insufficient output capacity` | 输出缓冲区容量不足 | `proto_encode_to()`；`proto_encode()` 正常不可达 | 增大目标缓冲区后重试 |
| `PROTO_E_NOFRAME` | -8 | `no valid frame in stream` | 未找到通过全部校验的候选帧：无有效帧头、流长度不足以容纳帧头、数据长度越界、帧尾或 CRC 失败、`window < 4` | `proto_find()`、`proto_find_bounded()` | 流长度不足时继续接收；其余情况丢弃噪声或坏帧，并检查链路误码与对端实现 |
| `PROTO_E_INCOMPLETE` | -9 | `frame incomplete` | 已识别候选帧头但未收全 | `proto_find()`、`proto_find_bounded()` | 继续接收，并设置上层超时 |

## 3. 按接口的状态矩阵

| 接口 | 可返回的状态 |
| :-- | :-- |
| `proto_decode()` | `PROTO_OK`、`PROTO_E_NULL`、`PROTO_E_LENGTH`、`PROTO_E_HEAD`、`PROTO_E_DATASIZE`、`PROTO_E_TAIL`、`PROTO_E_CRC` |
| `proto_encode()` | `PROTO_OK`、`PROTO_E_NULL`、`PROTO_E_DATASIZE`、`PROTO_E_NOSPACE`（固定容量下正常不可达） |
| `proto_encode_to()` | `PROTO_OK`、`PROTO_E_NULL`、`PROTO_E_DATASIZE`、`PROTO_E_NOSPACE` |
| `proto_find()` | `PROTO_OK`、`PROTO_E_NULL`、`PROTO_E_INCOMPLETE`、`PROTO_E_NOFRAME` |
| `proto_find_bounded()` | `PROTO_OK`、`PROTO_E_NULL`、`PROTO_E_INCOMPLETE`、`PROTO_E_NOFRAME` |
| `proto_status_str()` | 不适用：返回文本，不返回状态码 |

`proto_find()` 与 `proto_find_bounded()` 不返回具体的帧头、长度、帧尾或 CRC 错误；这些失败统一归并为 `PROTO_E_NOFRAME`。需要定位具体失败原因时，应对候选区间调用 `proto_decode()`。

## 4. 输出状态约定

- `proto_decode()` 与 `proto_encode()` 失败时仅写 `out->status`；`frame`、`size` 等数据字段保持不变。
- `proto_encode_to()` 不带 `out` 参数，状态仅通过返回值传递；失败时不写 `dst` 与 `*out_size`，成功时写 `*out_size`。
- `proto_find()` 与 `proto_find_bounded()` 返回 `PROTO_E_INCOMPLETE` 时会写 `span`：`span->offset` 指向候选起点，`span->size` 为 0（长度字段尚不可读）或 `14 + n`。
- 上述两个分帧接口在所有其他失败路径中不修改 `span`；任何失败都不修改 `out->frame`。
- 对于带 `out` 参数的接口，若 `out` 为 NULL，接口无法写入 `status`，仅通过返回值返回 `PROTO_E_NULL`。

## 5. 处置策略

1. **程序缺陷类**：`PROTO_E_NULL` 表明调用点未满足前置条件，应在开发阶段修正，不应作为正常运行分支处理。
2. **帧数据类**：`PROTO_E_HEAD`、`PROTO_E_TAIL`、`PROTO_E_LENGTH`、`PROTO_E_DATASIZE`、`PROTO_E_CRC` 表示帧不符合线格式。调用方应丢弃该帧，并根据持续失败情况检查对端实现或链路质量。
3. **容量类**：`PROTO_E_NOSPACE` 表示目标缓冲区不足；增大容量或改用固定容量接口后重试。
4. **分帧类**：`PROTO_E_INCOMPLETE` 要求继续接收并设置超时；`PROTO_E_NOFRAME` 要求根据链路策略丢弃噪声或坏帧后继续扫描，避免无限等待。

## 6. 日志规范

记录协议错误时 MUST 同时包含状态码数值、文本描述与上下文信息。示例：

```text
proto_decode failed: status=-6 (CRC mismatch), size=22
```

上下文 SHOULD 至少包含接口名、输入长度与候选偏移。日志 MUST NOT 输出完整载荷内容。
