# 相对 Seeed upstream 的差异记录

基准：Seeed upstream `8d97d8912987c07529287629f7e73039bb0e6384`，
PhotoPainter fork 引入前的提交。移植应以 Seeed 的应用为主体；
`references/ESP32-S3-PhotoPainter` 为只读硬件参考。

## 阶段 2：最小 Board（构建/真机状态见阶段提交）

| Seeed 原文件 | 变更 | 原因与影响 |
|---|---|---|
| `examples/official/SenseCraft_HMI/platformio.ini` | 增加独立 `waveshare_photopainter` 环境 | 16 MB Flash / 8 MB Octal PSRAM；ED2208 六色 800×480 与独立 GPIO 设置；不影响其他构建环境。 |
| `examples/official/SenseCraft_HMI/src/driver.h` | 注册 ED2208 combo 521（后续差异修正） | 复用 Seeed E1002 的 800×480 横屏 UI；板型和云端别名仍在 registry 独立维护。 |
| `examples/official/SenseCraft_HMI/src/boards/board_registry.h` | 增加 PhotoPainter profile 与屏幕 | 在原有 Board 注册点增加独立设备身份；仅新宏生效。 |
| `examples/official/SenseCraft_HMI/src/boards/common/screen_assets.cpp` | 复用现有 E1002 六色等待图 | 仅资源选择；无 APP 分支。 |
| `examples/official/SenseCraft_HMI/src/resources/pages/e1002_epd.h` | 撤销临时 combo 524 的资源条件扩展 | 避免复制 800×480 图；仅编译资源条件改变。 |

新增 `src/boards/waveshare_photopainter/{config.h,waveshare_photopainter.cpp}`：
独立薄 Board，参考 Waveshare GPIO。启动时检查 PMIC I2C 地址（**不是完整 PMIC 驱动**）；
Seeed_GFX ED2208 初始化后立即同步状态并关控制器电源；每次 `update()`
由 Seeed_GFX 完成唤醒/刷新/BUSY/`0x02`/BUSY。电气 rail 状态、按键极性与
实际六色方向均待真机验证。阶段 2 尚不支持 SD、RTC、SHTC3、电池/充电、物理 rail 测量；电池/充电见阶段 6。

安全边界：官方 ED2208 驱动的 BUSY 等待没有超时；此阶段不修改 Seeed_GFX
上游依赖，不宣称物理 rail 断电或异常条件安全性已通过。后续需要真机验证。

## 阶段 3：离线显示 Bring-up

仅追加 `platformio.ini` 诊断目标和新增
`src/boards/waveshare_photopainter/bringup.cpp`、`docs/photopainter/bringup.md`；
没有改 SenseCraft APP、Seeed_GFX 或其他已有业务文件。诊断在每次
`update()` 结束后才等待按键；真机验收仍待硬件。

## 阶段 4：SenseCraft 云端链路编译接入

`src/boards/board_registry.h` 中 PhotoPainter 的**云端上报** `board.type`
设为 `xiao_diy_ee04`，与已验证的 Python emulator 六色 800×480
配置一致；本地 `BoardModel` / `GetBoardType()` / 构建目标仍保持独立的
PhotoPainter 身份。这是协议兼容别名，**不意味着运行在 XIAO 硬件上**。
不修改 `app_sensecraft.cpp`、`app_download.cpp`、`app_view.cpp`。
固件构建通过只表示编译接入；Pair、MQTT、Manifest、图片下载和真机显示
仍需连接硬件与实际账号验证。

## 阶段 5：显示方向、唤醒与定时休眠差异修正

- `platformio.ini`、`driver.h`、`boards/board_registry.h`：PhotoPainter 使用现有
  `521` ED2208 横屏布局，避免 `524` 遗漏上游已有的组合号分支；registry
  仍上报 `xiao_diy_ee04`。Waveshare 示例的默认 rotation 0 对应横向
  800×480；物理朝向和左右镜像仍待实物验证。
- `boards/common/screen_assets.cpp`、`resources/pages/e1002_epd.h`：撤销阶段 2
  为 `524` 临时增加的两处等待图资源条件。
- `boards/common/button.h`、`hal/hal.cpp`：EXT1 ANY_LOW 唤醒掩码仅含低有效
  按键；PhotoPainter 的 GPIO5 PWR 为高有效，不再错误加入。
- `boards/common/board.h`、`hal/hal.{h,cpp}`、`APP/app_device_info.cpp`：新增
  板级 Deep Sleep 前钩子，在共享 SPI 锁内运行；PhotoPainter 钩子调用
  Seeed_GFX `sleep()`，确保驱动认为仍醒着时执行 POWER_OFF/BUSY。
  此钩子不能证明物理供电 rail 断开；BUSY 无限等待仍需真机验证。
