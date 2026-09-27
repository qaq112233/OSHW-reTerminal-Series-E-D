# PhotoPainter 显示诊断（无云端）

对应构建目标 `waveshare_photopainter_bringup`；主固件目标为
`waveshare_photopainter`。此诊断程序不启动 Wi-Fi、MQTT 或 SenseCraft APP，
不依赖 SD 卡。构建命令（在 `examples/official/SenseCraft_HMI` 运行）：

```bash
pio run -e waveshare_photopainter_bringup
```

程序开机以与 Waveshare 800×480 BMP 相同的 rotation 2 显示白色，
按 KEY（GPIO4）依次切换黑、红、黄、蓝、绿和 rotation 2/3/0/1
四种旋转布局。每页含边框、交叉线、文字和分辨率；
串口（115200）显示当前步骤。每次刷新都经 Seeed_GFX ED2208
`update()` 自动完成 POWER_ON -> 图像 -> DISPLAY_REFRESH -> BUSY ->
POWER_OFF(0x02) -> BUSY，等待按键前已执行控制器关电并关闭 V1 AXP2101 ALDO3，初始化后也
主动同步驱动状态、关控制器并关闭 ALDO3。若显示驱动的 BUSY 无限等待，
板级故障任务超过 180 秒会尝试关闭 ALDO3 并重启。

## 未完成的验收

无实物，本阶段只检查编译和源码调用链；六色调色板、方向/镜像、BUSY
极性、刷新时间、故障保护的有效性、供电 rail 真实掉电、电池与功耗
**均未实测**。故障时强制切电不等于正常 BUSY 完成；PMIC 通信失败时软件重启，也无法保证切电。实机应量测 J1 VDD 与信号线反灌，不可只看串口日志。

主固件现复用 Seeed E1002 的 `521` 横屏 UI；诊断程序仍逐一遍历旋转值。
Waveshare 对 800×480 BMP 明确使用 rotation 2，现作为主固件及诊断的
默认横屏方向；六色编码与面板索引在源码层面已对齐。实际装机方向、
光学颜色和镜像仍需目视确认。
