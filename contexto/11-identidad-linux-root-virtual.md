# 2026-10-13 — Root Linux virtual (no se mezclan usuarios Windows)

## Confirmación del problema en Windows
Usuario ejecutó `portablebox.exe run debian -- /bin/bash -c 'id -u; id -g; id; whoami; pwd'` y obtuvo UID 554, GID 554, `whoami` no encontró nombre, y cwd `/`. Por eso el prompt decía `I have no name!` y `su` pedía contraseña, aunque en la imagen Debian OCI existe `root:x:0:0` y `/etc/shadow` tiene cuenta root `*` (contraseña bloqueada). Correctamente exige que root Linux NO se mezcle con usuario ni privilegios Windows.

## Implementación
- En `blink/syscall.c`, con `#ifdef __COSMOPOLITAN__` y `IsWindows()`, crear `PortableBoxGuestRoot`.
- `getuid/geteuid/getgid/getegid` devuelven 0 al guest solo en Windows.
- `getresuid/getresgid` devuelven 0 para IDs real, efectivo, guardado al guest solo en Windows.
- `getgroups` entrega grupo root 0, validando tamaño/memoria del invitado.
- `setuid/setgid/setreuid/setregid/setresuid/setresgid`: permiten no-op sobre UID/GID0 y rechazan otras identidades con ENOSYS en Windows, sin efectuar llamadas que modifiquen identidad anfitriona; `setgroups` retorna ENOSYS.
- En `blink/hostfs.c`, se mantiene fallback Win32 openat->open de archivo pero se suprime ruido ENOENT en búsquedas normales. La identidad virtual NO otorga admin en Windows.

## Código
- `patches/005-blink-windows-virtual-uid0-gid0.patch` (diff frente a Blink upstream 1.1.0).
- `patches/006-blink-openat-fallback-silence-enoent.patch` (diff incremental tras parche 002).
- ZIP `dist/releases/PortableBox-1.0.8-Root-Virtual-Windows-x64.zip`, 879 KiB: **reemplaza solo blink.exe**, preserva portablebox.exe versión 1.0.7 que ya tiene alias Debian/Alpine, cwd=/ y PATH Linux.

## Pruebas
- Cosmopolitan Blink recompiló sin errores.
- Linux con rama normal: Debian 12.15 y Alpine 3.24.2 funcionan igual.
- Prueba controlada de root virtual (cambiando temporalmente retorno de PortableBoxGuestRoot() a true en Linux, nunca incluido en release), ejecutando Blink como host **UID1000/GID1000** con `setpriv` sin privilegios, devolvió `guest uid=0`, `guest gid=0`, `guest name=root`, `id=uid=0(root) gid=0(root) groups=0(root)` y Debian `12.15`. Se restauró el código para activar exclusivamente en Windows, se recompiló release y comprobó checksum.
- Windows real aún por comprobar para esta versión. Debe pedir salida de `portablebox.exe run debian -- /bin/bash -c 'id; whoami; pwd'`.

## Seguridad y límites
Separación de identidad de syscall NO es separación de archivos/privilegios a nivel kernel: Blink VFS expone `/SystemRoot` y operaciones de archivos del anfitrión siguen sujetas a ACL Windows del proceso anfitrión. No es Docker, chroot de seguridad, ni VM, no puede ofrecer aislamiento confiable y no ejecutar binarios desconocidos. ROOT invitado es representación lógica; no autoriza modificación de Windows. Usuarios secundarios, apt/red, PAM/SU, Linux capabilities, propietarios de archivos, procesos y permisos guest no están completamente implementados; no prometerlos. No hay contraseña root por defecto (bloqueada por `*`).
