"""Compile the real adapter against a tiny mock SDK; no electrical validation."""
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="photopainter-pmic-sdk-") as tmp:
    root = Path(tmp)
    for header in ("driver/i2c.h", "driver/gpio.h", "esp_err.h",
                   "freertos/FreeRTOS.h", "freertos/task.h", "soc/soc.h",
                   "soc/gpio_reg.h", "soc/gpio_sig_map.h", "soc/io_mux_reg.h"):
        path = root / header
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "fake_sdk.h"\n')
    binary = root / "native-bus-test"
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root), "-I", str(here / "photopainter_sdk_stubs"),
                    "-I", str(here.parent / "src"),
                    str(here / "photopainter_pmic_bus.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
