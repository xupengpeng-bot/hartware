# SCAN-IRR-CTRL-4G-A01 板级能力清单

更新时间：`2026-04-14`

这份清单用于把当前产品定义重新收敛到“机井泵控 + 电表计量收费”的板级事实。

## 1. 当前保留的板级硬件资源

- `4G` 通信模组
- `RS485` 隔离总线 `1` 路
- 本地刷卡串口 `1` 路
- 调试串口 `1` 路
- `SWD` 烧录/调试接口
- 本地继电器输出 `1` 路：`relay_1`
- 电源监测资源 `1` 组
- 电池电压采样
- 太阳能输入检测
- 复位按键

## 2. 当前产品定义下保留的软件能力

以下能力适合纳入这块板子的当前产品口径：

- `electric_meter_modbus`
- `breaker_control`
- `relay_output_control`
- `card_auth_reader`
- `power_monitoring`

说明：

- `breaker_control` 当前默认映射到本地继电器控泵
- `electric_meter_modbus` 在本产品里用于承载 `DLT645-2007` 电表能力

## 3. 当前默认启用的能力与资源

默认启用项：

- `card_auth_reader`
- `electric_meter_modbus`
- `breaker_control`
- `relay_output_control`
- `power_monitoring`

资源清单：

- `relay_output = 1`
- `motor_driver = 0`
- `rs485_modbus = 1`
- `power_monitor = 1`
- `card_reader = 1`

## 4. 当前默认通道绑定

- `pump_1` -> `breaker_control`
- `card_reader_1` -> `card_auth_reader`
- `meter_1` -> `electric_meter_modbus`
- `relay_1` -> `relay_output_control`

资源引用：

- `pump_1` -> `relay_pump_run`
- `meter_1` -> `rs485_meter_1`
- `relay_1` -> `relay_pump_run`

## 5. 电表与控泵链路说明

- 电表侧通信总线为单路 `RS485`
- 电表协议为 `DLT645-2007`
- 当前默认泵控模式为 `PUMP_CONTROL_RELAY_DIRECT`
- 若平台后续切换到 `PUMP_CONTROL_METER_BREAKER_485`，代码已具备协议注册与开合闸执行路径
- 电表合闸成功后，会先刷新一次电表量测缓存，再上报状态快照
- 收费安全链已接入电表快照、累计电量和 `meter_epoch`

## 6. 可配置运行参数

- 心跳时间：`rr.heartbeat_interval_sec`
- 过流保护开关：`pc.overload_protection`
- 过流阈值：`pc.over_current_limit_a`
- 缺相保护开关：`pc.phase_loss_protection`
- 过压保护开关：`pc.over_voltage_protection`
- 过压阈值：`pc.over_voltage_limit_v`

## 7. 不再纳入当前产品定义的硬件口径

以下能力不再纳入当前产品的对外能力描述：

- 第 `2` 路继电器
- 双路阀驱动
- `LoRa` 预留资源
- 语音播报
- 扫码支付
- 本地脉冲输入
- 本地模拟输入
- 本地数字输入
- 传感器网关型 `RS485` 口径

## 8. 当前板级结论

这块板按当前原理图和程序落点，应收敛为：

- `4G` 联网
- 单路 `RS485` 电表总线
- 单路本地继电器控泵
- 本地刷卡
- 电量计量与收费结算
- 状态上报、远程控制、OTA 升级
