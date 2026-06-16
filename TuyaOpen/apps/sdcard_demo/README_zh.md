# SD Card Demo

这是一个面向 `SPARKLEIOT_T5_DEV` 的独立 `SD` 卡读卡示例。

## 功能

- 挂载 `/sdcard`
- 打印根目录文件列表
- 读取 `/sdcard/test.txt`
- 将读取内容输出到串口日志

## 硬件连接

依据板级原理图，`TF Card` 使用 `SDIO 4-line`：

- `P14 -> SD_SCK`
- `P15 -> SD_CMD`
- `P16 -> SD_D0`
- `P17 -> SD_D1`
- `P18 -> SD_D2`
- `P19 -> SD_D3`

## 使用方式

1. 在 TF 卡根目录放入 `test.txt`
2. 编译并烧录 `sdcard_demo`
3. 观察串口日志中的挂载、目录和文件内容输出
