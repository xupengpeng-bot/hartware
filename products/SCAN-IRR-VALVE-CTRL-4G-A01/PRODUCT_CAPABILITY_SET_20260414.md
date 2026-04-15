# SCAN-IRR-VALVE-CTRL-4G-A01 完整能力集

更新时间：`2026-04-14`

## 1. 当前产品定位

`SCAN-IRR-VALVE-CTRL-4G-A01` 当前定义为一款面向灌溉场景的 `4G` 智能阀控终端。

这版产品能力已经按最新要求收敛为：

- 不包含 `LoRa` 预留能力
- 不包含本地脉冲输入能力
- 不包含本地模拟输入能力
- 不包含本地数字输入能力
- 不包含电表读取与控表能力
- `RS485` 仅用于外接传感器数据读取

## 2. 当前完整能力集

### 2.1 通信与联网

- `4G` 蜂窝通信
- 双 `SIM`
- `TCP` 长连接
- 设备注册
- 心跳保活
- 状态上报
- 远程控制
- OTA 升级

### 2.2 执行输出

- `2` 路电磁阀控制
- `2` 路继电器输出

### 2.3 本地交互

- 本地刷卡识别
- 语音播报
- 复位按键
- 调试串口
- `SWD` 烧录接口

### 2.4 供电与监测

- 电池供电管理
- 太阳能输入管理
- 电池电压检测
- 电源状态监测

### 2.5 传感器接入

- `RS485` 传感器总线 `1` 路
- 可用于后续接入传感器设备

## 3. 当前不纳入产品定义的部分

以下项目已经明确不作为当前产品能力输出：

- `LoRa`
- 本地脉冲输入
- 本地模拟输入
- 本地数字输入
- 电表读取
- 控表/跳闸控制
- 电表结算链路

## 4. 当前建议对外表述

`SCAN-IRR-VALVE-CTRL-4G-A01` 是一款支持 `4G` 联网、双 `SIM`、`RS485` 传感器接入、本地刷卡、语音播报、太阳能/电池供电管理的智能灌溉阀控终端，具备 `2` 路电磁阀控制与 `2` 路继电器输出能力，并支持设备注册、状态上报、远程控制与 OTA 升级。

## 5. 与当前固件的一致性

当前固件口径已经同步到这版能力定义：

- 电表相关能力已移出当前产品口径
- `RS485` 已按传感器总线保留
- 资源清单不再上报本地脉冲/模拟/数字输入
- 对外能力描述不再包含 `LoRa`

## 6. 依据文件

- 原理图：[SCH_Schematic1_1_2026-04-07.pdf](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-VALVE-CTRL-4G-A01/SCH_Schematic1_1_2026-04-07.pdf)
- 板级能力清单：[BOARD_CAPABILITY_INVENTORY_20260414.md](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-VALVE-CTRL-4G-A01/BOARD_CAPABILITY_INVENTORY_20260414.md)
- 产品基线：[README.md](/D:/Develop/houji/houjinongfuAI-Cursor/hartware/products/SCAN-IRR-VALVE-CTRL-4G-A01/README.md)
