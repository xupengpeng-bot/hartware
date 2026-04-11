# 嵌入式灌溉控制器协议与短指令合同 v2 对齐清单

更新时间：2026-04-11

## 1. 文档目的

这份清单只回答一件事：当前固件和《嵌入式灌溉控制器协议与短指令合同 v2》相比，哪些已经满足，哪些部分满足，哪些还没有闭环。

## 2. 总体结论

当前固件已经把协议主链收到了 v2 的方向上，尤其是：

- 顶层短码消息类型已经统一
- `qcs / qwf / qem` 查询主链已经存在
- `spu / tpu / pas / res` 执行动作已经有正式入口
- `AK / NK` 已经是一条命令一次明确结果
- `wf` 已经向 `RI / ST / RN / PA / PS / RS / SP / ED / ER` 这一组短码对齐
- `HB / SS / QS:qcs` 已经带 `rt / fq / ek`

但还存在 5 个需要和平台明确对齐的缺口：

1. `ovl / cvl` 还没有在当前执行入口闭环。
2. `meter_epoch / counter_reset` 还没有实现。
3. 同卡再次刷卡优先结束当前灌溉，还没有在当前 4G 主链统一收口。
4. `pause / resume` 有入口，但和主会话收费事实链还没有完全统一。
5. 刷卡审计队列当前是 RAM 队列，突然断电后未补发事件可能丢失。

## 3. 顶层报文合同

### 3.1 顶层短字段

当前实现状态：`已满足`

当前实现的顶层字段主链就是：

- `v`
- `t`
- `i`
- `m`
- `s`
- `c`
- `r`
- `p`

其中：

- `c` 当前已经作为命令关联号使用，语义上等于平台的 `command_token`
- 还没有单独增加一个字面量字段名 `command_token`

相关文件：

- [proto_envelope.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_envelope.c)
- [proto_command.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_command.c)

### 3.2 顶层 `c = command_token`

当前实现状态：`部分满足`

原因：

- 当前命令关联确实走顶层 `c`
- 平台如果把 `command_token` 放到 `c`，当前固件能完整回传
- 但代码里还没有显式解析一个名叫 `command_token` 的顶层字段

## 4. 消息类型短码

当前实现状态：`已满足`

当前协议层已经围绕这些短消息类型构建：

- `RG`
- `HB`
- `SS`
- `ER`
- `QR`
- `QS`
- `EX`
- `AK`
- `NK`
- `SC`

相关文件：

- [proto_register.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_register.c)
- [proto_heartbeat.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_heartbeat.c)
- [proto_state_snapshot.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_state_snapshot.c)
- [proto_event_report.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_event_report.c)
- [proto_query.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_query.c)
- [proto_execute_action.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_execute_action.c)
- [proto_sync_config.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_sync_config.c)

## 5. 查询合同

### 5.1 `qcs`

当前实现状态：`已满足`

当前 `qcs` 已经返回：

- `rd`
- `on`
- `tc`
- `wf`
- `rt`
- `fq`
- `ek`
- `csq`
- `bs`
- `bv`
- `sv`
- `cv`
- `pm`

若开启 `bkf` 还会带：

- `brs`

相关文件：

- [proto_query.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_query.c)

### 5.2 `qwf`

当前实现状态：`已满足`

当前 `qwf` 已返回短码化的 `wf`。

### 5.3 `qem`

当前实现状态：`已满足`

当前 `qem` 已返回：

- `mp`
- `vv`
- `ia`
- `pw`
- `ek`

## 6. 动作短码合同

### 6.1 `spu`

当前实现状态：`已满足`

当前已实现：

- 命令解析
- 真实执行
- 成功后 `AK`
- 失败后 `NK`
- 成功后 `SS`

当前板型电表控制链上，`close_breaker` 也已经落到真实实现。

### 6.2 `tpu`

当前实现状态：`已满足`

当前已实现：

- 命令解析
- 真实执行
- 成功后 `AK`
- 停机事实事件/快照
- 最终计量

### 6.3 `pas`

当前实现状态：`部分满足`

当前已实现：

- `wf/pas` 入口
- 通过 `workflow_engine_request_pause_session()` 进入暂停流程

当前未完全闭环：

- 还没有像 `start/stop` 那样形成同等强度的收费事实链
- 平台联调时要把它视为“已有入口、待继续收口”

### 6.4 `res`

当前实现状态：`部分满足`

与 `pas` 相同，入口已存在，但业务事实链未完全统一。

### 6.5 `ovl`

当前实现状态：`未满足`

当前执行入口 [proto_execute_action.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_execute_action.c) 尚未实现 `ovl` 分发。

### 6.6 `cvl`

当前实现状态：`未满足`

当前执行入口 [proto_execute_action.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_execute_action.c) 尚未实现 `cvl` 分发。

