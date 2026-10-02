# PhotoPainter 移植项目入口

## 最终目标

把 Seeed SenseCraft HMI 的云端链路和固件框架移植到 Waveshare ESP32-S3-PhotoPainter：
`Pair -> MQTT -> Manifest -> 图片下载 -> 本地解码 -> E6 电子纸刷新`。

## 三个项目各自负责什么

| 项目 | 角色 | 当前目录 |
|---|---|---|
| Seeed SenseCraft HMI | 最终固件主代码基础 | `examples/official/SenseCraft_HMI` |
| Waveshare ESP32-S3-PhotoPainter | 只读硬件参考：GPIO、PMIC、电源、按键、RTC、SHTC3、SD、Deep Sleep | `references/ESP32-S3-PhotoPainter` |
| Python Emulator | 已验证的云端协议参考实现 | `tools/sensecraft-emulator` |

Waveshare 仓库只作参考，不复制其完整 application，也不修改其上游代码。

主仓库保持 Seeed 历史：`origin` 指向你的 fork，`upstream` 指向 Seeed 官方仓库；
SenseCraft_HMI 不拆成脱离上游的新项目。

## 当前状态

- Python Emulator 已成功验证完整云端链路：
  `Pair -> MQTT -> Manifest -> Image Download`。
- Python Emulator 的验证不等于 ESP32 硬件验证；SPI、PMIC、电子纸供电、
  刷新时序、掉电恢复和长期稳定性仍需在后续硬件阶段单独验证。
- 2026-09-27 用户确定无实物期间按差异驱动继续移植，每个大阶段单独提交。
  已接入 PhotoPainter Board、离线显示诊断、云端 `xiao_diy_ee04` 别名、
  横屏组合号、AXP2101 电源读数与休眠前显示关电；主固件及诊断构建通过。
  用户已确认 V1，V1 图纸表明 ALDO3 供给 EPD_VCC；当前代码在刷新后
  关闭 ALDO3 并回读 PMIC 寄存器，驱动 BUSY 卡死时有 180 秒故障保护；
  `hardware.md` 区分软件关电、寄存器回读与未经实测的实际电压；
  编译通过不是真机或 PhotoPainter 云端端到端验收。

- 2026-10-02：增加 V1 首次烧录打包器与 ESP Launchpad 操作说明。两个目标
  清理后重建，输出从 `0x0` 写入的 16MB 整包；所有 Flash 设置/缓存会重置，
  OTA 初始化按实际 `0x8D000` 放置，应用在 `0x90000`。输出与工具缓存不提交 Git。
  文件校验通过仍不是实机/物理关电验收；具体使用见 `flashing.md`。

- 2026-10-02 首次实机反馈：旧 `bringup` 整包烧录成功，应用报
  `[Bring-up] PMIC initialization failed; restarting` 并重启；屏幕和 KEY
  测试尚未执行，不能称显示 bring-up 已通过。重新核对 V1 图纸 U2、U5
  和微雪 `I2cMasterBus(scl,sda,port)`，确认 SDA=47、SCL=48、地址0x34，
  两份 XPowers 源码均严格要求寄存器0x03的 ID=0x4A。
  当前增加两目标共用的 PMIC 分层诊断、明确100kHz/50ms I2C配置及有限重试。
  这是定位版本，不是已证实的根因修复；需取得新版 ACK/ID 日志后继续。
  发布目录使用 `photopainter-v1-2026-10-02-pmic-diagnostic-1`，保留旧包但不推荐重刷。

## 后续开发优先查阅

1. 项目入口和边界：`docs/photopainter/PROJECT.md`
2. 上游差异记录：`docs/photopainter/upstream-delta.md`
3. 硬件事实：`docs/photopainter/hardware.md`
4. 硬件源码参考：`references/ESP32-S3-PhotoPainter`
5. 协议参考：`tools/sensecraft-emulator`
6. 主固件：`examples/official/SenseCraft_HMI`

## 墨水屏安全红线（硬性要求）

> PhotoPainter 墨水屏不能长期保持供电。每次刷新必须等待 BUSY 确认刷新真正结束，
> 然后执行正确的 panel power-off / sleep 流程，并关闭墨水屏供电 rail。
> 网络等待、MQTT、下载、idle、异常流程和 Deep Sleep 前都不能让墨水屏长期带电。

具体断电时序必须以 Waveshare 当前源码和 Waveshare 官方文档为准，不得自行猜测。
