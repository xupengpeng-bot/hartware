# 灌溉收费正式规则对齐清单

更新时间：2026-04-11

## 1. 文档目的

本文用于把“灌溉收费正式规则”和当前固件实际实现做逐项对齐，避免平台、嵌入式、联调三方对同一条链路理解不一致。

本文不是目标愿景，而是**当前代码真实状态**。

## 2. 当前代码主链概览

当前固件里实际存在两条相关链路：

1. 刷卡审计链
   - 入口：`workflow_card_reader.c`
   - 作用：解析刷卡、去抖、留痕、发平台鉴权查询
   - 特点：刷卡事件和业务启动已经拆开

2. 业务执行链
   - 入口：`proto_execute_action.c` -> `safety_flow.c`
   - 作用：处理平台 `EX` 命令，执行 `start/pause/resume/stop`
   - 特点：当前真正的启停动作由平台命令驱动，不是“刷卡即本地直接起泵”

## 3. 逐项对齐结论

### 3.1 本地状态机

规则要求：

```text
READY -> STARTING -> RUNNING -> PAUSING -> PAUSED -> RESUMING -> RUNNING -> STOPPING -> STOPPED
READY -> STARTING -> FAULT
RUNNING -> FAULT
PAUSING -> FAULT
PAUSED -> FAULT
RESUMING -> FAULT
STOPPING -> FAULT
FAULT -> STOPPED
```

当前状态：

- 已具备外部工作流枚举：`WF_READY_IDLE / WF_STARTING / WF_RUNNING / WF_PAUSING / WF_PAUSED / WF_RESUMING / WF_STOPPING / WF_STOPPED / WF_ERROR_STOP`
- 内部主状态机仍以 `SESSION_STATE_*` 和 `RUNTIME_WORKFLOW_*` 为主
- `RECOVERY_LOCKED` 与 `FAULT_LATCHED` 仍作为内部安全态存在

结论：

- `部分满足`
- 外部状态名已经具备主要链路
- 但当前真实运行主链仍不是完全按“单一收费状态机”建模
- 平台对齐时必须接受：当前固件内部还有 `AUTH_PENDING / PRE_START_METERING / POST_STOP_METERING / RECOVERY_LOCKED` 等中间安全态

关键文件：

- [model_types.h](D:\20251211\智能体\hardware\new\code\firmware\model\model_types.h)
- [runtime_state.h](D:\20251211\智能体\hardware\new\code\firmware\runtime\runtime_state.h)
- [safety_flow.c](D:\20251211\智能体\hardware\new\code\firmware\safety\safety_flow.c)

### 3.2 `start/pause/resume/stop` 唯一 `command_token`

规则要求：

- 每条命令必须带唯一 `command_token`
- 设备回执必须一对一绑定 `command_token`

当前状态：

- 当前协议用的是 `c` 作为关联字段
- `proto_execute_action.c` 会从顶层读 `c`
- 回 `AK/NK` 时把 `c` 原样带回
- 当前没有独立字段名 `command_token`

结论：

- `部分满足`
- 语义上已经有“一次命令一次回执一对一绑定”的能力
- 但字段名还不是收费规则里写的 `command_token`

关键文件：

- [proto_execute_action.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_execute_action.c)
- [proto_command.c](D:\20251211\智能体\hardware\new\code\firmware\protocol\proto_command.c)

### 3.3 每条命令只回一次明确结果

规则要求：

- `ack` 或 `nack`
- 不允许“只说收到”

当前状态：

- `EX` 处理统一走 `AK/NK`
- 当前 `start/stop` 命令只有在真正执行成功后才回 `AK`
- JSON 非法、能力不支持、忙、未运行等都会回 `NK`

结论：

- `基本满足`

注意：

- 目前 `pause/resume` 的回执是通过 `workflow_engine_request_pause_session()` / `resume_session()` 返回，动作层比 `start/stop` 更轻，后续仍建议补“成功后快照 + 明确运行事实事件”

### 3.4 重连后不能自动重放旧启动命令

规则要求：

- 只能补发历史事实
- 不能重放旧 `start`

