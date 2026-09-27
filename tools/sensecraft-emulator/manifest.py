"""Manifest fetch and parse.

Mirrors app_download.cpp::__parse_manifest_tasks() and the manifest download
part of __download_to_file().

Authorization: app_download.cpp uses `http.addHeader("authorization",
cloud_token)` where cloud_token == mqtt.password from the bind response
(app_sensecraft.cpp::parseBindResp(), app_device_info.cpp::SetCloudToken).
"""

import json
import os
import sys

import requests

DOWNLOADS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "downloads")
MANIFEST_PATH = os.path.join(DOWNLOADS_DIR, "manifest.json")


def resource_kind_from_url(url: str) -> str:
    """Mirrors app_download.cpp::__resource_kind_from_url()."""
    marker = "/render/"
    idx = url.find(marker)
    if idx < 0:
        return "unknown"
    rest = url[idx + len(marker):]
    kind = rest.split("/", 1)[0]
    return {"img": "static_image", "layout": "dynamic_canvas"}.get(kind, "unknown")


class ManifestImage:
    def __init__(self, entry: dict, order: int):
        self.id = entry.get("id", "")
        self.index = entry.get("index", order)
        self.url = entry.get("url", "")
        self.order = order
        self.kind = resource_kind_from_url(self.url)


class Manifest:
    """Parsed manifest (docs/app_download_model.md "Manifest 模型")."""

    def __init__(self, payload: dict, url: str, version: str):
        self.url = url
        self.version = version           # data.version from the cloud push (batch version)
        self.album_id = payload.get("album_id", "")
        self.schema_version = payload.get("version")
        self.images: list[ManifestImage] = []
        for i, entry in enumerate(payload.get("images", []) or []):
            if not isinstance(entry, dict) or not entry.get("id") or not entry.get("url"):
                print(f"[MANIFEST] skipping invalid image entry at {i}", file=sys.stderr)
                continue
            self.images.append(ManifestImage(entry, i))

    def summary(self) -> str:
        return (
            f"[MANIFEST] version={self.schema_version} batch={self.version or '-'} "
            f"album_id={self.album_id} images={len(self.images)}"
        )


def fetch_manifest(url: str, cloud_token: str, *, insecure: bool = False,
                   debug: bool = False, timeout: float = 30.0,
                   session: requests.Session | None = None) -> Manifest:
    """HTTP GET the manifest with the cloud token, save the raw JSON, parse it."""
    if not url:
        raise ValueError("empty manifest_url")

    http = session or requests.Session()
    headers = {}
    if cloud_token:
        headers["authorization"] = cloud_token
    if debug:
        print(f"[MANIFEST] GET {url}")

    resp = http.get(url, headers=headers, timeout=timeout, verify=not insecure)
    if resp.status_code in (401, 403):
        raise PermissionError(f"HTTP {resp.status_code}: cloud token rejected for manifest")
    if resp.status_code == 404:
        raise FileNotFoundError(f"HTTP 404: manifest not found at {url}")
    if resp.status_code != 200:
        raise RuntimeError(f"HTTP {resp.status_code} fetching manifest: {resp.text[:256]}")

    try:
        payload = resp.json()
    except json.JSONDecodeError as e:
        raise RuntimeError(f"manifest is not valid JSON: {resp.text[:256]}") from e

    if not isinstance(payload, dict):
        raise TypeError(f"manifest root is {type(payload).__name__}, expected object")

    os.makedirs(DOWNLOADS_DIR, exist_ok=True)
    tmp = MANIFEST_PATH + ".tmp"
    with open(tmp, "wb") as f:
        f.write(resp.content)
    os.replace(tmp, MANIFEST_PATH)
    if debug:
        print(f"[MANIFEST] saved raw manifest -> {MANIFEST_PATH} ({len(resp.content)} bytes)")

    return Manifest(payload, url, version="")
