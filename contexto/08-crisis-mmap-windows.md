# Fecha: 2026-10-13
# Incidencia
En Windows 10/11 real, PortableBox 1.0.4 encontró Bash Debian pero reportó:
"unrecoverable mmap() crisis: Blink was built with NDEBUG".
Previamente pasó checks de permisos ELF y loader, VFS inicializa.
# Diagnóstico técnico
La alarma PanicDueToMmap() se dispara en blink/memorymalloc.c ReserveVirtual al fallar una asignación. El backend sin mapeo lineal crea una asignación por página de archivo del ELF de 4 KiB (PAGE_MUG). Windows mapea archivos con granularidad de desplazamientos y reservas diferente (habitualmente 64 KiB) por lo que esas operaciones de mmap en archivos son candidatas al error. Esta es hipótesis sólida pero no 100 % confirmada sin logs de mapeo Win32.
# Corrección
patches/004-blink-windows-private-file-page-copy.patch
En compilación Cosmopolitan sobre Windows, para páginas de mapeos privados de archivos, sustituir mmap de archivo por AllocateBig anónimo RW + VfsPread exacto por fragmentos, rellenando ceros al final. Dejar mapeos compartidos y modo Linux sin cambios, conservar permisos lógicos de páginas a través de PTE.
# Pruebas
- Compilación Blink 1.1 con Cosmopolitan: OK.
- Forzar temporalmente la ruta de copia de páginas también en Linux para probarla: Debian12 /bin/bash -c ejecuta y devuelve 12.15; Alpine3.24 /bin/sh devuelve 3.24.2. OK.
- Restaurar condición IsWindows() y compilar Release: OK; Debian12 Linux sigue ejecutando.
- No se ha podido probar Blink bajo Windows real localmente; se depende del usuario para verificar.
# Entrega
dist/releases/PortableBox-1.0.5-Parche-Blink-Windows-x64.zip (879 KiB) incluye blink.exe y licencia ISC. Reemplazar solo blink.exe en portablebox-final. No requiere instalar Hyper-V, WSL, QEMU.
# Estado
No equivale a Docker Engine ni a un sandbox, no debe ejecutar binarios desconocidos. El modo anónimo puede incrementar gasto de memoria. Si persiste, leer salida completa: ahora el error incluye tamaño, offset o errno del mapeo, o error pread.
# Siguiente paso
Pedir usuario probar con Debian: .\portablebox.exe run debian:12 -- /bin/bash -c "cat /etc/debian_version". Si funciona, probar Alpine y shell interactiva. No afirmar éxito nativo Win32 hasta recibir resultado.
