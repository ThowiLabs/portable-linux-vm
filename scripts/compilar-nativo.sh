#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build dist/native
CXX="${CXX:-x86_64-w64-mingw32-g++}"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Wno-cast-function-type \
  -municode -static -static-libgcc -static-libstdc++ \
  src/native_main.cpp -o dist/native/nativevm.exe
cp dist/native/nativevm.exe dist/nativevm.exe
g++ -std=c++20 -O2 -Wall -Wextra tests/test_native.cpp -o build/test_native
g++ -std=c++20 -O2 -Wall -Wextra tests/test_uart.cpp -o build/test_uart
build/test_native dist/native/distros/alpine/vmlinuz
build/test_uart
file dist/native/nativevm.exe
sha256sum dist/native/nativevm.exe
echo "Nativo compilado: dist/native/nativevm.exe"
