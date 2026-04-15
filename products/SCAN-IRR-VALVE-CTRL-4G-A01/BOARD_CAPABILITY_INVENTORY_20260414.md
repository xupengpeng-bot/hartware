# SCAN-IRR-VALVE-CTRL-4G-A01 板级能力清单

更新时间：`2026-04-14`

这份清单用于重新收敛当前产品定义。以下内容已经按你的最新要求精简，不再把 `LoRa` 预留、`1` 路脉冲输入、`2` 路模拟输入、`2` 路数字输入纳入当前产品能力。

## 1. 当前保留的板级硬件资源

- `4G` 通信模组：`EC801ECNLE-N01-SNNSA`
- 双 `SIM`
- `RS485` 隔离总线 `1` 路
- 本地刷卡串口 `1` 路
- 调试串口 `1` 路
- `SWD` 烧录/调试接口
- 语音芯片与语音播报链路
- 继电器输出 `2` 路：`relay_1`、`relay_2`
- 阀驱动输出 `2` 路：`valve_1`、`valve_2`
- 电池电压采样
- 太阳能/供电检测
- 电源监测资源 `1` 组
- 复位按键

## 2. 当前产品定义下建议保留的软件能力

以下能力仍适合纳入这个产品：

- `dual_valve_control`
- `relay_output_control`
- `rs485_sensor_gateway`
- `card_auth_reader`
- `power_monitoring`
- `payment_qr_control`
- `pump_vfd_control`
- `remote_start_enable`
- `auto_linkage_enable`
- `auto_stop_on_low_pressure`
- `auto_stop_on_high_pressure`

## 3. 已明确移出当前产品定义的能力

以下能力不再纳入这块阀控板的当前产品口径：

- `electric_meter_modbus`
- 基于电表的 `breaker_control`
- `breaker_feedback_monitor`
- `single_valve_control`
- `pressure_acquisition`
- `flow_acquisition`
- `level_acquisition`
- `soil_moisture_acquisition`
- `soil_temperature_acquisition`
- `valve_feedback_monitor`
- `pump_fault_feedback`
- `rs485_vfd_gateway`
- `LoRa` 预留
- 本地 `pulse_input`
- 本地 `analog_input`
- 本地 `digital_input`

说明：

- `RS485` 现在只按“传感器总线”保留，不再按“电表总线”定义。
- 依赖本地脉冲、模拟、数字输入的采集/反馈类功能，默认都不再作为本产品能力对外描述。

## 4. 当前默认启用的固件能力

来自默认配置的当前启用项：

- `payment_qr_control`
- `card_auth_reader`
- `pump_vfd_control`
- `dual_valve_control`
- `relay_output_control`
- `power_monitoring`
- `rs485_sensor_gateway`

## 5. 当前默认通道绑定

- `pump_1` -> `pump_vfd_control`
- `card_reader_1` -> `card_auth_reader`
- `sensor_bus_1` -> `rs485_sensor_gateway`
- `valve_1` -> `dual_valve_control`
- `valve_2` -> `dual_valve_control`
- `relay_1` -> `relay_output_control`
- `relay_2` -> `relay_output_control`

## 6. 当前上报的资源清单

固件资源清单已经同步精简为：

- `relay_output = 2`
- `motor_driver = 2`
- `rs485_modbus = 1`
- `power_monitor = 1`
- `card_reader = 1`

这意味着平台注册包里不应再出现：

- `digital_input`
- `analog_input`
- `pulse_input`

## 7. 当前推荐的对外产品定义

如果你现在就要收成一版对外能力描述，建议用下面这组：

- `4G` 联网
- 双 `SIM`
- `RS485` 传感器总线
- 本地刷卡
- 语音播报
- 电池/太阳能供电管理
- `2` 路阀控
- `2` 路继电器
- 远程控制
- 状态上报
- OTA 升级

## 8. 本轮精简结论

这轮已经把产品口径进一步收到了“`4G + RS485 传感器总线 + 2 路阀控 + 2 路继电器 + 本地刷卡 + 电源管理`”这一层，不再保留 `LoRa`、本地脉冲输入、本地模拟输入、本地数字输入的对外能力描述。
