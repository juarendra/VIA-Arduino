import json
from pathlib import Path

path = Path("examples/nice_nano_v2_VIA_BLE/nice_nano_v2_VIA_BLE.json")
definition = json.loads(path.read_text(encoding="utf-8"))
assert definition["name"] == "AirVIA nice!nano 2x3"
assert definition["matrix"] == {"rows": 2, "cols": 3}
positions = [tuple(key["matrix"]) for key in definition["layouts"]["keymap"]]
assert positions == [(0, 0), (0, 1), (0, 2), (1, 0), (1, 1), (1, 2)]
assert len(set(positions)) == 6
assert "encoders" not in definition
assert "lighting" not in definition
