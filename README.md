# imx6ull_greenhouse
这是基于正点原子阿尔法 i.MX6ULL 开发板 + Linux + Qt，实现多传感器数据采集与本地图形化监控
开发板：正点原子I.MX6U-ALPHA开发板
界面实现:Qt 5.12（Yocto SDK 交叉编译）
外设接线表：
| 外设 | 接口 | 引脚 | 读取方式 |
|------|------|------|---------|
| DS18B20 | 单总线 | GPIO1 | sysfs |
| BH1750 | I2C | I2C2 | /dev/i2c-1 |
| MH-Z19B | UART | UART3 | /dev/ttymxc2 |
| 风扇 | GPIO | GPIO0 | sysfs |
| OV5640 | CSI | CAMERA座 | V4L2 |

软件架构：
UI主线程 ←→ SensorWorker线程（500ms轮询传感器）
         ←→ CameraCapture线程（V4L2 15fps抓帧）
