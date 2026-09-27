# ESP32-S3-PhotoPainter 硬件记录

本文件只记录能从当前 Waveshare 官方源码或官方页面/资料确认的事实。没有唯一来源
或尚未核对的项目统一写 `TODO / 待确认`，不根据常见板型或相近产品推断。

参考基准：

- Waveshare 官方仓库：`references/ESP32-S3-PhotoPainter`
- 提交：`a5e8f757ba0cafbb5586f07d3e83bda3184c0845`
- 官方 Wiki（英文，读取日期 2026-09-27）：
  `https://www.waveshare.com/wiki/ESP32-S3-PhotoPainter`
- 官方产品页（读取日期 2026-09-27）：
  `https://www.waveshare.com/esp32-s3-photopainter.htm`
- 官方原理图：
  `https://files.waveshare.com/wiki/ESP32-S3-PhotoPainter/ESP32-S3-PhotoPainter-Schematic.pdf`
- 官方显示屏手册：
  `https://files.waveshare.com/wiki/7.3inch-e-Paper-HAT-(E)/7.3inch-e-Paper-(E)-user-manual.pdf`

## 已确认硬件

| 项目 | 已确认信息 | 来源 |
|---|---|---|
| MCU / 模组 | ESP32-S3-WROOM-1-N16R8；Xtensa LX7 双核，最高 240 MHz | 官方产品页“What's On Board” |
| Flash | 16 MB | 官方产品页；`sdkconfig.defaults` 的 `CONFIG_ESPTOOLPY_FLASHSIZE_16MB` |
| PSRAM | 8 MB，Octal PSRAM，80 MHz | 官方产品页；`sdkconfig.defaults` 的 `CONFIG_SPIRAM_MODE_OCT` / `CONFIG_SPIRAM_SPEED_80M` |
| 显示屏 | 7.3 英寸 E Ink Spectra 6（E6）六色电子纸；800 × 480 | 官方 Wiki 参数；官方 7.3inch e-Paper (E) 手册 |
| 显示颜色 | 黑、白、绿、蓝、红、黄 | 官方 Wiki 参数；显示手册 |
| 刷新时间 | 约 25 秒（官方页面标注为实验数据） | 官方 Wiki 参数 |
| EPD SPI | MOSI=GPIO11、SCK=GPIO10、DC=GPIO8、CS=GPIO9、RST=GPIO12、BUSY=GPIO13 | `components/user_app_bsp/user_app.cpp:15`；`04_PowerConsumptionTest/.../bsp_config.h` |
| EPD 逻辑 | 4-bit、SPI mode 0、40 MHz；BUSY 高电平表示就绪 | `components/port_bsp/display_bsp.cpp` 构造函数及 `EPD_LoopBusy()` |
| EPD 刷新命令 | `0x04` POWER_ON；`0x12` DISPLAY_REFRESH；`0x02` POWER_OFF；`0x07 + 0xA5` 深睡 | `components/port_bsp/display_bsp.cpp`；官方显示手册命令表 |
| PMIC | AXP2101，I2C 地址 `0x34` | `components/pmicpower/power_bsp.h`；`user_app.cpp`；官方原理图 |
| PMIC I2C | SCL=GPIO48、SDA=GPIO47，实例为 I2C port 0 | `components/user_app_bsp/user_app.cpp:16`；板级 `config.h` |
| 温湿度 | SHTC3，I2C 地址 `0x70` | `components/port_bsp/i2c_equipment.h`；官方产品页与原理图 |
| RTC | PCF85063ATL；I2C 器件；外部 32.768 kHz 晶振 | 官方产品页；官方 Wiki 资料；官方原理图 |
| 按键 | BOOT=GPIO0（低有效）、KEY/GP4=GPIO4（低有效）、PWR/GP5=GPIO5（源码中高有效） | 板级 `config.h`；`components/port_bsp/button_bsp.c` |
| 指示灯 | 红色 GPIO45、绿色 GPIO42；源码中低电平点亮 | `components/port_bsp/led_bsp.h` |
| SD/TF | SDMMC 1-bit 与 4-bit 源码配置；4-bit 默认：CLK=39、CMD=41、D0=40、D1=1、D2=2、D3=38 | `components/port_bsp/sdcard_bsp.h`；`sdcard_bsp.cpp` |
| 音频 | ES8311 DAC、ES7210 ADC；I2S MCLK=14、BCLK=15、WS=16、DOUT=17、DIN=18、PA=7 | 板级 `config.h`；官方产品页 |
| 电池接口 | 3.7 V 锂电池 PH1.25 2P；官方产品页可选 1500 mAh 电池 | 官方产品页；官方原理图 |
| RTC 备份 | PH1.25 2P RTC 电池座，官方页面说明仅支持可充电 RTC 电池 | 官方产品页；官方原理图 |
| 充电/供电 | USB 5 V 输入、锂电池充电管理；源码设置充电恒流 500 mA | `components/pmicpower/power_bsp.cpp`；官方产品页 |
| 低功耗 | 支持 ESP32-S3 Deep Sleep、EXT1 按键唤醒和定时唤醒 | `components/user_app_bsp/mode_src/Basic_mode.cpp`；`Network_mode.cpp` |

