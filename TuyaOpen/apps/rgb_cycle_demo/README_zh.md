# rgb_cycle_demo

这是一个独立的P25幻彩灯Demo，不依赖屏幕、传感器或AI功能。

默认配置：

- 灯珠：WS2812
- 数据引脚：P25
- 发送方式：GPIO精确时序
- 关键发送代码运行在ITCM，避免Flash缓存抖动
- 每种颜色只发送1帧，避免中间错误帧造成闪色
- 通道顺序：GRB
- 灯珠数量：1
- 效果：红、绿、蓝各保持1秒并循环

编译：

```bash
cd ~/0_Document/0_T5_TuyaOpen/TuyaOpen
source ./export.sh
cd apps/rgb_cycle_demo
tos.py config choice -c SPARKLEIOT_T5_DEV.config
tos.py build
```

正常运行时串口会循环输出：

```text
RGB cycle 0: RED
RGB cycle 0: GREEN
RGB cycle 0: BLUE
```

初始化成功日志应包含：

```text
WS2812 ready: P25 GPIO, GRB, count=1, repeat=1
```