当前状态：

- 已明确做了“启动不补发”
- pending 只用于刷卡审计和结束侧事件
- 当前不会因 TCP 重连自动再次执行旧的 `spu`

结论：

- `满足`

关键文件：

- [safety_flow.c](D:\20251211\智能体\hardware\new\code\firmware\safety\safety_flow.c)
- [workflow_card_reader.c](D:\20251211\智能体\hardware\new\code\firmware\workflow\workflow_card_reader.c)

### 3.5 刷卡事件与业务结果拆分

规则要求：

- `card_swipe_event`
- `command_ack_event`
- `runtime_state_event`
- `meter_snapshot_event`

当前状态：

- 刷卡审计已独立留痕，走 `ER ec="cse"`
- 业务命令回执独立，走 `AK/NK`
- 运行事实主要通过 `SS/HB/ER`
- 电表快照通过 `SS/QS:qem`

结论：

- `部分满足`
- “拆开发”已经成立
- 但消息类型名称还不是规则中建议的那组业务语义名
- 当前更偏“协议短码消息”，不是“收费事件模型消息”

### 3.6 同卡再次刷卡优先结束当前订单

规则要求：

- 同卡二刷默认理解为结束本单

当前状态：

- 在 `workflow_local_access.c` 这套本地 token workflow 中，已实现“同卡在运行中 -> stop”
- 但当前 4G 主链实际刷卡流程是：
  - 刷卡审计
  - 平台鉴权
  - 平台再发 `EX`
- 主链并没有把“同卡二刷 = stop”作为唯一正式规则收口

结论：

- `未完全满足`
- 这是当前收费规则和固件主链之间的一个真实缺口

关键文件：

- [workflow_card_reader.c](D:\20251211\智能体\hardware\new\code\firmware\workflow\workflow_card_reader.c)
- [workflow_local_access.c](D:\20251211\智能体\hardware\new\code\firmware\workflow\workflow_local_access.c)

### 3.7 去抖规则

规则要求：

- 只消除物理抖动
- 不吞真实二次操作

当前状态：

- `global_debounce = 250ms`
- `same_token_debounce = 1500ms`
- 超过窗口的同卡二刷不会被去抖吞掉

结论：

- `基本满足`

风险提示：

- `same_token_debounce = 1500ms` 对“非常快的真实二刷停机”仍有一定影响，平台联调时应确认这个窗口是否接受

### 3.8 启动成功后至少带完整事实快照

规则要求：

- `command_token`
- `session_ref`
- `runtime_state=RUNNING`
- `occurred_at`
- `cumulative_runtime_sec`
- `total_m3`
- `energy_kwh`
- `meter_epoch`

当前状态：

- 启动成功会：
  - 回 `AK`
  - 立刻发一条 `SS`
  - `SS` 带 `wf/rt/vv/ia/pw/ek/mp/ch`
- 其中：
  - `session_ref` 有
  - `rt/ek` 有
  - `total_m3` 当前只在真实流量有效时才可能存在
  - `occurred_at` 没有
  - `meter_epoch` 没有
  - `command_token` 仍是 `c`，不是显式字段

结论：

- `部分满足`

### 3.9 暂停/恢复逻辑

规则要求：

- `pause` 真暂停成功后再回 ACK
- `resume` 真恢复成功后再回 ACK
- 暂停期间运行时长不增长

当前状态：

- 协议上支持 `wf/pas` 和 `wf/res`
- `workflow_engine` 里已有 `WF_PAUSING/WF_PAUSED/WF_RESUMING`
- `runtime_sec` 在 `RUNTIME_RUN_RUNNING` 才增长

结论：

- `部分满足`

风险点：

- 当前 `pause/resume` 主要接在 `workflow_engine`，而主启停事实链主要在 `safety_flow`
- 这两条链还没有完全统一成一套收费事实模型

### 3.10 停机成功事实

规则要求：

- 停机成功后回 ACK
- 上报最终计量快照
- 带 `stop_reason_code`

当前状态：