## 7. ACK / NACK 合同

### 7.1 一条命令只回一次明确结果

当前实现状态：`已满足`

`EX` 成功只回一次 `AK`，失败只回一次 `NK`。

### 7.2 `AK` 只能表示真正成功

当前实现状态：`基本满足`

当前 `spu / tpu` 主链已经按“真正动作成功后才 `AK`”执行。

### 7.3 `NK` 推荐拒绝码

当前实现状态：`部分满足`

当前映射已经按 v2 方向收口到：

- `BZ`
- `UC`
- `MN`
- `SI`
- `PR`
- `PI`
- `EX`

但仍保留了少量旧映射兼容项，例如：

- `CV`

相关文件：

- [proto_codec_json.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_codec_json.c)

## 8. 工作流短码合同

当前实现状态：`部分满足`

当前映射已经对齐为：

- `RI`
- `ST`
- `RN`
- `PA`
- `PS`
- `RS`
- `SP`
- `ED`
- `ER`

但内部状态机仍保留这些安全中间态：

- `AUTH_PENDING`
- `PRE_START_METERING`
- `POST_STOP_METERING`
- `RECOVERY_LOCKED`
- `FAULT_LATCHED`

这些内部态已被压缩映射成 v2 短码，而不是从系统中完全删除。

## 9. 计量合同

### 9.1 `rt / fq / ek`

当前实现状态：`部分满足`

当前：

- `rt` 已在 `HB / SS / QS:qcs` 中输出
- `fq` 已在 `HB / SS / QS:qcs` 中按有效值输出
- `ek` 已在 `HB / SS / QS:qcs / QS:qem / SS` 中输出

但仍有两个边界：

- `fq` 依赖真实流量源，当前没有真实流量时不会伪造
- `meter_epoch` 还没有，平台仍缺少“计数器世代”这一层解释能力

### 9.2 `meter_epoch / counter_reset`

当前实现状态：`未满足`

当前仓库中还没有：

- `meter_epoch`
- `counter_reset_event`

这是 v2 合同里最关键的未闭环点之一。

## 10. 刷卡事件合同

### 10.1 一次刷卡一条独立事件

当前实现状态：`已满足`

当前 `workflow_card_reader.c` 已将刷卡事件独立成审计流，独立于业务执行。

### 10.2 去抖只能做极短窗口

当前实现状态：`基本满足`

当前窗口：

- 全局去抖 `250ms`
- 同卡去抖 `1500ms`

### 10.3 同卡再次刷卡优先结束当前灌溉

当前实现状态：`未完全满足`

这是当前主链的真实缺口：

- `workflow_local_access.c` 有“同卡再次刷卡 -> stop”的本地逻辑
- 但当前 4G 主链是 `workflow_card_reader + safety_flow + platform EX`
- 这条主链还没有把“同卡再次刷卡优先结束当前灌溉”收成唯一正式规则

## 11. 重连与重启合同

### 11.1 可以补发历史事实事件

当前实现状态：`已满足`

当前：

- 刷卡审计可在线恢复后补发
- 结束侧事件可补发

### 11.2 禁止自动重放旧启动

当前实现状态：`已满足`

当前固件已明确保证：

- 不自动补执行旧 `spu`
- 不在 TCP 恢复后自动重放启动

## 12. 异常停机合同

当前实现状态：`基本满足`

当前已覆盖：

- 断电恢复高风险 -> `RECOVERY_LOCKED`
- 泵掉电 -> 进入安全停机/恢复锁路径
- 离线超过 5 分钟 -> 强制停泵
- 停机不确定 -> 不再假停机

但平台仍需接受一个现实：

- 当前异常事件字段名仍以现有 `ER` 极简上报结构为主
- 不是合同中那组完全展开的业务语义事件名

## 13. 当前最值得平台优先对齐的 6 条

1. 当前设备已按短码协议主链联调，不再参考旧短码。
2. `c` 当前就是命令关联号，语义上等于 `command_token`。
3. `spu / tpu` 已经是真动作成功后才 `AK`。
4. `pas / res` 有入口，但还不是和 `start / stop` 同级的完整收费事实链。
5. `ovl / cvl` 当前还没闭环，不应纳入本轮正式联调范围。
6. `meter_epoch / counter_reset` 还没有，平台暂时不能把“计量重置可信解释”完全依赖设备完成。

## 14. 建议后续开发优先级

1. 增加 `meter_epoch / counter_reset_event`
2. 把 `ovl / cvl` 正式接入执行链
3. 把 `pas / res` 收口到和 `spu / tpu` 同级的事实链
4. 把“同卡二刷结束当前灌溉”统一进当前 4G 主链
5. 把刷卡审计队列从 RAM 队列升级成掉电持久化队列
