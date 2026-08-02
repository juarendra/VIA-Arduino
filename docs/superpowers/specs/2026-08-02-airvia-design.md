# AirVIA — BLE Wireless Keyboard Configurator Design Spec

## Overview

AirVIA is a single-page web application that configures a VIA-compatible
keyboard over Bluetooth Low Energy (BLE) using the same VIA protocol v13
packet format. It mirrors the VIA desktop experience — keymap grid,
encoder maps, macros, RGB lighting, layout options — but connects
wirelessly without a USB cable.

**Repository:** separate repo from VIA-Arduino, name `AirVIA`.
**Target platform:** Chrome, Edge (Web Bluetooth API).
**Wireless stack:** BLE GATT with a vendor-defined VIA service (usage page
0xFF60).

## Architecture

Three layers with no cross-dependencies between core and I/O:

```
   [ESP32-S3 Keyboard]          ← BLE GATT characteristic (32B IN/OUT)
        |                                  FF61: write + notify
        |                                  FF62: read (device info)
   [Web Bluetooth API]           ← built-in browser API
        |
   [viatle-core]                 ← TypeScript pure, no DOM dependencies
        |                             Protocol encode/decode, CRC, commands
        |
   [viatle-ble]                  ← Web Bluetooth transport adapter
        |                             Scan, connect, subscribe, queue, retry
        |
   [Svelte 5 UI]                 ← keymap grid, encoders, macros, lighting
        ^
   [v3-definition.json]          ← keyboard physical layout, matrix, layers
```

### Layer Responsibilities

| Layer | Technology | Purpose |
|---|---|---|
| `viatle-core` | TypeScript strict | Protocol encode/decode, CRC32, packet builder/parser, exactly mirrors `VIA_Protocol.cpp` |
| `viatle-ble` | Web Bluetooth API | Discover VIA GATT service, connect, queue packets, retry with timeout, reconnect |
| `viatle-ui` | Svelte 5 + Tailwind | Keymap grid (drag-drop), encoder editor, macro sequence editor, lighting panel, layout options, V3 JSON loader |

Each layer is independently testable. `viatle-core` has zero DOM / BLE imports.
`viatle-ble` has zero UI imports. `viatle-ui` can swap transport adapters
(WebUSB, WebHID) without touching protocol logic.

## BLE Protocol

### GATT Service

```
VIA Service UUID:  0000FF60-0000-1000-8000-00805F9B34FB

VIA Data Characteristic (FF61):
  UUID:   0000FF61-0000-1000-8000-00805F9B34FB
  Props:  Read | Write | Notify
  Size:   32 bytes fixed

VIA Info Characteristic (FF62):
  UUID:   0000FF62-0000-1000-8000-00805F9B34FB
  Props:  Read
  Value:  Firmware version (4B) + device name (up to 28B)
```

### Packet Flow

```
App writes 32-byte VIA command → FF61 (Write without response)
Keyboard processes command     → FF61 (Notify) ← 32-byte response
```

Single characteristic for both directions simplifies the GATT table and matches
the raw HID model exactly. Notify is preferred over polling reads because it
provides implicit delivery confirmation.

### Packet Queue

- One request in-flight at a time
- Retry up to 3 times with 500ms timeout per attempt
- Queue depth: 8 pending commands
- Bulk reads (keymap buffer, macro buffer) stream sequentially
- Dropped notify = retry the last sent packet

## VIA Protocol Coverage (v13)

All 23 commands from `VIA_Protocol.cpp` are supported:

| Command | ID | Description |
|---|---|---|
| Get protocol version | 0x01 | Returns `0x000D` |
| Get keyboard value | 0x02 | Uptime, layout options, matrix state, firmware version, QMK ABI |
| Set keyboard value | 0x03 | Layout options, device indication |
| Get keycode | 0x04 | Single keycode per layer/row/col |
| Set keycode | 0x05 | Single keycode per layer/row/col |
| Reset keymap | 0x06 | Restore defaults (keymap + encoder maps) |
| Set custom value | 0x07 | RGB, backlight, etc. |
| Get custom value | 0x08 | Read custom value |
| Save | 0x09 | Commit to storage |
| Factory reset | 0x0A | Opt-in, requires policy flag |
| Bootloader jump | 0x0B | Opt-in |
| Get macro count | 0x0C | Number of macros |
| Get macro buffer size | 0x0D | Total macro storage bytes |
| Get macro buffer | 0x0E | Read macro data |
| Set macro buffer | 0x0F | Write macro data |
| Reset macros | 0x10 | Clear all macros |
| Get layer count | 0x11 | Number of layers |
| Get keymap buffer | 0x12 | Bulk keymap read |
| Set keymap buffer | 0x13 | Bulk keymap write |
| Get encoder keycode | 0x14 | CW/CCW keycode per layer per encoder |
| Set encoder keycode | 0x15 | Set encoder keycode |

