# sensor_display_demo

这是 SPARKLEIOT_T5_DEV 开发板上的独立传感器显示 demo，不接入也不修改
`your_chat_bot`。

## 当前功能

- 初始化 ST7789 屏幕和 LVGL。
- P42 为 SCL，P43 为 SDA。
- 使用逻辑 I2C2 的软件 I2C，避免占用触摸使用的硬件 I2C1。
- M117B 固定地址为 `0x45`，每秒采集一次温度。
- SH3001 自动探测 `0x37` 和 `0x36`，读取寄存器 `0x0F`，期望芯片 ID 为 `0x61`。
- SH3001 使用参考代码的 500 Hz、加速度 ±16g、陀螺仪 ±2000 dps 配置。
- 启动后采集 20 个静止样本，计算陀螺仪 X/Y/Z 零偏。
- 屏幕显示温度、加速度 `g`、角速度 `dps`、设备状态和采样次数。
- M117B 与 SH3001 独立运行；一个设备故障不会阻塞另一个设备。

## 文件结构

- `src/sensor_i2c_bus.c`：P42/P43 共享 I2C 总线和寄存器读写接口。
- `src/m117b.c`：M117B 命令、CRC 和温度换算。
- `src/sh3001.c`：SH3001 地址探测、初始化和六轴数据读取。
- `src/sensor_service.c`：独立设备状态机、采集线程和线程安全缓存。
- `src/sensor_ui.c`：LVGL 温度和六轴数据显示页面。
- `src/tuya_main.c`：应用初始化入口。

## 编译

```bash
cd ~/0_Document/0_T5_TuyaOpen/TuyaOpen
source ./export.sh
cd apps/sensor_display_demo
tos.py config choice -c SPARKLEIOT_T5_DEV.config
tos.py build
```

## 烧录后检查

SH3001 正常时，串口应出现：

```text
sensor software I2C ready: port=2, SCL=P42, SDA=P43
SH3001 probe address 0x37: chip ID=0x61
SH3001 initialized at address 0x37
```

屏幕应显示 `SH3001: OK @ 0x37`（也可能为 `0x36`），转动板子时
`GYRO X/Y/Z` 和 `ACC g` 数据应持续变化。

上电后屏幕会短暂显示 `SH3001: CAL 0/20`。校准期间需要把开发板静止放置，
大约 2 秒后状态自动变为 `SH3001: OK`。串口同时输出校准得到的三轴零偏。

如果显示 `SH3001: ERROR -1`，说明 `0x37`、`0x36` 均未收到正确应答。
优先检查 U5 供电、焊接、SDO 地址电平以及 P42/P43 的上拉和连线。
