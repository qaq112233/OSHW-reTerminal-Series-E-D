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
