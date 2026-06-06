#!/usr/bin/env bash
cd /mnt/c/Users/xRookieFight/Desktop/Hepsi/orbit || exit 1
IN=""
[ -f test_input.txt ] && IN=$(cat test_input.txt | tr -d '\r')
TO="${1:-25}"
printf '\n\n%s\n' "$IN" | timeout "$TO" qemu-system-i386 \
  -drive file=build/orbit.img,format=raw,if=ide -m 128M -no-reboot \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 -display none -serial stdio
echo
echo "[qemu done rc=$?]"
