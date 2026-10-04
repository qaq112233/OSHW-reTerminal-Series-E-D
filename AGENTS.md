# PhotoPainter 项目指导

## 项目事实与源码边界

- 以 Seeed SenseCraft HMI 为主固件，适配 Waveshare ESP32-S3-PhotoPainter **V1**：ESP32-S3、16MB Flash、8MB PSRAM、7.3 英寸 800×480 六色屏，目标横屏显示。
- 云端 `board.type` 使用 `xiao_diy_ee04`；这是协议别名，不代表采用 XIAO 的硬件配置。
- 主固件在 `examples/official/SenseCraft_HMI`，板级适配在其 `src/boards/waveshare_photopainter`。优先沿用当前 Board/HAL 扩展点和公共实现，保持薄适配，不复制整套应用；必要的通用修复不受限于板目录。
- `references/ESP32-S3-PhotoPainter` 是只读的 Waveshare 官方硬件参考；`tools/sensecraft-emulator` 是独立的云端协议参考与回归工具，不替代主固件或硬件验证。
- 对 Seeed 原有文件的修改记录到 `docs/photopainter/upstream-delta.md`，便于后续同步上游。

## 墨水屏供电不变量

- 只在初始化、传输和刷新所需窗口内给屏幕供电。正常流程是：上电 → 初始化与发送 framebuffer → 刷新 → 等待 BUSY 确认真正完成 → 执行面板要求的 power-off / sleep 时序并等待完成 → 关闭物理供电 rail。
- V1 原理图中，AXP2101 的 **ALDO3 → EPD_VCC**；GPIO5 不是屏幕电源开关。Controller sleep 不等于物理断电。
- 网络、下载、用户操作等待及 idle 期间不得保持屏幕供电；异常路径也必须有有界的关电处理，进入 Deep Sleep 前必须确认屏幕已断电。诊断固件同样遵守这些要求。
- PMIC 通信失败时不能宣称已关闭物理供电，也不能绕过检查继续显示或进入 Deep Sleep；应明确提示断开全部供电（含 USB 和电池）。

## 证据与按需查阅

硬件行为以匹配 V1 的 Waveshare 官方源码、原理图和文档为依据，不套用 V2 行为；无法确认的内容标为待确认，不猜测 GPIO、rail 或电源时序。构建、主机测试、模拟器和寄存器回读各有验证范围，不能代替实际电压、显示效果与整机行为的实测。

按当前任务补充上下文，无需每次通读全部文档：

- 项目状态、续接工作及历史结论：`docs/photopainter/PROJECT.md`。
- 引脚、供电、外设及硬件证据：`docs/photopainter/hardware.md`，再查对应官方参考实现。
- 构建、打包与烧录：`docs/photopainter/flashing.md`。
- Seeed 上游同步与已有改动：`docs/photopainter/upstream-delta.md`。

历史计划和诊断记录是上下文，不是固定执行脚本；根据当前源码和新证据选择最小、自然的实现。
