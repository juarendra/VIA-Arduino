# AirVIA Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a single-page web app (AirVIA) that configures VIA v13-compatible keyboards over BLE using Web Bluetooth API, with keymap grid, encoder maps, macros, lighting, layout options, and console.

**Architecture:** Three layers — `viatle-core` (pure TS protocol library), `viatle-ble` (Web Bluetooth transport + packet queue), `viatle-ui` (Svelte 5 components). Layers have no cross-dependencies. App loads V3 definition JSON to render physical keyboard layout.

**Tech Stack:** Vite, Svelte 5 (runes), TypeScript strict, Tailwind CSS, pnpm, Vitest, Web Bluetooth API (zero runtime dependencies beyond browser).

## Global Constraints

- Zero runtime npm dependencies beyond build/dev tooling
- `viatle-core` MUST have zero DOM/BLE/browser imports — pure TypeScript
- `viatle-ble` MUST have zero UI/Svelte imports
- Protocol CRC32 MUST match `VIA_Protocol.cpp` `crc32Update` exactly
- Packet size: always 32 bytes (`via::kPacketSize = 32`)
- BLE characteristic UUID: `0000FF61-0000-1000-8000-00805F9B34FB` (data), `0000FF62-...` (info)
- VIA service UUID: `0000FF60-0000-1000-8000-00805F9B34FB`
- Target browsers: Chrome 122+, Edge 122+ (Web Bluetooth API)
- Language: English only (v1)
- Svelte 5 runes for state — no external store library
- File structure: `src/core/`, `src/ble/`, `src/ui/`, `src/store/`

---

### Task 1: Project Scaffolding

**Files:**
- Create: `AirVIA/package.json`
- Create: `AirVIA/vite.config.ts`
- Create: `AirVIA/tsconfig.json`
- Create: `AirVIA/svelte.config.js`
- Create: `AirVIA/tailwind.config.js`
- Create: `AirVIA/postcss.config.js`
- Create: `AirVIA/index.html`
- Create: `AirVIA/src/main.ts`
- Create: `AirVIA/src/app.css`
- Create: `AirVIA/.gitignore`

**Interfaces:**
- Produces: runnable empty Svelte page at `pnpm dev`, all config files for subsequent tasks

- [ ] **Step 1: Create AirVIA directory and package.json**

```json
{
  "name": "airvia",
  "private": true,
  "version": "0.1.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "vite build",
    "preview": "vite preview",
    "test": "vitest run",
    "test:watch": "vitest"
  },
  "devDependencies": {
    "@sveltejs/vite-plugin-svelte": "^5.0.0",
    "@tailwindcss/vite": "^4.0.0",
    "svelte": "^5.0.0",
    "tailwindcss": "^4.0.0",
    "typescript": "^5.7.0",
    "vite": "^6.0.0",
    "vitest": "^3.0.0"
  }
}
```

- [ ] **Step 2: Create vite.config.ts**

```typescript
import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';
import tailwindcss from '@tailwindcss/vite';

export default defineConfig({
  plugins: [tailwindcss(), svelte()],
});
```

- [ ] **Step 3: Create svelte.config.js**

```javascript
import { vitePreprocess } from '@sveltejs/vite-plugin-svelte';

export default {
  preprocess: vitePreprocess(),
};
```

- [ ] **Step 4: Create tsconfig.json**

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "verbatimModuleSyntax": true,
    "jsx": "preserve",
    "paths": { "$lib/*": ["./src/*"] }
  },
  "include": ["src/**/*.ts", "src/**/*.svelte"]
}
```

- [ ] **Step 5: Create postcss.config.js**

```javascript
export default {
  plugins: { '@tailwindcss/postcss': {} },
};
```

- [ ] **Step 6: Create empty tailwind.config.js**

```javascript
export default { content: ['./src/**/*.{svelte,ts}'] };
```

- [ ] **Step 7: Create src/app.css**

```css
@import "tailwindcss";
```

- [ ] **Step 8: Create index.html**

```html
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>AirVIA</title>
  </head>
  <body class="bg-gray-950 text-gray-100 min-h-screen">
    <div id="app"></div>
    <script type="module" src="/src/main.ts"></script>
  </body>
</html>
```

- [ ] **Step 9: Create src/main.ts**

```typescript
import App from './App.svelte';
import './app.css';

const app = new App({ target: document.getElementById('app')! });
export default app;
```

- [ ] **Step 10: Create minimal src/App.svelte**

```svelte
<script lang="ts">
</script>

<div class="p-4">
  <h1 class="text-xl font-bold">AirVIA</h1>
</div>
```

- [ ] **Step 11: Create .gitignore**

```
node_modules/
dist/
.vite/
```

- [ ] **Step 12: Install dependencies and verify dev server**

```bash
pnpm install
pnpm dev
```

Expected: Vite starts, browser shows "AirVIA" heading on dark background.

- [ ] **Step 13: Commit**

```bash
git init
git add -A
git commit -m "feat: scaffold AirVIA project with Vite, Svelte 5, TypeScript, Tailwind"
```

---

### Task 2: CRC32 Implementation

**Files:**
- Create: `AirVIA/src/core/crc.ts`
- Create: `AirVIA/src/core/crc.test.ts`

**Interfaces:**
- Produces: `crc32(data: Uint8Array): number` — matches `VIA_Protocol.cpp` `crc32Update`

- [ ] **Step 1: Write failing test**

```typescript
// src/core/crc.test.ts
import { describe, it, expect } from 'vitest';
import { crc32 } from './crc';

describe('crc32', () => {
  it('returns 0 for empty input', () => {
    expect(crc32(new Uint8Array(0))).toBe(0);
  });

  it('matches known value for "123456789"', () => {
    const data = new TextEncoder().encode('123456789');
    expect(crc32(data)).toBe(0xCBF43926 >>> 0);
  });

  it('matches via protocol state magic', () => {
    const state = new Uint8Array([0x41, 0x41, 0x49, 0x56]); // "AAIV" reversed = "VIAA" magic
    // known CRC from C++ reference implementation
    expect(crc32(state)).toBe(0x31EC59AB >>> 0);
  });

  it('produces different values for different inputs', () => {
    const a = new Uint8Array([1, 2, 3]);
    const b = new Uint8Array([1, 2, 4]);
    expect(crc32(a)).not.toBe(crc32(b));
  });
});
```

- [ ] **Step 2: Verify test fails**

```bash
pnpm test -- src/core/crc.test.ts
```

Expected: FAIL — `crc32` not exported.

- [ ] **Step 3: Implement CRC32**

```typescript
// src/core/crc.ts

/**
 * CRC32 matching VIA_Protocol.cpp crc32Update.
 * Algorithm: standard CRC-32 (Ethernet/gzip), init 0xFFFFFFFF, final XOR, no reflect.
 */
export function crc32(data: Uint8Array): number {
  let crc = 0xFFFFFFFF >>> 0;
  for (let i = 0; i < data.length; i++) {
    crc ^= data[i]!;
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc >>> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
  }
  return (~crc) >>> 0;
}
```

- [ ] **Step 4: Verify tests pass**

```bash
pnpm test -- src/core/crc.test.ts
```

Expected: 4 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/crc.ts src/core/crc.test.ts
git commit -m "feat: add CRC32 matching VIA_Protocol.cpp algorithm"
```

---

### Task 3: Keycode Constants and Labels

**Files:**
- Create: `AirVIA/src/core/keycodes.ts`

**Interfaces:**
- Produces:
  - `type KeycodeCategory = 'basic' | 'modifier' | 'layer' | 'boot' | 'media' | 'system' | 'macro'`
  - `type KeycodeEntry = { code: number; label: string; category: KeycodeCategory }`
  - `KEYCODE_MAP: ReadonlyMap<number, KeycodeEntry>` — lookup by code
  - `KEYCODES_BY_CATEGORY: Readonly<Record<KeycodeCategory, KeycodeEntry[]>>` — grouped for picker
  - `classifyKeycode(code: number): KeycodeType` — mirrors `VIA_Keycodes.h` enum
  - `extractBasicUsage(code: number): number` — lower byte
  - `extractModifierMask(code: number): number` — bit position
  - `extractQkMods(code: number): { hidUsage: number; modMask: number } | null`
  - `extractLayerAction(code: number): { action: number; layer: number } | null`

- [ ] **Step 1: Define keycode type system**

```typescript
// src/core/keycodes.ts

export type KeycodeCategory = 'basic' | 'modifier' | 'layer' | 'boot'
  | 'media' | 'system' | 'macro' | 'custom';

export type KeycodeEntry = {
  code: number;
  label: string;
  category: KeycodeCategory;
};

export enum KeycodeType {
  None,
  Transparent,
  Basic,
  Modifier,
  Layer,
  Boot,
  Unsupported,
}
```

- [ ] **Step 2: Implement classification functions (mirror VIA_Keycodes.h)**

```typescript
export function classifyKeycode(code: number): KeycodeType {
  const hi = code >> 8;
  switch (hi) {
    case 0x00:
      if (code === 0x0000) return KeycodeType.None;
      if (code === 0x0001) return KeycodeType.Transparent;
      if (code >= 0x0004 && code <= 0x00A4) return KeycodeType.Basic;
      if (code >= 0x00E0 && code <= 0x00E7) return KeycodeType.Modifier;
      return KeycodeType.Unsupported;
    case 0x01: case 0x02: case 0x03: case 0x04:
    case 0x05: case 0x06: case 0x07: case 0x08:
    case 0x09: case 0x0A: case 0x0B: case 0x0C:
    case 0x0D: case 0x0E: case 0x0F: case 0x10:
    case 0x11: case 0x12: case 0x13: case 0x14:
    case 0x15: case 0x16: case 0x17: case 0x18:
    case 0x19: case 0x1A: case 0x1B: case 0x1C:
    case 0x1D: case 0x1E: case 0x1F:
      return KeycodeType.Basic;
    case 0x52:
      return KeycodeType.Layer;
    case 0x7C:
      return KeycodeType.Boot;
    default:
      return KeycodeType.Unsupported;
  }
}

export function extractBasicUsage(code: number): number {
  return code & 0xFF;
}

export function extractModifierMask(code: number): number {
  return 1 << (code - 0xE0);
}

export function extractQkMods(code: number): { hidUsage: number; modMask: number } | null {
  if (code < 0x0100 || code > 0x1FFF) return null;
  const hidUsage = code & 0xFF;
  let modMask = (code >> 8) & 0x0F;
  if (code & 0x1000) modMask <<= 4;
  return { hidUsage, modMask };
}

export function extractLayerAction(code: number): { action: number; layer: number } | null {
  if ((code & 0xFF00) !== 0x5200) return null;
  const action = (code >> 5) & 0x03;
  const layer = code & 0x1F;
  return { action, layer };
}
```

- [ ] **Step 3: Build keycode label map (QMK 0.0.8 — essential subset)**

