"""SenseCraft bind (session request).

Mirrors app_sensecraft.cpp::buildBindBody(), parseBindResp() and requestBind().

bind semantics (docs/app_sensecraft_model.md): it is not a one-time binding
call, it is "request the current cloud session configuration for this device".
While the device is not yet activated by the user, the response carries an
activation code (pair code) instead of a usable MQTT session.
"""

import json

import requests

import protocol as P


class BindError(Exception):
    """Raised when the bind endpoint is unreachable or returns a non-bind payload."""

    def __init__(self, message: str, http_code: int | None = None, body: str | None = None):
        super().__init__(message)
        self.http_code = http_code
        self.body = body


def build_bind_body(mac: str) -> dict:
    """Mirrors app_sensecraft.cpp::buildBindBody()."""
    return {
        "mac_address": mac,
        "chip_model_name": P.CHIP_MODEL_NAME,
        "version": P.APP_VERSION,
        "board": {
            "type": P.BOARD_TYPE,
            "screen_type": P.BOARD_SCREEN_TYPE,
            "resolution": P.BOARD_RESOLUTION,
            "ssid": P.WIFI_SSID,
            "rssi": P.WIFI_RSSI,
            "channel": P.WIFI_CHANNEL,
            "ip": P.WIFI_IP,
            "mac": mac,
            "img_format": P.BOARD_IMG_FORMAT,
        },
    }


def _parse_mqtt_endpoint(endpoint: str | None) -> tuple[str, int]:
    """Mirrors app_sensecraft.cpp::parseMqttEndpoint()."""
    if not endpoint:
        return "", 1883
    sep = endpoint.find(":")
    if sep > 0:
        host = endpoint[:sep]
        try:
            port = int(endpoint[sep + 1:])
        except ValueError:
            port = 1883
        return host, port
    return endpoint, 1883


def _mask(value: str | None, keep: int = 8) -> str:
    if not value:
        return ""
    if len(value) <= keep:
        return "*" * len(value)
    return value[: keep // 2] + "..." + value[-(keep // 2):]


class BindResult:
    """Parsed bind response (docs/app_sensecraft_model.md "会话定义")."""

    def __init__(self, payload: dict, debug_secrets: bool = False):
        result = payload.get("result") or {}
        mqtt = result.get("mqtt") or {}
        activation = result.get("activation") or {}
        server_time = result.get("server_time") or {}

        self.host, self.port = _parse_mqtt_endpoint(mqtt.get("endpoint"))
        self.client_id = mqtt.get("client_id", "")
        self.username = mqtt.get("username", "")
        self.password = mqtt.get("password", "")
        self.publish_topic = mqtt.get("publish_topic", "")
        self.subscribe_topic = mqtt.get("subscribe_topic", "")
        # app_sensecraft.cpp::parseBindResp(): token = mqtt.password (cloud token)
        self.cloud_token = self.password

        # app_sensecraft.cpp::parseBindResp(): activation.code defaults to -1
        # when the field is absent, but a bound device always reports code == 0.
        if "code" in activation:
            self.activation_code = int(activation["code"])
        else:
            self.activation_code = -1
        self.activation_message = activation.get("message", "")

        self.server_time = server_time.get("timestamp")
        self.timezone = server_time.get("timezone")
        self.timezone_offset = server_time.get("timezone_offset")

        self.firmware_url = (result.get("firmware") or {}).get("url", "")
        self.firmware_version = (result.get("firmware") or {}).get("version", "")
        self.debug_secrets = debug_secrets

    @property
    def activated(self) -> bool:
        return self.activation_code == 0

    def describe(self) -> str:
        show = self.password if self.debug_secrets else _mask(self.password)
        return (
            f"  mqtt.endpoint       = {self.host}:{self.port}\n"
            f"  mqtt.client_id      = {self.client_id}\n"
            f"  mqtt.username       = {self.username}\n"
            f"  mqtt.password       = {show}\n"
            f"  mqtt.publish_topic  = {self.publish_topic}\n"
            f"  mqtt.subscribe_topic= {self.subscribe_topic}\n"
            f"  activation.code     = {self.activation_code}\n"
            f"  activation.message  = {self.activation_message}"
        )


def request_bind(mac: str, *, insecure: bool = False, debug: bool = False,
                 debug_secrets: bool = False, timeout: float = P.HTTP_TIMEOUT_S,
                 session: requests.Session | None = None) -> BindResult:
    """Mirrors app_sensecraft.cpp::requestBind().

    Raises BindError on transport failure, HTTP error, or an unexpected payload.
    """
    url = P.API_BASE_URL + P.API_PATH_DEVICE_BIND
    body = build_bind_body(mac)
    if debug:
        print(f"[BIND] POST {url}\n[BIND] body={json.dumps(body)}")

    http = session or requests.Session()
    try:
        resp = http.post(
            url,
            json=body,
            headers={"Content-Type": "application/json"},
            timeout=timeout,
            verify=not insecure,
        )
    except requests.exceptions.SSLError as e:
        raise BindError(f"TLS verification failed ({e}); retry with --insecure to debug") from e
    except requests.exceptions.ConnectionError as e:
        raise BindError(f"connection failed: {e}") from e
    except requests.exceptions.Timeout as e:
        raise BindError(f"timeout after {timeout}s") from e

    if resp.status_code != 200:
        raise BindError(
            f"HTTP {resp.status_code} from {url}",
            http_code=resp.status_code,
            body=resp.text[:2048],
        )

    try:
        payload = resp.json()
    except json.JSONDecodeError as e:
        raise BindError(f"non-JSON response: {resp.text[:512]}", body=resp.text[:2048]) from e

    if debug:
        text = resp.text
        if not debug_secrets:
            pw = (payload.get("result") or {}).get("mqtt", {}).get("password", "")
            if pw:
                text = text.replace(pw, _mask(pw))
        print(f"[BIND] response:\n{text}")

    if payload.get("code") != 200 or not isinstance(payload.get("result"), dict):
        raise BindError(
            f"unexpected bind payload (code={payload.get('code')})",
            body=json.dumps(payload)[:2048],
        )

    return BindResult(payload, debug_secrets=debug_secrets)


def activation_banner(code: int, message: str) -> str:
    return (
        "\n=====================================\n"
        f" SenseCraft Pair Code: {code}\n"
        "=====================================\n"
        f" Bind URL: {message}\n"
        " Open the URL above and enter the pair code.\n"
        " Waiting for activation...\n"
    )
