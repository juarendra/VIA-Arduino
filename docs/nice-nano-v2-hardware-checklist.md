# nice!nano v2 Hardware Verification Checklist

*Status: Pending physical hardware verification.*

- [ ] Flash firmware successfully via UF2.
- [ ] Connect and type over BLE to OS.
- [ ] Discover keyboard in VIA (Web Bluetooth) using FF60 service.
- [ ] Sync initial keymap from device.
- [ ] Remap a key and verify it works immediately.
- [ ] Disconnect power, reconnect, and verify remap persists.
- [ ] Send invalid packets and verify they are rejected safely.
- [ ] Simulate corrupt latest storage slot; verify fallback to previous slot or defaults.