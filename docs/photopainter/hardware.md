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
- 官方 V1 原理图（现有引用仅适用于 V1，不可直接外推到 V2）：
  `https://files.waveshare.com/wiki/ESP32-S3-PhotoPainter/ESP32-S3-PhotoPainter-Schematic.pdf`
- 官方新版资料页（读取日期 2026-09-27，分别列出 V1、V2 原理图）：
  `https://docs.waveshare.net/ESP32-S3-PhotoPainter/Resources-And-Documents`
- 资料页列出的 V2 原理图（直连返回验证 HTML，**未读取到 PDF**）：
  `https://www.waveshare.net/w/upload/a/ae/ESP32-S3-PhotoPainter-Schematic-v2.0.pdf`
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

- 用户提供的 `/mnt/c/Users/QAQ/Desktop/ESP32-S3-PhotoPainter-Schematic.pdf` 为与资料页 V1 同名且同大小（1,251,764 字节）的单页 Altium 原理图；其 PDF 创建日期为 2025-10-20，SHA-256 为 `82472764688ed346dcb264a71b0260bb74de4efa6fc8843f8da0417e9f99c6ec`。这是 **V1 资料**，不是 V2。用户已明确确认实物为 V1。
- **V1 图纸可直接沿网络追踪**：AXP2101 的 ALDO3 引脚 16 输出 `EPD_VCC`；屏幕连接器 J1 的 VDD 引脚 38 接 `EPD_VCC`，同网络还供给面板驱动升压电路 Q1/Q2（AO3400/AO3401）及相关电容。`VCC3V3` 为另一条主电源，不应与 `EPD_VCC` 混同。
- 现有 Waveshare `power_bsp.cpp::Custom_PmicRegisterInit()` 只将 ALDO3 目标电压设为 3300 mV，**没有**调用 `disableALDO3()`；本移植现另行执行 ALDO3 物理开关。Seeed_GFX/Waveshare 的 `0x02 POWER_OFF` 不是 AXP2101 ALDO3 的开关命令；即便刷新完成，它也不能证明 V1 的物理 `EPD_VCC` 已断。
- 已记录的 V1 原理图把 `SYS_OUT` 与 `GP5` 相连；源码同时把 GPIO5 配置为 PWR 按键。**不能据此把 GPIO5 当作 EPD 电源开关。**
- 当前应用源码的刷新路径为：写显示 RAM -> `0x04` POWER_ON -> BUSY 等待
  -> `0x12` DISPLAY_REFRESH -> BUSY 等待 -> `0x02` POWER_OFF -> BUSY 等待
  （`components/port_bsp/display_bsp.cpp:149-168`）。**这里只确认控制器命令，不能证明物理 `EPD_VCC` rail 已关。**
- `EPD_Display()` 已调用上述 `EPD_TurnOnDisplay()` 路径；官方手册要求刷新结束前后
  必须处理 POWER_OFF，深睡时使用 `0x07` + `0xA5`，并且退出深睡需要硬件复位。

> 安全红线：PhotoPainter 墨水屏不能长期保持供电。每次刷新必须等待 BUSY 确认真实结束，
> 执行正确的 panel power-off / sleep 流程，并关闭墨水屏供电 rail。网络等待、MQTT、
> 下载、idle、异常流程和 Deep Sleep 前都不能让墨水屏长期带电。

本移植在 V1 上额外通过 AXP2101 ALDO3 控制 EPD_VCC。寄存器回读只能证明 PMIC 接受开关操作，不能代替在屏幕 J1 VDD 上量测电压及检查 GPIO 反向供电。

### 2026-09-27 开发基准与验证边界

- 用户确认本阶段以 Waveshare 示例的上电、BUSY 等待、刷新、`0x02 POWER_OFF`
  做法为软件实现基准；用户已确定实物是 V1，故本移植依 V1 原理图直接控制 ALDO3，**不以 V2 图纸或物理 rail 测量为软件开发阻塞项**。
- 该基准不等于已验证 `EPD_VCC` 物理掉电。没有实物时，代码与文档一律只
  宣称“执行示例控制器关电时序并要求 PMIC ALDO3 寄存器回读为关闭”，不声称真机掉电或电气安全验收完成。
