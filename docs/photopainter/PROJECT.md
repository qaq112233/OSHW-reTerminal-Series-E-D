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
- 2026-09-27 用户确定在无实物条件下按 Waveshare 示例做法继续移植，
  每个大阶段完成后单独提交。`hardware.md` 区分源码级控制器关电与
  未经实测的物理 rail 掉电；不能把编译通过当作真机验收。

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
