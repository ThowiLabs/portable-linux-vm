# PortableVM Native 0.2 — prototipo WHPX sin QEMU

**Estado:** prototipo experimental de motor de microVM en C++20 para Windows x64. Esta edición no usa los binarios QEMU, ni WSL, ni Docker Desktop. El motor invoca directamente la API **Windows Hypervisor Platform (WHPX)** incluida en Windows.

## Archivos

- `nativevm.exe`: monitor WHPX de aproximadamente 2.7 MB.
- `distros/alpine/vmlinuz`: kernel oficial Alpine Linux 3.24.2, versión 6.18.52-0-virt.
- `distros/alpine/initramfs`: sistema Linux mínimo BusyBox preparado a partir del minirootfs Alpine oficial.

Los 16 MB aproximados de archivos de Linux son distintos de la RAM asignada a la VM. Esta RAM se utiliza del equipo anfitrión.

## Preparación de Windows

Windows 10/11 x64 con CPU compatible con virtualización. Activa en Características de Windows la opción **Windows Hypervisor Platform**. Activa Intel VT-x o AMD-V en el firmware del equipo si está deshabilitado. Puede requerir permisos de administrador y reiniciar Windows. Estas características **no viajan en la USB**.

En Windows Terminal o PowerShell, desde la carpeta extraída:

```powershell
.\nativevm.exe --help
.\nativevm.exe doctor
.\nativevm.exe list
.\nativevm.exe validate alpine
.\nativevm.exe smoke
```

`smoke` es la primera prueba de aceleración real: crea la partición WHPX, mapea 2 MiB, configura una CPU x86 protegida y ejecuta instrucciones `OUT` al UART COM1, terminando con `HLT`. **Debe pasar antes de probar el kernel**. Aquí, desde el entorno Linux del desarrollo, no es posible ejecutar WHPX; aún falta verificar esa prueba en un Windows físico.

## Intentar cargar Alpine con el motor nativo

**Solo si `smoke` pasa:**

```powershell
.\nativevm.exe run alpine --ram 256 --timeout 120
```

Al fallar se informa la primera salida de CPU, incluyendo el código y RIP, para implementar el dispositivo virtual que falte. El comando `validate` solo comprueba imágenes, no intenta ejecutar Linux.

Para registrar otra distribución que ya proporcione kernel x86_64 e initramfs compatibles:

```powershell
.\nativevm.exe add prueba --kernel "D:\imagenes\vmlinuz" --initrd "D:\imagenes\initramfs"
.\nativevm.exe validate prueba
.\nativevm.exe list
```

Cada perfil conserva sus archivos dentro de `distros/NOMBRE/`, por lo que se pueden mover junto con el ejecutable entre letras de unidad.

## Capacidades reales de esta versión

| Función | Estado |
|---|---|
| C++20 y ejecutable PE64 sin QEMU | Compilado |
| Crear partición WHPX y mapear CPU/RAM | Implementado; requiere validar en Windows |
| Programa invitado x86 con UART COM1 y HLT | Implementado en `smoke`; no probado en Windows |
| Cargador directo Linux bzImage e initramfs | Implementado; validado con pruebas de cabeceras y memoria |
| Alpine mínimo y comando `/init` | **Arranca hasta shell** en máquina de referencia QEMU-TCG (prueba separada) |
| El mismo Alpine sobre WHPX propio | **Pendiente de verificar** |
| Consola de entrada interactiva completa | Pendiente: UART RX existe, falta integración con interrupciones/teclado |
| Interruptores de timer, PIC/APIC, PCI | No implementados |
| Red, discos, ISO, QCOW2 en este motor | No implementados |
| Docker dentro del invitado | No disponible todavía |
| Sistema de archivos persistente / Windows montado | No implementado |

La diferencia entre las dos pruebas es importante: Alpine arrancó en el entorno de referencia, donde QEMU emula todos los dispositivos habituales. Nuestro WHPX aún no implementa ese chipset; el intento de Linux puede salir de inmediato al consultar un puerto desconocido. **No equivale a una VM terminada ni a un sustituto de WSL 2.**

## Reproducción de la compilación en Linux

Se requiere MinGW-w64 (x86_64), g++, cpio, gzip y herramientas estándar:

```sh
sh scripts/preparar-initramfs.sh
sh scripts/compilar-nativo.sh
```

`preparar-initramfs.sh` comprueba el SHA256 oficial del minirootfs antes de extraerlo. Requiere permisos de root para incluir `/dev/console` y `/dev/null` en el CPIO. Los archivos de Alpine y QEMU no se atribuyen al autor del gestor. Alpine, BusyBox, musl y el kernel conservan sus licencias respectivas.

## Seguridad y límites

La VM usa memoria separada por WHPX. No recomendamos ejecutar kernels maliciosos: los fallos en un hipervisor pueden comprometer el anfitrión. Es un laboratorio experimental, no una barrera de seguridad auditada. Los archivos `initramfs` corren como root **dentro del invitado**. La imagen distribuida es efímera y no ofrece red ni disco. No guardes credenciales personales dentro de esta versión.

Para compatibilidad amplia con ISOs, QCOW2, Docker y WSL 2 equivalente faltan emuladores de dispositivos, carga de disco, pila de red, almacenamiento y soporte de interrupciones. Mantenerlo ligero es viable si se restringe a un dispositivo microVM; emular cualquier PC completo aumentaría mucho el proyecto.