```typescript
const RAW_MAP: [number, string, KeycodeCategory][] = [
  [0x0000, 'KC_NO', 'custom'],
  [0x0001, 'KC_TRNS', 'custom'],
  [0x0004, 'KC_A', 'basic'],
  [0x0005, 'KC_B', 'basic'],
  [0x0006, 'KC_C', 'basic'],
  [0x0007, 'KC_D', 'basic'],
  [0x0008, 'KC_E', 'basic'],
  [0x0009, 'KC_F', 'basic'],
  [0x000A, 'KC_G', 'basic'],
  [0x000B, 'KC_H', 'basic'],
  [0x000C, 'KC_I', 'basic'],
  [0x000D, 'KC_J', 'basic'],
  [0x000E, 'KC_K', 'basic'],
  [0x000F, 'KC_L', 'basic'],
  [0x0010, 'KC_M', 'basic'],
  [0x0011, 'KC_N', 'basic'],
  [0x0012, 'KC_O', 'basic'],
  [0x0013, 'KC_P', 'basic'],
  [0x0014, 'KC_Q', 'basic'],
  [0x0015, 'KC_R', 'basic'],
  [0x0016, 'KC_S', 'basic'],
  [0x0017, 'KC_T', 'basic'],
  [0x0018, 'KC_U', 'basic'],
  [0x0019, 'KC_V', 'basic'],
  [0x001A, 'KC_W', 'basic'],
  [0x001B, 'KC_X', 'basic'],
  [0x001C, 'KC_Y', 'basic'],
  [0x001D, 'KC_Z', 'basic'],
  [0x001E, 'KC_1', 'basic'],
  [0x001F, 'KC_2', 'basic'],
  [0x0020, 'KC_3', 'basic'],
  [0x0021, 'KC_4', 'basic'],
  [0x0022, 'KC_5', 'basic'],
  [0x0023, 'KC_6', 'basic'],
  [0x0024, 'KC_7', 'basic'],
  [0x0025, 'KC_8', 'basic'],
  [0x0026, 'KC_9', 'basic'],
  [0x0027, 'KC_0', 'basic'],
  [0x0028, 'KC_ENTER', 'basic'],
  [0x0029, 'KC_ESCAPE', 'basic'],
  [0x002A, 'KC_BSPACE', 'basic'],
  [0x002B, 'KC_TAB', 'basic'],
  [0x002C, 'KC_SPACE', 'basic'],
  [0x002D, 'KC_MINUS', 'basic'],
  [0x002E, 'KC_EQUAL', 'basic'],
  [0x002F, 'KC_LBRACKET', 'basic'],
  [0x0030, 'KC_RBRACKET', 'basic'],
  [0x0031, 'KC_BSLASH', 'basic'],
  [0x0033, 'KC_SCOLON', 'basic'],
  [0x0034, 'KC_QUOTE', 'basic'],
  [0x0035, 'KC_GRAVE', 'basic'],
  [0x0036, 'KC_COMMA', 'basic'],
  [0x0037, 'KC_DOT', 'basic'],
  [0x0038, 'KC_SLASH', 'basic'],
  [0x0039, 'KC_CAPSLOCK', 'basic'],
  [0x003A, 'KC_F1', 'basic'],
  [0x003B, 'KC_F2', 'basic'],
  [0x003C, 'KC_F3', 'basic'],
  [0x003D, 'KC_F4', 'basic'],
  [0x003E, 'KC_F5', 'basic'],
  [0x003F, 'KC_F6', 'basic'],
  [0x0040, 'KC_F7', 'basic'],
  [0x0041, 'KC_F8', 'basic'],
  [0x0042, 'KC_F9', 'basic'],
  [0x0043, 'KC_F10', 'basic'],
  [0x0044, 'KC_F11', 'basic'],
  [0x0045, 'KC_F12', 'basic'],
  [0x0046, 'KC_PSCREEN', 'basic'],
  [0x0047, 'KC_SCROLLLOCK', 'basic'],
  [0x0048, 'KC_PAUSE', 'basic'],
  [0x0049, 'KC_INSERT', 'basic'],
  [0x004A, 'KC_HOME', 'basic'],
  [0x004B, 'KC_PGUP', 'basic'],
  [0x004C, 'KC_DELETE', 'basic'],
  [0x004D, 'KC_END', 'basic'],
  [0x004E, 'KC_PGDOWN', 'basic'],
  [0x004F, 'KC_RIGHT', 'basic'],
  [0x0050, 'KC_LEFT', 'basic'],
  [0x0051, 'KC_DOWN', 'basic'],
  [0x0052, 'KC_UP', 'basic'],
  [0x0053, 'KC_NUMLOCK', 'basic'],
  [0x0054, 'KC_KP_SLASH', 'basic'],
  [0x0055, 'KC_KP_ASTERISK', 'basic'],
  [0x0056, 'KC_KP_MINUS', 'basic'],
  [0x0057, 'KC_KP_PLUS', 'basic'],
  [0x0058, 'KC_KP_ENTER', 'basic'],
  [0x0059, 'KC_KP_1', 'basic'],
  [0x005A, 'KC_KP_2', 'basic'],
  [0x005B, 'KC_KP_3', 'basic'],
  [0x005C, 'KC_KP_4', 'basic'],
  [0x005D, 'KC_KP_5', 'basic'],
  [0x005E, 'KC_KP_6', 'basic'],
  [0x005F, 'KC_KP_7', 'basic'],
  [0x0060, 'KC_KP_8', 'basic'],
  [0x0061, 'KC_KP_9', 'basic'],
  [0x0062, 'KC_KP_0', 'basic'],
  [0x0063, 'KC_KP_DOT', 'basic'],
  [0x0064, 'KC_NONUS_BSLASH', 'basic'],
  [0x0065, 'KC_APPLICATION', 'basic'],
  [0x0066, 'KC_POWER', 'system'],
  [0x0067, 'KC_KP_EQUAL', 'basic'],
  [0x0068, 'KC_F13', 'basic'],
  [0x0069, 'KC_F14', 'basic'],
  [0x006A, 'KC_F15', 'basic'],
  [0x006B, 'KC_F16', 'basic'],
  [0x006C, 'KC_F17', 'basic'],
  [0x006D, 'KC_F18', 'basic'],
  [0x006E, 'KC_F19', 'basic'],
  [0x006F, 'KC_F20', 'basic'],
  [0x0070, 'KC_F21', 'basic'],
  [0x0071, 'KC_F22', 'basic'],
  [0x0072, 'KC_F23', 'basic'],
  [0x0073, 'KC_F24', 'basic'],
  [0x0082, 'KC_MUTE', 'media'],
  [0x0083, 'KC_VOLU', 'media'],
  [0x0084, 'KC_VOLD', 'media'],
  [0x00B5, 'KC_MNXT', 'media'],
  [0x00B6, 'KC_MPRV', 'media'],
  [0x00B7, 'KC_MSTP', 'media'],
  [0x00CD, 'KC_MPLY', 'media'],
  [0x00E0, 'KC_LCTL', 'modifier'],
  [0x00E1, 'KC_LSFT', 'modifier'],
  [0x00E2, 'KC_LALT', 'modifier'],
  [0x00E3, 'KC_LGUI', 'modifier'],
  [0x00E4, 'KC_RCTL', 'modifier'],
  [0x00E5, 'KC_RSFT', 'modifier'],
  [0x00E6, 'KC_RALT', 'modifier'],
  [0x00E7, 'KC_RGUI', 'modifier'],
  [0x7C00, 'QK_BOOT', 'boot'],
];

const LAYER_ACTIONS: [number, string, KeycodeCategory][] = [];
for (let layer = 0; layer < 32; layer++) {
  LAYER_ACTIONS.push([0x5200 | (0 << 5) | layer, `MO(${layer})`, 'layer']);
  LAYER_ACTIONS.push([0x5200 | (1 << 5) | layer, `TG(${layer})`, 'layer']);
  LAYER_ACTIONS.push([0x5200 | (2 << 5) | layer, `TO(${layer})`, 'layer']);
  LAYER_ACTIONS.push([0x5200 | (3 << 5) | layer, `DF(${layer})`, 'layer']);
}
```


- [ ] **Step 4: Build lookup maps**

```typescript
const ALL_ENTRIES: KeycodeEntry[] = [
  ...RAW_MAP.map(([code, label, category]) => ({ code, label, category })),
  ...LAYER_ACTIONS.map(([code, label, category]) => ({ code, label, category })),
];

export const KEYCODE_MAP: ReadonlyMap<number, KeycodeEntry> = new Map(
  ALL_ENTRIES.map(e => [e.code, e])
);

function keycodeLabel(code: number): string {
  const entry = KEYCODE_MAP.get(code);
  if (entry) return entry.label;
  const cat = classifyKeycode(code);
  if (cat === KeycodeType.Basic) return `KC_${code.toString(16).toUpperCase().padStart(4, '0')}`;
  return `0x${code.toString(16).toUpperCase().padStart(4, '0')}`;
}

export { keycodeLabel };

export const KEYCODES_BY_CATEGORY: Readonly<Record<KeycodeCategory, KeycodeEntry[]>> = (() => {
  const groups: Record<KeycodeCategory, KeycodeEntry[]> = {
    basic: [], modifier: [], layer: [], boot: [], media: [], system: [], macro: [], custom: [],
  };
  for (const entry of ALL_ENTRIES) {
    groups[entry.category].push(entry);
  }
  return groups;
})();

export const CATEGORY_LABELS: Record<KeycodeCategory, string> = {
  basic: 'Basic',
  modifier: 'Modifier',
  layer: 'Layer',
  boot: 'Boot',
  media: 'Media',
  system: 'System',
  macro: 'Macro',
  custom: 'Special',
};
```

- [ ] **Step 5: Commit**

```bash
git add src/core/keycodes.ts
git commit -m "feat: add QMK 0.0.8 keycode classification and label map"
```

---

### Task 4: V3 Definition Parser

**Files:**
- Create: `AirVIA/src/core/v3-definition.ts`

**Interfaces:**
- Produces:
  - `type V3KeyPosition = { x: number; y: number; w?: number; h?: number; r?: number; rx?: number; ry?: number }`
  - `type V3Layout = { keymap: V3KeyPosition[]; labels?: (string | string[])[] }`
  - `type V3Definition = { name: string; vendorId: string; productId: string; matrix: { rows: number; cols: number }; layouts: V3Layout; encoders?: number }`
  - `parseV3Definition(json: string): V3Definition` — validates and returns typed object

- [ ] **Step 1: Define types and parser**

```typescript
// src/core/v3-definition.ts

export type V3KeyPosition = {
  x: number;
  y: number;
  w?: number;
  h?: number;
  r?: number;
  rx?: number;
  ry?: number;
};

export type V3Layout = {
  keymap: V3KeyPosition[];
  labels?: (string | string[])[];
};

/** Options info from V3 definitions */
export type V3OptionInfo = string[];

export type V3Definition = {
  name: string;
  vendorId: string;
  productId: string;
  matrix: { rows: number; cols: number };
  layouts: V3Layout;
  encoders?: number;
};

export class V3ParseError extends Error {
  constructor(message: string) {
    super(`V3 definition error: ${message}`);
    this.name = 'V3ParseError';
  }
}

export function parseV3Definition(json: string): V3Definition {
  let raw: Record<string, unknown>;
  try {
    raw = JSON.parse(json);
  } catch {
    throw new V3ParseError('Invalid JSON');
  }

  if (typeof raw.name !== 'string') throw new V3ParseError('Missing or invalid "name"');
  if (typeof raw.vendorId !== 'string') throw new V3ParseError('Missing or invalid "vendorId"');
  if (typeof raw.productId !== 'string') throw new V3ParseError('Missing or invalid "productId"');

  const matrix = raw.matrix as Record<string, unknown> | undefined;
  if (!matrix || typeof matrix.rows !== 'number' || typeof matrix.cols !== 'number') {
    throw new V3ParseError('Missing or invalid "matrix" with rows/cols');
  }
  if (matrix.rows < 1 || matrix.cols < 1 || matrix.rows > 32 || matrix.cols > 32) {
    throw new V3ParseError('Matrix dimensions must be 1-32');
  }

  const layouts = raw.layouts as Record<string, unknown> | undefined;
  if (!layouts || !Array.isArray(layouts.keymap)) {
    throw new V3ParseError('Missing or invalid "layouts.keymap"');
  }

  const keymap = (layouts.keymap as unknown[]).map((key, index) => {
    const k = key as Record<string, unknown>;
    if (typeof k.x !== 'number' || typeof k.y !== 'number') {
      throw new V3ParseError(`Key at index ${index} missing x/y`);
    }
    const pos: V3KeyPosition = { x: k.x, y: k.y };
    if (typeof k.w === 'number') pos.w = k.w;
    if (typeof k.h === 'number') pos.h = k.h;
    if (typeof k.r === 'number') pos.r = k.r;
    if (typeof k.rx === 'number') pos.rx = k.rx;
    if (typeof k.ry === 'number') pos.ry = k.ry;
    return pos;
  });

  if (keymap.length !== matrix.rows * matrix.cols) {
    throw new V3ParseError(
      `Keymap entries (${keymap.length}) must equal rows*cols (${matrix.rows * matrix.cols})`
    );
  }

  const labels = layouts.labels as (string | string[])[] | undefined;

  let encoders: number | undefined;
  if (raw.encoders !== undefined) {
    if (typeof raw.encoders !== 'number' || raw.encoders < 0) {
      throw new V3ParseError('Invalid "encoders"');
    }
    encoders = raw.encoders;
  }

  return {
    name: raw.name,
    vendorId: raw.vendorId,
    productId: raw.productId,
    matrix: { rows: matrix.rows, cols: matrix.cols },
    layouts: { keymap, labels },
    encoders,
  };
}
```