- 子模块当前提交 `a5e8f757ba0cafbb5586f07d3e83bda3184c0845`；本次查询
  Waveshare `origin/HEAD` 返回相同 SHA。
- `components/port_bsp/display_bsp.cpp:92-99` 的 BUSY 等待没有超时；
  `:149-168` 的正常刷新末尾发送 `0x02` 并等待 BUSY。实现与测试应覆盖
  失败、重启、Deep Sleep 路径，不把“驱动返回”等同于物理掉电。
- `components/pmicpower/power_bsp.cpp:84-100` 设置 ALDO1–4 为 3300 mV，
  现在 V1 图纸已证实 ALDO3 输出 `EPD_VCC`；V2 网络尚未读取到，不能将 V1 结论外推。
- Seeed `src/boards/common/epaper_display.cpp` 在 `begin()` 中直接调用
  `display_.begin()`；`src/APP/app_view.cpp` 与 `src/boards/common/screen_assets.cpp`
  有多处 `EPaper::update()`。必须检查当前 Seeed_GFX 驱动能否在所有刷新结束时
  完成 Waveshare 同等 `POWER_OFF`，避免屏幕在网络等待中停留在上电状态。

## TODO / 待确认

- `EPD_VCC` 的电气验收：用户确认 V1，现有实现使用 ALDO3。待实物测量 J1 VDD 在刷新中/后/深睡时的电压、测 GPIO 信号反灌和整机电流；V2 不在本轮适配范围。GPIO5 不是 EPD rail 开关。
- PMIC 型号表述差异：源码和原理图使用 `AXP2101`，官方产品页文字写作 `TG28`；两者
  的版本/封装/对应关系待向 Waveshare 资料确认。
- 锂电接口名称差异：官方原理图标为 `PH1.25 2P`，官方产品页写作 `MX1.25 2PIN`；
  实际连接器规格待确认。
- 面板厂商/完整模组料号：官方手册称 7.3inch e-Paper (E)，但仓库中未见独立模组料号字符串。
- 主板 Flash/PSRAM 的独立丝印与模组封装版本：产品页和 SDK 配置都支持 16 MB Flash /
  8 MB PSRAM，但原理图文本只出现 `WROOM-1/2`，未直接打印 `N16R8`。
- 电池容量：官方产品页把 3.7 V 1500 mAh 列为可选，是否为实际随附电池需按订单确认。
- 当前源码中未见 PCF85063 RTC 驱动代码；V1 原理图标注 I2C 地址 `0x51`，具体寄存器访问方式和 V2 接线仍待核对。
- SHTC3 是否与其他 I2C 设备共用同一总线及上拉/供电域：官方原理图已显示同网络，
  但最终 GPIO 映射需在移植板级定义中再次核对。
- SD/TF 的最终运行模式：源码构造函数默认 width=4，也支持传入 1-bit；当前 application 的实际调用为 4-bit。
- 各休眠模式的精确电流、唤醒源去抖时间和异常掉电恢复策略：待实测。
- EPD 刷新途中断网不应延长供电；重启入口与故障保护任务会尝试关闭 ALDO3；异常重启、PMIC/I2C 故障以及 BUSY 永不释放的恢复效果待实测。

### PMIC 软件契约补充（2026-09-27）

Waveshare `power_bsp.cpp` 主例程及 `05_ArduinoExample/01_Audio_Test/power_bsp.cpp`
均设置 DCDC1、ALDO1–4 为 3300 mV，并设置 USB 输入限流 2000 mA、
预充 50 mA、恒流充电 500 mA、终止 25 mA。当前 PhotoPainter Board
使用定版 XPowersLib 复刻这些**寄存器配置**，没有据此推断 ALDO 的负载
或调用任何 `enable/disablePowerOutput`。XPowersLib 的 AXP2101 STATUS1
bit 3 为电池在位、bit 5 为 VBUS good；STATUS2 高 3 bit 指充/放电状态，
bit 3 辅助判断 USB 输入。电池电压/电量经芯片 ADC/fuel gauge 寄存器
读取，不是采用 Seeed 板的外部 ADC 分压曲线。

第二轮静态审查的逐路径证据与未验证清单见 `source-review-2026-09-27.md`。

