# PhotoPainter 显示诊断（无云端）

对应构建目标 `waveshare_photopainter_bringup`；主固件目标为
`waveshare_photopainter`。此诊断程序不启动 Wi-Fi、MQTT 或 SenseCraft APP，
不依赖 SD 卡。构建命令（在 `examples/official/SenseCraft_HMI` 运行）：

```bash
pio run -e waveshare_photopainter_bringup
```

程序开机显示白色，按 KEY（GPIO4）依次切换黑、红、黄、蓝、绿和
0°/90°/180°/270° 四种旋转布局。每页含边框、交叉线、文字和分辨率；
串口（115200）显示当前步骤。每次刷新都经 Seeed_GFX ED2208
`update()` 自动完成 POWER_ON -> 图像 -> DISPLAY_REFRESH -> BUSY ->
POWER_OFF(0x02) -> BUSY，等待按键前已执行控制器关电；初始化后也
主动同步驱动状态并执行一次控制器关电。

## 未完成的验收

无实物，本阶段只检查编译和源码调用链；六色调色板、方向/镜像、BUSY
极性、刷新时间、异常超时、供电 rail 真实掉电、电池与功耗**均未实测**。
如 BUSY 不释放，当前 Seeed_GFX 会一直等待；遇到此情况应先断电检查
硬件连接，而不是让显示屏持续保持供电进行调试。绝不以本文件宣称真机通过。

主固件现复用 Seeed E1002 的 `521` 横屏 UI；诊断程序仍逐一遍历旋转值。
Waveshare 示例默认 rotation 0，实际装机方向和镜像待真机确认。
