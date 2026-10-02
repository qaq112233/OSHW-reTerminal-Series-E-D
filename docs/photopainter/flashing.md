# PhotoPainter V1：ESP Launchpad 首次烧录

## 适用范围与警告

仅适用于用户确认的 **Waveshare ESP32-S3-PhotoPainter V1，16MB Flash / 8MB PSRAM**。
这是已通过构建与离线文件检查的测试固件，不是已经完成真机验收的量产固件。

**本包两个 `*-full.bin` 都是 16MB 完整初次烧录镜像，从 `0x0` 写入。
写入任何一个都会覆盖板内全部 Flash：原固件、Wi-Fi/配对设置、图片缓存和 OTA 状态都会重置。**
若需保留旧资料，请在烧录前另行备份；不要把此包当成保留数据的 OTA 更新包。
新固件的 EPD 供电保护仅在新固件正常启动后运行，**不在 ROM 下载/烧录阶段运行**。
进入下载模式前先等旧固件的刷新结束；成功烧录后尽快正常启动新固件，不长期停在下载模式。

烧录包本身不改写 SD 卡，也不写 eFuse。已有 Secure Boot / Flash Encryption
配置的设备不适用此通用明文镜像；不要为了烧录去修改 eFuse。

## 文件选择

| 文件 | 用途 | ESP Launchpad 地址 |
|---|---|---|
| `photopainter-v1-bringup-full.bin` | 离线六色/方向诊断，无 Wi-Fi/MQTT | **`0x0`** |
| `photopainter-v1-sensecraft-full.bin` | SenseCraft 配网、Pair、MQTT、图片链路 | **`0x0`** |

一次只选择一个 `.bin`。ZIP 要先解压，不能把 ZIP、JSON、校验文件当固件上传。
**不要把 `.pio/build/.../firmware.bin` 单独放到 `0x0`**；那个只是应用部分。

包内 `manifest.json` 记录源码提交、分区、输入文件哈希、两个整包哈希。
`SHA256SUMS.txt` 可用于核对解压后的文件，另有合并/镜像检查及构建日志。
可在 Windows PowerShell 中运行 `Get-FileHash .\photopainter-v1-bringup-full.bin -Algorithm SHA256`
并与清单对应行比较。校验值一致仅证明文件未改变，不代表硬件测试通过。

## 先刷诊断版

1. 使用电脑上的 Chrome / Edge 打开 <https://espressif.github.io/esp-launchpad/>。
   进入 **DIY**，不是选择内置的 RainMaker/Matter 示例。
2. 用支持数据传输的 USB 线连接板子，关闭串口监视器、Arduino IDE 等占用串口的软件。
3. 点击连接并允许访问对应串口。确认检测到 **ESP32-S3**；若识别成其他芯片则停止。
4. 添加 `photopainter-v1-bringup-full.bin`，地址输入 **`0x0`**。
5. Settings 中烧录速度建议 **460800**；若不稳定降为 **115200**。
   Console baud rate 使用 **115200**。无需改 Flash mode/frequency。
6. 点击 Flash，等到整个过程成功。中途不要拔线或让电源掉电。
   此镜像已覆盖全部 16MB，**无需再单独点击 Erase Flash**。
7. 完成后确保 BOOT 已松开，执行 Reset / 让整板重新上电。
   若重置时 USB 串口消失，重新选择设备并打开页面的 Console，波特率 115200。

### 连接失败时

先换数据线/USB 端口，并确认串口没有被其他应用占用。
自动进入下载模式失败时：**按住 BOOT（GPIO0），让整板复位或重新上电，
连接/同步到下载模式后松开 BOOT**。不要假定 PWR 就是 RESET。
板上电池仍在供电时，仅拔插 USB 不一定构成整板重新上电。
如果仍然失败，保留 Launchpad 控制台的完整错误，不要尝试修改 eFuse。

### 诊断预期

启动后显示白底图，包含边框、文字、交叉线；默认横屏 rotation 2。
每次刷新结束之后，再按 KEY（GPIO4），依次显示黑、红、黄、蓝、绿，
随后是 rotation 2/3/0/1 的方向测试，再循环。旋转测试中的竖屏页是预期行为。

每页成功后的串口消息类似：

```text
[Bring-up] pattern 1 done; controller POWER_OFF, EPD_VCC rail off; press KEY for next
```

日志表示程序完成控制器关电并回读 ALDO3 为关闭，**不等于量测 J1 VDD 为 0V**。
墨水屏断电后图像仍会保留，不能用“图像仍在”判断有没有供电。
有条件时应量测 EPD_VCC/J1 VDD 并检查 GPIO 信号反向供电。

