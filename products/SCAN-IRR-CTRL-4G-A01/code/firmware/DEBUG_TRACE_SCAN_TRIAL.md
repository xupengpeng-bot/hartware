# 扫码灌溉控制器（试验版）联调追溯记录

更新时间：2026-04-10

## 当前联调结论

1. 设备当前使用的 IMEI 已改为模组实读，不再使用硬编码占位值。
   当前实读值：`861295087573980`

2. 设备当前使用的 ICCID 已改为模组实读，不再使用硬编码占位值。
   当前实读值：`898604B72622C0311625`

3. 上行主链路已基本打通：
   - `REGISTER` 可发送
   - `HEARTBEAT` 可周期发送
   - `STATE_SNAPSHOT` 可发送
   - 刷卡后 `QUERY(card_swipe)` 可发送

4. 刷卡解析链路已打通：
   - UART1 读卡可稳定解析二进制卡帧
   - 当前测试卡号解析结果持续为：`2602552928`

5. 当前仍未完全打通的平台链路：
   - 暂未在串口日志中稳定看到 `TCP-RX`
   - 暂未看到 `REGISTER_ACK`
   - 暂未看到 `QUERY_RESULT`
   - 刷卡后出现的 `AUTH_DENIED` 目前更像“等待云侧结果超时”，不是设备本地拒绝

6. 当前仍存在非预期重启，原因未最终定位：
   - 已确认不是软件看门狗复位
   - 当前未捕获到 `[FAULT] HardFault`
   - 更像外部复位、供电波动、或未覆盖到的异常复位路径

## 本轮日志确认到的现状

### 已确认正常

1. RTC 时间已不再固定在第一次授时那一秒。
   日志已看到 `REGISTER/HEARTBEAT/QUERY` 的 `ts` 持续递增。

2. 刷卡上行 JSON 已不再被本地校验拦截。
   之前的：
   - `[PROTO] outbound JSON validation failed`
   已消失。

3. 刷卡后已可见真实上行日志：
   - `[FLOW] card swipe request sent token=2602552928 session=...`

4. 预热期刷卡现在会有语音反馈：
   - `starting_wait`

5. 平台链路抖动时设备会重连：
   - `TCP closed by peer`
   - `QIOPEN err=565`
   - 自动重连并重新 `REGISTER`

### 仍待解决

1. 刷卡后云侧回包仍未证实到达设备。
   现象：
   - 设备发送 `QUERY(card_swipe)` 成功
   - 约 8 秒后进入 `AUTH_DENIED`
   推断：
   - 更可能是未收到平台回包导致超时
   - 而不是平台明确拒绝

2. 仍然存在运行中重启。
   现象：
   - 日志中多次出现 `=== APP main() entry ===`
   现状：
   - `bsp_watchdog` 为空实现，不是喂狗超时
   - `HardFault` 日志未出现
   后续需要继续定位复位来源

3. TCP 下行诊断日志仍不够强。
   当前重点缺口：
   - 平台是否发回数据
   - 模组是否收到 `+QIURC: "recv"`
   - 设备是否成功 `QIRD`

## 本轮已落地改动

### 1. 身份与时间

1. 去掉了设备身份硬编码占位值。
   文件：
   - `code/firmware/common/common_identity.c`
   - `code/firmware/net/net_4g_modem.c`
   - `code/firmware/net/net_connectivity.c`

2. 增加模组实读身份：
   - `AT+CGSN` 读 IMEI
   - `AT+QCCID` 读 ICCID

3. 修复了 `QCCID` 解析错误。

4. 修复了 `QLTS` 成功后又被 `CCLK` 回拨 8 小时的问题。

5. 增加 RTC 走时：
   - 新增 `bsp_rtc_tick()`
   - 在主循环中推进 RTC 秒数
   文件：
   - `code/firmware/bsp/bsp_rtc.c`
   - `code/firmware/bsp/bsp_rtc.h`
   - `code/firmware/app/app_main.c`

### 2. 4G 建链与收发

1. 增加 modem warmup 窗口，避免刚上线立刻重建 TCP。
   文件：
   - `code/firmware/net/net_connectivity.c`

2. 增加 `CGATT` 预检查，未附着时跳过 `QIACT`，避免长时间空等。
   文件：
   - `code/firmware/net/net_4g_modem.c`

3. 增加 `QIACT=ERROR` 但 `QIACT?` 已激活的容错继续。

4. 增加 `QIRD` 快失败和断链回收日志，避免长时间闷住不动。

5. 限制每轮 inbound frame 处理数量，避免主循环被下行处理长期占住。
   文件：
   - `code/firmware/net/net_connectivity.c`

### 3. 注册与周期上报

1. 禁止正常在线时重复刷 `REGISTER`。

2. `REGISTER` 后立即开启：
   - `HEARTBEAT`
   - `STATE_SNAPSHOT`

3. 增加心跳/快照构包与发送日志，便于定位卡点。
   文件：
   - `code/firmware/app/app_scheduler.c`

### 4. 刷卡链路

1. 将读卡 UART 改为更可靠的 FIFO/IRQ 接收。
   文件：
   - `code/firmware/bsp/bsp_uart.c`

2. 将读卡口默认波特率调整为 `9600`。

3. 增加 `[CARD]` 全链路日志：
   - 收包
   - 解析
   - guard
   - 分发结果
   文件：
   - `code/firmware/workflow/workflow_card_reader.c`

