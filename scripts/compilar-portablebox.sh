#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build dist/soft
as --64 tests/soft/hello.s -o build/hola.o
ld -nostdlib --build-id=none -static -o dist/soft/hola-linux.elf build/hola.o
as --64 tests/soft/eco.s -o build/eco.o
ld -nostdlib --build-id=none -static -o dist/soft/eco-linux.elf build/eco.o
x86_64-w64-mingw32-g++ -std=c++20 -Os -s -Wall -Wextra -municode \
  -static -static-libgcc -static-libstdc++ \
  src/soft_main.cpp -o dist/soft/portablebox.exe
g++ -std=c++20 -O2 -Wall -Wextra tests/test_soft.cpp -o build/test_soft
build/test_soft dist/soft/hola-linux.elf
file dist/soft/portablebox.exe dist/soft/*.elf
sha256sum dist/soft/portablebox.exe