Unsupported command responses (0xFF in byte 0) are shown as errors in the
Console tab.

## UI Components

```
App.svelte
├── ConnectBar.svelte         ← BLE scan, connect/disconnect, device info
├── TabBar.svelte             ← KEYMAP | ENCODER | MACROS | LIGHTING | LAYOUT | CONSOLE
│
├── [tab=KEYMAP]
│   ├── LayerSelector.svelte  ← layer 0-N tabs (from V3 JSON)
│   ├── KeymapGrid.svelte     ← physical layout grid rendered from V3 JSON
│   │   └── KeymapCell.svelte ← shows keycode label, click → picker
│   └── KeycodePicker.svelte  ← sidebar: category tabs + search, shared across tabs
│
├── [tab=ENCODER]
│   ├── EncoderList.svelte    ← encoder index, CW/CCW assignment per layer
│   └── KeycodePicker.svelte  ← shared
│
├── [tab=MACROS]
│   ├── MacroList.svelte      ← macro slots (0 to macroCount-1)
│   └── MacroEditor.svelte    ← sequence: key down/up, delay ms
│
├── [tab=LIGHTING]
│   └── LightingPanel.svelte  ← brightness 0-255, effect 0-N, speed, hue, saturation
│
├── [tab=LAYOUT]
│   └── LayoutOptions.svelte  ← toggle bitfield options (defined in V3 JSON)
│
└── [tab=CONSOLE]
    └── PacketLog.svelte      ← raw hex dump of last N packets (debug, hidden by default)
```

### Shared Components
- `Modal.svelte` — keycode picker, confirm dialogs (factory reset, save)
- `Toast.svelte` — transient notification (connected, saved, error)
- `Icon.svelte` — inline SVG, no icon library dependency

## Keycode Picker

Mirrors VIA desktop layout:
- Left sidebar with category tabs: Basic, Media, Layer, Macro, Special, Modifier
- Search bar with fuzzy filtering
- Grid of keycode tiles, each showing label + hex
- Click applies to currently selected cell(s)

QMK keycode labels come from a static map built from QMK source (0.0.8 ABI).

## V3 Definition JSON

VIA V3 definitions are loaded by URL or file drop. The JSON specifies:

```json
{
  "name": "My Keyboard",
  "vendorId": "0xFEED",
  "productId": "0x0001",
  "matrix": { "rows": 5, "cols": 15 },
  "layouts": {
    "keymap": [ /* physical key positions */ ],
    "labels": [ /* optional key labels per layout option */ ]
  }
}
```

On load, the app:
1. Validates JSON schema
2. Renders keymap grid matching physical layout positions
3. Sets up layer count and encoder count from the definition
4. Reads current keymap from device via bulk read (0x12)

## Tech Stack

| Concern | Choice | Reason |
|---|---|---|
| Build | Vite | Fast dev, native TS/TSX |
| UI framework | Svelte 5 | Compile-time, runes ($state), tiny runtime |
| Language | TypeScript strict | Protocol layer needs precise bit ops |
| Styling | Tailwind CSS | Utility-first, fast iteration, minimal CSS |
| Package manager | pnpm | Strict, fast |
| Testing | Vitest | Same ecosystem as Vite |
| BLE transport | Web Bluetooth API | Zero dependencies, built into browser |
| State management | Svelte 5 runes | Sufficient for single-page app |
| Routing | None | Tab-based single page |

### NOT included (YAGNI)
- ~~SvelteKit~~ — no SSR, no routing needed
- ~~shadcn-svelte / component library~~ — simple app, Tailwind is enough
- ~~Zustand / Pinia / store~~ — Svelte runes handle all reactive state
- ~~Icon libraries~~ — 5-6 inline SVGs total

## File Structure