## 墨水屏供电与安全

已确认：

- 官方原理图存在 `EPD_VCC`、驱动开关/升压电路和 `Q2 AO3401`。
- 官方原理图把 `SYS_OUT` 与 `GP5` 相连；源码同时把 GPIO5 配置为 PWR 按键。
- 当前应用源码的完整刷新路径为：写显示 RAM -> `0x04` POWER_ON -> BUSY 等待
  -> `0x12` DISPLAY_REFRESH -> BUSY 等待 -> `0x02` POWER_OFF -> BUSY 等待。
- `EPD_Display()` 已调用上述 `EPD_TurnOnDisplay()` 路径；官方手册要求刷新结束前后
  必须处理 POWER_OFF，深睡时使用 `0x07` + `0xA5`，并且退出深睡需要硬件复位。

> 安全红线：PhotoPainter 墨水屏不能长期保持供电。每次刷新必须等待 BUSY 确认真实结束，
> 执行正确的 panel power-off / sleep 流程，并关闭墨水屏供电 rail。网络等待、MQTT、
> 下载、idle、异常流程和 Deep Sleep 前都不能让墨水屏长期带电。

具体断电时序以 Waveshare 当前源码和官方文档为准，不要自行猜测。

## TODO / 待确认

- `EPD_VCC` 的完整使能/关断条件：需要结合原理图网络 `Q2 AO3401`、`SYS_OUT`、
  `GP5` 和 PMIC 寄存器确认，当前不把“GPIO5 即 EPD rail 开关”作为结论。
- PMIC 型号表述差异：源码和原理图使用 `AXP2101`，官方产品页文字写作 `TG28`；两者
  的版本/封装/对应关系待向 Waveshare 资料确认。
- 锂电接口名称差异：官方原理图标为 `PH1.25 2P`，官方产品页写作 `MX1.25 2PIN`；
  实际连接器规格待确认。
- 面板厂商/完整模组料号：官方手册称 7.3inch e-Paper (E)，但仓库中未见独立模组料号字符串。
- 主板 Flash/PSRAM 的独立丝印与模组封装版本：产品页和 SDK 配置都支持 16 MB Flash /
  8 MB PSRAM，但原理图文本只出现 `WROOM-1/2`，未直接打印 `N16R8`。
- 电池容量：官方产品页把 3.7 V 1500 mAh 列为可选，是否为实际随附电池需按订单确认。
- 当前源码中未见 PCF85063 RTC 驱动代码；I2C 地址和具体寄存器访问方式待确认。
- SHTC3 是否与其他 I2C 设备共用同一总线及上拉/供电域：官方原理图已显示同网络，
  但最终 GPIO 映射需在移植板级定义中再次核对。
- SD/TF 的最终运行模式：源码构造函数默认 width=4，也支持传入 1-bit；当前 application 的实际调用为 4-bit。
- 各休眠模式的精确电流、唤醒源去抖时间和异常掉电恢复策略：待实测。
- EPD 刷新过程中断网、重启、看门狗复位后的恢复策略：待设计并实测。