### 用户提供 V1 原理图的适用边界（2026-09-27）

中文资源页可访问并列出 V1、V2 两份图纸；V1 官方文件标称 1,251,764 字节，
与用户本地 PDF 相同。V2 官方链接本次直连返回约 12 KB 的验证 HTML（尽管
HEAD 自称 PDF、长度 2,102,659 字节），因此**未读取 V2 图纸**。不能据
验证页或 V1 网络假装完成 V2 电源设计。V1 图纸只证实网络连接；ALDO3
实际启用位、面板 VDD 关断后的电压/残余供电路径仍需真机测量。

### V1 ALDO3 软件保护实现（2026-09-27）

PhotoPainter Board 与离线诊断先关闭 ALDO3、设为 3300 mV，仅在初始化/
刷新时使能；再次执行 Waveshare 的面板寄存器初始化，复用 Seeed_GFX 的
帧传输、`0x12` 刷新/BUSY、`0x02` POWER_OFF/BUSY，然后关闭 ALDO3
并回读开关寄存器 bit 2。休眠前再次要求关断成功，失败则取消休眠。

Seeed_GFX ED2208 内部的 BUSY 仍是无超时循环，因此另起板级 FreeRTOS
故障保护任务：EPD_VCC 上电前开始计时；超过 180 秒时尝试关 ALDO3
并重启。180 秒是异常上限，不是正常刷新延时；故障时无法保证等待
BUSY 正常结束，也不能在 PMIC 通信失效或 GPIO 信号反灌时保证物理
电压为零。若故障任务创建失败，则拒绝上电显示；PMIC 初始化或关电回读失败时
重启而非继续进入 Wi-Fi/idle。反复重启仍不能修复 PMIC/I2C 硬件故障。Waveshare 示例并未
实现上述 ALDO3 开关，不能把“复刻示例”与“实物断电已验收”混为一谈。

### 首次实机 PMIC 启动失败与分层诊断（2026-10-02）

用户在 V1 上从0x0烧入整包，烧录器检测16MB Flash和8MB PSRAM，启动后报
`[Bring-up] PMIC initialization failed; restarting`。日志证明应用运行到 PMIC
检测失败分支，尚未进入 EPD 初始化、刷新或 KEY 循环；不能据此判定屏幕坏、
供电 rail 已关闭、SDA/SCL 反接或 Flash 烧录地址错误。

本轮重新核对的证据：

- 用户提供的 V1 `ESP32-S3-PhotoPainter-Schematic.pdf` 第1页 U2：
  IO47（模块pin24）→ `ESP_I2C_SDA`，IO48（模块pin25）→ `ESP_I2C_SCL`；
  R33/R35为上拉。U5 pin39 SDA / pin40 SCK 经 `AXP_SDA` / `AXP_SCL` 接同一总线。
- 微雪 `components/user_app_bsp/user_app.cpp:16`：`I2cMasterBus I2cBus(48,47,0)`；
  `components/port_bsp/i2c_bsp.cpp:8-16` 明确构造参数顺序为 **SCL、SDA、port**。
- 本项目定版 XPowersLib 与微雪随附版本均使用地址0x34；
  `REG/AXP2101Constants.h` 定义 IC_TYPE=0x03、CHIP_ID=0x4A；
  两版 `initImpl()` 均严格比较 ID，没有证据支持跳过或放宽 ID 判定。

`pmic_startup.h` 对两目标共用：显式检查 `Wire.begin(47,48,100000)` 返回值，
设置50ms总线超时，最多三次检测（间隔20ms）；逐次保留地址 ACK、ID寄存器
发送结果、接收长度、ID值和库初始化结果。读取事务沿用 XPowers 的 STOP 后读，
不倒换引脚、不遍历未知地址、不对未识别芯片写 PMIC 寄存器、不使能任何供电rail。
成功后仍先关闭 ALDO3并回读，再进入原来的显示流程。失败日志重复约1秒后重启，
不是无限等待串口；诊断目标成功时仅在 ALDO3关闭确认后最多等待USB连接1.5秒。

