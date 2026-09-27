"""Static guard for V1 rail-gated refresh entry points (not hardware testing)."""
from pathlib import Path
import re

src = Path(__file__).resolve().parents[1] / "src"
view = (src / "APP/app_view.cpp").read_text()
assets = (src / "boards/common/screen_assets.cpp").read_text()
board = (src / "boards/waveshare_photopainter/waveshare_photopainter.cpp").read_text()
bringup = (src / "boards/waveshare_photopainter/bringup.cpp").read_text()
power = (src / "boards/waveshare_photopainter/panel_power.h").read_text()
assert not re.search(r"\bdisplay\(\)\.update\s*\(", view)
assert not re.search(r"\bdisplay\.update\s*\(\s*\)", assets)
assert view.count("HAL::GetHAL().displayUpdate();") == 11
assert "Board::GetInstance().RefreshDisplay();" in assets
assert "display_.native().update();\n        return powerOffPanel();" in board
assert "display->update();\n    if (!photopainter_panel::railOff(pmic))" in bringup
assert board.count("rail_guard_.arm();") >= 3
assert "rail_guard_.disarm();" in board
assert bringup.count("rail_guard.arm();") == 3
assert "esp_restart();" in power and "millis() - self.started_ms_" in power
assert "pmic.enableALDO3()" in power and "pmic.disableALDO3()" in power
assert "XPOWERS_AXP2101_LDO_ONOFF_CTRL0" in power
assert "if (!HAL::GetHAL().prepareForDeepSleep())" in (src / "APP/app_device_info.cpp").read_text()
print("V1 EPD rail-gated refresh sites: 11 APP sites, waiting/cover, bring-up, deep sleep")
