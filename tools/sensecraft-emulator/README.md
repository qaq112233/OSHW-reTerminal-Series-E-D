# SenseCraft HMI Python 设备模拟器

这是一个独立的 Python 工具，用来模拟 `xiao_diy_ee04` + 7.3 英寸
800×480 六色电子纸设备，验证 SenseCraft HMI 云端的完整链路：

```text
bind -> Pair Code -> 网页绑定 -> MQTT -> img_flash -> manifest
     -> 图片下载 -> img_flash_res
```

本阶段只验证协议和云端行为，不涉及 ESP32 固件、GPIO、SPI、PMIC、
电子纸驱动或 Seeed_GFX。

完整验收记录见 `REPORT.md`。

## 当前结论

完整链路已经实测通过（PASS）：

- Pair Code 能正常获得，网页可以完成绑定；
- MQTT TLS 连接和订阅成功；
- `img_flash` 请求得到 `manifest_url`；
- manifest 和图片下载成功；
- 云端图片格式为 `EPD0`（EPD）；
- 分辨率 800×480，4-bit，Color6/4；
- `img_flash_res` 已按图片上报 100% 和 `img_apply=true`；
- Canvas 修改后，模拟器能够实时收到新的资源推送。

## 上游参考

| 项目 | 值 |
|---|---|
| 仓库 | https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D |
| 路径 | `examples/official/SenseCraft_HMI` |
| Commit | `8d97d8912987c07529287629f7e73039bb0e6384` |
| 参考日期 | 2026-08-31 |

协议常量集中放在 `protocol.py`，每个常量都标注了对应的上游文件。
Seeed 更新源码后，优先 diff 这个文件和相关函数。

## 环境要求

