# SenseCraft HMI Python 模拟器验收报告

本文是第一阶段 Python 协议模拟的交付报告。详细协议说明见 `README.md`。

## 1. 创建的文件

```text
tools/sensecraft-emulator/
├── README.md
├── REPORT.md
├── pyproject.toml
├── uv.lock
├── .gitignore
├── protocol.py
├── device.py
├── bind.py
├── mqtt_client.py
├── manifest.py
├── downloader.py
├── decode_epd.py
└── main.py
```

运行产物（不属于源码，已 git-ignore）：

```text
device.json
downloads/manifest.json
downloads/<id>.epd
downloads/<id>.png       # decode_epd.py 生成的可视化检查图
```

## 2. 上游 commit

```text
Seeed-Projects/OSHW-reTerminal-Series-E-D
examples/official/SenseCraft_HMI
8d97d8912987c07529287629f7e73039bb0e6384
2026-08-31
```

## 3. 实际 bind request

`POST https://sensecraft-hmi-api.seeed.cc/api/v1/device/bind`

```json
{
  "mac_address": "02:xx:xx:xx:xx:xx",
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
    "mac": "02:xx:xx:xx:xx:xx",
    "img_format": "epd"
  }
}
```

## 4. 实际 bind response

实际结构包含：

```text
code
message
result.firmware
result.server_time
result.activation.code / message
result.mqtt.endpoint / client_id / username / password
result.mqtt.publish_topic / subscribe_topic
```

未绑定时 `activation.code != 0` 且网页无法下发图片；绑定完成后
`activation.code == 0`，返回 MQTT 会话配置。

## 5. Pair Code

成功获得。终端会打印：

```text
=====================================
 SenseCraft Pair Code: <code>
=====================================
 Bind URL: https://sensecraft.seeed.cc/hmi
```

## 6. 网页 Pair

成功。网页输入 Pair Code 后，后续 bind 轮询返回 `activation.code == 0`。

## 7. MQTT

成功。使用 bind 响应中的 endpoint、username、password、topic 建立 TLS 连接，
订阅成功，等待期间能收到云端下推。

实测现象是 broker ACL 对 `client_id` 字符串高度敏感，同一个 MAC 的不同写法
不会自动归一化。模拟器会依次尝试：

```text
bind 返回的 client_id（下划线）
-> 冒号格式 MAC
-> 纯十六进制 MAC
```

只有 `CONNACK == 0` 且订阅返回非 `0x80` 才认为 MQTT 就绪。

## 8. Manifest

成功。收到 `manifest_url` 后使用 `authorization: <mqtt.password>` 请求，
原始 JSON 保存到 `downloads/manifest.json`。

本次验证的 manifest：

```text
album_id = 20239858
schema version = 2
images = 2
```

## 9. 图片下载

成功。两张图片都下载完成，并分别发送了 20/40/60/80/100% 进度和
`img_apply=true` 的 `img_flash_res`。

本次文件：

```text
99e7486a_2026_26150_20239858_3_800x480.epd  20535 bytes
26e28710_2026_26151_20239858_3_800x480.epd  79321 bytes
```

## 10. 实际原格式

不是 BMP，也不是 PNG，而是 `EPD0`：

```text
EPD0
16-byte header
bit_depth = 4
mode = 2 (Color6/4)
zlib payload = 192000 bytes
```

## 11. 图片尺寸

```text
800 × 480
```

两张图片的 EPD header 都是 800×480，解码后的 PNG 也是 800×480。

## 12. img_flash_res

成功。每张图片最终上报：

```json
{
  "version": "0.2",
  "type": "img_flash_res",
  "data": {
    "version": "20239858",
    "img_id": "<image id>",
    "img_apply": true,
    "img_progress": 100,
    "img_total": 2,
    "img_index": 0
  }
}
```

## 13. Canvas 修改后的实时推送

成功。保持模拟器常驻运行后，在 SenseCraft Canvas 修改内容，云端会主动推送新的
`img_flash` / `album` 消息；模拟器重新下载 manifest 和 EPD 图片，并再次上报
`img_flash_res`。

## 14. 协议差异和维护要点

1. `session_id` 实测位于消息顶层，而上游部分代码从 `data.session_id` 读取；
   模拟器兼容两种位置。
2. 云端播放列表预检使用缓存的 `Storage` 状态，flash/SD 为空时会返回
   `3032`（本次实测）或 `3033`。必须先让云端收到非零存储状态。
3. `client_id` ACL 与 MAC 的归一化形式不一致，需按候选字符串重试。
4. Color6 调色板不是黑到白的普通顺序：`0x00` 是白色、`0x0F` 是黑色，
   其余为红/绿/蓝/黄。按普通灰度或 RGB 顺序解码会造成“图片反色”。
5. EPD 下载 URL 可能是 `/render/img/` 或 `/render/layout/`，格式必须靠 magic
   bytes 判断，不能靠 URL 或 `Content-Type`。
6. 旧设备如果 ACL、绑定状态或云端记录已经异常，重新生成一个模拟设备并重新
   Pair 比继续修复旧记录更可靠。

## 15. PhotoPainter 可直接复用的部分

可以直接复用或作为实现参照：

```text
bind 请求体与会话解析
MQTT topic 和上下行消息格式
cloud token 授权
manifest 拉取和图片 URL 选择
BMP / PNG / EPD magic-byte 检测
EPD0 header 解析
Color6 调色板
IoT Storage / SD 状态字段
```

不应把本报告的 PASS 当成硬件验证结论。实体屏幕刷新、SPI、PMIC、供电、
掉电恢复和长期稳定性仍需 ESP32 阶段单独验证。

## 运行命令

```bash
cd /home/newphotopainter/tools/sensecraft-emulator
export UV_CACHE_DIR=/tmp/uv-cache

uv run python main.py
uv run python main.py --once
uv run python main.py --debug
uv run python main.py --debug-secrets
uv run python main.py --reset-device
uv run --with pillow python decode_epd.py downloads/<file>.epd
```

## Git 状态说明

`/home/newphotopainter` 当前不是可用 Git 仓库（`.git` 不是有效的 Git
工作树），因此无法生成原始任务要求的 `git diff --stat`。本项目目录和文件均为
实际整理后的交付物；如需要版本历史，应把
`/home/newphotopainter/tools/sensecraft-emulator` 放入真实 Git 仓库后再提交。