若出现 PMIC 初始化失败、ALDO3 关闭失败、BUSY 超时或反复重启，先断开整板供电
（注意电池供电），保存日志并反馈。不要把 180 秒故障保护当成正常刷新时间，也不要
故意在正常刷新中途断电。故障保护会尝试切电重启，但无法修复 PMIC/I2C 硬件故障。

## 再刷 SenseCraft 正式版

六色/方向检查无异常后，再按相同过程写入
`photopainter-v1-sensecraft-full.bin`，仍然从 **`0x0`**。
这次也是完整重置，不保留诊断版或旧固件数据。

重启后继续原 SenseCraft 配网/配对流程；云端设备标识仍为 `xiao_diy_ee04`，
不是 Waveshare 原示例的联网流程。初次启动会格式化并挂载空 LittleFS；
等待配对、MQTT、图片下载时 EPD rail 应为关闭，仅实际显示更新时上电。

若页面/网络未工作，请保留从启动开始的 115200 串口日志，至少包含 PMIC、
文件系统、固件环境、Wi-Fi 与 Pair/MQTT 报错。没有这些日志，不能把所有异常归因于屏幕。
当前 RTC、SHTC3、SDMMC 等仍未做完整板级适配，不宣称外设与低功耗真机全部通过。

## 分区依据与合并检查

来自本项目 `partitions_diykit_new.csv` 与每个目标的实际 `partitions.bin`：

| 组件 | 整包内地址 | 说明 |
|---|---|---|
| ESP32-S3 bootloader | `0x000000` | 不套用 ESP32 的 `0x1000` |
| 分区表 | `0x008000` | 固定 Arduino 构建器的分区表地址 |
| OTA 初始化 | `0x08D000` | Arduino `boot_app0.bin` 的 sequence=1 记录，CRC 已检查，选择 app0 |
| 应用 app0 | `0x090000` | 源码配置与实际分区表一致 |
| LittleFS/spiffs 分区 | `0xA90000` | 不预装资源，填 FF；主固件 `LittleFS.begin(true)` 创建 |

Arduino 默认构建器把 `boot_app0.bin` 放到 `0xE000`，但这**不是本项目 otadata 地址**。
打包时将这个不含绝对地址的 OTA 初始化记录放在实际 `0x8D000`，不在默认地址留下记录。
其余未写入区域均为 `0xFF`，包括 NVS、备用 app1 与文件系统。

打包器检查：分区表 MD5、地址/容量/重叠、ESP32-S3 芯片 ID、16MB 镜像头、
OTA sequence/CRC、应用大小；合并后逐段与输入文件比较，并检查空白区；
同时使用官方 esptool 读取镜像校验信息，并对 ZIP 内容回读验证。
这些是静态文件检查，不会连接或烧录设备，不替代刷新/供电实测。

## 重新生成（开发者）

在仓库根目录运行，先完成当前两个目标的构建，再打包；不要只复用过期 `.pio` 文件。
以下例子将工具缓存留在本地 `.build`，避免 WSL 重启后 `/tmp` 清空而丢失工具链。

```bash
export PLATFORMIO_CORE_DIR="$PWD/.build/platformio-core"
export UV_CACHE_DIR="$PWD/.build/uv-cache"
export UV_TOOL_DIR="$PWD/.build/uv-tools"
uvx --with pip --from 'platformio==6.1.19' platformio run \
  --project-dir examples/official/SenseCraft_HMI \
  -e waveshare_photopainter -e waveshare_photopainter_bringup
uv run --no-project --with 'platformio==6.1.19' --with pip python \
  tools/photopainter_flash_package.py \
  --core-dir "$PLATFORMIO_CORE_DIR" \
  --output-dir "$PWD/build/photopainter-v1-RELEASE_ID"
```

固件源文件或配置有未提交变更时拒绝打包，已存在的发布目录也不会被覆盖。
编译工具的安装需要 pip，因此示例显式带上 `--with pip`；普通已安装 PlatformIO
环境也可以直接调用 `pio` 与其 Python 环境下的打包脚本。

官方工具参考：

- ESP Launchpad DIY：<https://github.com/espressif/esp-launchpad/blob/main/README.md>
- ESP Launchpad 烧录逻辑：<https://github.com/espressif/esp-launchpad/blob/main/js/index.js>
- esptool merge_bin：<https://docs.espressif.com/projects/esptool/en/release-v4/esp32s3/esptool/basic-commands.html>
- Arduino 2.0.17 构建器：<https://github.com/espressif/arduino-esp32/blob/2.0.17/tools/platformio-build.py>
