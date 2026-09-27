# 相对 Seeed upstream 的差异记录

基准：Seeed upstream `8d97d8912987c07529287629f7e73039bb0e6384`，
PhotoPainter fork 引入前的提交。移植应以 Seeed 的应用为主体；
`references/ESP32-S3-PhotoPainter` 为只读硬件参考。

## 阶段 2：最小 Board（构建/真机状态见阶段提交）

| Seeed 原文件 | 变更 | 原因与影响 |
|---|---|---|
| `examples/official/SenseCraft_HMI/platformio.ini` | 增加独立 `waveshare_photopainter` 环境 | 16 MB Flash / 8 MB Octal PSRAM；ED2208 六色 800×480 与独立 GPIO 设置；不影响其他构建环境。 |
| `examples/official/SenseCraft_HMI/src/driver.h` | 注册 combo 524 | 不重用 Seeed E1002 的设备身份；显示驱动仍使用官方 Seeed_GFX ED2208。 |
| `examples/official/SenseCraft_HMI/src/boards/board_registry.h` | 增加 PhotoPainter profile 与屏幕 | 在原有 Board 注册点增加独立设备身份；仅新宏生效。 |
| `examples/official/SenseCraft_HMI/src/boards/common/screen_assets.cpp` | 新 combo 复用 E1002 六色等待图 | 仅资源选择；无 APP 分支。 |
| `examples/official/SenseCraft_HMI/src/resources/pages/e1002_epd.h` | 将新 combo 加入旧有六色资源条件 | 避免复制 800×480 图；仅编译资源条件改变。 |

新增 `src/boards/waveshare_photopainter/{config.h,waveshare_photopainter.cpp}`：
独立薄 Board，参考 Waveshare GPIO。启动时检查 PMIC I2C 地址（**不是完整 PMIC 驱动**）；
Seeed_GFX ED2208 初始化后立即同步状态并关控制器电源；每次 `update()`
由 Seeed_GFX 完成唤醒/刷新/BUSY/`0x02`/BUSY。电气 rail 状态、按键极性与
实际六色方向均待真机验证。尚不支持 SD、RTC、SHTC3、电池/充电、物理 rail 测量。

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