- Python 3.11+
- [uv](https://docs.astral.sh/uv/)

```bash
cd /home/newphotopainter/tools/sensecraft-emulator
export UV_CACHE_DIR=/tmp/uv-cache
uv sync
```

依赖只有 `requests`、`paho-mqtt` 和 `certifi`，没有 Web UI、数据库、
Docker 或异步框架。

## 运行

```bash
cd /home/newphotopainter/tools/sensecraft-emulator
export UV_CACHE_DIR=/tmp/uv-cache

uv run python main.py                 # 常驻运行，监听 Canvas 新推送
uv run python main.py --once          # 完成第一轮下载后退出
uv run python main.py --debug         # 打印协议细节
uv run python main.py --debug-secrets # 打印完整 token（谨慎使用）
uv run python main.py --insecure      # 仅在 TLS 排障时使用
uv run python main.py --reset-device  # 生成新模拟设备，需要重新 Pair
```

首次运行会打印 Pair Code，例如：

```text
=====================================
 SenseCraft Pair Code: 123456
=====================================
 Bind URL: https://sensecraft.seeed.cc/hmi
```

在浏览器中打开绑定地址、输入 Pair Code 后，模拟器会自动继续连接 MQTT。

## 设备身份和持久化

- 第一次运行生成一个本地管理、单播的模拟 MAC：`02:xx:xx:xx:xx:xx`；
- 之后从 `device.json` 读取同一个 MAC；
- `device.json`、`downloads/`、虚拟环境和缓存均已加入 `.gitignore`；
- `device.json` 只保存模拟身份的本地状态，不保存 MQTT 密码；
- 每次启动都会重新调用 bind 获取最新会话凭据。

默认不会打印完整 token。只有显式使用 `--debug-secrets` 才会输出完整凭据。

## Bind

### 请求

`POST https://sensecraft-hmi-api.seeed.cc/api/v1/device/bind`

请求体由 `bind.py::build_bind_body()` 构造，对应上游
`app_sensecraft.cpp::buildBindBody()`：

```json
{
  "mac_address": "02:9B:44:5A:62:DB",
  "chip_model_name": "esp32s3",
  "version": "1.1.5",
  "board": {
    "type": "xiao_diy_ee04",
    "screen_type": "7_3_color_800_480",
    "resolution": "800x480",
    "ssid": "PythonEmulator",
    "rssi": -40,
    "channel": 1,
    "ip": "127.0.0.1",
    "mac": "02:9B:44:5A:62:DB",
    "img_format": "epd"
  }
}
```

`ssid`、`rssi`、`channel`、`ip` 是模拟占位值，不表示真实网络。

### 响应

实际会拿到 `server_time`、`activation`、`mqtt` 和 `firmware`：

```json
{
  "code": 200,
  "result": {
    "firmware": {"url": "", "version": "1.1.5"},
    "server_time": {
      "timestamp": 1790000000000,
      "timezone": "Asia/Shanghai",
      "timezone_offset": 480
    },
    "mqtt": {
      "endpoint": "watcher-agent-mqtt-broker.seeed.cc:8883",
      "client_id": "02_9B_44_5A_62_DB",
      "username": "sensecraft-hmi|02_9B_44_5A_62_DB",
      "password": "<JWT/cloud token>",
      "publish_topic": "sensecraft-hmi/server/02_9B_44_5A_62_DB",
      "subscribe_topic": "sensecraft-hmi/devices/02_9B_44_5A_62_DB"
    },
    "activation": {"code": 123456, "message": "https://sensecraft.seeed.cc/hmi"}
  },
  "message": "success"
}
```

判定规则：

- `activation.code != 0`：设备尚未被网页绑定，继续按约 10 秒轮询；
- `activation.code == 0`：绑定完成，可以连接 MQTT；
- HTTP 非 200、非 JSON、缺少 `result`：按 bind 失败处理并打印诊断信息。

### MQTT client_id 的 ACL 特性

这是实测发现、会直接影响能否订阅成功的关键点：

1. bind 返回的 `client_id` 使用下划线形式，例如 `02_9B_44_5A_62_DB`；
2. 从实测现象看，broker 的设备 ACL 对 `client_id` 字符串高度敏感，并不会
   对同一个 MAC 的三种写法自动归一化；
3. 同一个设备可能出现下划线、冒号或纯十六进制三种形式；
4. 如果字符串不匹配，表现可能是：
   - `CONNACK 135`（Not authorized）；或
   - 连接成功但订阅得到 `SUBACK 0x80`；
5. 因此模拟器按以下顺序尝试，并要求“连接成功 + SUBACK 成功”才算就绪：

```text
bind 返回的 client_id
  -> MAC 冒号形式（02:9B:44:5A:62:DB）
  -> MAC 纯十六进制形式（029B445A62DB）
```

这段逻辑位于 `mqtt_client.py::connect_blocking()` 和
`main.py::Emulator.run_mqtt()`。旧设备如果连 ACL 对应的第一个字符串也已经失效，
只能重新生成设备并重新 Pair，不能靠修改空闲空间绕过。

## MQTT 协议

TLS 使用系统 CA（certifi）验证；端口从 bind 响应解析，默认是 `8883`。
只有 `--insecure` 会关闭校验。

### 上行：请求最新内容

`main.py::build_img_flash()` 对应
`app_sensecraft.cpp::sendImageRefreshReq()`：

```json
{
  "version": "0.2",
  "session_id": "1234567890",
  "type": "img_flash",
  "timestamp": 1790000000000,
  "data": {"version": "20239858"}
}
```

### 下行：Manifest 或 Canvas 更新

`img_flash` / `album` 的核心字段：

```json
{
  "version": "0.2",
  "session_id": "1234567890",
  "type": "img_flash",
  "timestamp": 1790000000000,
  "data": {
    "version": "20239858",
    "manifest_url": "https://..."
  }
}
```

实测协议差异：云端把 `session_id` 放在消息顶层，而上游代码从
`data.session_id` 读取。模拟器同时接受两种位置，避免把同一条消息误判为
重复或丢失。

### 上行：IoT 状态

`main.py::build_iot_report()` 对应
`app_sensecraft.cpp::publishIotReport()` 和 `src/APP/iot.h`。它会报告：

- `Storage.state.flash_freeBytes`
- `Storage.state.sd_freeBytes`
- `SD.state.is_inserted` / `is_mounted` / `is_ready`
- `Devicestatus.state.status`
- `Buttons`、`Battery`、`Power`、`Sensor` 等模拟占位状态

第一次 IoT report 在 MQTT 连接后约 2 秒发送，之后每 60 秒发送一次。

### 上行：下载结果

`main.py::build_img_flash_res()` 对应
`app_sensecraft.cpp::imgRefreshRes()`：

```json
{
  "version": "0.2",
  "session_id": "1234567890",
  "type": "img_flash_res",
  "timestamp": 1790000000000,
  "data": {
    "version": "20239858",
    "img_id": "99e7486a_2026_26150_20239858_3_800x480",
    "img_apply": true,
    "img_progress": 100,
    "img_total": 2,
    "img_index": 0
  }
}
```

每张图片都有进度 20/40/60/80% 和最终 100%；`img_apply=true` 只表示模拟器
已经成功接收并“应用”资源，不表示真实电子纸已经刷新。

## 云端存储检查（3032 / 3033）

云端推送播放列表前会读取该设备最近一次 IoT report 中缓存的存储值，而不是
当前 MQTT 连接刚建立时的最新值。若数值为 0，推送会被拒绝：

```json
{
  "code": 3032,
  "message": "Device flash storage is not enough for the playlist: playlist size: 55679 bytes, available flash: 0 bytes"
}
```

当前模拟器明确模拟一块已插入、已挂载、可用的存储介质：

| 项目 | 默认值 | 环境变量 |
|---|---:|---|
| Flash 总容量 | 32 MiB | `SENSECRAFT_FLASH_BYTES` |
| SD 总容量 | 32 GiB | `SENSECRAFT_SD_BYTES` |

已用空间按 `downloads/*.epd` 的实际字节数计算，因此报告值是：

```text
freeBytes = totalBytes - sum(downloads/*.epd)
```

例如：

```bash
SENSECRAFT_FLASH_BYTES=67108864 \
SENSECRAFT_SD_BYTES=34359738368 \
uv run python main.py
```

修改容量后必须先让云端收到一次非零的 `Storage` 状态，再在网页推送播放列表。
代码也对 SD 路径预期错误 `3033` 做了相同处理；本次实测触发的是 `3032`。

## Manifest

`manifest.py` 对应 `app_download.cpp::__parse_manifest_tasks()`。请求头使用
`authorization: <mqtt.password>`，原始 JSON 保存到 `downloads/manifest.json`：

```json
{
  "album_id": "20239858",
  "images": [
    {
      "id": "99e7486a_2026_26150_20239858_3_800x480",
      "index": 0,
      "url": "https://sensecraft-hmi-api.seeed.cc/render/img/02:9B:44:5A:62:DB/26150"
    }
  ],
  "version": 2
}
```

- `/render/img/` 表示静态图片；
- `/render/layout/` 表示动态 Canvas；
- URL 类型只说明资源种类，不能用来判断实际文件格式；
- 实际格式始终通过文件 magic bytes 判断。

## 图片和 EPD

`downloader.py` 支持按 magic bytes 识别：

| 格式 | Magic |
|---|---|
| BMP | `42 4D` |
| PNG | `89 50 4E 47 0D 0A 1A 0A` |
| EPD | `45 50 44 30`（`EPD0`） |

文件保存到 `downloads/<image_id>.<ext>`。本次云端实际返回的是 EPD：

- 800×480；
- 16 字节 EPD0 header；
- `bit_depth=4`；
- `mode=2`（Color6/4）；
- header 后是 zlib 压缩的 4-bit 索引像素。

### Color6 调色板不是 RGB 顺序

这是图片“反色”问题的根因。上游 `src/utils/epd_color_map.h` 的
`MAP_COLOR6` 定义为：

| 索引 | 颜色 |
|---:|---|
| `0x00` | 白 |
| `0x0F` | 黑 |
| `0x06` | 红 |
| `0x02` | 绿 |
| `0x0D` | 蓝 |
| `0x0B` | 黄 |

如果按“0=黑、F=白”或普通 RGB 灰度理解，就会得到整张图反色。解码时必须
使用上表。`decode_epd.py` 已经使用正确调色板：

```bash
uv run --with pillow python decode_epd.py downloads/<file>.epd
```

不指定输出路径时会在同目录生成同名 PNG，方便直接检查白底图片。

## 上游文件对应关系

| 本项目 | 上游 |
|---|---|
| `protocol.py` | `src/app_config.h`、`src/boards/board_registry.h` |
| `device.py` | `app_sensecraft.cpp::ensureSession()` |
| `bind.py` | `app_sensecraft.cpp::buildBindBody()` / `parseBindResp()` / `requestBind()` |
| `mqtt_client.py` | `app_sensecraft.cpp::connectMqtt()` / `mqttEventHandler()` / `handleMqttData()` |
| `manifest.py` | `app_download.cpp::__parse_manifest_tasks()` |
| `downloader.py` | `app_download.cpp::__fetch_psram_image()` / `__image_format_from_buffer()` |
| `main.py` | `sendImageRefreshReq()` / `imgRefreshRes()` / `publishIotReport()` / `ensureSession()` |
| `decode_epd.py` | `src/utils/epd_color_map.h`、`web_tools/hmi_image_get_epd.html` |

## 与固件的有意差异

1. 不连接真实硬件；Wi-Fi、电池、SHT40、SD、按键等都是模拟值。
2. IoT report 使用 wall-clock 毫秒，固件使用 `millis()`。
3. TLS 使用系统 CA，而不是固件内固定的 GoDaddy G2 根证书。
4. 只处理实际需要的 `img_flash`、`album`、`iot`；未知类型只记录日志。
5. 云端 `session_id` 的顶层位置被兼容处理，见上文协议差异。
6. `client_id` 会按 bind 值、冒号 MAC、纯十六进制 MAC 依次尝试 ACL。

## 产物和清理

```text
device.json                      模拟 MAC 和内容版本（git-ignored）
downloads/manifest.json          最近一次原始 manifest（git-ignored）
downloads/<id>.epd|.png|.bmp     下载内容（git-ignored）
```

这些是运行产物，不属于源码。删除 `downloads/` 不会影响协议实现，但会清空
本地图片样例；删除 `.venv/` 后下次 `uv run` 会重新创建。不要删除
`uv.lock`，它用于锁定依赖版本。

## 验收状态

- [x] bind API 可访问
- [x] `xiao_diy_ee04` profile 被接受
- [x] 获得 Pair Code
- [x] SenseCraft 网页完成 Pair
- [x] `activation.code` 变为 0
- [x] 获取 MQTT credentials
- [x] MQTT TLS 连接成功
- [x] subscribe 成功
- [x] `img_flash` publish 成功
- [x] 收到 `manifest_url`
- [x] manifest 下载和解析成功
- [x] 图片下载成功
- [x] 识别为 EPD，800×480
- [x] `img_flash_res` 成功发送
- [x] Canvas 修改后实时收到新资源

## Go / No-Go

当前验证结果是 **PASS**：协议层可以继续作为 PhotoPainter 移植的参考。
PhotoPainter 侧可以优先复用：

- bind 请求和会话解析；
- MQTT topic / message 格式；
- 基于 cloud token 的 manifest 和图片下载；
- magic-byte 格式探测；
- EPD0 header 和 Color6 解码表。

实体屏幕刷新、供电、SPI、PMIC 和掉电恢复仍属于后续 ESP32 阶段，不能由本
模拟器的 PASS 结果代替。
