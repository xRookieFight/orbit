#!/usr/bin/env bash
cd /mnt/c/Users/xRookieFight/Desktop/Hepsi/orbit || exit 1
find . -type f \( -name '*.c' -o -name '*.h' -o -name '*.asm' -o -name '*.ld' -o -name 'Makefile' \) -exec sed -i 's/\r$//' {} +
echo "--- tab check in Makefile (want >0):"
grep -cP '^\t' Makefile || true
echo "--- build:"
make 2>&1 | tail -60
echo "--- artifacts:"
ls -la build 2>/dev/null || true
