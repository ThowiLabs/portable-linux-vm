# PortableBox 1.0.7 — usuario Linux, prompt y alias Alpine (2026-10-13)

## Evidencia real Windows
- `portablebox.exe run debian:12` abre sesión Bash con Blink 1.0.5.
- Prompt `I have no name!@DESKTOP-8S6S2G8:/SystemRoot/C/Users/Admin/Desktop/portablebox-final$`.
- Comando `su` pide `Password:`. No hay contraseña root predefinida: root en `distros/debian-12/etc/shadow` tiene `root:*:` (bloqueada).
- `portablebox.exe run alpine` devuelve error "Imagen no incluida", mientras `alpine:3.24` está disponible.

## Diagnóstico
- Blink utiliza `getuid()` desde host para el invitado, que puede no coincidir con ninguna entrada de `/etc/passwd`, por eso `I have no name!`. Se desconoce UID concreto en Windows: no prometer que UID0 de guest, pedir `id -u` y `id -g`. No proporcionar contraseña de root ni simular admin Windows.
- `VfsInit` usa el cwd Windows del proceso cuando comienza. Si el cwd está fuera del rootfs, guest termina en `/SystemRoot/C/Users/...` porque Blink expone el anfitrión con `/SystemRoot`. Arrancar CreateProcessW con cwd=rootfs y `-C .` hace que Blink establezca `/`.
- Alias `debian` -> `debian:12`, `alpine` -> `alpine:3.24` solo en el gestor.

## Corrección
- `src/portablebox_app.cpp` 1.0.7 admite alias, fija directorio de trabajo de Blink en la ruta absoluta del rootfs, pasa `-C .` en lugar de la ruta relativa desde carpeta programa. Deja `blink.exe` 1.0.5 que ya es funcional sin cambios.
- `tests/soft/mock_blink.cpp` imprime cwd; `tests/soft/test_portablebox_launch.sh` valida cwd de Debian/Alpine, -C punto, alias y entorno Linux bajo Wine.
- ZIP `dist/releases/PortableBox-1.0.7-Usuario-Alpine-Windows-x64.zip` (~378 KB), sustituye SOLO portablebox.exe.

## Pruebas
- En Linux real Blink -C . desde rootfs Debian, con PATH Linux devuelve `PWD=/`, `UID=0`, `USER=root`, `VERSION=12.15`, `ls /` funciona. Alpine -C . devuelve pwd=/, usuario root.
- En Wine con mock Blink Windows: args y cwd correctos, aliases y PATH/HOME/LANG/LC_ALL correctos.
- **Pendiente prueba de esta revisión en Windows físico del usuario**. UID Linux en Windows todavía no verificado.

## Seguridad
PortableBox no tiene aislamiento Docker ni contenedor real; Blink usa un VFS que monta `/SystemRoot` y puede acceder a archivos del host. No confundir UID0 emulado con Windows admin. La cuenta root Debian está bloqueada por contraseña y no se debe sugerir una clave ficticia.
