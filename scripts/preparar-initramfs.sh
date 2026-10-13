#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
version=3.24.2
package="alpine-minirootfs-${version}-x86_64.tar.gz"
origin="https://dl-cdn.alpinelinux.org/alpine/v3.24/releases/x86_64"
mkdir -p build/alpine dist/native/distros/alpine
[ -s "build/alpine/$package" ] || curl -fL --retry 3 -o "build/alpine/$package" "$origin/$package"
curl -fLsS --retry 2 -o "build/alpine/$package.sha256" "$origin/$package.sha256"
(cd build/alpine && sha256sum -c "$package.sha256")
command -v cpio >/dev/null || { echo "Necesitas instalar cpio" >&2;exit 1; }
command -v gzip >/dev/null || { echo "Necesitas gzip" >&2;exit 1; }
staging="$(mktemp -d)"
trap 'rm -rf "$staging"' EXIT HUP INT TERM
tar -xzf "build/alpine/$package" -C "$staging"
mkdir -p "$staging/dev" "$staging/proc" "$staging/sys" "$staging/tmp" "$staging/run"
# initramfs necesita /dev/console incluso antes de ejecutar /init.
if [ "$(id -u)" -ne 0 ]; then
  echo "Se necesita usuario root para crear los nodos de dispositivo en el initramfs" >&2
  exit 1
fi
mknod -m 600 "$staging/dev/console" c 5 1
mknod -m 666 "$staging/dev/null" c 1 3
cat > "$staging/init" <<'INIT'
#!/bin/sh
export PATH=/sbin:/bin:/usr/sbin:/usr/bin
mount -t devtmpfs devtmpfs /dev 2>/dev/null || true
mount -t proc proc /proc 2>/dev/null || true
mount -t sysfs sysfs /sys 2>/dev/null || true
mkdir -p /dev/pts /tmp /run
mount -t devpts devpts /dev/pts 2>/dev/null || true
printf '\nPortableVM Native: Alpine minimo en RAM\n' >/dev/console
printf 'Sin red, discos ni interruptores de dispositivo completos.\n' >/dev/console
printf 'Kernel: ' >/dev/console
uname -r >/dev/console
printf '\n' >/dev/console
exec /bin/sh -i </dev/console >/dev/console 2>&1
INIT
chmod 755 "$staging/init"
(
 cd "$staging"
 find . -print0 | cpio --null --quiet -o -H newc --owner 0:0 | gzip -n -9 > "$OLDPWD/dist/native/distros/alpine/initramfs"
)
cp dist/native/boot/vmlinuz-virt dist/native/distros/alpine/vmlinuz
echo "Imagen Alpine creada: dist/native/distros/alpine/"
sha256sum dist/native/distros/alpine/*
