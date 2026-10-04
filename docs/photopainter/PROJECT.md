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
  测试尚未执行，不能称显示 bring-up 已通过。重新核对 V1 图纸 U2、UP1
  和微雪 `I2cMasterBus(scl,sda,port)`，确认 SDA=47、SCL=48、地址0x34，
  两份 XPowers 源码均严格要求寄存器0x03的 ID=0x4A。
  当前增加两目标共用的 PMIC 分层诊断、明确100kHz/50ms I2C配置及有限重试。
  这是定位版本，不是已证实的根因修复；需取得新版 ACK/ID 日志后继续。
  发布目录使用 `photopainter-v1-2026-10-02-pmic-diagnostic-1`，保留旧包但不推荐重刷。

- 2026-10-02 第二轮实机日志：诊断1 `bus=OK`，三次 `ACK=5`、
  `ID_TX=255`、`chip_id=-1`。Arduino 2.0.17中5代表超时，不能等同地址NACK，
  也没有实际读到ID。现将板内 PMIC改为微雪同类的 native I2C+XPowers回调，
  统一采用 repeated-START寄存器读取；输出原生错误、空闲电平及只读RTC总线对照。
  新目录 `photopainter-v1-2026-10-02-pmic-diagnostic-2`；这是源码差异修正和
  下一轮实机定位版本，未证实 STOP/repeated-START差异就是此次超时根因。
  刷入前应彻底断开USB和电池再上电，避免只复位ESP而不复位外设。

- 2026-10-02 第三轮实机日志：诊断2 native驱动配置/安装/滤波均成功，
  但PMIC三次读取均为 `263 / ESP_ERR_TIMEOUT`、`chip_id=-1`，同总线
  RTC读取也超时；读取前后SDA/SCL软件采样均为0。诊断2未解决启动故障，
  repeated-START差异不能作为已证实根因。已静态核对IDF4.4.7初始化会启用
  SDA/SCL输入、开漏及上拉；软件0仍不等同已量测的物理电压。
  暂不生成仅调参数的新固件、不绕过PMIC检测。下一步需确认USB和电池完全
  断开后的冷启动、实际供电与PWR开机过程，必要时用官方固件作对照。
  PMIC通信失败时无法保证EPD_VCC断电，应保存日志后断开全部供电。

- 2026-10-05再核查：本地微雪reference与官方最新HEAD一致。修正日志等级
  与深睡准备失败返回值，新增只读GPIO三阶段寄存器快照（诊断3）。两个目标
  构建及主机回归通过，PMIC实测仍未通过。微雪低功耗例程有额外GPIO21唤醒
  流程，其适用前置状态待确认。见 `source-review-2026-10-05.md` 和 `flashing.md`。

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
