# Arduino library packaging and Library Manager

`VIA_Arduino` follows Arduino's current library specification:

- `library.properties` lives in the repository root and uses UTF-8 key/value
  metadata.
- The package name is `VIA_Arduino`, not `Arduino_VIA`: Arduino reserves names
  starting with `Arduino` for official libraries.
- Source is under `src/`; Arduino recursively compiles this directory.
- The public umbrella header `src/VIA_Arduino.h` is named in the `includes`
  metadata field.
- Sketches live under the lowercase `examples/` directory so the Arduino IDE
  lists them in **File → Examples**.
- `keywords.txt` uses literal tab separators for IDE syntax highlighting.
- README, license, tests, and extra documentation remain outside `src/` and do
  not affect a user's firmware build.

## Installation

Users can install a GitHub ZIP through **Sketch → Include Library → Add .ZIP
Library**, or place the `VIA-Arduino` directory under their sketchbook
`libraries/` folder.

## Publish an update

`VIA_Arduino` is already registered in Arduino Library Manager. For each new
release:

1. Keep `main` passing CI.
2. Set `library.properties` to the release version.
3. Run the registered-library checks:

   ```bash
   arduino-lint --compliance specification --project-type library --library-manager update
   ```

4. Create a matching semantic-version GitHub release, for example `v0.2.0`.
5. Wait for the Library Manager indexer to discover the tag; do not submit the
   registered name as a new library.

The 0.2.0 package is a protocol core with a compile-tested RP2040 adapter, not
complete keyboard firmware or a hardware-certified platform list.
