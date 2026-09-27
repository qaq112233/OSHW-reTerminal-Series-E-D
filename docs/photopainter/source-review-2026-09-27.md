# PhotoPainter × SenseCraft 第二轮源码审查（2026-09-27）

## 已核对的调用链

| 范围 | 源码事实 | 结论 / 边界 |
|---|---|---|
| 云端标识 | `board_registry.h` 的 PhotoPainter combo 521 报 `xiao_diy_ee04` / `7_3_color_800_480` / `800x480`；`app_sensecraft.cpp::ensureSession()` 原样发送 | 与 Python emulator 的 board 标识一致；不代表真实设备云端端到端通过 |
| EPD0 | `image_parser.cpp::draw_epd_frame_internal()` 解压后以 4 bpp `pushImage`；`epd_color_map`、Seeed_GFX ED2208 的颜色映射与 Waveshare 枚举逐色一致 | 真实下载样本六种索引的软件链路已核对；物理颜色未测 |
| 横屏 | Waveshare `EPD_ParseBMPImage()` 对 800×480 BMP 取 Rotation 2；Seeed_GFX rotation 2 同样反转 180° | `rotation_map={2,3,0,1}`；补足首次配对页面初始方向；装机正反与镜像仍需肉眼确认 |
| 刷新关电 | Seeed_GFX `EPaper::update()` 为 wake→写入→refresh/BUSY→sleep/BUSY；板初始化修正初始化后上电状态；Deep Sleep 钩子再次 sleep | 软件时序对齐 Waveshare `EPD_TurnOnDisplay()` 的 0x04/0x12/0x02；驱动 BUSY 无限等待，不能从这里推断物理 rail 掉电 |
| AXP2101 | PhotoPainter 状态位直接对照本地定版 XPowersLib：STATUS1 bit3 电池、bit5 VBUS good，STATUS2 bit3 VBUS 有效、bits5–7 充电状态；电压/百分比沿用芯片寄存器 | 读失败返回不可用；使用外部供电含义的 `charging` 以兼容 Seeed 自动休眠策略；真实电压、百分比精度和供电状态待测 |
| 定时与低电量 | `app_sensecraft.cpp::request_update_timer_start()` 使用保存的睡眠间隔；`app_device_info.cpp` 定时器唤醒选图，`app_power_manager` 阻止下载/刷新时休眠 | 未改协议与调度；无 RTC 板级实现时不执行 04:30 RTC 维护窗口，改走周期 timer 唤醒 |
| 唤醒 | 通用 HAL 已启用低有效按键 EXT1；Waveshare Basic_mode 用 GPIO0/4、GPIO4 RTC 上拉且长按路径等待松开 | PhotoPainter 休眠前补齐 RTC 上拉和双按键释放；GPIO5 是高有效 PWR，不进入 ANY_LOW |
| 储存与外设 | Seeed 公共 SD 是 SPI，Waveshare 示例采用 SDMMC 4-bit；RTC 型号 PCF85063 与通用 PCF8563 不同，SHTC3 与 SHT4x 不同 | 均未假装兼容；当前 LittleFS 本地缓存，图库容量与掉电一致性未实测 |

## 明确未通过的验收

无实物，**不能**宣称六色光学正确、装机朝向、EPD BUSY 电平、控制器
POWER_OFF 后物理 `EPD_VCC` 已断、真实电流、按键唤醒、PMIC 量测、
Wi-Fi/MQTT/下载/深睡完整运行。V2 原理图尚未取得；若需判断 EPD_VCC
和 ALDO/Q2 的具体网络，请提供对应硬件版本的 Waveshare V2 schematic PDF
或官方 Resources-And-Documents 页导出的 `.md`，不能猜测。

## 回归方法

先构建 PhotoPainter 主固件与离线诊断，然后运行
`python3 examples/official/SenseCraft_HMI/tests/photopainter_color_contract.py`。
模拟器 `downloads/*.epd` 为本地可选样本，不纳入 Git；零样本时仍执行
Seeed_GFX × Waveshare 六色源码契约测试。再构建 `reterminal_e1001` 检查
通用源文件回归；以上均不代替实机刷新后量测供电。
