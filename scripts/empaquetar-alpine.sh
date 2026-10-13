#!/usr/bin/env bash
# Crea localmente un paquete completo PortableBox+Alpine; nunca hace push ni publica binarios.
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
version=3.24.2
expected_alpine=c5ca053cfe1d85c5b96dff8b9bc57045f7f184a30ffb6b65776409ca90388677
expected_blink=2c326c70e0f3dbe06c01609d11e5ad4c303f5f206b0127ff6deea8d8cd04aaed
blink_src=${BLINK_EXE:-"$repo/dist/apt-diagnostico/blink.exe"}
blink_license=${BLINK_LICENSE:-"$(dirname "$blink_src")/LICENSE-BLINK.txt"}
out="$repo/dist/PortableBox-Alpine-${version}-Windows-x64.zip"
work="$repo/build/alpine-pack"
stage="$work/PortableBox"
tarball="$work/alpine-minirootfs.tar.gz"
for tool in curl sha256sum tar zip unzip python3 x86_64-w64-mingw32-g++; do
  command -v "$tool" >/dev/null || { echo "Falta herramienta: $tool" >&2; exit 1; }
done
[[ -f "$blink_src" ]] || { echo "Falta blink.exe; define BLINK_EXE" >&2; exit 1; }
actual=$(sha256sum "$blink_src" | cut -d' ' -f1)
[[ "$actual" == "$expected_blink" ]] || { echo 'blink.exe no coincide con el motor verificado 1.0.9' >&2; exit 1; }
mkdir -p "$work" "$repo/dist"
url="https://dl-cdn.alpinelinux.org/alpine/v3.24/releases/x86_64/alpine-minirootfs-${version}-x86_64.tar.gz"
curl -fL --retry 3 --connect-timeout 15 "$url" -o "$tarball"
printf '%s  %s\n' "$expected_alpine" "$tarball" | sha256sum --check --status || { echo 'SHA256 Alpine incorrecto' >&2; exit 1; }
rm -rf "$stage" "$work/rootfs-original"
mkdir -p "$stage/distros" "$stage/scripts" "$work/rootfs-original"
tar -xzf "$tarball" --no-same-owner -C "$work/rootfs-original"
python3 "$repo/scripts/make_windows_rootfs.py" "$work/rootfs-original" "$stage/distros/alpine-3.24"
printf 'nameserver 1.1.1.1\nnameserver 1.0.0.1\noptions timeout:2 attempts:2\n' > "$stage/distros/alpine-3.24/etc/resolv.conf"
x86_64-w64-mingw32-g++ -std=c++20 -Os -s -Wall -Wextra -municode \
  -static -static-libgcc -static-libstdc++ "$repo/src/portablebox_app.cpp" -o "$stage/portablebox.exe"
cp "$blink_src" "$stage/blink.exe"
cp "$repo/scripts/diagnostico-apt.ps1" "$stage/scripts/diagnostico-apt.ps1"
[[ -f "$blink_license" ]] || { echo "Falta LICENSE-BLINK.txt; define BLINK_LICENSE" >&2; exit 1; }
cp "$blink_license" "$stage/LICENSE-BLINK.txt"
cat > "$stage/LEEME.txt" <<'LEEME'
PortableBox para Windows x64 con Alpine Linux 3.24.2 integrado.
Extraer todo el ZIP y ejecutar desde PowerShell:
  .\portablebox.exe images
  .\portablebox.exe run alpine
Dentro de Alpine:
  apk update
  apk add curl
Se usa apk, no apt; Debian no esta incluido.
No es una VM completa ni un sandbox; no ejecutar codigo Linux no confiable.
La red/DNS depende de Windows. DNS del invitado: distros/alpine-3.24/etc/resolv.conf.
LEEME
# Windows no admite ':' y es case-insensitive: rechazo deliberado antes de empacar.
python3 - "$stage" <<'PY'
import os, sys, re
root=sys.argv[1]
for d, folders, files in os.walk(root):
  lower=set()
  for name in folders+files:
    if re.search(r'[<>:"\\|?*]',name) or name.endswith((' ','.')): raise SystemExit(f'Ruta incompatible Windows: {name}')
    if name.lower() in lower: raise SystemExit(f'Colision mayusculas/minusculas: {name}')
    lower.add(name.lower())
    if os.path.islink(os.path.join(d,name)): raise SystemExit(f'Symlink restante: {name}')
PY
rm -f "$out"
(cd "$stage" && zip -q -9 -r "$out" .)
unzip -t "$out" | tail -n 1
sha256sum "$out"
echo "Paquete creado localmente en $out; NO se ha enviado a GitHub"
