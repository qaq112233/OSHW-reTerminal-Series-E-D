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
