"""SenseCraft HMI protocol constants.

Every value here mirrors the upstream SenseCraft_HMI firmware:

  repo:   https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D
  path:   examples/official/SenseCraft_HMI
  commit: 8d97d8912987c07529287629f7e73039bb0e6384 (2026-08-31)

Source of each constant is annotated inline. When Seeed updates the upstream
protocol, diff this file against the referenced upstream functions.

Upstream mapping
  bind.py        <-> app_sensecraft.cpp::buildBindBody() / parseBindResp() / requestBind()
  mqtt_client.py <-> app_sensecraft.cpp::connectMqtt() / mqttEventHandler() / handleMqttData()
  manifest.py    <-> app_download.cpp::__parse_manifest_tasks()
  downloader.py  <-> app_download.cpp::__download_to_file() / __fetch_psram_image()
                    app_download.cpp::__image_format_from_buffer()
  main.py        <-> app_sensecraft.cpp::sendImageRefreshReq() / imgRefreshRes()
                    app_sensecraft.cpp::publishIotReport()
                    app_sensecraft.cpp::ensureSession() (bind poll loop)
"""

# --- API endpoints (src/app_config.h, HMI_TEST_ENV = 0) ---
import os

API_BASE_URL = "https://sensecraft-hmi-api.seeed.cc"
API_PATH_DEVICE_BIND = "/api/v1/device/bind"
API_PATH_REFRESH_TOKEN = "/api/v1/device/refresh_token"

# --- Firmware / protocol identity (src/app_config.h) ---
APP_VERSION = "1.1.5"          # g_currentAppVersion
CHIP_MODEL_NAME = "esp32s3"    # chip_model_name
PROTOCOL_VERSION = "0.2"       # Protocol_version

# --- Board profile (src/boards/board_registry.h) ---
# XiaoDiyEE04 + combo 509 -> { "xiao_diy_ee04", "7_3_color_800_480", "800x480", 800x480, Chromatic }
# This matches a 7.3" Spectra 6 (ED2208-class) 800x480 six-color e-paper panel.
BOARD_TYPE = "xiao_diy_ee04"
BOARD_SCREEN_TYPE = "7_3_color_800_480"
BOARD_RESOLUTION = "800x480"
# app_sensecraft.cpp::ensureSession(): g_board.img_type = "epd"
BOARD_IMG_FORMAT = "epd"
# Local image cache (emulator equivalent of LittleFS image storage).
DOWNLOAD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "downloads")
# Emulated storage capacities reported to the cloud in the IoT "Storage"
# state. The cloud rejects a playlist push with error 3032/3033 when
# flash_freeBytes / sd_freeBytes does not cover the playlist, so both are
# overridable without editing code:
#   SENSECRAFT_FLASH_BYTES=67108864 uv run python main.py
FLASH_TOTAL_BYTES = int(os.environ.get("SENSECRAFT_FLASH_BYTES", str(32 * 1024 ** 2)))
SD_TOTAL_BYTES = int(os.environ.get("SENSECRAFT_SD_BYTES", str(32 * 1024 ** 3)))
BOARD_WIDTH = 800
BOARD_HEIGHT = 480

# --- Emulated Wi-Fi (Python is not a real Wi-Fi STA; these are placeholders) ---
WIFI_SSID = "PythonEmulator"
WIFI_RSSI = -40
WIFI_CHANNEL = 1
WIFI_IP = "127.0.0.1"

# --- Timing (src/app_config.h) ---
HTTP_TIMEOUT_S = 6.0                       # HTTP_REQUEST_TIMEOUT_MS
BIND_MAX_RETRIES = 3                       # MAX_RETRIES
BIND_RETRY_DELAY_S = 5.0                   # INITIAL_BIND_API_RETRY_DELAY_MS
BIND_BACKOFF_S = 60.0                      # BIND_RETRY_BACKOFF_US
BIND_POLL_INTERVAL_S = 10.0                # BIND_POLL_US (activation poll)
MQTT_RECONNECT_INTERVAL_S = 10             # MQTT_RECONNECT_INTERVAL_MS
MQTT_KEEPALIVE_S = 60                      # keepalive
FIRST_REPORT_DELAY_S = 2.0                 # FIRST_REPORT_DELAY_US
IOT_REPORT_INTERVAL_S = 60.0               # IOT_REPORT_INTERVAL_US
IMAGE_REFRESH_MAX_RETRY = 3                # IMAGE_REFRESH_MAX_RETRY
IMAGE_REFRESH_RETRY_S = 5.0                # IMAGE_REFRESH_RETRY_US
CONTENT_VERSION_DEFAULT = "0.0"            # app_device_info.cpp::GetContentVersion() default

# --- MQTT message types (app_sensecraft.cpp::handleMqttData()) ---
MSG_TYPE_IOT = "iot"
MSG_TYPE_IMG_FLASH = "img_flash"
MSG_TYPE_IMG_FLASH_RES = "img_flash_res"
MSG_TYPE_ALBUM = "album"

# Downlink cloud commands (app_sensecraft.cpp::handleCloudCommand())
IOT_CMD_INTERVAL = ("DataAccess", "SetInterval")
IOT_CMD_DEEPSLEEP = ("Power", "SetDeepSleep")

# Device status codes (app_config.h)
DEVICE_ONLINE = 1
DEVICE_OFFLINE = 2
DEVICE_SLEEP = 3

# --- Image format magic bytes (app_download.cpp::__image_format_from_buffer()) ---
EPD_MAGIC = b"EPD0"
EPD_MODE_NAMES = {0: "BW (1-bit)", 1: "Gray4", 2: "Color6/4", 3: "Gray16"}