4. 增加预热期和平台未就绪时的语音反馈：
   - `starting_wait`

5. 增加刷卡去抖与语音节流，避免连续刷卡把设备“刷爆”。

6. 修复 `card_swipe` 上行 JSON 拼接错误。
   之前问题：
   - `swipe_at` 有值时缺少结束引号
   结果：
   - 设备本地判包失败
   文件：
   - `code/firmware/safety/safety_flow.c`

### 5. 语音芯片

1. 已接入真实语音芯片码映射，不再只是日志映射。
   文件：
   - `code/firmware/config/voice_chip_prompts.h`
   - `code/firmware/bsp/bsp_voice.c`
   - `code/firmware/config/board_hw_config.h`

2. 当前已使用的关键提示：
   - `welcome -> 18`
   - `starting_wait -> 28`
   - `auth_denied/unavailable -> 36`

### 6. 虚拟表计与假值治理

1. 保留了联调期虚拟电表/流量结构，但明确作为 `virtual` 输出。
   文件：
   - `code/firmware/modules/module_meter.c`
   - `code/firmware/modules/module_flow.c`

2. 去掉了误导性的固定压力值。
   文件：
   - `code/firmware/modules/module_pressure.c`

3. 去掉了 OTA 侧伪造的稳定 TCP、固定信号和伪造容量。
   文件：
   - `code/firmware/port/ota_port_board.c`

### 7. 崩溃可观测性

1. 增加主循环存活日志：
   - `loop tick 0`
   - `loop tick 1000ms alive`
   - `loop 5s alive`
   文件：
   - `code/firmware/app/main.c`

2. 增加 `HardFault` 日志。
   文件：
   - `code/firmware/port/fault_handlers.c`

3. 修复缺失字段时的 JSON 取值空指针崩溃风险。
   文件：
   - `code/firmware/protocol/proto_codec_json.c`

### 8. 2026-04-10 新增定点修复

1. 修复 `STATE_SNAPSHOT malformed_json` 的根因：
   - 不再依赖 `%f` 构造 snapshot JSON
   - 改为定点数字字符串输出
   文件：
   - `code/firmware/protocol/proto_codec_json.c`
   - `code/firmware/protocol/proto_codec_json.h`
   - `code/firmware/protocol/proto_state_snapshot.c`

2. 增加发送前追溯日志：
   - `json_len`
   - `be32_len`
   - `wire_len`
   - `payload_tail32`
   文件：
   - `code/firmware/net/net_connectivity.c`

3. 修复 RTC 只授时不走时的问题：
   - `ts` 现已随主循环推进
   文件：
   - `code/firmware/bsp/bsp_rtc.c`
   - `code/firmware/bsp/bsp_rtc.h`
   - `code/firmware/app/app_main.c`

4. 修复刷卡 `card_swipe` 上行 JSON 拼接错误：
   - `swipe_at` 有值时缺少结束引号的问题已修复
   文件：
   - `code/firmware/safety/safety_flow.c`

5. 增强 `signal_csq` 与 `battery_soc` 质量：
   - 上电初始化即采样电池
   - 周期查询 `AT+CSQ`
   文件：
   - `code/firmware/app/app_main.c`
   - `code/firmware/net/net_4g_modem.c`

6. 缓和 `QIRD` 半包直接断链的问题：
   - payload 超时但已收部分字节时，先保留已收数据
   - 不再第一时间判死连接
   文件：
   - `code/firmware/net/net_4g_modem.c`

7. 去掉关键日志中的 `%f` 输出依赖：
   - 电表快照日志
   - 流量快照日志
   - 会话摘要日志
   - 停机事件摘要 JSON
   文件：
   - `code/firmware/safety/safety_flow.c`

## 当前开放问题

### P1：刷卡云鉴权回包未确认

现象：
- `QUERY(card_swipe)` 已成功发送
- 数秒后进入 `AUTH_DENIED`
- 日志里仍无 `QUERY_RESULT routed to safety_flow`

需要继续做：
- 增强 `TCP-RX`
- 增强 `QIRD`
- 明确打印平台回包正文

### P1：异常重启未定位

现象：
- 多轮日志中反复出现 `=== APP main() entry ===`

已排除：
- 当前不是软件看门狗

仍需排查：
- 外部复位来源
- 电源波动
- 未覆盖到的 fault/abort 路径
- 关键状态持久化前后是否有非法访问

### P2：刷卡忙态下的提示仍可优化

现象：
- 连续刷卡时会出现大量 `guard=2`
- 用户能听到提示，但仍会形成很多重复卡帧日志

后续可做：
- 在 `CLOUD_AUTH_PENDING` 期间对同卡直接丢弃更久
- 减少重复 `welcome`
- 区分“正在鉴权中”与“网络不可用”

### P2：平台下行注册确认未证实

现象：
- `REGISTER` 已发
- 但还没稳定看到 `REGISTER_ACK received`

## 推荐下一步

1. 给 `TCP-RX/QIRD/proto_dispatch` 再补一层完整日志。
2. 把刷卡云鉴权超时与明确拒绝在语音和日志上分开。
3. 增加复位原因记录，确认是外部复位、上电复位还是异常复位。
4. 在平台侧同时核对：
   - 是否收到 `QUERY(card_swipe)`
   - 是否向 IMEI `861295087573980` 回了 `QUERY_RESULT`
