# SCAN-IRR-CTRL-4G-A01 完整能力集

更新时间：`2026-04-14`

## 1. 当前产品定位

`SCAN-IRR-CTRL-4G-A01` 当前定义为一款面向机井灌溉场景的 `4G` 智能泵控终端。

这版产品按照现有原理图和平台协议口径，聚焦以下主能力：

- 本地继电器直接控泵
- `RS485` 电表通信
- `DLT645-2007` 电表协议
- 电量计量与收费结算
- 本地刷卡与平台远程控制
- 不支持语音播报
- 不支持扫码支付

## 2. 当前完整能力集

### 2.1 通信与联网

- `4G` 蜂窝通信
- 设备注册
- 心跳保活
- 状态上报
- 远程控制
- OTA 升级

### 2.2 泵控与执行

- `1` 路本地继电器输出 `relay_1`
- 默认泵控模式：`relay_direct`
- 平台动作支持开闸、合闸、启泵、停泵语义映射
- 继电器通道可独立执行输出控制

### 2.3 电表与收费

- `1` 路 `RS485` 电表总线
- 电表协议：`DLT645-2007`
- 支持电表地址发现
- 支持电能、电压、电流、有功功率读取
- 支持 `qem` 电表查询
- 支持收费前后电量快照
- 支持 `meter_epoch` 与清零/换表识别
- 默认按电量 `kWh` 结算
- 电表合闸成功后会先刷新量测，再上报启动快照

### 2.4 本地交互

- 本地刷卡识别
- 复位按键
- 调试串口
- `SWD` 烧录接口

### 2.5 保护与运行参数

- 支持配置心跳时间 `heartbeat_interval_sec`
- 支持配置过流保护开关 `overload_protection`
- 支持配置过流阈值 `over_current_limit_a`
- 支持配置缺相保护开关 `phase_loss_protection`
- 支持配置过压保护开关 `over_voltage_protection`
- 支持配置过压阈值 `over_voltage_limit_v`

### 2.6 供电与监测

- 电源状态监测
- 电池电压采样
- 太阳能输入检测
- 基础运行健康监测

## 3. 当前默认启用的固件能力

来自默认配置的当前启用项：

- `card_auth_reader`
- `electric_meter_modbus`
- `breaker_control`
- `relay_output_control`
- `power_monitoring`

说明：

- 这里的 `electric_meter_modbus` 是平台能力名，底层接入协议实际为 `DLT645-2007`
- 当前默认控泵路径走本地继电器，不默认走“电表 485 远程拉合闸”
- 若后续平台配置把 `pump_control_mode` 切到 `meter_breaker_485`，协议注册层也已保留该路径

## 4. 当前默认通道绑定

- `pump_1` -> `breaker_control`
- `card_reader_1` -> `card_auth_reader`
- `meter_1` -> `electric_meter_modbus`
- `relay_1` -> `relay_output_control`

对应资源引用：

- `pump_1` 使用 `relay_pump_run`
- `meter_1` 使用 `rs485_meter_1`
- `relay_1` 使用 `relay_pump_run`

## 5. 当前对外协议口径

平台注册与状态链路当前对齐为：

- 功能模块包含 `cdr`、`ebr`、`bkr`、`rly`、`pwm`
- `qcs` 可返回设备在线状态、运行状态和累计电量字段
- `qem` 可返回电表实时量测与协议类型
- 状态快照会按绑定关系上报电表与继电器通道
- 启动快照包含电流、电压、电量、功率字段
- 收费安全链已接入电表快照、`meter_epoch` 与计数器重置事件

## 6. 当前不纳入产品定义的部分

以下项不再作为这块板子的当前产品能力对外描述：

- 双路阀控
- 第二路继电器
- `LoRa` 预留能力
- 语音播报
- 扫码支付
- 本地脉冲输入
- 本地模拟输入
- 本地数字输入
- 传感器网关型 `RS485` 产品口径

## 7. 建议对外描述

`SCAN-IRR-CTRL-4G-A01` 是一款支持 `4G` 联网、本地刷卡、单路继电器控泵和 `RS485 DLT645-2007` 电表接入的智能灌溉控制终端，具备电量计量、收费结算、设备注册、状态上报、远程控制与 OTA 升级能力，并支持心跳周期、过流、过压和缺相保护参数配置。

## 8. 依据文件

- 原理图：[机井3.0原理图优化.pdf](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-CTRL-4G-A01/机井3.0原理图优化.pdf)
- 板级能力清单：[BOARD_CAPABILITY_INVENTORY_20260414.md](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-CTRL-4G-A01/BOARD_CAPABILITY_INVENTORY_20260414.md)
- 产品基线：[README.md](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-CTRL-4G-A01/README.md)
