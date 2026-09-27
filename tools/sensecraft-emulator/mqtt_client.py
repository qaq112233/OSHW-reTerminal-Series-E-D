"""MQTT session for the SenseCraft emulator.

Mirrors app_sensecraft.cpp::connectMqtt(), mqttEventHandler() and
handleMqttData().

TLS: prod firmware always uses MQTT_TRANSPORT_OVER_SSL with the pinned GoDaddy
G2 root. Here system CA (certifi) is used and verified by default; use
--insecure only for certificate debugging.
"""

import json
import queue
import ssl
import threading
import time

import certifi
import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion

import protocol as P


class SenseCraftMqtt:
    """Thin paho-mqtt wrapper speaking the SenseCraft device protocol."""

    def __init__(self, host: str, port: int, client_id: str, username: str, password: str,
                 publish_topic: str, subscribe_topic: str, *,
                 client_id_candidates: list[str] | None = None,
                 on_image_resource=None, on_cloud_command=None, on_connected=None,
                 on_disconnected=None, insecure: bool = False, debug: bool = False):
        self.host = host
        self.port = port
        self.publish_topic = publish_topic
        self.subscribe_topic = subscribe_topic
        self.on_image_resource = on_image_resource
        self.on_cloud_command = on_cloud_command
        self.on_connected = on_connected
        self.on_disconnected = on_disconnected
        self.insecure = insecure
        self.debug = debug

        self.username = username
        self.password = password

        self._connected = threading.Event()
        self._ready = threading.Event()
        self._connack_seen = threading.Event()
        self._subscribe_seen = threading.Event()
        self._last_connack = None
        self._work = queue.Queue()
        self._worker = None
        self._stop = threading.Event()
        self._known_ids: list[str] = []
        for cid in [client_id, *(client_id_candidates or [])]:
            if cid and cid not in self._known_ids:
                self._known_ids.append(cid)
        self._client_id_index = 0
        self.client = None
        self._build_client(self._known_ids[0] if self._known_ids else "")

    # -- client construction --------------------------------------------
    def _build_client(self, client_id: str) -> mqtt.Client:
        """(Re)create the paho client for one candidate client_id.

        clean_session=False mirrors app_sensecraft.cpp disable_clean_session=true.
        """
        self._connack_seen.clear()
        self._subscribe_seen.clear()
        self._ready.clear()
        self._last_connack = None
        client = mqtt.Client(
            callback_api_version=CallbackAPIVersion.VERSION2,
            client_id=client_id or "",
            clean_session=False,
            protocol=mqtt.MQTTv311,
        )
        if self.username:
            client.username_pw_set(self.username, self.password)
        client.keepalive = P.MQTT_KEEPALIVE_S
        client.reconnect_delay_set(min_delay=1, max_delay=P.MQTT_RECONNECT_INTERVAL_S)
        # app_config.h rootCACertificate pins the GoDaddy G2 root; emulator
        # trusts the system CA bundle instead and verifies by default.
        use_tls = self.port == 8883 or not self.insecure
        if use_tls:
            client.tls_set(
                ca_certs=certifi.where(),
                cert_reqs=ssl.CERT_NONE if self.insecure else ssl.CERT_REQUIRED,
            )
            if self.insecure:
                client.tls_insecure_set(True)

        client.on_connect = self._on_connect
        client.on_disconnect = self._on_disconnect
        client.on_subscribe = self._on_subscribe
        client.on_message = self._on_message
        self.client = client
        return client

    def _wait_ready(self, timeout: float) -> bool:
        """Wait until this client is usable: CONNACK accepted AND SUBACK ok."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self._ready.is_set():
                return True
            if self._connack_seen.is_set() and self._last_connack != mqtt.CONNACK_ACCEPTED:
                return False
            if self._subscribe_seen.is_set():
                return False
            time.sleep(0.05)
        return self._ready.is_set()

    def _mark_ready(self) -> None:
        if self._ready.is_set():
            return
        self._ready.set()
        print("[MQTT] Connected.", flush=True)
        if self.on_connected:
            self.on_connected()

    # -- paho callbacks -------------------------------------------------
    def _on_connect(self, client, userdata, flags, reason_code, properties):
        rc_value = getattr(reason_code, "value", reason_code)
        self._last_connack = rc_value
        self._connack_seen.set()
        if rc_value == mqtt.CONNACK_ACCEPTED:
            self._connected.set()
            if self.subscribe_topic:
                # Broker may accept the connection yet deny the downlink topic
                # (SUBACK 0x80); readiness is only declared after a good SUBACK.
                client.subscribe(self.subscribe_topic, qos=1)
            else:
                self._mark_ready()
        else:
            print(f"[MQTT] Connection refused: rc={rc_value} ({reason_code})", flush=True)
            self._connected.clear()
            if self.on_disconnected:
                self.on_disconnected()

    def _on_disconnect(self, client, userdata, *args, **kwargs):
        # paho v2: (client, userdata, disconnect_flags, reason_code, properties)
        rc = args[1] if len(args) > 1 else None
        reason = getattr(rc, "value", rc)
        print(f"[MQTT] Disconnected (rc={reason}); paho will auto-reconnect.")
        self._connected.clear()
        if self.on_disconnected:
            self.on_disconnected()

    def _on_subscribe(self, client, userdata, mid, reason_codes, properties):
        codes = [rc.value if hasattr(rc, "value") else rc for rc in reason_codes]
        print(f"[MQTT] Subscribed: {self.subscribe_topic} (qos={codes})", flush=True)
        if codes and all(0 <= int(c) < 128 for c in codes):
            self._mark_ready()
        else:
            print("[MQTT] Downlink topic denied by the broker (SUBACK failure).", flush=True)
            self._subscribe_seen.set()

    def _on_message(self, client, userdata, message):
        payload = message.payload.decode("utf-8", errors="replace")
        if self.debug:
            topic = message.topic
            print(f"[MQTT] <- [{topic}]: {payload[:4096]}")
        try:
            doc = json.loads(payload)
        except json.JSONDecodeError:
            print("[MQTT] Ignoring non-JSON message")
            return
        msg_type = doc.get("type")
        if msg_type in (P.MSG_TYPE_IMG_FLASH, P.MSG_TYPE_ALBUM):
            # Download work runs off the network thread (firmware has a dedicated
            # download task); enqueue to keep the MQTT loop responsive.
            self._work.put(doc)
        elif msg_type == P.MSG_TYPE_IOT:
            self.on_cloud_command(doc)
        else:
            print(f"[MQTT] Unknown message type: {msg_type!r}")

    # -- lifecycle ------------------------------------------------------
    def connect_blocking(self, timeout: float = 30.0) -> bool:
        print(f"[MQTT] Connecting to {self.host}:{self.port} ...")
        self._worker = threading.Thread(target=self._work_loop, name="sc-download", daemon=True)
        self._worker.start()

        while self._client_id_index < len(self._known_ids):
            cid = self._known_ids[self._client_id_index]
            self._build_client(cid)
            try:
                self.client.connect(self.host, self.port, keepalive=P.MQTT_KEEPALIVE_S)
            except Exception as e:  # noqa: BLE001 - report socket/TLS/DNS failure clearly
                print(f"[MQTT] connect() failed: {type(e).__name__}: {e}")
                return False
            self.client.loop_start()
            if self._wait_ready(timeout):
                return True

            # The broker's ACL for a device is keyed to the exact client_id
            # string used when the device first registered, and the ACL keys
            # seen in the wild differ (underscore / colon / bare hex). Try the
            # next candidate until one both connects and can subscribe.
            try:
                self.client.loop_stop()
                self.client.disconnect()
            except Exception as e:  # noqa: BLE001 - best-effort teardown
                if self.debug:
                    print(f"[MQTT] teardown after failed client_id: {type(e).__name__}: {e}")
            self._client_id_index += 1
            if self._client_id_index < len(self._known_ids):
                nxt = self._known_ids[self._client_id_index]
                why = ("connack=135" if self._last_connack == 135
                       else "suback denied" if self._subscribe_seen.is_set()
                       else f"no connack within {timeout:.0f}s")
                print(f"[MQTT] client_id={cid} rejected ({why}); "
                      f"retrying with client_id={nxt} ...", flush=True)
        return False

    def _work_loop(self):
        while not self._stop.is_set():
            try:
                doc = self._work.get(timeout=1.0)
            except queue.Empty:
                continue
            try:
                self.on_image_resource(doc)
            except Exception as e:  # noqa: BLE001 - never kill the worker
                print(f"[IMAGE] download round failed: {type(e).__name__}: {e}")

    def stop(self):
        self._stop.set()
        try:
            self.client.loop_stop()
            self.client.disconnect()
        except Exception as e:  # noqa: BLE001 - best-effort teardown
            if self.debug:
                print(f"[MQTT] teardown: {type(e).__name__}: {e}")

    # -- protocol -------------------------------------------------------
    def publish_json(self, obj: dict, qos: int = 1) -> bool:
        payload = json.dumps(obj, separators=(",", ":"))
        if self.debug:
            print(f"[MQTT] -> [{self.publish_topic}]: {payload[:4096]}")
        info = self.client.publish(self.publish_topic, payload, qos=qos)
        return info.rc == mqtt.MQTT_ERR_SUCCESS

    def is_connected(self) -> bool:
        return self._connected.is_set()