- `boards/common/sleep_schedule.h`、`APP/app_device_info.cpp`：修复短刷新周期
  减去 20 秒导致无符号下溢，最少休眠 1 秒。
- `tests/photopainter_registry.cpp`、`tests/photopainter_sleep_schedule.cpp`：
  更新组合号断言并覆盖边界周期。

软件验证：两个 PhotoPainter 构建目标、两个主机测试以及 `git diff --check`。
不代表唤醒、功耗、BUSY 和云端端到端的真机验收。

## 阶段 6：AXP2101 电源状态适配

- `platformio.ini`：只给 PhotoPainter 主固件加入定版的 XPowersLib
  (`d6997586e68f65afd51baa775903df930db39821`)；诊断目标不会调用 PMIC。
- `boards/waveshare_photopainter/{waveshare_photopainter.cpp,axp2101_status.h}`：
  使用 Waveshare 示例所用的 XPowers AXP2101，读取电池连接、充电/外部
  供电、原始电压和电量；参照官方示例设置 USB 限流与充电电流，启用电池
  检测/电压 ADC。**不推断或切换任何未知 ALDO 与 EPD rail。**
- `boards/common/board.h`、`hal/hal.cpp`：提供可选的板级 PMIC 读数，
  未实现该接口的 Seeed 板继续使用原有 SY6974 + ADC 行为。
  此处沿用 Seeed `pmicIsCharging()` 的既有语义：外部电源有效或正在充电
  都应阻止自动 Deep Sleep；与严格的 AXP2101 “充电阶段”不完全同义。
- `tests/photopainter_axp2101_status.cpp`：主机测试 AXP2101 状态位，
  包括 USB 输入、电池缺失以及电池放电边界。

无实物，PMIC 芯片识别、I2C 状态、电池电量精度、USB 供电与低功耗行为
均未得到实机验收。I2C 读取失败会返回不可用读数，不伪造电池数据。

## 阶段 7：Deep Sleep 最终显示锁

`APP/app_device_info.cpp`：在最终 `app_power_manager_has_blocker()` 检查和
`esp_deep_sleep_start()` 期间持续持有已有的共享 SPI/显示锁。阶段 5 的钩子
虽已在锁内执行，但此前锁在 Deep Sleep 前释放，允许新显示操作插入；
现在若被 blocker 取消，作用域析构会正常释放该锁。不修改云端应用逻辑。
该改动缩小软件竞态窗口，**未经过并发实机压力与物理 rail 验证**。

## 阶段 8：ED2208 官方六色与朝向源码对照

- `boards/board_registry.h`：PhotoPainter 的旋转映射从 `{0,1,2,3}` 改为
  `{2,3,0,1}`。Waveshare `display_bsp.cpp::EPD_ParseBMPImage()` 对 800×480
  BMP 明确设 `Rotation=2`，`EPD_PixelRotate()` 把缓冲区转 180°；
  Seeed_GFX 4-bpp `TFT_eSprite::drawPixel()` 的 rotation 2 也作 180° 变换。
  `combo 521` 仍保留原 Seeed 的横屏 UI 和云端别名。
- `boards/waveshare_photopainter/waveshare_photopainter.cpp`、`bringup.cpp`：
  Seeed_GFX ED2208 初始化在命令 `0x30` 写 `0x08`，但 Waveshare 主例程
  和低功耗例程都写 `0x03`。仅在 PhotoPainter 初始化后写回 `0x03`，
  再按原有 POWER_OFF/BUSY 流程关闭控制器；不修改 Seeed_GFX 依赖。
  诊断程序默认也从 rotation 2 开始，同时保留四向测试。
- `tests/photopainter_registry.cpp`：断言四向旋转映射。

六色的**软件编码已可从三侧源代码静态确定**：真实下载样本 EPD0
`800×480`、4 bpp、mode 2、解压 192000 字节，六种索引均出现。
Seeed `MAP_COLOR6` 与 Seeed_GFX `TFT_BLACK/WHITE/RED/YELLOW/BLUE/GREEN`
均采用内部索引 `F/0/6/B/D/2`，ED2208 `COLOR_GET` 将它们映射为
Waveshare `ColorBlack/White/Red/Yellow/Blue/Green` 的面板索引
`0/1/3/2/5/6`。因此不需额外重排六色，但这只验证**编码一致**，
不验证实物光学颜色、面板批次或最终装机方向。

## 阶段 9：AXP2101 上电配置再核对

