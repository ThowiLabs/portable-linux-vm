# PortableBox 1.0 — Debian 12 y Alpine portable

**Esta es una versión experimental para Windows x64. NO es Docker Engine, una VM completa ni un contenedor aislado.**

PortableBox utiliza Blink 1.1, emulador Linux de modo usuario (licencia ISC), y gestor Windows portable en C++20. Sin QEMU, Hyper-V, WSL, Docker Desktop ni servicios que instalar.

## Uso desde PowerShell en la carpeta extraída

    .\portablebox.exe images
    .\portablebox.exe doctor
    .\portablebox.exe run debian:12
    .\portablebox.exe run alpine:3.24

Para ejecutar comandos:

    .\portablebox.exe run debian:12 -- /bin/bash -c "cat /etc/debian_version"
    .\portablebox.exe run alpine:3.24 -- /bin/sh -c "cat /etc/alpine-release"

Estos comandos no son Docker Engine: Debian y Alpine vienen como rootfs locales, ejecutados en modo usuario. No existe pull/build/compose, namespaces, cgroups, ISO, QCOW2 ni kernel Linux. No hay sandbox.

## Carpeta de USB

    PortableBox/
      portablebox.exe
      blink.exe
      LICENSE-BLINK.txt
      LEEME.md
      distros/debian-12/
      distros/alpine-3.24/

La carpeta completa se puede mover entre letras de unidad; se han convertido symlinks a archivos normales para no exigir privilegios elevados al descomprimir.

## Pruebas y limitaciones honestas

- Gestor C++ PE64 compilado con MinGW, comandos help/images/doctor y argumentos de run verificados bajo Wine con motor simulado.
- Blink 1.1 compiló con Cosmopolitan 4.0.2 y funcionó en Linux con VFS.
- Debian 12.15 amd64 desde la imagen OCI oficial debian:12: Bash arrancó y ejecutó comandos dentro del rootfs de Blink.
- Alpine 3.24.2: /bin/sh arrancó y ejecutó comandos.
- Las copias de sistemas de archivos sin enlaces simbólicos mantuvieron estas pruebas satisfactorias.
- **La ejecución del motor Blink APE en Windows físico NO está verificada**. En nuestro entorno Wine se bloquea incluso una app Cosmopolitan mínima, por lo que no se puede garantizar su funcionamiento en tu PC. Prueba doctor antes de run.

## Seguridad

**No es aislamiento de contenedores ni un sandbox**. Blink proporciona un sistema de archivos virtual y las aplicaciones Linux pueden tener acceso a recursos del anfitrión. No ejecutes aplicaciones Linux desconocidas o maliciosas ni utilices esta herramienta con datos importantes sin respaldo. El sistema no simula un kernel Linux real ni implementa todos los syscalls y dispositivos; ciertos programas, apt y servicios pueden fallar.

La interfaz se ejecuta sin habilitar características de hipervisor de Windows, por lo que el error 0x80073712 que aparece al intentar habilitar Hyper-V no debería impedir iniciar el gestor.

## Procedencia y licencias

Blink 1.1.0, https://github.com/jart/blink, licencia ISC (adjunta). Adaptaciones menores para compilar con Cosmopolitan Libc 4.0.2 https://github.com/jart/cosmopolitan. Los sistemas Linux incluyen licencias de software libre y sus avisos originales.

Imagen oficial debian:12 de Docker Hub, arquitectura amd64:
- Manifest: sha256:bc49dc1918ee1a47a93e65b5e4676e8680fb754b133197b92ca52bfe6731d5f0
- Capa: sha256:8cb71386cfcfc55b27b559f73c6d5c0cdcb93b83be8281314116e271cc42cea6 (comprobada).
Alpine minirootfs oficial 3.24.2, SHA256 verificado.

## Pendientes

Validación en Windows real, interoperabilidad de señales/procesos, acceso controlado a carpetas Windows, importación dinámica de más imágenes OCI y autenticación, y pruebas de aplicaciones no triviales. Para Docker completo y aislamiento real hará falta soporte de kernel o tecnologías equivalentes.