**待实机进一步定位**：是总线开始失败、无ACK、读寄存器失败、ID不匹配，还是库
初始化失败。软件检查不能证明真实 PMIC/走线故障，也不能保证通信失败时物理断电。
若失败重复，先保存一组完整日志，然后断开全部供电（包括电池），不要反复按 KEY
或改刷同样依赖 PMIC 的主固件。主机模拟测试与编译不替代这些实物证据。

### 事务超时与 native 回调适配（2026-10-02，诊断2）

第二轮实机日志稳定显示诊断1 `bus=OK` / `ACK=5`，三个attempt都超时，
ID_TX=255（未尝试）、received=0、chip_id=-1；此时尚未开始EPD操作。
`Arduino-ESP32 2.0.17 libraries/Wire/src/Wire.cpp` 的endTransmission返回映射明确：
ESP_ERR_TIMEOUT→5，ESP_FAIL→2。**5不是普通地址NACK**；`bus=OK`仅说明
驱动开始成功，不说明电气总线或器件响应正常。旧 `PMIC_NO_ACK`是泛化分类，
必须连同ACK数字解释；本次没有证据支持改地址、交换GPIO或忽略芯片ID。

确认并修正的实现差异：

- 微雪 `components/pmicpower/power_bsp.cpp:19-42,69`：XPowers `begin(address,
  readCallback,writeCallback)`，回调最多三次重试、100ms间隔。
- 微雪 `components/port_bsp/i2c_bsp.cpp::i2c_read_buff`：寄存器读取调用
  `i2c_master_transmit_receive`，写寄存器前缀后repeated START读取；
  `i2c_write_buff`将寄存器号和payload放在同一写事务中。
- 原移植用库的Arduino接口；定版 `XPowersCommon.hpp::readRegister`在
  `endTransmission()`后单独`requestFrom()`，总线在两阶段间STOP释放。
  诊断1还把空payload地址probe作为读取ID的必要前置步骤。
- 当前 `pmic_bus.h`直接建立板内 I2C0：SDA47/SCL48、100kHz、上拉、
  7周期毛刺滤波。当前Arduino框架带IDF4.4，使用该SDK的
  `i2c_master_write_read_device`实现相同repeated-START**事务形式**，
  不复制IDF5结构，不将两个SDK实现宣称完全相同，也不全局升级Seeed依赖。
  XPowers仍严格检查ID=0x4A；无需再依赖Arduino零payloadprobe才能尝试读ID。

每个 native事务最多50ms；回调最多三次、100ms间隔，startup最多三次，
只对失败attempt间隔20ms。SDK驱动错误会原样报告，如ESP_ERR_TIMEOUT、ESP_FAIL。
记录驱动配置/安装/滤波状态、PMIC读取前后的SDA/SCL空闲电平；电平1只说明
抽样时为高，**不证明时钟波形、总线或PMIC正常**，电平0也不能定位是哪颗器件拉低。

PMIC失败后仅增加一次同总线RTC对照：V1图纸U8地址0x51；NXP PCF85063A
数据手册表4明确0x00为Control_1。仅读取，不设置日期、清中断、设置闹钟或开启SHTC3。
微雪提供的PCF85063 PDF链接本次返回验证页，未读取其内容；改用同型号NXP原厂手册：
<https://www.nxp.com/docs/en/data-sheet/PCF85063A.pdf>（寄存器表与I2C写/读地址A2/A3）。
RTC响应是辅助证据，失败也可能与RTC供电状态有关，不能仅据两个错误断言板坏。

诊断2依旧先严格识别PMIC、关闭并回读ALDO3，才进入任何EPD显示/网络等待；
通信失败时不启动显示、不修改未识别的PMIC寄存器，短暂报告后重启。
正常刷新后的POWER_OFF/BUSY、ALDO3关闭和故障guard保持不变。
Native I2C是该板唯一总线owner，不同时安装Wire和native驱动；回调日志错误缓存为atomic，
避免主任务/guard并发写造成C++数据竞争。未做GPIO手动时钟恢复或未知电源rail操作。

**实机待验证**：彻底断开USB及电池，再用诊断2获取错误/电平日志。
当前更改修正了源码中的事务差异，但不能从旧ACK=5证明其就是根因。
主机测试模拟SDK函数和启动策略，覆盖结合读、前缀写、重试、错误及严格ID，
不能代替真实I2C波形、PMIC通信、EPD显示和断电验收。
