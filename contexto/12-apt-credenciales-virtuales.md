# 2026-10-13 — Apt no ejecuta setgroups/setegid/seteuid en PortableBox 1.0.8

## Reporte Windows real
Usuario abre `root@DESKTOP-8S6S2G8:/#`, `ls` funciona. `apt update` produce:
```
E: setgroups 65534 failed - setgroups (38: Function not implemented)
E: setegid 65534 failed - setegid (38: Function not implemented)
E: seteuid 42 failed - seteuid (38: Function not implemented)
E: setgroups 0 failed - setgroups (38: Function not implemented)
```
`apt` aplica reducción temporal de privilegios al usuario `_apt`, UID42, GID65534 de Debian. Es comportamiento normal. La versión 1.0.8 evitaba afectar el host Windows pero rechazaba cualquier cambio de identidad Linux distinto de 0, ENOSYS.

## Corrección 1.0.9
A `struct System` (Blink machine.h) se añaden ruid/euid/suid, rgid/egid/sgid virtuales, lista guest de grupos hasta 32 entradas y cantidad. Inician en cero porque `NewSystem()` inicializa la estructura con `memset` a 0. Son datos propios del proceso guest, heredados al fork mediante copia del espacio del proceso anfitrión; threads comparten `System`.

En syscall.c solo bajo Cosmopolitan y IsWindows() se sustituye UID/GID POSIX anfitrión por guest, agregando semántica de setuid/setgid/setresuid/setresgid/setreuid/setregid, getuid/geteuid/getgid/getegid, getresuid/getresgid y setgroups/getgroups, sin llamar nunca a APIs host de cambio de identidad. El root guest UID0 permite cambiar identidad, se mantiene UID guardado y restauración si procede, un usuario sin root no puede subir arbitrariamente a UID0. Retornos fallidos son EPERM, no ENOSYS. Cambios de grupos exclusivos guest, sin alterar Windows. Modificar UID virtual NO implementa ACL/POSIX permisos del filesystem ni aislamiento.

`patches/007-blink-windows-guest-credentials.patch` contiene diff incremental desde 005 (blink machine.h y syscall.c). `scripts/patch_blink_virtual_guest_credentials.py` genera el parche en un checkout fuente que ya tiene 005 aplicado. `tests/soft/guest_credentials.c` programa de regresión.

## Pruebas efectuadas
- Compilación Blink con Cosmopolitan 4.0.2 OK.
- Ejecutable de prueba dinámico enlazado con bibliotecas de Debian dentro de rootfs: pasa getresuid/getresgid inicial 0, setgroups{65534,42}, getgroups, setresgid effective65534, setresuid effective42, EPERM para setgroups de usuario no root, restauración efectiva root0, grupos vacíos y luego setuid(42) irreversible con EPERM al intentar volver a root. Resultado `CREDENTIALS_OK`, código de salida 0.
- La prueba forzó temporalmente branch guest-virtual Windows al ejecutar en Linux como host UID1000/GID1000 sin privilegios (`setpriv --reuid=1000 --regid=1000 --clear-groups`); mismo resultado `CREDENTIALS_OK` sin cambiar identidad anfitriona.
- Se RESTAURÓ IsWindows() y recompiló Blink. Debian12 `cat /etc/debian_version` y `id` OK; Alpine 3.24 `id` y `cat /etc/alpine-release` OK. Blink ejecuta `/usr/bin/apt-get --version` y devuelve APT 2.6.1.
- Una prueba de ejecutable vinculado estáticamente dio segfault independiente del nuevo código; el ejecutable dinámico Debian pasó. Hay limitaciones Blink aparte de UID/GID, y apt red/fork no validados aún en Windows físico.

## Entrega
`dist/releases/PortableBox-1.0.9-APT-Credenciales-Windows-x64.zip` 880KB (ZIP test OK, URL temporal HTTP 200). Contiene solo `blink.exe`, licencia ISC y LEEME. Mantener `portablebox.exe` versión 1.0.7, todo `distros` sin tocar. SHA256 `fa722b4bbbf77090ade52a2f06272f1c8aead10c2e6f703e4fcd19831302906a`.

## Próxima prueba usuario
Reemplazar blink.exe, ejecutar `portablebox.exe run debian -- /bin/bash -c 'id; whoami; pwd'`, `portablebox.exe run debian -- /usr/bin/apt-get --version`, y dentro de root Debian `apt update` si desea probar, ideal con timeouts `apt-get -o Acquire::Retries=0 -o Acquire::http::Timeout=10 -o Acquire::https::Timeout=10 update`. Pedir primera línea error completa. No afirmar que APT red y repo ya funcionan; solo los cambios de credenciales. No desactivar seguridad Windows ni pedir ejecución como admin.
