#!/bin/bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <arduino_data_dir>"
  exit 1
fi

ARDUINO_DIR="$1"
ADAFRUIT_CORE_DIR="$ARDUINO_DIR/packages/adafruit/hardware/nrf52/1.7.0"

if [ ! -d "$ADAFRUIT_CORE_DIR" ]; then
  echo "Error: Adafruit nRF52 1.7.0 core not found at $ADAFRUIT_CORE_DIR"
  exit 1
fi

VARIANT_COMMIT="bd0fdcf124f59662d0184c39126e456f89dccd9c"
TEMP_DIR=$(mktemp -d)

# Fetch the specific variant commit
echo "Fetching variant commit $VARIANT_COMMIT..."
git clone --quiet https://github.com/somik123/Adafruit_nRF52_Arduino_ProMicro.git "$TEMP_DIR/Adafruit_nRF52_Arduino"
cd "$TEMP_DIR/Adafruit_nRF52_Arduino"
git checkout --quiet "$VARIANT_COMMIT"
cd - > /dev/null

# Copy variant directory
echo "Copying variant directory..."
if [ ! -d "$ADAFRUIT_CORE_DIR/variants/pro_micro_nrf52840" ]; then
    cp -r "$TEMP_DIR/Adafruit_nRF52_Arduino/variants/pro_micro_nrf52840" "$ADAFRUIT_CORE_DIR/variants/"
fi

# Insert boards.txt block if it doesn't exist
echo "Updating boards.txt..."
if ! grep -q "promicronrf52840" "$ADAFRUIT_CORE_DIR/boards.txt"; then
  # Extract block from the repository's boards.txt, start at "promicronrf52840" and end at next blank line
  awk '/^promicronrf52840/{flag=1} flag && /^$/{print; flag=0} flag' "$TEMP_DIR/Adafruit_nRF52_Arduino/boards.txt" >> "$ADAFRUIT_CORE_DIR/boards.txt"
fi

rm -rf "$TEMP_DIR"

echo "nice!nano variant successfully installed."
