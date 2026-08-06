# nice!nano v2 Hardware Verification Checklist

Status: Pending physical hardware verification

## Build Evidence

- Date: 2026-08-06
- Commit SHA: dd22948e9123212c2d93c1e8770751c78793eda8
- GitHub Actions run: https://github.com/juarendra/VIA-Arduino/actions/runs/31058754636
- FQBN: `adafruit:nrf52:promicronrf52840`
- Adafruit nRF52 BSP: `1.7.0`
- Variant commit: `bd0fdcf124f59662d0184c39126e456f89dccd9c`
- Firmware SHA-256: 11E5ED790CB8A1120AE81AE2F5F09EE1C35D7B406B528BB47ADF52A08A710A21
- Corruptor SHA-256: D60B80E2F7DCDAC892B412ABA24BD6FF1564061F89AA6615017AF81403A97376

## Fixture

- Board: nice!nano v2
- Matrix: 2x3, rows D0/D1, columns D2/D3/D4
- Host OS: Microsoft Windows 11 Home Single Language
- Browser: Google Chrome 127.0.6533.89

## Flash and BLE HID

- [ ] UF2 hash verified before flash.
- [ ] Production UF2 flashed through nice!nano bootloader.
- [ ] Device advertises as `AirVIA nice!nano`.
- [ ] Layer 0 types A/B/C/D/E; MO(1) emits no literal key.
- [ ] MO(1) plus first five switches types 1/2/3/4/5.
- [ ] No stuck key after release or reconnect.

## AirVIA

- [ ] FF60 service discovered.
- [ ] VIA protocol version is `0x000D`.
- [ ] All 12 keymap entries synchronize.
- [ ] Remap acknowledgment received and typing changes immediately.

## Persistence and Recovery

- [ ] Remap survives BLE disconnect/reconnect.
- [ ] Remap survives reset.
- [ ] Remap survives at least ten seconds without power.
- [ ] HID and AirVIA recover after deleting and recreating the OS bond.
- [ ] 31-byte and 33-byte FF61 writes do not damage valid traffic or keymap.
- [ ] Corrupting newest slot restores the older committed keymap.

## Notes

- Record each failure with exact symptom before any retry.