```
AirVIA/
├── package.json
├── vite.config.ts
├── tsconfig.json
├── tailwind.config.js
├── index.html
├── public/
│   └── v3-examples/              ← sample V3 JSON definitions
├── src/
│   ├── main.ts                   ← entry point, mount Svelte app
│   ├── App.svelte                ← root component
│   │
│   ├── core/                     ← viatle-core
│   │   ├── protocol.ts           ← packet encode/decode, 23 commands
│   │   ├── crc.ts                ← CRC32 (matches C++ implementation)
│   │   ├── keycodes.ts           ← QMK keycode label map (0.0.8)
│   │   ├── v3-definition.ts      ← V3 JSON parser & types
│   │   └── packet.test.ts        ← unit tests
│   │
│   ├── ble/                      ← viatle-ble
│   │   ├── transport.ts          ← Web Bluetooth adapter (implements Transport interface)
│   │   ├── queue.ts              ← packet queue with retry
│   │   └── transport.test.ts
│   │
│   ├── ui/                       ← viatle-ui
│   │   ├── ConnectBar.svelte
│   │   ├── TabBar.svelte
│   │   ├── keymap/
│   │   │   ├── KeymapGrid.svelte
│   │   │   ├── KeymapCell.svelte
│   │   │   ├── LayerSelector.svelte
│   │   │   └── KeycodePicker.svelte
│   │   ├── encoder/
│   │   │   ├── EncoderList.svelte
│   │   │   └── EncoderEditor.svelte
│   │   ├── macro/
│   │   │   ├── MacroList.svelte
│   │   │   └── MacroEditor.svelte
│   │   ├── lighting/
│   │   │   └── LightingPanel.svelte
│   │   ├── layout/
│   │   │   └── LayoutOptions.svelte
│   │   ├── console/
│   │   │   └── PacketLog.svelte
│   │   └── shared/
│   │       ├── Modal.svelte
│   │       ├── Toast.svelte
│   │       └── Icon.svelte
│   │
│   └── store/
│       └── app.svelte.ts         ← Svelte runes global state
│
├── tests/                        ← integration + E2E notes
│   └── README.md                 ← how to test against VIA-Arduino firmware
│
└── README.md
```

## Key Behaviors

### Connection Lifecycle
1. User clicks "Connect" → `navigator.bluetooth.requestDevice()` with FF60 service filter
2. On connect → discover FF61 and FF62 characteristics
3. Subscribe to FF61 notify
4. Read FF62 → verify firmware compatibility (protocol version 0x0D)
5. Issue 0x01 (get protocol version) → confirm handshake
6. Bulk read 0x12 (keymap buffer) → populate grid
7. On disconnect → clean state, show "disconnected" toast
8. Auto-reconnect option (debounced 3s after disconnect)

### Keymap Editing
1. Click cell → open KeycodePicker modal
2. Select keycode → send 0x05 (set keycode) + 0x09 (save) with auto-save debounce (2s)
3. Drag-drop between cells (swap keycodes) → send two 0x05 + one 0x09
4. Right-click cell → "Copy / Reset to default"
5. Reset entire layer → 0x06 (reset keymap) with confirmation dialog

### Macro Editing
1. List shows macro slots (0 to N-1 based on firmware macro count)
2. Tap slot → sequence editor: timeline of key events
3. Each event: { type: "down"|"up"|"delay", keycode: uint16, delayMs: uint16 }
4. Save pushes entire macro buffer via 0x0F + 0x09

### Lighting
1. Channel 2 (QMK rgblight) — custom value protocol
2. Brightness slider (0-255), effect dropdown, speed slider, hue picker, saturation slider
3. Each slider sends set custom value (0x07) on drag end
4. Save (0x09) explicitly or via auto-save

### Factory Reset
1. Requires `eepromResetEnabled` flag (same as firmware)
2. Two-step confirmation: "Reset ALL settings to defaults?" → "This cannot be undone"
3. Sends 0x0A, then re-reads keymap state

## Testing Strategy

### Unit Tests (Vitest)
- `viatle-core/packet.test.ts` — encode/decode every command, CRC correctness, bounds checks
- `viatle-ble/transport.test.ts` — mock BLE GATT, queue logic, retry, timeout

### Integration Tests
- Manual against VIA-Arduino `Protocol_Self_Test` firmware over mock transport
- Manual against RP2040_VIA_RawHID via WebUSB (validate protocol parity with BLE)
- Manual against ESP32-S3 BLE firmware (requires hardware)

### E2E
- Chrome DevTools BLE simulator for packet-level testing
- Not automated — manual verification against real hardware

## Scope Boundaries

### Included in v1
- Full VIA v13 protocol (23 commands)
- BLE connection management
- Keymap grid with label rendering from V3 JSON
- Keycode picker with QMK labels
- Encoder map editor
- Macro sequence editor
- Lighting panel (RGB channel 2)
- Layout options panel
- Packet console (debug)
- Factory reset and bootloader jump (opt-in)

### NOT included in v1
- Multiple simultaneous device connections
- WebUSB / WebHID transport (separate adapter, not in v1 scope)
- Offline keymap editing (no device = read-only mode)
- Keymap export/import to file
- Custom firmware building / flashing
- Multi-language UI (English only)
- Touch drag-drop on mobile (desktop Chrome/Edge primary target)

## Open Questions for Implementation
1. How does the firmware BLE transport adapter structure the GATT table? (blocked on firmware side)
2. V3 definition JSON — host on GitHub Pages CDN or bundle with app?
3. Keycode label strings — bundle all QMK keycode names (large JSON) or fetch lazily?