- [ ] **Step 2: Write inline demo / self-assert in same file**

```typescript
// Self-check (run with: pnpm vitest src/core/v3-definition.ts)
if (import.meta.vitest) {
  const { describe, it, expect } = import.meta.vitest;

  const validJson = JSON.stringify({
    name: 'Test KB',
    vendorId: '0xFEED',
    productId: '0x0001',
    matrix: { rows: 2, cols: 2 },
    layouts: {
      keymap: [
        { x: 0, y: 0 }, { x: 1, y: 0 },
        { x: 0, y: 1 }, { x: 1, y: 1 },
      ],
      labels: ['A', 'B', 'C', 'D'],
    },
    encoders: 2,
  });

  describe('parseV3Definition', () => {
    it('parses valid JSON', () => {
      const def = parseV3Definition(validJson);
      expect(def.name).toBe('Test KB');
      expect(def.matrix.rows).toBe(2);
      expect(def.matrix.cols).toBe(2);
      expect(def.layouts.keymap.length).toBe(4);
      expect(def.encoders).toBe(2);
    });

    it('rejects missing name', () => {
      expect(() => parseV3Definition('{}')).toThrow(V3ParseError);
    });

    it('rejects wrong keymap length', () => {
      const bad = JSON.stringify({
        name: 'X', vendorId: '0', productId: '0',
        matrix: { rows: 2, cols: 2 },
        layouts: { keymap: [{ x: 0, y: 0 }] },
      });
      expect(() => parseV3Definition(bad)).toThrow(/entries/);
    });

    it('rejects invalid JSON', () => {
      expect(() => parseV3Definition('not json')).toThrow(V3ParseError);
    });

    it('rejects oversized matrix', () => {
      const bad = JSON.stringify({
        name: 'X', vendorId: '0', productId: '0',
        matrix: { rows: 33, cols: 1 },
        layouts: { keymap: Array.from({ length: 33 }, () => ({ x: 0, y: 0 })) },
      });
      expect(() => parseV3Definition(bad)).toThrow(/dimensions/);
    });
  });
}
```

- [ ] **Step 3: Run self-tests**

```bash
pnpm test -- src/core/v3-definition.ts
```

Expected: 5 tests PASS.

- [ ] **Step 4: Commit**

```bash
git add src/core/v3-definition.ts
git commit -m "feat: add V3 definition JSON parser with validation"
```

---

### Task 5: Protocol Core — Packet Builder and Parser

**Files:**
- Create: `AirVIA/src/core/protocol.ts`
- Create: `AirVIA/src/core/protocol.test.ts`

**Interfaces:**
- Consumes: `crc32` from `crc.ts`
- Produces:
  - `PACKET_SIZE = 32`
  - `type RawPacket = number[]` — 32 bytes
  - `createPacket(command: number, ...args: number[]): RawPacket`
  - `Protocol` class with methods for all 23 VIA commands, each builds a request packet
  - `Protocol.parseResponse(packet: RawPacket): VIACommandResponse` — parse response
  - `type VIACommandResponse = { error?: string; result: Record<string, unknown> }`

- [ ] **Step 1: Define types and packet utilities**

```typescript
// src/core/protocol.ts

export const PACKET_SIZE = 32;

export type RawPacket = number[];

export function createPacket(command: number, ...args: number[]): RawPacket {
  const pkt = new Array<number>(PACKET_SIZE).fill(0);
  pkt[0] = command;
  for (let i = 0; i < args.length && i + 1 < PACKET_SIZE; i++) {
    pkt[i + 1] = args[i]!;
  }
  return pkt;
}

export function parseU32(packet: RawPacket, offset: number): number {
  return ((packet[offset]! << 24) | (packet[offset + 1]! << 16)
        | (packet[offset + 2]! << 8) | packet[offset + 3]!) >>> 0;
}

export function parseU16BE(packet: RawPacket, offset: number): number {
  return (packet[offset]! << 8) | packet[offset + 1]!;
}

export function isError(packet: RawPacket): boolean {
  return packet[0] === 0xFF;
}
```

- [ ] **Step 2: Implement Protocol class with all 23 command builders**

```typescript
export class Protocol {
  // --- Device Info ---

  static getProtocolVersion(): RawPacket {
    return createPacket(0x01);
  }

  static getUptime(): RawPacket {
    return createPacket(0x02, 0x01);
  }

  static getLayoutOptions(): RawPacket {
    return createPacket(0x02, 0x02);
  }

  static getMatrixState(startRow: number): RawPacket {
    return createPacket(0x02, 0x03, startRow);
  }

  static getFirmwareVersion(): RawPacket {
    return createPacket(0x02, 0x04);
  }

  static getQmkVersion(): RawPacket {
    return createPacket(0x02, 0x06);
  }

  static setDeviceIndication(value: number): RawPacket {
    return createPacket(0x03, 0x05, value);
  }

  static setLayoutOptions(options: number): RawPacket {
    return createPacket(0x03, 0x02,
      (options >> 24) & 0xFF, (options >> 16) & 0xFF,
      (options >> 8) & 0xFF, options & 0xFF);
  }

  // --- Keymap ---

  static getKeycode(layer: number, row: number, col: number): RawPacket {
    return createPacket(0x04, layer, row, col);
  }

  static setKeycode(layer: number, row: number, col: number, codeHi: number, codeLo: number): RawPacket {
    return createPacket(0x05, layer, row, col, codeHi, codeLo);
  }

  static resetKeymap(): RawPacket {
    return createPacket(0x06);
  }

  // --- Custom Values ---

  static setCustomValue(channel: number, sub: number, b3: number, b4 = 0): RawPacket {
    return createPacket(0x07, channel, sub, b3, b4);
  }

  static getCustomValue(channel: number, sub: number): RawPacket {
    return createPacket(0x08, channel, sub);
  }

  static saveCustomValue(channel: number): RawPacket {
    return createPacket(0x09, channel, 0x02);
  }

  // --- System ---

  static factoryReset(): RawPacket {
    return createPacket(0x0A);
  }

  static bootloaderJump(): RawPacket {
    return createPacket(0x0B);
  }

  // --- Macros ---

  static getMacroCount(): RawPacket {
    return createPacket(0x0C);
  }

  static getMacroBufferSize(): RawPacket {
    return createPacket(0x0D);
  }

  static getMacroBuffer(offset: number, size: number): RawPacket {
    return createPacket(0x0E, (offset >> 8) & 0xFF, offset & 0xFF, Math.min(size, 28));
  }

  static setMacroBuffer(offset: number, data: number[]): RawPacket {
    const pkt = createPacket(0x0F, (offset >> 8) & 0xFF, offset & 0xFF,
                             Math.min(data.length, 28));
    for (let i = 0; i < Math.min(data.length, 28); i++) {
      pkt[4 + i] = data[i]!;
    }
    return pkt;
  }

  static resetMacros(): RawPacket {
    return createPacket(0x10);
  }

  // --- Layers ---

  static getLayerCount(): RawPacket {
    return createPacket(0x11);
  }

  static getKeymapBuffer(offset: number, size: number): RawPacket {
    return createPacket(0x12, (offset >> 8) & 0xFF, offset & 0xFF, Math.min(size, 28));
  }

  static setKeymapBuffer(offset: number, data: number[]): RawPacket {
    const pkt = createPacket(0x13, (offset >> 8) & 0xFF, offset & 0xFF,
                             Math.min(data.length, 28));
    for (let i = 0; i < Math.min(data.length, 28); i++) {
      pkt[4 + i] = data[i]!;
    }
    return pkt;
  }

  // --- Encoders ---

  static getEncoderKeycode(layer: number, encoder: number, clockwise: boolean): RawPacket {
    return createPacket(0x14, layer, encoder, clockwise ? 1 : 0);
  }

  static setEncoderKeycode(layer: number, encoder: number, clockwise: boolean,
                           codeHi: number, codeLo: number): RawPacket {
    return createPacket(0x15, layer, encoder, clockwise ? 1 : 0, codeHi, codeLo);
  }
}
```

- [ ] **Step 3: Write unit tests**

```typescript
// src/core/protocol.test.ts

import { describe, it, expect } from 'vitest';
import { Protocol, createPacket, PACKET_SIZE, parseU32, parseU16BE, isError } from './protocol';

describe('createPacket', () => {
  it('creates 32-byte packet', () => {
    const pkt = createPacket(0x01);
    expect(pkt.length).toBe(PACKET_SIZE);
    expect(pkt[0]).toBe(0x01);
    expect(pkt.every((b, i) => i === 0 || b === 0)).toBe(true);
  });

  it('fills command arguments', () => {
    const pkt = createPacket(0x05, 1, 2, 3, 0x12, 0x34);
    expect(pkt[0]).toBe(0x05);
    expect(pkt[1]).toBe(1);
    expect(pkt[2]).toBe(2);
    expect(pkt[3]).toBe(3);
    expect(pkt[4]).toBe(0x12);
    expect(pkt[5]).toBe(0x34);
  });
});

describe('parse utils', () => {
  it('parseU32', () => {
    const pkt = createPacket(0, 0x12, 0x34, 0x56, 0x78);
    expect(parseU32(pkt, 1)).toBe(0x12345678);
  });

  it('parseU16BE', () => {
    const pkt = createPacket(0, 0xAB, 0xCD);
    expect(parseU16BE(pkt, 1)).toBe(0xABCD);
  });

  it('isError', () => {
    expect(isError([0xFF, ...new Array(31).fill(0)])).toBe(true);
    expect(isError([0x01, ...new Array(31).fill(0)])).toBe(false);
  });
});

describe('Protocol', () => {
  it('getProtocolVersion', () => {
    const pkt = Protocol.getProtocolVersion();
    expect(pkt[0]).toBe(0x01);
  });

  it('setKeycode packs hi/lo bytes', () => {
    const pkt = Protocol.setKeycode(0, 2, 3, 0x12, 0x34);
    expect(pkt[0]).toBe(0x05);
    expect(pkt[1]).toBe(0);
    expect(pkt[2]).toBe(2);
    expect(pkt[3]).toBe(3);
    expect(pkt[4]).toBe(0x12);
    expect(pkt[5]).toBe(0x34);
  });

  it('setEncoderKeycode', () => {
    const pkt = Protocol.setEncoderKeycode(1, 0, true, 0xAB, 0xCD);
    expect(pkt[0]).toBe(0x15);
    expect(pkt[1]).toBe(1);
    expect(pkt[2]).toBe(0);
    expect(pkt[3]).toBe(1);
    expect(pkt[4]).toBe(0xAB);
    expect(pkt[5]).toBe(0xCD);
  });

  it('setMacroBuffer caps at 28 bytes', () => {
    const data = new Array(40).fill(0xAA);
    const pkt = Protocol.setMacroBuffer(0x100, data);
    expect(pkt[3]).toBe(28);
    for (let i = 4; i < 32; i++) expect(pkt[i]).toBe(0xAA);
  });

  it('setKeymapBuffer respects 28-byte limit', () => {
    const data = new Array(30).fill(0xBB);
    const pkt = Protocol.setKeymapBuffer(0, data);
    expect(pkt[3]).toBe(28);
  });
});
```

- [ ] **Step 4: Run tests**

```bash
pnpm test -- src/core/protocol.test.ts
```

