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

The layout follows the same public convention used by established libraries:
metadata at the root, implementation in `src/`, examples in `examples/`, and
documentation/automation beside them. Adafruit TinyUSB is a useful reference
because it keeps a portable Arduino-facing API while delegating USB details to
supported cores.

## Install before Library Manager

Users can install a GitHub ZIP through **Sketch → Include Library → Add .ZIP
Library**, or place the `VIA-Arduino` directory under their sketchbook
`libraries/` folder.

## Submit after the first stable adapter release

1. Keep `main` passing CI.
2. Create a semantic-version GitHub release, for example `v0.1.0`.
3. Confirm `library.properties` has the same semantic version.
4. Run Arduino Lint with the Library Manager submission checks.
5. Open a pull request to the Arduino `library-registry` repository pointing
   to this repository and release tag.

Until an actual native-USB adapter is released and hardware-tested, this
repository should be described as an alpha/protocol-core library rather than a
fully plug-and-play keyboard library.
