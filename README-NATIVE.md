# PortableVM Native 0.2 — ejecutable C++ sin QEMU

**Estado: prototipo experimental de microhipervisor nativo WHPX. No es todavía un reemplazo funcional de WSL 2.**

El ejecutable `nativevm.exe` se compila con MinGW-w64 para Windows x64 y usa únicamente las APIs `WinHvPlatform.dll` del sistema anfitrión. No necesita QEMU, BIOS, instalaciones de drivers propios, Docker Desktop, ni DLL de terceros.

## Requisitos reales de la PC

- Windows x64 con Windows Hypervisor Platform activado en **Activar o desactivar las características de Windows**.
- Virtualización Intel VT-x / AMD-V habilitada en el firmware y disponible para Windows.
- RAM libre para el Linux invitado, al menos 256 MiB recomendados.
- USB preferiblemente NTFS/exFAT para guardar varios sistemas. No basta con copiar el EXE para activar la plataforma de Windows; esa característica se configura por PC (normalmente con permisos de administrador y reinicio).

Si el host no soporta WHPX, **no existe fallback en esta versión nativa**. La ejecución no puede continuar y da un mensaje de error claro.

## Primera prueba indispensable (Windows físico)

En PowerShell, dentro de la carpeta que contiene `nativevm.exe`:

```powershell
.\nativevm.exe --help
.\nativevm.exe doctor
.\nativevm.exe smoke
```

El comando `smoke` crea una partición WHPX, mapea 2 MiB de RAM invitada, configura una CPU x86 de 32 bits, ejecuta instrucciones máquina `mov`, `out` y `hlt`, e intercepta salidas por el puerto COM1 (0x3f8). Si funciona, debe imprimirse una línea que comienza por `WHPX ejecuta instrucciones x86 reales` y luego `PRUEBA WHPX COMPLETA`.

**No se ha ejecutado esta prueba en Windows físico**. En Linux/Wine solo se verificó la compilación Windows y que maneja correctamente la ausencia de WHPX. Si falla en Windows, guarda la salida para diagnosticar:
```powershell
.\nativevm.exe smoke *> prueba-whpx.txt
```

## Varios kernels Linux en una USB

```powershell
.\nativevm.exe list
.\nativevm.exe inspect .\distros\alpine\vmlinuz
.\nativevm.exe run alpine --ram 256 --timeout 15
```

El paquete lleva archivos oficiales extraídos de Alpine Linux 3.24.2 x86_64, verificados desde una ISO con SHA-256 oficial: `distros/alpine/vmlinuz` y `distros/alpine/initramfs`.

Para añadir otra distribución preparada con un kernel `bzImage` x86 compatible y un initramfs Linux:

```powershell
.\nativevm.exe add debian --kernel "D:\linux\vmlinuz" --initrd "D:\linux\initrd.img"
.\nativevm.exe list
.\nativevm.exe run debian --ram 512 --timeout 20
```

`add` copia ambos archivos bajo `distros/NOMBRE/`, por lo que la USB no necesita las rutas de origen posteriormente.

## Arranque experimental (no validado en Windows)

La opción `run` configura un procesador virtual en modo protegido de 32 bits sin paginación, GDT plano con selectores 0x10/0x18, ESI señalando `boot_params`, el mapa de RAM E820, argumentos de kernel, la imagen protegida Linux a 0x100000 y el initramfs en memoria alta, según el protocolo oficial del kernel. Usa una emulación muy limitada del puerto serie COM1 para observar mensajes.

**No hay todavía controladores de interrupción, reloj, chipset PCI, almacenamiento VirtIO, red, teclado o consola interactiva completos**. Un Linux actual puede detenerse en un acceso I/O no implementado o por timeout; **no se ha logrado ni verificado una shell**. El propósito de esta versión es probar la CPU y el protocolo de arranque paso a paso en Windows físico.

De momento **NO permite instalar ISOs, abrir QCOW2, arrancar Docker ni montar carpetas de Windows**. El motor basado en QEMU de `portablevm.exe` anterior sí gestiona ISO/QCOW2, pero no forma parte del paquete Native.

## Arquitectura real

```text
PortableVM-Native/
  nativevm.exe            # gestor + CPU/memoria/intercepción de I/O C++ estático
  distros/
    alpine/
      vmlinuz             # bzImage real de Alpine
      initramfs           # initramfs de Alpine
```

Windows carga `WinHvPlatform.dll` desde System32. La aceleración es de WHPX, **no un driver de aceleración escrito desde cero**; lo propio es el VMM, el manejo de la memoria, la configuración de CPU y la emulación de I/O. Más adelante deberán implementarse controladores de virtualización propios y una API de almacenamiento persistente.

## Compilación cruzada (Linux)

```sh
sh scripts/compilar-linux.sh
```

Compila ambos gestores (legacy QEMU y WHPX Native) y tests. Para compilar solo el nativo:

```sh
x86_64-w64-mingw32-g++ -std=c++20 -Os -flto -ffunction-sections -fdata-sections -Wl,--gc-sections -s -municode -static -static-libgcc -static-libstdc++ src/native_main.cpp -o nativevm.exe
```

No redirige comandos Linux del invitado a servidores. Los kernels y initramfs son datos no confiables: hay que mantener el anfitrión actualizado, revisar los accesos a memoria y asegurar aislamiento del hipervisor antes de utilizar kernels desconocidos.

### Estado de pruebas

- Compilación Windows PE32+ x64 con MinGW: **OK**.
- Ayuda, doctor, alta/listado de perfiles y validación de bzImage en Wine: **OK**.
- Tests C++ de cabecera bzImage, boot_params, E820, initramfs, comprobaciones de memoria: **OK**.
- Archivo kernel Alpine real analizado: **OK**.
- Aceleración WHPX sobre Windows: **pendiente**.
- Arranque hasta kernel, initramfs y shell: **pendiente**.

Referencia: [protocolo de arranque Linux x86 (kernel.org)](https://www.kernel.org/doc/html/latest/arch/x86/boot.html) y [Windows Hypervisor Platform (Microsoft)](https://learn.microsoft.com/en-us/virtualization/api/hypervisor-platform/hypervisor-platform).
