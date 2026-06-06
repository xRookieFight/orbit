#!/usr/bin/env bash
cd /mnt/c/Users/xRookieFight/Desktop/Hepsi/orbit || exit 1
find . -type f \( -name '*.c' -o -name '*.h' -o -name '*.asm' -o -name '*.ld' -o -name 'Makefile' \) -exec sed -i 's/\r$//' {} +
make || exit 1
if [ "$1" = "headless" ]; then
  make run
else
  make gui
fi
