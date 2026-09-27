#!/usr/bin/env python3
"""SenseCraft HMI Python device emulator.

Usage:
    uv run python main.py [--reset-device] [--debug] [--debug-secrets]
                          [--insecure] [--once]

Validates the full cloud chain for a 7.3" 800x480 color (xiao_diy_ee04) DIY
device: bind -> pair -> MQTT -> manifest -> image download -> img_flash_res.

No ESP32 hardware involved; this is protocol-layer validation only.
"""

from __future__ import annotations

import argparse
import os
import random
import sys
import threading
import time

import requests

import bind
import device
import downloader
import manifest
import mqtt_client
import protocol as P


def now_ms() -> int:
    """app_sensecraft.cpp::timestampMs() (wall clock, ms)."""
    return int(time.time() * 1000)


def session_id() -> str:
    """app_sensecraft.cpp::sessionId(): a random uint32 as a decimal string."""
    return str(random.randrange(1 << 32))


def emulated_used_bytes() -> int:
    """Bytes the emulator charges against flash/SD: the images it downloaded.

    The firmware reports free space as totalBytes() - usedBytes() for a real
    LittleFS partition and a real MicroSD card. Python has neither, so the
    local downloads/ directory stands in for "images stored on the device".
    """
    try:
        return sum(
            os.path.getsize(os.path.join(P.DOWNLOAD_DIR, f))
            for f in os.listdir(P.DOWNLOAD_DIR)
            if f.endswith(".epd")
        )
    except OSError:
        return 0


def storage_free_bytes(total: int = P.FLASH_TOTAL_BYTES) -> int:
    """LittleFS free bytes as the firmware would report them.

    Partition size comes from P.FLASH_TOTAL_BYTES (default 32 MiB, override
    with SENSECRAFT_FLASH_BYTES). The cloud refuses to push a playlist when
    this is too small, answering code 3032 "available flash: N bytes".
    """
    return max(0, total - emulated_used_bytes())


def sd_free_bytes(total: int = P.SD_TOTAL_BYTES) -> int:
    """MicroSD free bytes as the firmware would report them.

    The firmware reports 0 while no card is mounted; the emulator models an
    always-present card (P.SD_TOTAL_BYTES, default 32 GiB, override with
    SENSECRAFT_SD_BYTES) so the cloud's SD-based playlist check (code 3033)
    has a value to work with.
    """
    return max(0, total - emulated_used_bytes())


def build_iot_report(status: int, refresh_interval: int) -> dict:
    """Mirrors app_sensecraft.cpp::publishIotReport() + src/APP/iot.h.

    Values are emulator placeholders (no real battery/SHT40/storage on a
    Python host); the wire format matches the firmware exactly.
    Timestamp uses wall-clock ms instead of millis().
    """
    return {
        "version": P.PROTOCOL_VERSION,
        "session_id": session_id(),
        "type": P.MSG_TYPE_IOT,
        "timestamp": now_ms(),
        "data": {
            "states": [
                {"name": "Buttons", "state": {"left": 1, "right": 1}},
                {"name": "DataAccess", "state": {"interval": refresh_interval}},
                {"name": "Battery", "state": {"level": 100, "charging": False}},
                {"name": "Power", "state": {"deep_sleep_disabled": 1}},
                {"name": "Storage", "state": {
                    # app_sensecraft.cpp::publishIotReport(): LittleFS.totalBytes()
                    # minus usedBytes(). Python has no LittleFS partition, so
                    # P.FLASH_TOTAL_BYTES models the partition and downloads/*.epd
                    # stands in for locally stored images.
                    "flash_freeBytes": storage_free_bytes(),
                    "sd_freeBytes": sd_free_bytes(),
                }},
                {"name": "Sensor", "state": {"temp": 0, "humidity": 0}},
                {"name": "SD", "state": {"is_inserted": True, "is_mounted": True, "is_ready": True}},
                {"name": "Devicestatus", "state": {"status": status}},
            ],
            "descriptors": [
                {
                    "methods": {
                        "Buttons": {
                            "SetButtons": {
                                "description": "Control",
                                "parameters": {
                                    "left": {"description": "Left key state,0=enable,1=disable", "type": "number"},
                                    "right": {"description": "Right key state,0=enable,1=disable", "type": "number"},
                                },
                            }
                        },
                        "DataAccess": {
                            "SetInterval": {
                                "description": "Screen refresh interval",
                                "parameters": {
                                    "interval": {"description": "refresh interval(second),minimum: 1", "type": "number"}
                                },
                            }
                        },
                    }
                }
            ],
        },
    }


