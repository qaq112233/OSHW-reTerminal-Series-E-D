"""Persistent emulated device identity.

Mirrors app_sensecraft.cpp::ensureSession() g_board population:
the MAC is generated once (locally administered, 02:xx:xx:xx:xx:xx) and
reused on every run, exactly like a real ESP32 reads its own MAC.

This is an emulator-only identity. It is not a copy of any real Seeed device.
"""

import json
import os
import random
import sys

DEVICE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "device.json")

STATE_DEFAULT = {
    "mac": None,                  # stable, locally administered MAC
    "content_version": None,      # app_device_info.cpp::GetContentVersion()
    "bound": False,               # app_device_info.cpp::GetBindState()
    "activation_code": 0,
}

STATE_KEYS = set(STATE_DEFAULT)


def _generate_mac() -> str:
    """Locally administered, unicast MAC: 02:xx:xx:xx:xx:xx (not a real Seeed device)."""
    return "02:" + ":".join(f"{random.randrange(256):02X}" for _ in range(5))


def _normalize_mac(mac: str | None) -> str | None:
    if not isinstance(mac, str):
        return None
    if len(mac) != 17 or mac.count(":") != 5:
        return None
    parts = mac.split(":")
    if not all(len(p) == 2 for p in parts):
        return None
    try:
        b = [int(p, 16) for p in parts]
    except ValueError:
        return None
    if b[0] & 0b10 == 0:  # must be locally administered (L/A bit set)
        return None
    if b[0] & 0b1:        # must be unicast
        return None
    return ":".join(f"{x:02X}" for x in b)


def load_state() -> dict:
    """Load (or create) the persistent device state."""
    state = dict(STATE_DEFAULT)
    if os.path.exists(DEVICE_FILE):
        try:
            with open(DEVICE_FILE, "r", encoding="utf-8") as f:
                raw = json.load(f)
            if isinstance(raw, dict):
                for k, v in raw.items():
                    if k in STATE_KEYS:
                        state[k] = v
        except (json.JSONDecodeError, OSError, UnicodeDecodeError) as e:
            print(f"[DEVICE] unreadable {DEVICE_FILE} ({e}); starting fresh state.", file=sys.stderr)

    if state.get("mac") is None:
        state["mac"] = _normalize_mac(state.get("mac")) or _generate_mac()
        _save_state(state)
    else:
        normalized = _normalize_mac(state["mac"])
        if normalized is None:
            print(f"[DEVICE] invalid MAC in {DEVICE_FILE}; regenerating.", file=sys.stderr)
            state["mac"] = _generate_mac()
        else:
            state["mac"] = normalized
        _save_state(state)

    return state


def _save_state(state: dict) -> None:
    tmp = DEVICE_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump({k: state.get(k) for k in STATE_KEYS}, f, indent=2, sort_keys=True)
        f.write("\n")
    os.replace(tmp, DEVICE_FILE)


def save_state(state: dict) -> None:
    _save_state(state)


def reset_device() -> dict:
    """--reset-device: delete local identity and generate a new one."""
    if os.path.exists(DEVICE_FILE):
        os.remove(DEVICE_FILE)
    return load_state()