`boards/waveshare_photopainter/waveshare_photopainter.cpp`：XPowersLib 的
Arduino `init(Wire, SDA, SCL, 0x34)` 自身调用 `Wire.begin()`；移除板级重复
调用，在初始化后设定 100 kHz。参照 Waveshare 主例程与 Arduino 示例
`Custom_PmicRegisterInit()`，除了此前充电限流外，补齐 DCDC1 和
ALDO1–4 的 3300 mV 设置；**只设定电压，不启停未知电源 rail**。
PMIC 寄存器状态位、是否插电、锂电百分比与电压的读法与 XPowersLib
一致，但芯片响应及各电源域的实际连接仍需实物与对应版本原理图验收。

## 阶段 10：源码审查后修正首次朝向与休眠按键

- `boards/waveshare_photopainter/waveshare_photopainter.cpp`：显示初始化并按
  `POWER_OFF/BUSY` 关控制器后，给 EPaper 的缓冲区设置板级 rotation 2。
  图片解码器会自行设置该 rotation，但首次配对/激活页面并不保证调用
  解码器；避免第一次刷新沿用 Seeed_GFX 默认 rotation 0。
- 同文件的 Deep Sleep 钩子：在控制器关电后，按 Waveshare Basic_mode
  设置 GPIO0/GPIO4 的低有效 EXT1 唤醒与 GPIO4 RTC 上拉，并等两个按键
  松开；这一配置即使 `DeviceInfoInit()` 因低电量提前返回仍会执行。
  保留通用 HAL 已有按键唤醒逻辑，不把高有效 GPIO5 加入 ANY_LOW。
- `tests/photopainter_color_contract.py`：从本地固定依赖 Seeed_GFX 与
  Waveshare reference 抽取六色索引，若模拟器下载样本存在也核验真实
  EPD0 头、尺寸、解压长度及索引。无需复制显示驱动。

这是源码契约与 clean build，不是实机装机方向、按键唤醒、电流或物理 rail 验收。

## 阶段 11：无电池/读数失败的图片角标

`APP/app_view.cpp::finish_image_draw()`：只有电池百分比读数有效（>=0）
才显示 `<3%` 的红色/黑色电池警告角标。原逻辑把 HAL 的未知值 `-1`
四舍五入成 `0`，导致 AXP2101 无电池或读数暂时失败时错误提示低电量。
这是通用有效性修复，不含 PhotoPainter 型号分支；其他低电量停机逻辑
原已检查无效读数。主机/构建验证不等于 PMIC 实机精度验证。

## 阶段 12：用户提供的 V1 图纸核对

- `hardware.md` / `source-review-2026-09-27.md`：据本地 Altium PDF 和中文
  Resources-And-Documents 页，确认文件属于 V1 资料；在 V1 上，AXP2101
  ALDO3（pin 16）→ `EPD_VCC` → 屏幕 J1 VDD（pin 38），同网络连接 EPD
  驱动升压电路。`0x02 POWER_OFF` 不能代替 PMIC ALDO3 断电；此前“未知哪路
  ALDO”只对 V2/未知实物版本成立。
- PhotoPainter Board 仅更新注释：没有更改 PMIC 运行行为，也没有在未知板版
  上擅自关闭 ALDO3。V2 官方 PDF 链接直连仍返回验证 HTML，虽可读中文目录，
  但目录内容不能代替原理图；需 V2 PDF 或实物版本确认后再设计物理开关。

## 阶段 13：V1 物理 rail 软件开关与故障保护

- 新增板级 `panel_power.h`：据用户确认的 V1 图纸，ALDO3 →
  `EPD_VCC` → J1 VDD；上电/断电及 PMIC 位回读；按 Waveshare
  `EPD_Init` 重新初始化每个物理电源周期。正常刷新复用 Seeed_GFX
  的写帧、刷新/BUSY、POWER_OFF/BUSY；诊断目标也遵守同一流程。
- `boards/waveshare_photopainter/{waveshare_photopainter.cpp,bringup.cpp}`：
  启动恢复先关 ALDO3，初始化后关电；显示前才上电，完成后关电；
  深睡前再次确保关电；PMIC 初始化或关电失败时重启而不继续网络等待。
  板级独立故障任务防止 Seeed_GFX 无限 BUSY
  时长期上电：180 秒后尝试关 ALDO3 并重启。PMIC 通信故障与信号
  反灌无法在软件中保证，待实机验证。
- Seeed 公共文件：`boards/common/board.{h,cpp}` 新增可覆写的
  `RefreshDisplay()`，`hal/hal.{h,cpp}` 增加 `displayUpdate()`，
  `APP/app_view.cpp` 与 `boards/common/screen_assets.cpp` 将零参数
  `update()` 接入通用入口；`APP/app_device_info.cpp` 在断电失败时
  取消深睡。其他 Seeed 板默认仍调用原 `EPaper::update()`。
- 新增 `tests/photopainter_rail_contract.py` 静态验证入口与保护；
  没有硬件时无法验收 J1 VDD 实际电压、BUSY 和残余电流。