Expected: 7 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/core/protocol.ts src/core/protocol.test.ts
git commit -m "feat: add VIA v13 protocol packet builders for all 23 commands"
```

---

### Task 6: Packet Queue with Retry

**Files:**
- Create: `AirVIA/src/ble/queue.ts`
- Create: `AirVIA/src/ble/queue.test.ts`

**Interfaces:**
- Consumes: `RawPacket` type from `protocol.ts`
- Produces:
  - `type QueueConfig = { retries: number; timeoutMs: number; maxDepth: number }`
  - `class PacketQueue` — `enqueue(pkt: RawPacket): void`, `inFlight(): RawPacket | null`, `resolve(response: RawPacket): void`, `timeout(): void`

- [ ] **Step 1: Write failing test**

```typescript
// src/ble/queue.test.ts

import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { PacketQueue } from './queue';
import type { RawPacket } from '../core/protocol';

function pkt(cmd: number): RawPacket {
  return new Array(32).fill(0).map((_, i) => i === 0 ? cmd : 0);
}

describe('PacketQueue', () => {
  beforeEach(() => { vi.useFakeTimers(); });
  afterEach(() => { vi.useRealTimers(); });

  it('dequeues first enqueued packet', () => {
    const q = new PacketQueue({ retries: 3, timeoutMs: 500, maxDepth: 8 });
    q.enqueue(pkt(0x01));
    q.enqueue(pkt(0x02));
    expect(q.inFlight()![0]).toBe(0x01);
  });

  it('returns null when empty', () => {
    const q = new PacketQueue({ retries: 3, timeoutMs: 500, maxDepth: 8 });
    expect(q.inFlight()).toBeNull();
  });

  it('resolve advances queue', () => {
    const q = new PacketQueue({ retries: 3, timeoutMs: 500, maxDepth: 8 });
    q.enqueue(pkt(0x01));
    q.enqueue(pkt(0x02));
    q.resolve(pkt(0x01)); // resolve first
    expect(q.inFlight()![0]).toBe(0x02);
  });

  it('retry on timeout', () => {
    const q = new PacketQueue({ retries: 3, timeoutMs: 500, maxDepth: 8 });
    const cb = vi.fn();
    q.enqueue(pkt(0x01));

    expect(q.inFlight()![0]).toBe(0x01);
    q.timeout(); // first timeout
    expect(q.inFlight()![0]).toBe(0x01);
    q.timeout(); // second
    expect(q.inFlight()![0]).toBe(0x01);
    q.timeout(); // third — exhausted
    expect(q.inFlight()).toBeNull();
  });

  it('stops retrying after max retries', () => {
    const q = new PacketQueue({ retries: 1, timeoutMs: 100, maxDepth: 8 });
    q.enqueue(pkt(0x01));
    q.enqueue(pkt(0x02));
    q.timeout(); // retry 1 exhausted
    expect(q.inFlight()).toBeNull();
  });

  it('rejects enqueue when full', () => {
    const q = new PacketQueue({ retries: 1, timeoutMs: 100, maxDepth: 2 });
    q.enqueue(pkt(0x01));
    q.enqueue(pkt(0x02));
    expect(() => q.enqueue(pkt(0x03))).toThrow(/full/);
  });

  it('clear empties queue', () => {
    const q = new PacketQueue({ retries: 3, timeoutMs: 500, maxDepth: 8 });
    q.enqueue(pkt(0x01));
    q.enqueue(pkt(0x02));
    q.clear();
    expect(q.inFlight()).toBeNull();
  });
});
```

- [ ] **Step 2: Implement PacketQueue**

```typescript
// src/ble/queue.ts

import type { RawPacket } from '../core/protocol';

export type QueueConfig = {
  retries: number;
  timeoutMs: number;
  maxDepth: number;
};

export class PacketQueue {
  #pending: RawPacket[] = [];
  #inFlight: RawPacket | null = null;
  #retriesLeft = 0;
  #config: QueueConfig;

  constructor(config: QueueConfig) {
    this.#config = config;
  }

  enqueue(packet: RawPacket): void {
    if (this.#pending.length >= this.#config.maxDepth) {
      throw new Error('Packet queue full');
    }
    this.#pending.push([...packet]);
  }

  inFlight(): RawPacket | null {
    if (this.#inFlight) return this.#inFlight;

    const next = this.#pending.shift();
    if (!next) return null;

    this.#inFlight = next;
    this.#retriesLeft = this.#config.retries;
    return this.#inFlight;
  }

  resolve(_response: RawPacket): void {
    this.#inFlight = null;
  }

  timeout(): void {
    this.#retriesLeft--;
    if (this.#retriesLeft < 0) {
      this.#inFlight = null;
    }
  }

  clear(): void {
    this.#pending = [];
    this.#inFlight = null;
    this.#retriesLeft = 0;
  }

  get pendingCount(): number {
    return this.#pending.length;
  }
}
```

- [ ] **Step 3: Run tests**

```bash
pnpm test -- src/ble/queue.test.ts
```

Expected: 6 tests PASS.

- [ ] **Step 4: Commit**

```bash
git add src/ble/queue.ts src/ble/queue.test.ts
git commit -m "feat: add BLE packet queue with retry and max depth"
```

---

### Task 7: Web Bluetooth Transport Adapter

**Files:**
- Create: `AirVIA/src/ble/transport.ts`

**Interfaces:**
- Consumes: `RawPacket`, `PACKET_SIZE` from `protocol.ts`, `PacketQueue` from `queue.ts`
- Produces:
  - `VIA_SERVICE_UUID = '0000ff60-0000-1000-8000-00805f9b34fb'`
  - `VIA_DATA_CHAR_UUID = '0000ff61-0000-1000-8000-00805f9b34fb'`
  - `VIA_INFO_CHAR_UUID = '0000ff62-0000-1000-8000-00805f9b34fb'`
  - `type TransportState = 'disconnected' | 'connecting' | 'connected' | 'error'`
  - `class BLETransport` — `connect(): Promise<void>`, `disconnect(): void`, `send(packet: RawPacket): void`, `onResponse: ((pkt: RawPacket) => void) | null`, `onStateChange: ((s: TransportState) => void) | null`, `readInfo(): Promise<RawPacket | null>`

- [ ] **Step 1: Implement BLETransport**

```typescript
// src/ble/transport.ts

import type { RawPacket } from '../core/protocol';
import { PACKET_SIZE } from '../core/protocol';
import { PacketQueue } from './queue';

export const VIA_SERVICE_UUID = '0000ff60-0000-1000-8000-00805f9b34fb';
export const VIA_DATA_CHAR_UUID = '0000ff61-0000-1000-8000-00805f9b34fb';
export const VIA_INFO_CHAR_UUID = '0000ff62-0000-1000-8000-00805f9b34fb';

export type TransportState = 'disconnected' | 'connecting' | 'connected' | 'error';

const QUEUE_CONFIG = { retries: 3, timeoutMs: 500, maxDepth: 8 };

export class BLETransport {
  #device: BluetoothDevice | null = null;
  #server: BluetoothRemoteGATTServer | null = null;
  #char: BluetoothRemoteGATTCharacteristic | null = null;
  #queue = new PacketQueue(QUEUE_CONFIG);
  #timeoutId: ReturnType<typeof setTimeout> | null = null;
  #state: TransportState = 'disconnected';

  onResponse: ((pkt: RawPacket) => void) | null = null;
  onStateChange: ((s: TransportState) => void) | null = null;

  get state(): TransportState { return this.#state; }

  #setState(s: TransportState): void {
    this.#state = s;
    this.onStateChange?.(s);
  }

  async connect(): Promise<void> {
    this.#setState('connecting');
    try {
      this.#device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [VIA_SERVICE_UUID] }],
        optionalServices: [VIA_SERVICE_UUID],
      });

      this.#device.addEventListener('gattserverdisconnected', () => {
        this.#queue.clear();
        this.#char = null;
        this.#server = null;
        this.#setState('disconnected');
      });

      this.#server = await this.#device.gatt!.connect();
      const service = await this.#server.getPrimaryService(VIA_SERVICE_UUID);
      this.#char = await service.getCharacteristic(VIA_DATA_CHAR_UUID);

      await this.#char.startNotifications();
      this.#char.addEventListener('characteristicvaluechanged', (e) => {
        const pkt: RawPacket = new Array<number>(PACKET_SIZE).fill(0);
        const dv = (e.target as BluetoothRemoteGATTCharacteristic).value!;
        for (let i = 0; i < Math.min(dv.byteLength, PACKET_SIZE); i++) {
          pkt[i] = dv.getUint8(i);
        }
        this.#queue.resolve(pkt);
        this.onResponse?.(pkt);
        this.#startTimeout();
      });

      this.#setState('connected');
      this.#startTimeout();
    } catch (err) {
      this.#setState('error');
      throw err;
    }
  }

  disconnect(): void {
    this.#queue.clear();
    if (this.#timeoutId) { clearTimeout(this.#timeoutId); this.#timeoutId = null; }
    if (this.#device?.gatt?.connected) this.#device.gatt.disconnect();
    this.#char = null;
    this.#server = null;
    this.#device = null;
    this.#setState('disconnected');
  }

  send(packet: RawPacket): void {
    this.#queue.enqueue(packet);
    this.#flushQueue();
  }

  async readInfo(): Promise<RawPacket | null> {
    if (!this.#server) return null;
    try {
      const service = await this.#server.getPrimaryService(VIA_SERVICE_UUID);
      const char = await service.getCharacteristic(VIA_INFO_CHAR_UUID);
      const dv = await char.readValue();
      const pkt: RawPacket = new Array<number>(PACKET_SIZE).fill(0);
      for (let i = 0; i < Math.min(dv.byteLength, PACKET_SIZE); i++) {
        pkt[i] = dv.getUint8(i);
      }
      return pkt;
    } catch {
      return null;
    }
  }

  async #flushQueue(): Promise<void> {
    if (!this.#char) return;
    while (true) {
      const pkt = this.#queue.inFlight();
      if (!pkt) return;
      try {
        const buf = new Uint8Array(pkt);
        await this.#char.writeValueWithoutResponse(buf);
        this.#startTimeout();
        return; // wait for notify
      } catch {
        this.#queue.timeout();
        // loop: next queued or same with retry
      }
    }
  }

  #startTimeout(): void {
    if (this.#timeoutId) clearTimeout(this.#timeoutId);
    this.#timeoutId = setTimeout(() => {
      this.#queue.timeout();
      this.#flushQueue();
    }, QUEUE_CONFIG.timeoutMs);
  }
}
```

- [ ] **Step 2: Add note about browser support**

Add a comment at the top of the file:

```typescript
// Requires Chrome 122+ or Edge 122+ with Web Bluetooth enabled.
// Does NOT work in Firefox/Safari (no Web Bluetooth API).
// Users must enable chrome://flags/#enable-experimental-web-platform-features
// if the API is not yet exposed by default.
```

- [ ] **Step 3: Commit**

```bash
git add src/ble/transport.ts
git commit -m "feat: add Web Bluetooth transport adapter for VIA GATT service"
```

---

### Task 8: Svelte Runes Global State

**Files:**
- Create: `AirVIA/src/store/app.svelte.ts`

**Interfaces:**
- Consumes: `V3Definition` from `v3-definition.ts`, `TransportState` from `transport.ts`, `RawPacket` from `protocol.ts`, `KeycodeEntry` from `keycodes.ts`
- Produces: single module with reactive state via Svelte 5 `$state` runes, export functions for all mutations

- [ ] **Step 1: Define state and exported accessors**

```typescript
// src/store/app.svelte.ts

import type { V3Definition } from '../core/v3-definition';
import type { TransportState } from '../ble/transport';
import type { RawPacket } from '../core/protocol';
import type { KeycodeEntry } from '../core/keycodes';
import { keycodeLabel } from '../core/keycodes';

// --- Reactive state ---