def build_img_flash(content_version: str) -> dict:
    """Mirrors app_sensecraft.cpp::sendImageRefreshReq()."""
    return {
        "version": P.PROTOCOL_VERSION,
        "session_id": session_id(),
        "type": P.MSG_TYPE_IMG_FLASH,
        "timestamp": now_ms(),
        "data": {"version": content_version},
    }


def build_img_flash_res(version: str, image_id: str, apply: bool, progress: int,
                        index: int, total: int) -> dict:
    """Mirrors app_sensecraft.cpp::imgRefreshRes()."""
    return {
        "version": P.PROTOCOL_VERSION,
        "session_id": session_id(),
        "type": P.MSG_TYPE_IMG_FLASH_RES,
        "timestamp": now_ms(),
        "data": {
            "version": version,
            "img_id": image_id,
            "img_apply": apply,
            "img_progress": progress,
            "img_total": total,
            "img_index": index,
        },
    }


class Emulator:
    """Orchestrates the SenseCraft session (main.cpp + app_sensecraft.cpp flow)."""

    def __init__(self, args, state: dict, session: requests.Session):
        self.args = args
        self.state = state
        self.http = session
        self.mac = state["mac"]
        self.content_version = state.get("content_version") or P.CONTENT_VERSION_DEFAULT
        self.refresh_interval = 1
        self.deep_sleep_disabled = 1

        self.bind_result: bind.BindResult | None = None
        self.mqtt: mqtt_client.SenseCraftMqtt | None = None

        self._last_image_session: str | None = None
        self._pending_refresh = 0          # IMAGE_REFRESH_MAX_RETRY counter
        self._refresh_timer: threading.Timer | None = None
        self._iot_timer: threading.Timer | None = None
        self._lock = threading.Lock()
        self._done = threading.Event()
        self._first_round_done = False

    # -- cloud commands -------------------------------------------------
    def handle_cloud_command(self, doc: dict) -> None:
        """Mirrors app_sensecraft.cpp::handleCloudCommand()."""
        data = doc.get("data") or {}
        commands = data.get("commands")
        if not isinstance(commands, list):
            print("[IOT] downlink missing commands array; ignoring")
            return
        for cmd in commands:
            if not isinstance(cmd, dict):
                continue
            name, method = cmd.get("name"), cmd.get("method")
            params = cmd.get("parameters") or {}
            if (name, method) == P.IOT_CMD_INTERVAL:
                self.refresh_interval = int(params.get("interval", 0) or 0)
                print(f"[IOT] DataAccess.SetInterval = {self.refresh_interval}s (emulator: accepted)")
            elif (name, method) == P.IOT_CMD_DEEPSLEEP:
                # enable=0 means deep sleep allowed
                self.deep_sleep_disabled = 0 if int(params.get("enable", 1) or 1) == 0 else 1
                print(f"[IOT] Power.SetDeepSleep -> deep sleep "
                      f"{'disabled' if self.deep_sleep_disabled else 'allowed'} (emulator: accepted)")
            else:
                print(f"[IOT] unknown command: {name}.{method}")

    # -- image refresh --------------------------------------------------
    def request_image_refresh(self) -> bool:
        if not self.mqtt or not self.mqtt.is_connected():
            print("[IMAGE] MQTT not online; skipping refresh request")
            return False
        ok = self.mqtt.publish_json(build_img_flash(self.content_version))
        print(f"[IMAGE] Requesting latest content (version={self.content_version}) ...")
        # arm retry: resend if no manifest arrives (app_sensecraft.cpp imageRetryCb)
        self._cancel_refresh_timer()
        self._refresh_timer = threading.Timer(P.IMAGE_REFRESH_RETRY_S, self._refresh_retry)
        self._refresh_timer.daemon = True
        self._refresh_timer.start()
        return ok

    def _cancel_refresh_timer(self):
        if self._refresh_timer:
            self._refresh_timer.cancel()
            self._refresh_timer = None

    def _refresh_retry(self):
        with self._lock:
            self._pending_refresh += 1
            if self._pending_refresh >= P.IMAGE_REFRESH_MAX_RETRY:
                print(f"[IMAGE] No response after {self._pending_refresh} retries; "
                      f"will wait for the next server push.")
                self._pending_refresh = 0
                self._cancel_refresh_timer()
                return
        print(f"[IMAGE] No manifest yet, retrying ({self._pending_refresh}/{P.IMAGE_REFRESH_MAX_RETRY}) ...")
        self.request_image_refresh()

    # -- cloud push -----------------------------------------------------
    def handle_image_resource(self, doc: dict) -> None:
        """Mirrors app_sensecraft.cpp::handleImageResource()."""
        data = doc.get("data") or {}
        # The cloud puts session_id at the message top level; upstream reads it
        # from `data` (app_sensecraft.cpp::handleImageResource), which never
        # matches, so the guard never fires there. Accept both locations here.
        sid = doc.get("session_id") or data.get("session_id")
        if sid:
            with self._lock:
                if sid == self._last_image_session:
                    print(f"[IMAGE] duplicate session {sid}; ignoring")
                    return
                self._last_image_session = sid
        self._cancel_refresh_timer()
        with self._lock:
            self._pending_refresh = 0

        version = data.get("version", "")
        manifest_url = data.get("manifest_url", "")
        if not manifest_url:
            print(f"[IMAGE] Empty manifest (version={version}); device would show the "
                  f"waiting page. Waiting for content...")
            return

        print(f"[IMAGE] Manifest received, version={version}")
        try:
            mf = manifest.fetch_manifest(
                manifest_url, self.bind_result.cloud_token,
                insecure=self.args.insecure, debug=self.args.debug, session=self.http,
            )
            mf.version = version
        except PermissionError as e:
            print(f"[MANIFEST] {e}")
            return
        except FileNotFoundError as e:
            print(f"[MANIFEST] {e}")
            return
        except (RuntimeError, TypeError, ValueError) as e:
            print(f"[MANIFEST] {type(e).__name__}: {e}")
            return
        print(mf.summary())

        total = len(mf.images)
        for image in mf.images:
            try:
                def on_progress(pct: int, image=image):
                    self.mqtt.publish_json(build_img_flash_res(
                        version, image.id, apply=False, progress=pct,
                        index=image.index, total=total))
                    print(f"[DOWNLOAD] {image.id}: {pct}%")

                path, fmt, size = downloader.download_image(
                    image.url, image.id,
                    cloud_token=self.bind_result.cloud_token,
                    insecure=self.args.insecure, debug=self.args.debug,
                    on_progress=on_progress, session=self.http,
                )
            except (PermissionError, RuntimeError) as e:
                print(f"[DOWNLOAD] {image.id} failed: {e}")
                self.mqtt.publish_json(build_img_flash_res(
                    version, image.id, apply=False, progress=0,
                    index=image.index, total=total))
                continue

            print(downloader.describe_image(path, fmt, size))
            self.mqtt.publish_json(build_img_flash_res(
                version, image.id, apply=True, progress=100,
                index=image.index, total=total))
            print(f"[REPORT] img_flash_res sent (apply=true, 100%) for {image.id}")

        with self._lock:
            self.content_version = version
            self.state["content_version"] = version
            device.save_state(self.state)
        print(f"[IMAGE] Round complete; content_version={self.content_version}")

        with self._lock:
            if not self._first_round_done:
                self._first_round_done = True
                if self.args.once:
                    self._done.set()

    # -- MQTT lifecycle -------------------------------------------------
    def _on_connected(self):
        # app_sensecraft.cpp::MQTT_EVENT_CONNECTED: first report after 2s,
        # then periodic IoT report every 60s.
        timer = threading.Timer(P.FIRST_REPORT_DELAY_S, self._first_report)
        timer.daemon = True
        timer.start()
        self._schedule_iot_report()

    def _first_report(self):
        with self._lock:
            if not self._first_round_done:
                self.request_image_refresh()
        self._send_iot_report()

    def _send_iot_report(self):
        if self.mqtt and self.mqtt.is_connected():
            self.mqtt.publish_json(build_iot_report(P.DEVICE_ONLINE, self.refresh_interval))

    def _schedule_iot_report(self):
        self._iot_timer = threading.Timer(P.IOT_REPORT_INTERVAL_S, self._iot_report_tick)
        self._iot_timer.daemon = True
        self._iot_timer.start()

    def _iot_report_tick(self):
        self._send_iot_report()
        self._schedule_iot_report()

    # -- session --------------------------------------------------------
    def run_mqtt(self) -> bool:
        res = self.bind_result
        if self.args.debug:
            print("[BIND] session config:\n" + res.describe())

        self.mqtt = mqtt_client.SenseCraftMqtt(
            host=res.host, port=res.port,
            client_id=res.client_id, username=res.username, password=res.password,
            # The broker's per-device ACL is keyed to the exact client_id string
            # used at first registration; the bind endpoint returns the
            # underscore form, but some device records were provisioned with the
            # colon or bare-hex form. mqtt_client tries each until one can both
            # connect and subscribe to the downlink topic.
            client_id_candidates=[self.mac, self.mac.replace(":", "")],
            publish_topic=res.publish_topic, subscribe_topic=res.subscribe_topic,
            on_image_resource=self.handle_image_resource,
            on_cloud_command=self.handle_cloud_command,
            on_connected=self._on_connected,
            insecure=self.args.insecure, debug=self.args.debug,
        )
        if not self.mqtt.connect_blocking(timeout=30.0):
            print("[MQTT] Failed to connect within 30s.")
            return False
        return True

    def wait(self):
        try:
            if self.args.once:
                while not self._done.wait(timeout=1.0):
                    pass
                print("[DONE] First download round complete (--once); exiting.")
                self.mqtt.stop()
            else:
                print("[MQTT] Listening for cloud pushes (Ctrl-C to quit) ...")
                while True:
                    time.sleep(1.0)
        except KeyboardInterrupt:
            print("\n[QUIT] Stopping emulator.")
            self.mqtt.stop()


