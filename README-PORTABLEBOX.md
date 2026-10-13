# PortableBox 0.1 — ejecutor Linux experimental sin hipervisor

**PortableBox es un prototipo de compatibilidad Linux x86-64 en Windows. No es una máquina virtual ni un sistema Docker completo.**

Escrito en C++20, compilado con MinGW-w64 para Windows x64. No requiere QEMU, Windows Hypervisor Platform, Hyper-V, WSL, Docker Desktop, instaladores, servicios ni permisos de administrador para la demostración. Solo utiliza APIs incluidas en Windows (Kernel32 y CRT estándar). El ejecutable ocupa aproximadamente 1.1 MB.

## Qué hace realmente

- Lee archivos ELF64 Linux x86-64 **ET_EXEC estáticos**.
- Valida cabeceras, arquitectura, segmentos, tamaños, rangos y punto de entrada.
- Carga código x86-64 a memoria virtual y lo ejecuta en la CPU física del PC, sin iniciar un kernel Linux.
- Traduce un **subconjunto mínimo** de syscalls Linux mediante intercepción de las instrucciones `syscall`: `read` (stdin), `write` (stdout/stderr), `exit`, `exit_group` y `getpid` (ID virtual).
- El programa `hola-linux.elf` es un ELF real compilado con el ensamblador de Linux. `eco-linux.elf` demuestra entrada y salida interactiva básica mediante redirección.

Ejemplos desde PowerShell o Windows Terminal:

```powershell
.\portablebox.exe --help
.\portablebox.exe inspect .\hola-linux.elf
.\portablebox.exe run .\hola-linux.elf
"Hola" | .\portablebox.exe run .\eco-linux.elf
```

Resultado esperado de `hola-linux.elf`:

```text
Hola desde un ELF Linux real, sin Hyper-V ni QEMU!
```

## Limitaciones críticas

**No ejecutar un contenedor Docker con `debian:12` ni ninguna imagen Docker en esta versión.** No se incluyen un kernel, sistema de archivos de distribución, gestor de procesos Linux, dinámica de libc, namespaces, cgroups, sockets ni un motor OCI. Aún no puede ejecutar herramientas Linux comunes de Debian/Ubuntu/Alpine. El soporte futuro requeriría un subconjunto grande de syscalls y un loader/linker dinámico, POSIX FS, gestión de procesos, memoria, señales y traducción de ABI. Muchos programas usan instrucciones y convenciones incompatibles con las implementadas hoy.

**No hay aislamiento de seguridad.** El código Linux invitado se ejecuta dentro del mismo proceso de Windows que PortableBox. Un binario malicioso o modificado puede comprometer el host. **Nunca ejecutar ejecutables descargados de internet ni imágenes Docker desconocidas** con este prototipo. Utiliza únicamente los ELF de demostración incluidos. No hay sandbox ni contención equivalente a Docker o VM.

El soporte de CPU es solo x86-64. En equipos ARM/Windows ARM esta edición no sirve. La traducción de `syscall` se realiza parcheando instrucciones en segmentos ejecutables y manejando una excepción `INT3`. No interpreta todas las instrucciones: son instrucciones x86-64 **nativas** que ejecuta la CPU. El tiempo límite de ejecución es de 5 segundos por programa.

## Compilación del ejecutable y los ejemplos desde Linux

```sh
as --64 tests/soft/hello.s -o build/hola.o
ld -nostdlib --build-id=none -static -o dist/soft/hola-linux.elf build/hola.o
as --64 tests/soft/eco.s -o build/eco.o
ld -nostdlib --build-id=none -static -o dist/soft/eco-linux.elf build/eco.o

x86_64-w64-mingw32-g++ -std=c++20 -Os -s -Wall -Wextra -municode -static \
  -static-libgcc -static-libstdc++ src/soft_main.cpp -o dist/soft/portablebox.exe

g++ -std=c++20 -O2 tests/test_soft.cpp -o build/test_soft
build/test_soft dist/soft/hola-linux.elf
```

Se verificó con Wine en el entorno Linux que el ejecutable responde a `--help`, `inspect` y ejecuta los dos programas Linux de ejemplo (salida y eco por stdin). **La prueba nativa en Windows físico sigue pendiente.**

## Dirección futura

PortableBox corresponde al **camino sin hipervisor** del proyecto PortableVM. Permite estudiar compatibilidad Linux sin habilitar ninguna característica de Windows. El motor WHPX y el viejo gestor QEMU permanecen separados en el repositorio original; no son dependencias de este paquete.

Para ejecutar `debian:12` de Docker de verdad sin Hyper-V se necesitarían numerosas piezas adicionales, no solo cambiar la sintaxis de un comando. No se anuncia esta funcionalidad hasta que esté implementada y probada.

## Código y seguridad

No se necesitan bibliotecas externas para compilar el ejecutable aparte de las herramientas de compilación. Los binarios de prueba están escritos en ensamblador propio; no se incluyen bibliotecas ni imágenes Docker de terceros. Se validan límites y segmentos ELF, pero esto no hace segura la ejecución de archivos hostiles.