- `stop` 成功会回 `AK`
- 会发 `ER ec="ss"`，带 `rc` 作为停止原因
- 会刷新 `summary`
- 会在停止前后采集电表/流量快照

缺项：

- 没有显式 `occurred_at`
- 没有 `meter_epoch`
- 最终停机事实模型仍是通用事件，不是规则中的 `runtime_stopped_event`

结论：

- `部分满足`

### 3.11 累计型计量值

规则要求：

- `cumulative_runtime_sec`
- `total_m3`
- `energy_kwh`
- 单调可解释

当前状态：

- `runtime_sec` 已存在
- `energy_kwh` 已存在
- `total_m3` 已存在，但依赖真实流量模块；当前没有真实流量时就是无效
- 暂停和停机时 `runtime_sec` 不会继续增长

结论：

- `部分满足`

风险点：

- 当前没有真实流量硬件时，`total_m3` 无法支持按水量计费
- 当前默认更适合按电量或时长结算

### 3.12 `meter_epoch / counter_reset`

规则要求：

- 设备重启、换表、计数器清零必须通知平台

当前状态：

- 当前代码里没有 `meter_epoch`
- 当前协议里没有 `counter_reset_event`
- 当前持久化里有 `power_loss_persist`，但不是收费计量纪元

结论：

- `未满足`

这是收费可信链路的关键缺口之一。

### 3.13 异常停机主动上报

规则要求：

- 断电
- 故障保护
- 急停
- 接触器跳闸
- 传感器异常

当前状态：

- 保护触发会停机
- 泵掉电会停机
- 离线超时 5 分钟会停机
- 控制器断电恢复后会进入 `RECOVERY_LOCKED`
- 停机事实会发 `ER`

结论：

- `基本满足`

新增规则已落地：

- 离线超过 300 秒必须停泵
- 停机不确定时进入 `RECOVERY_LOCKED`
- 启动不会补发
- 结束事件可以在同一开机周期补发

### 3.14 平台不会接受的实现方式

对照当前实现：

- 自动重放旧启动命令：`已禁止`
- 只回“收到”不回最终结果：`已避免`
- 暂停期间 `runtime_sec` 继续增长：`当前不会`
- 停机事件不带最终计量快照：`部分已有，但还不够完整标准化`
- 刷卡动作和业务执行结果混发：`当前已拆开`
- 计数器清零但不通知平台：`当前仍存在漏洞`

## 4. 当前最大漏洞

### 漏洞 1：主链“同卡二刷结束订单”没有正式收口

影响：

- 平台如果按“同卡启停”设计，当前 4G 主链并不是严格这样执行

### 漏洞 2：没有 `meter_epoch / counter_reset_event`

影响：

- 设备重启、换表、计数器清零后，平台无法建立可信计费连续性

### 漏洞 3：暂停/恢复链与 `safety_flow` 主启停链尚未完全统一

影响：

- 收费状态机可以表达暂停恢复
- 但当前动作和事实闭环还没有像 start/stop 那样收紧

### 漏洞 4：刷卡审计队列仍为 RAM 队列

影响：

- 断电重启后，未补发审计事件仍可能丢失

## 5. 当前建议的联调口径

平台和嵌入式当前最安全的对齐方式如下：

1. 刷卡事件视为独立审计流，不直接等于启动成功。
2. 平台鉴权通过后，仍需显式发 `start/pause/resume/stop` 命令。
3. `start` 不补发，`stop` 类结果允许补发。
4. 离线超过 5 分钟强制停泵。
5. 停机不确定时进入 `RECOVERY_LOCKED`，不能假装停止成功。
6. 当前按电表 485 开合闸已真实执行，但真实 `breaker_feedback` 仍未完成。

## 6. 下一轮代码优先级

建议按这个顺序继续补：

1. 增加显式 `command_token` 字段映射
2. 增加 `meter_epoch / counter_reset_event`
3. 把 pause/resume 收紧到和 start/stop 同级的收费事实链
4. 把“同卡二刷结束订单”统一收口到主链
5. 将刷卡审计从 RAM 队列升级为掉电持久化队列