def wait_for_activation(args, http: requests.Session, mac: str) -> bind.BindResult:
    """Phase 1: bind loop, mirroring ensureSession() + bind poll."""
    last_code = None
    retry_count = 0
    while True:
        try:
            result = bind.request_bind(
                mac, insecure=args.insecure, debug=args.debug,
                debug_secrets=args.debug_secrets, session=http,
            )
        except bind.BindError as e:
            retry_count += 1
            if retry_count < P.BIND_MAX_RETRIES:
                print(f"[BIND] failed: {e} (retry {retry_count}/{P.BIND_MAX_RETRIES} in "
                      f"{P.BIND_RETRY_DELAY_S:.0f}s)")
                time.sleep(P.BIND_RETRY_DELAY_S)
            else:
                print(f"[BIND] failed after {P.BIND_MAX_RETRIES} retries: {e}")
                if e.http_code is not None:
                    print(f"[BIND] HTTP status: {e.http_code}")
                if e.body:
                    print(f"[BIND] server body (truncated): {e.body[:1024]}")
                print(f"[BIND] backoff {P.BIND_BACKOFF_S:.0f}s before next attempt.")
                retry_count = 0
                time.sleep(P.BIND_BACKOFF_S)
            continue

        retry_count = 0
        if result.activated:
            print("[PAIR] Device activated successfully.")
            return result

        # waiting for the user to enter the pair code in the SenseCraft web UI
        if result.activation_code < 0:
            print("[BIND] NOTE: activation.code missing from the bind response; "
                  "the backend uses a bound-but-unactivated convention. Treating "
                  "this as 'not activated' and continuing to poll.", flush=True)
        elif result.activation_code != last_code:
            last_code = result.activation_code
            print(bind.activation_banner(result.activation_code, result.activation_message),
                  flush=True)
        else:
            print(f"[PAIR] Waiting for activation (code {last_code}) ...", flush=True)
        time.sleep(P.BIND_POLL_INTERVAL_S)