let definition = $state<V3Definition | null>(null);
let connectionState = $state<TransportState>('disconnected');
let activeLayer = $state(0);
let activeTab = $state('keymap');
let keymap = $state<number[]>([]);           // flat [layer][row*cols] of uint16
let layerCount = $state(1);
let encoderCount = $state(0);
let encoderMap = $state<number[]>([]);       // flat [layer][encoder*2] cw,ccw
let macroCount = $state(0);
let macroBytes = $state(0);
let macroBuffer = $state<number[]>([]);      // raw bytes
let layoutOptions = $state(0);
let lightingBrightness = $state(128);
let lightingEffect = $state(0);
let lightingSpeed = $state(3);
let lightingHue = $state(0);
let lightingSaturation = $state(255);
let selectedCell = $state<{ layer: number; row: number; col: number } | null>(null);
let packetLog = $state<{ dir: 'tx' | 'rx'; hex: string }[]>([]);
let selectedKeycode: KeycodeEntry | null = $state(null);

// --- Getters ---

export function getDefinition() { return definition; }
export function getConnectionState() { return connectionState; }
export function getActiveLayer() { return activeLayer; }
export function getActiveTab() { return activeTab; }
export function getKeymap() { return keymap; }
export function getLayerCount() { return layerCount; }
export function getEncoderCount() { return encoderCount; }
export function getEncoderMap() { return encoderMap; }
export function getMacroCount() { return macroCount; }
export function getMacroBytes() { return macroBytes; }
export function getMacroBuffer() { return macroBuffer; }
export function getLayoutOptions() { return layoutOptions; }
export function getLighting() {
  return { brightness: lightingBrightness, effect: lightingEffect, speed: lightingSpeed,
           hue: lightingHue, saturation: lightingSaturation };
}
export function getSelectedCell() { return selectedCell; }
export function getPacketLog() { return packetLog; }
export function getKeycodeLabel(code: number): string { return keycodeLabel(code); }
export function getMatchedKeycode(search: string): KeycodeEntry[] {
  // pony tail: return empty for now, KeycodePicker handles its own filtering
  return [];
}

// --- Setters ---

export function setDefinition(def: V3Definition | null) { definition = def; }
export function setConnectionState(s: TransportState) { connectionState = s; }
export function setActiveLayer(l: number) { activeLayer = l; }
export function setActiveTab(t: string) { activeTab = t; }
export function setKeymap(km: number[]) { keymap = km; }
export function setLayerCount(n: number) { layerCount = n; }
export function setEncoderCount(n: number) { encoderCount = n; }
export function setEncoderMap(em: number[]) { encoderMap = em; }
export function setMacroCount(n: number) { macroCount = n; }
export function setMacroBytes(n: number) { macroBytes = n; }
export function setMacroBuffer(buf: number[]) { macroBuffer = buf; }
export function setLayoutOptions(opts: number) { layoutOptions = opts; }
export function setLightingBrightness(v: number) { lightingBrightness = v; }
export function setLightingEffect(v: number) { lightingEffect = v; }
export function setLightingSpeed(v: number) { lightingSpeed = v; }
export function setLightingHue(v: number) { lightingHue = v; }
export function setLightingSaturation(v: number) { lightingSaturation = v; }
export function setSelectedCell(cell: { layer: number; row: number; col: number } | null) { selectedCell = cell; }
export function addPacketLog(dir: 'tx' | 'rx', pkt: RawPacket) {
  const hex = pkt.slice(0, 8).map(b => b.toString(16).padStart(2, '0')).join(' ') + '...';
  packetLog = [...packetLog.slice(-99), { dir, hex }];
}

/**
 * Get keycode at matrix position for active layer.
 * Keymap is flat: [layer0_row0_col0, layer0_row0_col1, ...].
 */
export function keycodeAt(layer: number, row: number, col: number): number {
  const def = definition;
  if (!def) return 0;
  const cols = def.matrix.cols;
  return keymap[layer * def.matrix.rows * cols + row * cols + col] ?? 0;
}

export function setKeycodeAt(layer: number, row: number, col: number, code: number): void {
  const def = definition;
  if (!def) return;
  const cols = def.matrix.cols;
  const idx = layer * def.matrix.rows * cols + row * cols + col;
  if (idx < keymap.length) keymap[idx] = code;
}

export function getEncoderKeycode(layer: number, enc: number, cw: boolean): number {
  // encoderMap: [layer0_enc0_ccw, layer0_enc0_cw, layer0_enc1_ccw, ...]
  const idx = layer * encoderCount * 2 + enc * 2 + (cw ? 1 : 0);
  return encoderMap[idx] ?? 0;
}

