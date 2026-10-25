import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location(
    "wroom_ota", Path(__file__).resolve().parents[1] / "extras" / "wroom_ota.py"
)
ota = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ota)


class ProvisionValidation(unittest.TestCase):
    def test_rejects_control_characters_and_lengths_before_provisioning(self):
        invalid = [
            {"ssid": "", "password": "public-fixture-pass"},
            {"ssid": "s" * 33, "password": "public-fixture-pass"},
            {"ssid": "bench", "password": "short"},
            {"ssid": "bench", "password": "z" * 64},
            {"ssid": "bench\nOTA_STATUS", "password": "public-fixture-pass"},
            {"ssid": "bench", "password": "public\x00fixture-pass"},
            {"ssid": 123, "password": "public-fixture-pass"},
            {"ssid": "bench", "password": "public-fixture-pass", "extra": True},
        ]
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(ValueError):
                ota.setup_command(value, "public-fixture-ota-pass")

    def test_unicode_ssid_limits_are_bytes(self):
        ota.setup_command({"ssid": "é" * 16, "password": "public-fixture-pass"},
                          "public-fixture-ota-pass")
        with self.assertRaises(ValueError):
            ota.setup_command({"ssid": "é" * 17, "password": "public-fixture-pass"},
                              "public-fixture-ota-pass")

    def test_short_ota_password_is_rejected(self):
        with self.assertRaises(ValueError):
            ota.setup_command({"ssid": "bench", "password": "public-fixture-pass"}, "short")


if __name__ == "__main__":
    unittest.main()
