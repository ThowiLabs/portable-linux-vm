#!/bin/sh
# Regresion del bug C: con -C en Blink para Windows.
set -eu
cd "$(dirname "$0")/../.."
dir=build/portablebox-tests/case
mkdir -p "$dir/distros/debian-12/etc" "$dir/distros/alpine-3.24/etc"
printf 'NAME=Debian\n' > "$dir/distros/debian-12/etc/os-release"
printf 'NAME=Alpine\n' > "$dir/distros/alpine-3.24/etc/os-release"
cp dist/portablebox-final/portablebox.exe "$dir/portablebox.exe"
x86_64-w64-mingw32-g++ -std=c++20 -Os -s -municode -static tests/soft/mock_blink.cpp -o "$dir/blink.exe"
wine_output=$(WINEDEBUG=-all wine "$dir/portablebox.exe" run debian:12 -- /bin/bash -c 'cat /etc/debian_version')
printf '%s\n' "$wine_output" | grep -F 'ARG2=distros/debian-12' >/dev/null
printf '%s\n' "$wine_output" | grep -F 'ARG3=/bin/bash' >/dev/null
printf '%s\n' "$wine_output" | grep -F 'ARG5=cat /etc/debian_version' >/dev/null
alpine_output=$(WINEDEBUG=-all wine "$dir/portablebox.exe" run alpine:3.24)
printf '%s\n' "$alpine_output" | grep -F 'ARG2=distros/alpine-3.24' >/dev/null
printf 'OK: rutas relativas Blink -C y argumentos Windows de Debian y Alpine\n'