def parse_args(argv=None):
    p = argparse.ArgumentParser(
        description="Emulate a SenseCraft HMI device (xiao_diy_ee04, 7.3\" 800x480 color).",
    )
    p.add_argument("--reset-device", action="store_true",
                   help="delete device.json and generate a new emulated MAC")
    p.add_argument("--debug", action="store_true",
                   help="verbose protocol logging (bind bodies, MQTT frames)")
    p.add_argument("--debug-secrets", action="store_true",
                   help="print full tokens / MQTT credentials (use with care)")
    p.add_argument("--insecure", action="store_true",
                   help="disable TLS certificate verification (debugging only)")
    p.add_argument("--once", action="store_true",
                   help="exit after the first successful manifest+image round")
    return p.parse_args(argv)


def main(argv=None) -> int:
    args = parse_args(argv)
    state = device.reset_device() if args.reset_device else device.load_state()
    print(f"[DEVICE] MAC: {state['mac']}  board={P.BOARD_TYPE}  screen="
          f"{P.BOARD_SCREEN_TYPE}  resolution={P.BOARD_RESOLUTION}  "
          f"firmware={P.APP_VERSION}")
    if args.insecure:
        print("[CONFIG] TLS verification DISABLED (--insecure)")

    http = requests.Session()
    result = wait_for_activation(args, http, state["mac"])
    state["bound"] = True
    state["activation_code"] = 0
    device.save_state(state)

    emu = Emulator(args, state, http)
    emu.bind_result = result
    if not emu.run_mqtt():
        return 1
    emu.wait()
    return 0


if __name__ == "__main__":
    sys.exit(main())
