# Portable Linux VM — C++20 para Windows x64

Gestor de máquinas virtuales portable para USB. Administra múltiples discos QCOW2, arranca instaladores ISO y permite ejecutar distribuciones Linux reales sin instalar WSL ni Docker Desktop. **El monitor de máquinas virtuales de esta versión es QEMU; el gestor, su configuración y sus controles están escritos en C++20.**

## Preparación de la USB (una vez)

1. Formatea la USB o unidad externa como **exFAT o NTFS**, preferentemente SSD. FAT32 no sirve bien: limita el tamaño de cada archivo a 4 GiB.
2. Descomprime este paquete.
3. Instala 7-Zip en el Windows que prepares (solo se usa para extraer QEMU; no hace falta en otras PCs).
4. En PowerShell, desde la carpeta PortableVM:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\preparar-motor.ps1
.\portablevm.exe doctor
```

El script descarga el instalador oficial de QEMU 11.1.0 (2026-08-11), verifica su hash SHA-512 fijo y **extrae QEMU sin ejecutar su instalador ni registrar servicios**. Deja qemu-system-x86_64.exe, qemu-img.exe, DLL y firmware en `engine\`. Después esa carpeta se puede mover a cualquier unidad sin repetir la preparación.

En cada equipo Windows, la aceleración WHPX necesita que **Windows Hypervisor Platform** y la virtualización de CPU estén disponibles. Puede habilitarse en Características de Windows y puede pedir reinicio; es una condición del **equipo anfitrión**, no algo portable dentro de la USB. Si no está disponible, QEMU recurre a TCG, más lento.

## Usar Alpine Linux x86_64

La ISO oficial Alpine `isos\alpine-virt-3.24.2-x86_64.iso` ya está incluida y verificada con SHA-256.

```powershell
.\portablevm.exe init alpine --size 16G --ram 2048 --cpus 2 --iso ".\isos\alpine-virt-3.24.2-x86_64.iso"
.\portablevm.exe run alpine
```

Se abre la ventana VGA de QEMU con el menú de arranque del ISO. En Alpine inicia como `root` y ejecuta `setup-alpine`. El disco virtual suele aparecer como `/dev/vda` (virtio). Para instalar el sistema al disco elige ese dispositivo **dentro de la VM**, no un disco real de Windows.

Apaga Alpine antes de retirar la ISO:

```powershell
.\portablevm.exe eject alpine
.\portablevm.exe run alpine
```

`eject` conserva la copia de la ISO como `installer-backup.iso`, no la borra.

## Varias distribuciones y otros QCOW2

```powershell
.\portablevm.exe init debian --size 32G --ram 4096 --iso "D:\ISOs\debian-amd64.iso"
.\portablevm.exe init ubuntu --size 40G --ram 4096 --cpus 4 --iso "D:\ISOs\ubuntu.iso"
.\portablevm.exe import laboratorio "D:\maquinas\disco.qcow2"
.\portablevm.exe list
.\portablevm.exe info laboratorio
.\portablevm.exe run laboratorio --accel tcg
.\portablevm.exe run alpine --dry-run
.\portablevm.exe check alpine
```

`import` copia el QCOW2 a la USB para evitar depender de rutas externas. NO ejecutes `check` mientras la VM esté encendida. Una imagen con firmware UEFI o hardware específico puede requerir configuración adicional: esta primera versión inicia máquinas PC con BIOS SeaBIOS.

## Acceso a archivos de Windows

```powershell
.\portablevm.exe run alpine --share "C:\Users\Public\Documents"
```

Expone la carpeta como **disco FAT virtual de solo lectura**: dentro de Linux usa `lsblk -f` para identificarlo y `mount -o ro` para montarlo. Esta modalidad de QEMU recomienda nombres ASCII y **no es un montaje completo y bidireccional tipo /mnt/c de WSL 2**. El montaje de Windows con escritura y sincronización fiable queda pendiente para una siguiente etapa.

## Terminal de Windows y Docker

```powershell
.\portablevm.exe run alpine --console
```

`--console` usa el terminal serie de QEMU. **Solo verás la shell Linux si el sistema invitado tiene consola serie ttyS0 configurada**. Para instalaciones desde ISOs genéricas usa el modo normal con ventana gráfica.

Una vez instalado Alpine en el QCOW2 y con internet en el invitado, los comandos de Docker son:

```sh
apk update
apk add docker
rc-update add docker default
service docker start
docker run --rm -it alpine sh
docker run --rm -it debian:stable-slim bash
```

Docker ejecuta los contenedores **dentro del kernel Linux invitado**. Necesita red, cgroups y soporte del kernel, y no convierte contenedores en VMs independientes: no puedes arrancar ISOs ni kernels arbitrarios en un contenedor normal. No está verificado todavía el funcionamiento de Docker en esta distribución inicial.

## Archivos en la USB

```text
PortableVM/
  portablevm.exe
  whpx-probe.exe
  engine/               (descargado con el script)
  isos/                 (incluye Alpine x86_64)
  vms/
    alpine/
      vm.cfg
      disk.qcow2
      install.iso
  scripts/
    preparar-motor.ps1
```

`whpx-probe.exe` es una prueba **nativa y separada** de la API de Windows Hypervisor Platform: crea y libera una partición y CPU virtual, si WHPX está activo. **NO** arranca Linux ni reemplaza QEMU. Preparación para un futuro backend propio con emulación de BIOS, almacenamiento, interrupciones, E/S y red.

## Estado real de pruebas

- Compilación MinGW-w64 de ejecutables Windows x64 estáticos: **OK**.
- Pruebas unitarias del núcleo C++ en Linux: **OK**.
- `help`, `doctor`, `init`, `list`, `info`, `run --dry-run` del EXE bajo Wine: **OK**.
- QEMU Windows 11.1.0 y `qemu-img.exe` arrancados bajo Wine: **OK**.
- VM x86_64 con ISO real: SeaBIOS e ISOLINUX iniciados bajo QEMU-TCG/Wine: **OK hasta menú de arranque**.
- Instalación completa, red, Docker dentro de invitado, ejecución WHPX en Windows físico, carpeta FAT virtual: **sin verificar todavía**.

## Seguridad, licencias y límites

Los discos son archivos del propio USB, pero cuando el sistema está ejecutándose **nunca retires la unidad sin apagar la VM**. QEMU, firmware y DLL se descargan desde el proveedor, mantienen sus propias licencias (incluidas GPL/LGPL); este paquete **no los redistribuye**. Comprueba siempre que las ISOs proceden de fuentes oficiales. Un sistema invitado no confiable puede explotar fallos del hipervisor; mantén QEMU actualizado.

La portabilidad es de archivos y configuración, **no** elimina las restricciones de CPU, Windows ni WHPX del anfitrión. El proyecto no es un reemplazo completo de WSL 2.