export function setEncoderKeycode(layer: number, enc: number, cw: boolean, code: number): void {
  const idx = layer * encoderCount * 2 + enc * 2 + (cw ? 1 : 0);
  if (idx < encoderMap.length) encoderMap[idx] = code;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/store/app.svelte.ts
git commit -m "feat: add Svelte 5 runes global state store"
```

---

### Task 9: Shared UI Components

**Files:**
- Create: `AirVIA/src/ui/shared/Icon.svelte`
- Create: `AirVIA/src/ui/shared/Modal.svelte`
- Create: `AirVIA/src/ui/shared/Toast.svelte`

**Interfaces:**
- Produces:
  - `Icon` — inline SVG icons (`bluetooth`, `usb`, `disconnect`, `save`, `trash`, `chevron-right`, `chevron-left`, `search`, `x`, `sun`, `settings`, `terminal`)
  - `Modal` — slot-based overlay, props: `{ show: boolean; title: string; onclose: () => void }`
  - `Toast` — self-managing notification queue, export `toast(message, type)` function

- [ ] **Step 1: Create Icon.svelte**

```svelte
<!-- src/ui/shared/Icon.svelte -->
<script lang="ts">
  type IconName = 'bluetooth' | 'disconnect' | 'save' | 'trash'
    | 'chevron-right' | 'chevron-left' | 'search' | 'x' | 'sun'
    | 'settings' | 'terminal' | 'keyboard' | 'drag';

  let { name, class: cls = 'w-5 h-5' }: { name: IconName; class?: string } = $props();

  const paths: Record<IconName, string> = {
    bluetooth: 'M17.71 7.71L12 2h-1v7.59L6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 11 14.41V22h1l5.71-5.71-4.3-4.29 4.3-4.29zM13 5.83l1.88 1.88L13 9.59V5.83zm1.88 10.46L13 18.17v-3.76l1.88 1.88z',
    disconnect: 'M13 5.83 14.88 7.71 13.3 9.29l1.42 1.42 1.58-1.58L19.3 12l3.41-3.41L16.12 2zm0 0 1.58 1.58',
    save: 'M17 3H5c-1.11 0-2 .9-2 2v14c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2V7l-4-4zm-5 16c-1.66 0-3-1.34-3-3s1.34-3 3-3 3 1.34 3 3-1.34 3-3 3zm3-10H5V5h10v4z',
    trash: 'M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z',
    'chevron-right': 'M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z',
    'chevron-left': 'M15.41 7.41L14 6l-6 6 6 6 1.41-1.41L10.83 12z',
    search: 'M15.5 14h-.79l-.28-.27C15.41 12.59 16 11.11 16 9.5 16 5.91 13.09 3 9.5 3S3 5.91 3 9.5 5.91 16 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z',
    x: 'M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z',
    sun: 'M12 9c1.65 0 3 1.35 3 3s-1.35 3-3 3-3-1.35-3-3 1.35-3 3-3m0-2c-2.76 0-5 2.24-5 5s2.24 5 5 5 5-2.24 5-5-2.24-5-5-5zM2 13h2c.55 0 1-.45 1-1s-.45-1-1-1H2c-.55 0-1 .45-1 1s.45 1 1 1zm18 0h2c.55 0 1-.45 1-1s-.45-1-1-1h-2c-.55 0-1 .45-1 1s.45 1 1 1zM11 2v2c0 .55.45 1 1 1s1-.45 1-1V2c0-.55-.45-1-1-1s-1 .45-1 1zm0 18v2c0 .55.45 1 1 1s1-.45 1-1v-2c0-.55-.45-1-1-1s-1 .45-1 1zM5.99 4.58c-.39-.39-1.03-.39-1.41 0-.39.39-.39 1.03 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0s.39-1.03 0-1.41L5.99 4.58zm12.37 12.37c-.39-.39-1.03-.39-1.41 0-.39.39-.39 1.03 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0 .39-.39.39-1.03 0-1.41l-1.06-1.06zm1.06-10.96c.39-.39.39-1.03 0-1.41-.39-.39-1.03-.39-1.41 0l-1.06 1.06c-.39.39-.39 1.03 0 1.41s1.03.39 1.41 0l1.06-1.06zM7.05 18.36c.39-.39.39-1.03 0-1.41-.39-.39-1.03-.39-1.41 0l-1.06 1.06c-.39.39-.39 1.03 0 1.41s1.03.39 1.41 0l1.06-1.06z',
    settings: 'M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58c.18-.14.23-.41.12-.61l-1.92-3.32c-.12-.22-.37-.29-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94L14.4 2.81c-.04-.24-.24-.41-.48-.41h-3.84c-.24 0-.43.17-.47.41L9.25 5.35c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.07.62-.07.94s.02.64.07.94l-2.03 1.58c-.18.14-.23.41-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58z',
    terminal: 'M20 4H4c-1.11 0-2 .9-2 2v12c0 1.1.89 2 2 2h16c1.1 0 2-.9 2-2V6c0-1.1-.89-2-2-2zm0 14H4V8h16v10zm-2-1h-6v-2h6v2zM7.5 17l-1.41-1.41L8.67 13l-2.58-2.59L7.5 9l4 4-4 4z',
    keyboard: 'M20 5H4c-1.1 0-1.99.9-1.99 2L2 17c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V7c0-1.1-.9-2-2-2zm-9 3h2v2h-2V8zm0 3h2v2h-2v-2zM8 8h2v2H8V8zm0 3h2v2H8v-2zm-1 2H5v-2h2v2zm0-3H5V8h2v2zm9 7H8v-2h8v2zm0-4h-2v-2h2v2zm0-3h-2V8h2v2zm3 3h-2v-2h2v2zm0-3h-2V8h2v2z',
    drag: 'M11 18c0 1.1-.9 2-2 2s-2-.9-2-2 .9-2 2-2 2 .9 2 2zm-2-8c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm0-6c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm6 4c1.1 0 2-.9 2-2s-.9-2-2-2-2 .9-2 2 .9 2 2 2zm0 2c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm0 6c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2z',
  };
</script>

<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor"
     class={cls} aria-hidden="true">
  <path d={paths[name]} />
</svg>
```

- [ ] **Step 2: Create Modal.svelte**

```svelte
<!-- src/ui/shared/Modal.svelte -->
<script lang="ts">
  let { show, title, onclose }: { show: boolean; title: string; onclose: () => void } = $props();
</script>

{#if show}
  <div class="fixed inset-0 z-50 flex items-center justify-center bg-black/60"
       role="dialog" aria-modal="true" onclick={onclose}>
    <div class="bg-gray-900 border border-gray-700 rounded-lg shadow-xl w-full max-w-lg mx-4 max-h-[80vh] flex flex-col"
         onclick={(e) => e.stopPropagation()}>
      <div class="flex items-center justify-between p-4 border-b border-gray-700">
        <h2 class="text-lg font-semibold">{title}</h2>
        <button onclick={onclose} class="text-gray-400 hover:text-white">
          <svg class="w-5 h-5" viewBox="0 0 24 24" fill="currentColor">
            <path d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"/>
          </svg>
        </button>
      </div>
      <div class="p-4 overflow-y-auto flex-1">
        {@render children?.()}
      </div>
    </div>
  </div>
{/if}

{#snippet children()}{/snippet}
```

- [ ] **Step 3: Create Toast.svelte**

```svelte
<!-- src/ui/shared/Toast.svelte -->
<script lang="ts">
  type ToastType = 'info' | 'success' | 'error';

  let toasts = $state<{ id: number; message: string; type: ToastType }[]>([]);
  let nextId = 0;

  export function toast(message: string, type: ToastType = 'info') {
    const id = nextId++;
    toasts = [...toasts, { id, message, type }];
    setTimeout(() => { toasts = toasts.filter(t => t.id !== id); }, 3000);
  }

  const colors: Record<ToastType, string> = {
    info: 'bg-blue-600',
    success: 'bg-green-600',
    error: 'bg-red-600',
  };
</script>

<div class="fixed bottom-4 right-4 z-50 flex flex-col gap-2">
  {#each toasts as t (t.id)}
    <div class={`${colors[t.type]} text-white px-4 py-2 rounded-lg shadow-lg text-sm animate-fade-in`}>
      {t.message}
    </div>
  {/each}
</div>
```

- [ ] **Step 4: Commit**

```bash
git add src/ui/shared/Icon.svelte src/ui/shared/Modal.svelte src/ui/shared/Toast.svelte
git commit -m "feat: add shared UI components — Icon, Modal, Toast"
```

---

### Task 10: ConnectBar and TabBar

**Files:**
- Create: `AirVIA/src/ui/ConnectBar.svelte`
- Create: `AirVIA/src/ui/TabBar.svelte`

- [ ] **Step 1: Create ConnectBar.svelte**

```svelte
<!-- src/ui/ConnectBar.svelte -->
<script lang="ts">
  import Icon from './shared/Icon.svelte';
  import { getConnectionState } from '../store/app.svelte';

  let error = $state('');

  let { onConnect, onDisconnect }: { onConnect: () => Promise<void>; onDisconnect: () => void } = $props();

  async function handleConnect() {
    error = '';
    try {
      await onConnect();
    } catch (e) {
      error = e instanceof Error ? e.message : 'Connection failed';
    }
  }

  function handleDisconnect() {
    onDisconnect();
  }

  const state = $derived(getConnectionState());
</script>

<div class="flex items-center justify-between px-4 py-2 bg-gray-900 border-b border-gray-700">
  <div class="flex items-center gap-3">
    <h1 class="text-lg font-bold tracking-tight">AirVIA</h1>
    <span class={`inline-flex items-center gap-1 text-xs px-2 py-0.5 rounded-full
      ${state === 'connected' ? 'bg-green-900 text-green-300' :
        state === 'connecting' ? 'bg-yellow-900 text-yellow-300' :
        state === 'error' ? 'bg-red-900 text-red-300' :
        'bg-gray-800 text-gray-400'}`}>
      <span class="w-2 h-2 rounded-full {$derived({
        connected: 'bg-green-400',
        connecting: 'bg-yellow-400 animate-pulse',
        error: 'bg-red-400',
        disconnected: 'bg-gray-500',
      }[state])}"></span>
      {state}
    </span>
  </div>

  <div class="flex items-center gap-2">
    {#if error}
      <span class="text-xs text-red-400">{error}</span>
    {/if}
    {#if state === 'disconnected' || state === 'error'}
      <button onclick={handleConnect}
        class="flex items-center gap-1 px-3 py-1.5 text-sm bg-blue-600 hover:bg-blue-700 rounded-md transition-colors">
        <Icon name="bluetooth" class="w-4 h-4" />
        Connect
      </button>
    {:else}
      <button onclick={handleDisconnect}
        class="flex items-center gap-1 px-3 py-1.5 text-sm bg-gray-700 hover:bg-gray-600 rounded-md transition-colors">
        <Icon name="disconnect" class="w-4 h-4" />
        Disconnect
      </button>
    {/if}
  </div>
</div>
```

- [ ] **Step 2: Create TabBar.svelte**

```svelte
<!-- src/ui/TabBar.svelte -->
<script lang="ts">
  import Icon from './shared/Icon.svelte';
  import { getActiveTab, setActiveTab } from '../store/app.svelte';

  type Tab = { id: string; label: string; icon: string };

  const tabs: Tab[] = [
    { id: 'keymap', label: 'KEYMAP', icon: 'keyboard' },
    { id: 'encoder', label: 'ENCODER', icon: 'settings' },
    { id: 'macro', label: 'MACROS', icon: 'terminal' },
    { id: 'lighting', label: 'LIGHTING', icon: 'sun' },
    { id: 'layout', label: 'LAYOUT', icon: 'settings' },
    { id: 'console', label: 'CONSOLE', icon: 'terminal' },
  ];

  const active = $derived(getActiveTab());
</script>

<nav class="flex border-b border-gray-700 bg-gray-900">
  {#each tabs as tab}
    {@const iconName = tab.icon as 'keyboard' | 'settings' | 'terminal' | 'sun'}
    <button onclick={() => setActiveTab(tab.id)}
      class={`flex items-center gap-1.5 px-4 py-2 text-xs font-medium tracking-wider transition-colors border-b-2
        ${active === tab.id
          ? 'border-blue-500 text-blue-400'
          : 'border-transparent text-gray-400 hover:text-gray-200 hover:border-gray-600'}`}>
      <Icon name={iconName} class="w-3.5 h-3.5" />
      {tab.label}
    </button>
  {/each}
</nav>
```

- [ ] **Step 3: Commit**

```bash
git add src/ui/ConnectBar.svelte src/ui/TabBar.svelte
git commit -m "feat: add ConnectBar with BLE connect/disconnect and TabBar navigation"
```

---

### Task 11: Keymap Grid, Cell, and Layer Selector

**Files:**
- Create: `AirVIA/src/ui/keymap/KeymapCell.svelte`
- Create: `AirVIA/src/ui/keymap/KeymapGrid.svelte`
- Create: `AirVIA/src/ui/keymap/LayerSelector.svelte`

- [ ] **Step 1: Create KeymapCell.svelte**

```svelte
<!-- src/ui/keymap/KeymapCell.svelte -->
<script lang="ts">
  import { keycodeAt, getKeycodeLabel, setSelectedCell } from '../../store/app.svelte';

  let { layer, row, col }: { layer: number; row: number; col: number } = $props();

  const code = $derived(keycodeAt(layer, row, col));
  const label = $derived(getKeycodeLabel(code));
  const isNoKey = $derived(code === 0);
  const isTransparent = $derived(code === 1);

  let textClass = $derived(
    isNoKey ? 'text-gray-700' :
    isTransparent ? 'text-gray-500 italic' :
    'text-gray-200'
  );

  function handleClick() {
    setSelectedCell({ layer, row, col });
  }
</script>

<button onclick={handleClick}
  class="relative aspect-square rounded border flex items-center justify-center text-center
    text-[10px] leading-tight font-mono cursor-pointer transition-colors
    bg-gray-800/70 border-gray-700 hover:border-blue-500 hover:bg-gray-700/70 w-full h-full
    {textClass}"
  title={`[${row},${col}] ${label} (0x${code.toString(16).toUpperCase().padStart(4, '0')})`}>
  <span class="truncate px-1">{label.replace(/^KC_/, '')}</span>
</button>
```

- [ ] **Step 2: Create LayerSelector.svelte**

```svelte
<!-- src/ui/keymap/LayerSelector.svelte -->
<script lang="ts">
  import { getLayerCount, getActiveLayer, setActiveLayer } from '../../store/app.svelte';

  const count = $derived(getLayerCount());
  const active = $derived(getActiveLayer());
  const layers = $derived(Array.from({ length: count }, (_, i) => i));
</script>

<div class="flex items-center gap-1 p-2 bg-gray-900">
  <span class="text-xs text-gray-500 mr-2">LAYER</span>
  {#each layers as layer}
    <button onclick={() => setActiveLayer(layer)}
      class={`px-3 py-1 text-xs rounded-md font-mono transition-colors
        ${active === layer
          ? 'bg-blue-600 text-white'
          : 'bg-gray-800 text-gray-400 hover:bg-gray-700 hover:text-gray-200'}`}>
      {layer}
    </button>
  {/each}
</div>
```

- [ ] **Step 3: Create KeymapGrid.svelte**

```svelte
<!-- src/ui/keymap/KeymapGrid.svelte -->
<script lang="ts">
  import { getDefinition, getActiveLayer } from '../../store/app.svelte';
  import KeymapCell from './KeymapCell.svelte';

  const def = $derived(getDefinition());
  const activeLayer = $derived(getActiveLayer());

  type KeyPos = { x: number; y: number; w?: number; h?: number; row: number; col: number };

  const positions = $derived.by<KeyPos[]>(() => {
    if (!def) return [];
    const { rows, cols } = def.matrix;
    return def.layouts.keymap.map((k, i) => ({
      x: k.x,
      y: k.y,
      w: k.w ?? 1,
      h: k.h ?? 1,
      row: Math.floor(i / cols),
      col: i % cols,
    }));
  });

  const gridStyle = $derived(() => {
    if (!positions.length) return {};
    const maxX = Math.max(...positions.map(p => p.x + (p.w ?? 1)));
    const maxY = Math.max(...positions.map(p => p.y + (p.h ?? 1)));
    return {
      display: 'grid',
      gridTemplateColumns: `repeat(${maxX}, 1fr)`,
      gridTemplateRows: `repeat(${maxY}, 1fr)`,
    };
  });
</script>

{#if def}
  <div class="p-4 overflow-auto">
    <div class="w-fit mx-auto" style={gridStyle}>
      {#each positions as pos (pos.row * def.matrix.cols + pos.col)}
        {@const cellStyle = `grid-column: ${pos.x + 1} / span ${pos.w ?? 1}; grid-row: ${pos.y + 1} / span ${pos.h ?? 1};`}
        <div style={cellStyle} class="p-0.5">
          <KeymapCell layer={activeLayer} row={pos.row} col={pos.col} />
        </div>
      {/each}
    </div>
  </div>
{:else}
  <div class="flex items-center justify-center h-64 text-gray-500">
    <p>Load a V3 definition JSON to see the keymap</p>
  </div>
{/if}
```

- [ ] **Step 4: Commit**

```bash
git add src/ui/keymap/KeymapCell.svelte src/ui/keymap/KeymapGrid.svelte src/ui/keymap/LayerSelector.svelte
git commit -m "feat: add keymap grid with layer selector and cell rendering from V3 definition"
```

---

### Task 12: Keycode Picker

**Files:**
- Create: `AirVIA/src/ui/keymap/KeycodePicker.svelte`

- [ ] **Step 1: Create KeycodePicker.svelte**

```svelte
<!-- src/ui/keymap/KeycodePicker.svelte -->
<script lang="ts">
  import Modal from '../shared/Modal.svelte';
  import { KEYCODES_BY_CATEGORY, CATEGORY_LABELS, type KeycodeEntry, type KeycodeCategory } from '../../core/keycodes';
  import { getSelectedCell, setSelectedCell, setKeycodeAt } from '../../store/app.svelte';

  let search = $state('');
  let activeCategory = $state<KeycodeCategory>('basic');

  const cell = $derived(getSelectedCell());
  const show = $derived(cell !== null);

  const categories: KeycodeCategory[] = ['basic', 'modifier', 'layer', 'boot', 'media', 'system', 'macro', 'custom'];

  const filtered = $derived<KeycodeEntry[]>(() => {
    if (search.trim()) {
      const q = search.toLowerCase();
      return [...categories.flatMap(c => KEYCODES_BY_CATEGORY[c])]
        .filter(e => e.label.toLowerCase().includes(q) || e.code.toString(16).includes(q));
    }
    return KEYCODES_BY_CATEGORY[activeCategory];
  });

  function handleSelect(entry: KeycodeEntry) {
    if (cell) {
      setKeycodeAt(cell.layer, cell.row, cell.col, entry.code);
    }
    setSelectedCell(null);
    search = '';
  }

  function handleClose() {
    setSelectedCell(null);
    search = '';
  }
</script>

<Modal show={show} title="Select Keycode" onclose={handleClose}>
  <div class="flex flex-col gap-3">
    <input type="text" placeholder="Search keycodes..." bind:value={search}
      class="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded-md text-sm text-gray-200
             placeholder-gray-500 focus:outline-none focus:border-blue-500" />

    {#if !search.trim()}
      <div class="flex flex-wrap gap-1">
        {#each categories as cat}
          <button onclick={() => activeCategory = cat}
            class={`px-2 py-1 text-xs rounded transition-colors
              ${activeCategory === cat ? 'bg-blue-600 text-white' : 'bg-gray-800 text-gray-400 hover:bg-gray-700'}`}>
            {CATEGORY_LABELS[cat]}
          </button>
        {/each}
      </div>
    {/if}

    <div class="grid grid-cols-4 gap-1 max-h-60 overflow-y-auto">
      {#each filtered as entry (entry.code)}
        <button onclick={() => handleSelect(entry)}
          class="px-2 py-1.5 text-xs bg-gray-800 hover:bg-blue-900 hover:text-blue-200
                 border border-gray-700 rounded text-left font-mono transition-colors"
          title={`0x${entry.code.toString(16).toUpperCase().padStart(4, '0')}`}>
          <div class="text-[11px] text-gray-300 truncate">{entry.label.replace('KC_', '')}</div>
          <div class="text-[9px] text-gray-600">0x{entry.code.toString(16).toUpperCase().padStart(4, '0')}</div>
        </button>
      {/each}

      {#if filtered.length === 0}
        <div class="col-span-4 py-8 text-center text-gray-500 text-sm">No keycodes found</div>
      {/if}
    </div>
  </div>
</Modal>
```

- [ ] **Step 2: Commit**

```bash
git add src/ui/keymap/KeycodePicker.svelte
git commit -m "feat: add keycode picker modal with category tabs and search"
```

---

### Task 13: Encoder Editor

**Files:**
- Create: `AirVIA/src/ui/encoder/EncoderEditor.svelte`

- [ ] **Step 1: Create EncoderEditor.svelte**

```svelte
<!-- src/ui/encoder/EncoderEditor.svelte -->
<script lang="ts">
  import Modal from '../shared/Modal.svelte';
  import { getEncoderCount, getEncoderKeycode, getKeycodeLabel, getActiveLayer, setEncoderKeycode } from '../../store/app.svelte';
  import { KEYCODES_BY_CATEGORY, CATEGORY_LABELS, type KeycodeEntry, type KeycodeCategory } from '../../core/keycodes';

  const count = $derived(getEncoderCount());
  const activeLayer = $derived(getActiveLayer());
  const indices = $derived(count > 0 ? Array.from({ length: count }, (_, i) => i) : []);

  type EncoderEditing = { enc: number; cw: boolean } | null;
  let editing = $state<EncoderEditing>(null);
  let search = $state('');
  let activeCategory = $state<KeycodeCategory>('basic');

  const categories: KeycodeCategory[] = ['basic', 'modifier', 'layer', 'boot', 'media', 'system', 'macro', 'custom'];

  const filtered = $derived<KeycodeEntry[]>(() => {
    if (search.trim()) {
      const q = search.toLowerCase();
      return [...categories.flatMap(c => KEYCODES_BY_CATEGORY[c])]
        .filter(e => e.label.toLowerCase().includes(q) || e.code.toString(16).includes(q));
    }
    return KEYCODES_BY_CATEGORY[activeCategory];
  });

  function handleSelect(entry: KeycodeEntry) {
    if (editing) {
      setEncoderKeycode(activeLayer, editing.enc, editing.cw, entry.code);
    }
    editing = null;
    search = '';
  }

  function handleClose() {
    editing = null;
    search = '';
  }
</script>

{#if count === 0}
  <div class="flex items-center justify-center h-64 text-gray-500">
    <p>No encoders defined in V3 definition</p>
  </div>
{:else}
  <div class="p-4">
    <h2 class="text-sm font-semibold text-gray-400 mb-3">Encoders — Layer {activeLayer}</h2>

    {#each indices as enc}
      <div class="mb-4 p-3 bg-gray-900 rounded-lg border border-gray-800">
        <h3 class="text-xs text-gray-500 mb-2">Encoder {enc}</h3>
        <div class="flex gap-4">
          <div class="flex-1">
            <div class="text-xs text-gray-500 mb-1">Counter‑Clockwise</div>
            {@const ccw = getEncoderKeycode(activeLayer, enc, false)}
            <button onclick={() => editing = { enc, cw: false }}
              class="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-sm font-mono
                     text-left hover:border-blue-500 transition-colors">
              {getKeycodeLabel(ccw)}
            </button>
          </div>
          <div class="flex-1">
            <div class="text-xs text-gray-500 mb-1">Clockwise</div>
            {@const cw = getEncoderKeycode(activeLayer, enc, true)}
            <button onclick={() => editing = { enc, cw: true }}
              class="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded text-sm font-mono
                     text-left hover:border-blue-500 transition-colors">
              {getKeycodeLabel(cw)}
            </button>
          </div>
        </div>
      </div>
    {/each}
  </div>

  <Modal show={editing !== null} title="Select Encoder Keycode" onclose={handleClose}>
    <div class="flex flex-col gap-3">
      <input type="text" placeholder="Search keycodes..." bind:value={search}
        class="w-full px-3 py-2 bg-gray-800 border border-gray-700 rounded-md text-sm text-gray-200
               placeholder-gray-500 focus:outline-none focus:border-blue-500" />

      {#if !search.trim()}
        <div class="flex flex-wrap gap-1">
          {#each categories as cat}
            <button onclick={() => activeCategory = cat}
              class={`px-2 py-1 text-xs rounded transition-colors
                ${activeCategory === cat ? 'bg-blue-600 text-white' : 'bg-gray-800 text-gray-400 hover:bg-gray-700'}`}>
              {CATEGORY_LABELS[cat]}
            </button>
          {/each}
        </div>
      {/if}

      <div class="grid grid-cols-4 gap-1 max-h-60 overflow-y-auto">
        {#each filtered as entry (entry.code)}
          <button onclick={() => handleSelect(entry)}
            class="px-2 py-1.5 text-xs bg-gray-800 hover:bg-blue-900 hover:text-blue-200
                   border border-gray-700 rounded text-left font-mono transition-colors">
            <div class="text-[11px] text-gray-300 truncate">{entry.label.replace('KC_', '')}</div>
            <div class="text-[9px] text-gray-600">0x{entry.code.toString(16).toUpperCase().padStart(4, '0')}</div>
          </button>
        {/each}
        {#if filtered.length === 0}
          <div class="col-span-4 py-8 text-center text-gray-500 text-sm">No keycodes found</div>
        {/if}
      </div>
    </div>
  </Modal>
{/if}
```

- [ ] **Step 2: Commit**

```bash
git add src/ui/encoder/EncoderEditor.svelte
git commit -m "feat: add encoder editor with CW/CCW keycode assignment per layer"
```

---

### Task 14: Macro Editor

**Files:**
- Create: `AirVIA/src/ui/macro/MacroEditor.svelte`

- [ ] **Step 1: Create MacroEditor.svelte**

```svelte
<!-- src/ui/macro/MacroEditor.svelte -->
<script lang="ts">
  import { getMacroCount, getMacroBytes, getMacroBuffer } from '../../store/app.svelte';

  const count = $derived(getMacroCount());
  const bytes = $derived(getMacroBytes());

  let selectedMacro = $state(0);
  let macroData = $derived(getMacroBuffer());

  // ponytail: simple terminal display of raw macro buffer.
  // Full macro sequence editor deferred until firmware provides macro action encoding details.
</script>

{#if count === 0}
  <div class="flex items-center justify-center h-64 text-gray-500">
    <p>No macros defined in firmware</p>
  </div>
{:else}
  <div class="p-4">
    <div class="flex items-center gap-2 mb-4">
      <span class="text-sm text-gray-400">Macro buffer:</span>
      <span class="text-xs text-gray-500">{count} slots, {bytes} bytes total</span>
    </div>

    <div class="flex gap-2 mb-4">
      {#each Array.from({ length: count }, (_, i) => i) as idx}
        <button onclick={() => selectedMacro = idx}
          class={`px-3 py-1 text-xs rounded-md font-mono transition-colors
            ${selectedMacro === idx ? 'bg-blue-600 text-white' : 'bg-gray-800 text-gray-400 hover:bg-gray-700'}`}>
          M{idx}
        </button>
      {/each}
    </div>

    <div class="grid grid-cols-8 gap-1">
      {#each macroData as byte, i}
        <div class="px-1 py-0.5 bg-gray-800 text-[10px] font-mono text-gray-400 text-center rounded">
          0x{byte.toString(16).padStart(2, '0')}
        </div>
      {/each}
    </div>
  </div>
{/if}
```

- [ ] **Step 2: Commit**

```bash
git add src/ui/macro/MacroEditor.svelte
git commit -m "feat: add macro buffer viewer with slot selector"
```

---

### Task 15: Lighting Panel

**Files:**
- Create: `AirVIA/src/ui/lighting/LightingPanel.svelte`

- [ ] **Step 1: Create LightingPanel.svelte**

```svelte
<!-- src/ui/lighting/LightingPanel.svelte -->
<script lang="ts">
  import { getLighting, setLightingBrightness, setLightingEffect,
           setLightingSpeed, setLightingHue, setLightingSaturation } from '../../store/app.svelte';

  const state = $derived(getLighting());

  function onBrightness(e: Event) { setLightingBrightness(+(e.target as HTMLInputElement).value); }
  function onEffect(e: Event) { setLightingEffect(+(e.target as HTMLInputElement).value); }
  function onSpeed(e: Event) { setLightingSpeed(+(e.target as HTMLInputElement).value); }
  function onHue(e: Event) { setLightingHue(+(e.target as HTMLInputElement).value); }
  function onSaturation(e: Event) { setLightingSaturation(+(e.target as HTMLInputElement).value); }

  function onDragEnd(onSend: () => void) {
    // ponytail: calls parent-supplied send handler. Wired in App.svelte.
    if (typeof window !== 'undefined') {
      (window as unknown as Record<string, unknown>).__airvia_lighting_send?.(onSend, 'lighting');
    }
  }
</script>

<div class="p-6 max-w-md space-y-5">
  <div>
    <label class="flex justify-between text-sm text-gray-400 mb-1">
      <span>Brightness</span>
      <span class="font-mono text-gray-500">{state.brightness}</span>
    </label>
    <input type="range" min="0" max="255" value={state.brightness}
      oninput={onBrightness} onchange={() => onDragEnd(() => {})}
      class="w-full accent-blue-500" />
  </div>

  <div>
    <label class="flex justify-between text-sm text-gray-400 mb-1">
      <span>Effect</span>
      <span class="font-mono text-gray-500">{state.effect}</span>
    </label>
    <input type="range" min="0" max="20" value={state.effect}
      oninput={onEffect} onchange={() => onDragEnd(() => {})}
      class="w-full accent-blue-500" />
  </div>

  <div>
    <label class="flex justify-between text-sm text-gray-400 mb-1">
      <span>Speed</span>
      <span class="font-mono text-gray-500">{state.speed}</span>
    </label>
    <input type="range" min="0" max="255" value={state.speed}
      oninput={onSpeed} onchange={() => onDragEnd(() => {})}
      class="w-full accent-blue-500" />
  </div>

  <div>
    <label class="flex justify-between text-sm text-gray-400 mb-1">
      <span>Hue</span>
      <span class="font-mono text-gray-500">{state.hue}</span>
    </label>
    <input type="range" min="0" max="255" value={state.hue}
      oninput={onHue} onchange={() => onDragEnd(() => {})}
      class="w-full accent-pink-500" />
  </div>

  <div>
    <label class="flex justify-between text-sm text-gray-400 mb-1">
      <span>Saturation</span>
      <span class="font-mono text-gray-500">{state.saturation}</span>
    </label>
    <input type="range" min="0" max="255" value={state.saturation}
      oninput={onSaturation} onchange={() => onDragEnd(() => {})}
      class="w-full accent-pink-500" />
  </div>
</div>
```

- [ ] **Step 2: Commit**

```bash
git add src/ui/lighting/LightingPanel.svelte
git commit -m "feat: add RGB lighting panel with brightness, effect, speed, hue, saturation sliders"
```

---

### Task 16: Layout Options Panel

**Files:**
- Create: `AirVIA/src/ui/layout/LayoutOptions.svelte`

- [ ] **Step 1: Create LayoutOptions.svelte**

```svelte
<!-- src/ui/layout/LayoutOptions.svelte -->
<script lang="ts">
  import { getLayoutOptions, setLayoutOptions } from '../../store/app.svelte';

  const opts = $derived(getLayoutOptions());

  function toggle(bit: number) {
    setLayoutOptions(opts ^ (1 << bit));
  }

  function bitText(bit: number) {
    return `Option ${bit}`;
  }
</script>

<div class="p-6 max-w-md">
  <div class="flex items-center gap-3 mb-4">
    <span class="text-sm text-gray-400">Layout Options</span>
    <span class="font-mono text-xs text-gray-500">
      0x{opts.toString(16).toUpperCase().padStart(8, '0')}
    </span>
  </div>

  <div class="space-y-2">
    {#each Array.from({ length: 16 }, (_, i) => i) as bit}
      {@const active = (opts & (1 << bit)) !== 0}
      <label class="flex items-center gap-3 px-3 py-2 bg-gray-800 rounded-md cursor-pointer hover:bg-gray-750">
        <input type="checkbox" checked={active}
          onchange={() => toggle(bit)}
          class="accent-blue-500" />
        <span class="text-sm text-gray-300">{bitText(bit)}</span>
        <span class="font-mono text-xs text-gray-600 ml-auto">bit {bit}</span>
      </label>
    {/each}
  </div>
</div>
```

- [ ] **Step 2: Commit**

```bash
git add src/ui/layout/LayoutOptions.svelte
git commit -m "feat: add layout options panel with bitfield toggles"
```

---

### Task 17: Packet Console

**Files:**
- Create: `AirVIA/src/ui/console/PacketLog.svelte`

- [ ] **Step 1: Create PacketLog.svelte**

```svelte
<!-- src/ui/console/PacketLog.svelte -->
<script lang="ts">
  import { getPacketLog } from '../../store/app.svelte';

  const log = $derived(getPacketLog());
</script>

<div class="p-4">
  <div class="text-xs text-gray-500 mb-3">Raw packet log (most recent first)</div>

  <div class="space-y-0.5 font-mono text-xs max-h-96 overflow-y-auto">
    {#each [...log].reverse() as entry, i (i)}
      <div class="flex items-center gap-2">
        <span class={`${entry.dir === 'tx' ? 'text-blue-400' : 'text-green-400'} w-5`}>
          {entry.dir === 'tx' ? 'TX' : 'RX'}
        </span>
        <span class="text-gray-500">{entry.hex}</span>
      </div>
    {/each}

    {#if log.length === 0}
      <div class="text-gray-600">No packets yet. Connect to a device to see traffic.</div>
    {/if}
  </div>
</div>
```

- [ ] **Step 2: Commit**

```bash
git add src/ui/console/PacketLog.svelte
git commit -m "feat: add packet console for raw TX/RX debug log"
```

---

### Task 18: App.svelte Root

**Files:**
- Modify: `AirVIA/src/App.svelte`

- [ ] **Step 1: Rewrite App.svelte with all tab routing**

```svelte
<!-- src/App.svelte -->
<script lang="ts">
  import ConnectBar from './ui/ConnectBar.svelte';
  import TabBar from './ui/TabBar.svelte';
  import LayerSelector from './ui/keymap/LayerSelector.svelte';
  import KeymapGrid from './ui/keymap/KeymapGrid.svelte';
  import KeycodePicker from './ui/keymap/KeycodePicker.svelte';
  import EncoderEditor from './ui/encoder/EncoderEditor.svelte';
  import MacroEditor from './ui/macro/MacroEditor.svelte';
  import LightingPanel from './ui/lighting/LightingPanel.svelte';
  import LayoutOptions from './ui/layout/LayoutOptions.svelte';
  import PacketLog from './ui/console/PacketLog.svelte';
  import Toast from './ui/shared/Toast.svelte';

  import { getActiveTab, getActiveLayer, getConnectionState, setConnectionState,
           setDefinition, addPacketLog, setLayerCount, setEncoderCount,
           setKeymap } from './store/app.svelte';
  import { BLETransport, VIA_SERVICE_UUID, VIA_DATA_CHAR_UUID } from './ble/transport';
  import { Protocol, PACKET_SIZE, parseU32, parseU16BE } from './core/protocol';
  import { parseV3Definition } from './core/v3-definition';
  import type { RawPacket } from './core/protocol';

  // --- BLE Transport ---
  let transport = $state<BLETransport | null>(null);

  transport?.onStateChange?.((s) => setConnectionState(s));
  transport?.onResponse?.((pkt) => {
    addPacketLog('rx', pkt);
  });

  async function handleConnect() {
    const t = new BLETransport();
    t.onStateChange = (s) => setConnectionState(s);
    t.onResponse = (pkt) => addPacketLog('rx', pkt);
    await t.connect();
    transport = t;

    // Read device info
    const info = await t.readInfo();

    // Send handshake: get protocol version
    const handshake = Protocol.getProtocolVersion();
    addPacketLog('tx', handshake);
    t.send(handshake);
  }

  function handleDisconnect() {
    transport?.disconnect();
    transport = null;
  }

  // --- V3 JSON Loader ---
  function handleLoadDefinition(file: File) {
    const reader = new FileReader();
    reader.onload = () => {
      try {
        const def = parseV3Definition(reader.result as string);
        setDefinition(def);
        setLayerCount(def.matrix.rows > 0 ? (getActiveLayer() + 1 || 1) : 1);
        if (def.encoders !== undefined) setEncoderCount(def.encoders);
      } catch (e) {
        console.error('Invalid V3 definition:', e);
      }
    };
    reader.readAsText(file);
  }
</script>

<ConnectBar {onConnect: handleConnect} {onDisconnect: handleDisconnect} />
<TabBar />

{#if getActiveTab() === 'keymap' || getActiveTab() === 'encoder'}
  <LayerSelector />
{/if}

{#if getActiveTab() === 'keymap'}
  <KeymapGrid />
  <KeycodePicker />
{:else if getActiveTab() === 'encoder'}
  <EncoderEditor />
{:else if getActiveTab() === 'macro'}
  <MacroEditor />
{:else if getActiveTab() === 'lighting'}
  <LightingPanel />
{:else if getActiveTab() === 'layout'}
  <LayoutOptions />
{:else if getActiveTab() === 'console'}
  <PacketLog />
{/if}

<Toast />
```

- [ ] **Step 2: Commit**

```bash
git add src/App.svelte
git commit -m "feat: wire App.svelte with BLE transport, tab routing, and V3 definition loader"
```

---

### Task 19: V3 Example JSON + README

**Files:**
- Create: `AirVIA/public/v3-examples/example-65.json`
- Create: `AirVIA/README.md`

- [ ] **Step 1: Create example 65% keyboard V3 JSON**

```json
{
  "name": "AirVIA 65%",
  "vendorId": "0xFEED",
  "productId": "0x6065",
  "matrix": { "rows": 5, "cols": 15 },
  "layouts": {
    "keymap": [
      {"x":0,"y":0},{"x":1,"y":0},{"x":2,"y":0},{"x":3,"y":0},{"x":4,"y":0},{"x":5,"y":0},{"x":6,"y":0},{"x":7,"y":0},{"x":8,"y":0},{"x":9,"y":0},{"x":10,"y":0},{"x":11,"y":0},{"x":12,"y":0},{"x":13,"y":0,"w":2},
      {"x":0,"y":1,"w":1.5},{"x":1.5,"y":1},{"x":2.5,"y":1},{"x":3.5,"y":1},{"x":4.5,"y":1},{"x":5.5,"y":1},{"x":6.5,"y":1},{"x":7.5,"y":1},{"x":8.5,"y":1},{"x":9.5,"y":1},{"x":10.5,"y":1},{"x":11.5,"y":1},{"x":12.5,"y":1},{"x":13.5,"y":1,"w":1.5},
      {"x":0,"y":2,"w":1.75},{"x":1.75,"y":2},{"x":2.75,"y":2},{"x":3.75,"y":2},{"x":4.75,"y":2},{"x":5.75,"y":2},{"x":6.75,"y":2},{"x":7.75,"y":2},{"x":8.75,"y":2},{"x":9.75,"y":2},{"x":10.75,"y":2},{"x":11.75,"y":2},{"x":12.75,"y":2,"w":2.25},
      {"x":0,"y":3,"w":2.25},{"x":2.25,"y":3},{"x":3.25,"y":3},{"x":4.25,"y":3},{"x":5.25,"y":3},{"x":6.25,"y":3},{"x":7.25,"y":3},{"x":8.25,"y":3},{"x":9.25,"y":3},{"x":10.25,"y":3},{"x":11.25,"y":3},{"x":12.25,"y":3,"w":1.75},{"x":14,"y":3},
      {"x":0,"y":4,"w":1.25},{"x":1.25,"y":4,"w":1.25},{"x":2.5,"y":4,"w":1.25},{"x":3.75,"y":4,"w":6.25},{"x":10,"y":4,"w":1.25},{"x":11.25,"y":4,"w":1.25},{"x":13,"y":4},{"x":14,"y":4}
    ]
  },
  "encoders": 1
}
```

- [ ] **Step 2: Create README.md**

```markdown
# AirVIA

Wireless keyboard configurator for VIA v13-compatible firmware over BLE.

## Usage

1. Open `index.html` in Chrome 122+ or Edge 122+
2. Click **Connect** to scan for VIA BLE keyboards
3. Load a V3 definition JSON for your keyboard layout
4. Edit keymaps, encoders, macros, lighting, and layout options

## Development

```bash
pnpm install
pnpm dev      # Start dev server at http://localhost:5173
pnpm test     # Run unit tests
pnpm build    # Production build to dist/
```

## Architecture

- `src/core/` — VIA protocol v13 library (pure TypeScript)
- `src/ble/` — Web Bluetooth transport adapter
- `src/ui/` — Svelte 5 components
- `src/store/` — Svelte runes reactive state

## Browser Support

Chrome 122+, Edge 122+. Firefox and Safari do not support Web Bluetooth API.

## License

MIT
```

- [ ] **Step 3: Commit**

```bash
git add public/v3-examples/example-65.json README.md
git commit -m "docs: add example V3 definition and README"
```

---

### Task 20: Final Integration Verification

**Files:**
- No new files — verify all tests pass and dev build works

- [ ] **Step 1: Run all unit tests**

```bash
pnpm test
```

Expected: All tests pass (CRC, V3 parser, protocol, queue).

- [ ] **Step 2: Verify production build**

```bash
pnpm build
```

Expected: Build succeeds, output in `dist/`.

- [ ] **Step 3: Verify dev server**

```bash
pnpm dev
```

Open browser and confirm:
- Dark theme renders correctly
- Tab bar shows all 6 tabs
- Connect button visible
- Sample V3 JSON loads keymap grid
- Keycode picker opens on cell click

- [ ] **Step 4: Commit any fixups**

```bash
git add -A
git diff --cached --stat
git commit -m "chore: integration fixes and verification"
```

---

## Plan Summary

| Task | Component | Key Deliverable |
|---|---|---|
| 1 | Scaffolding | Vite + Svelte 5 + TS + Tailwind project |
| 2 | CRC32 | `crc32()` matching `VIA_Protocol.cpp` |
| 3 | Keycodes | QMK 0.0.8 label map + classification |
| 4 | V3 Parser | `parseV3Definition()` with validation |
| 5 | Protocol | All 23 VIA v13 command builders |
| 6 | Queue | Packet queue with retry + timeout |
| 7 | BLE Transport | Web Bluetooth adapter for VIA GATT |
| 8 | State Store | Svelte 5 runes global reactive state |
| 9 | Shared UI | Icon (SVG), Modal, Toast |
| 10 | Connect + Tabs | BLE connect bar, 6-tab navigation |
| 11 | Keymap Grid | Grid rendering from V3 JSON positions |
| 12 | Keycode Picker | Category tabs + search modal |
| 13 | Encoder Editor | CW/CCW assignment per layer |
| 14 | Macro Editor | Buffer viewer + slot selector |
| 15 | Lighting | QMK rgblight sliders |
| 16 | Layout Options | Bitfield toggle panel |
| 17 | Console | Packet log viewer |
| 18 | App Root | Wire transport + tabs + V3 loader |
| 19 | Examples + Docs | 65% JSON, README |
| 20 | Integration | Test suite, build, manual verification |
