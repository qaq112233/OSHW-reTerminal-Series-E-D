# SenseCraft MVP 软件接入状态

主固件目标：`waveshare_photopainter`。它沿用 Seeed 原有的
Wi-Fi → Pair → MQTT → Manifest → 下载 → 渲染逻辑，硬件入口为
`src/boards/waveshare_photopainter`；云端 `board.type` 使用已由
`tools/sensecraft-emulator` 验证的 `xiao_diy_ee04`，屏幕类型为
`7_3_color_800_480`、分辨率 `800x480`，本地 Board 身份不变。

构建（`examples/official/SenseCraft_HMI` 下）：

```bash
pio run -e waveshare_photopainter
c++ -std=c++11 -Isrc tests/photopainter_registry.cpp -o /tmp/photopainter_registry
/tmp/photopainter_registry
```

**这是静态/主机构建验证，不是端到端云端或真机验收。**
没有硬件，无法证明芯片 PSRAM 初始化、PMIC 电源、Pair/MQTT 登录、
下载存储、800×480 六色、BUSY 完成或真实供电掉电。Seeed_GFX 的
正常更新末尾会执行 `POWER_OFF(0x02)` 和 BUSY 等待；目前也没有
BUSY 超时及物理 rail 传感，后续真机测试必须先验证供电和刷新。

第一版不提供 SD 卡、RTC、SHTC3、电池读数；已有 HAL 的空设备返回值
在无外设情况下允许主固件运行，图像仍使用 LittleFS 缓存。云端实际
是否接受兼容 `board.type` 尚未通过 PhotoPainter 真机验证。

## 已接入的休眠差异（仅软件验证）

PhotoPainter 使用 ED2208 combo `521` 的 800×480 横屏布局；定时唤醒
对 0–20 秒输入钳制至最短 1 秒，避免无符号下溢。PWR GPIO5 高有效，
不加入 EXT1 ANY_LOW 唤醒掩码。进入 Deep Sleep 前在共享 SPI 锁内
再次调用板级显示 sleep；充电状态的 AXP2101 适配见下文；实机供电与休眠行为尚未验证。

AXP2101 现通过 PhotoPainter 板级实现接入 XPowersLib；云端电池状态与
Seeed 的充电时保持唤醒判定可以读到芯片状态。USB 输入视作外部供电，
即使电池已满也沿用 Seeed 的“不自动休眠”语义。芯片缺失或读取失败时
状态不可用，PMIC 与电池计量都需要实机核对。未为显示屏猜测 ALDO
供电关系，也未证明物理 rail 断开。
