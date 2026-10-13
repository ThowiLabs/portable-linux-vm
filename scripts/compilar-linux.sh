#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build dist
CXX=${CXX:-x86_64-w64-mingw32-g++}
"$CXX" -std=c++20 -O2 -Wall -Wextra -Wno-cast-function-type -municode -static -static-libgcc -static-libstdc++ src/main.cpp -o dist/portablevm.exe
"$CXX" -std=c++20 -O2 -Wall -Wextra -Wno-cast-function-type -municode -static -static-libgcc -static-libstdc++ src/whpx_probe.cpp -o dist/whpx-probe.exe
g++ -std=c++20 -O2 -Wall -Wextra tests/test_core.cpp -o build/test-core
build/test-core
file dist/*.exe
sha256sum dist/*.exe

"$CXX" -std=c++20 -Os -flto -ffunction-sections -fdata-sections -Wl,--gc-sections -s -Wall -Wextra -Wno-cast-function-type -municode -static -static-libgcc -static-libstdc++ src/native_main.cpp -o dist/nativevm.exe
g++ -std=c++20 -O2 -Wall -Wextra tests/test_native.cpp -o build/test-native
if [ -f dist/native/distros/alpine/vmlinuz ]; then build/test-native dist/native/distros/alpine/vmlinuz; else build/test-native; fi
