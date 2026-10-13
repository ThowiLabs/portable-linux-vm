# Fecha
2026-10-13
# Objetivo
Continuar microVM C++20 sin QEMU: preparar Linux mínimo autoiniciable, UART 16550, validar memoria y empaquetar Windows x64.
# Decisiones tomadas
Alpine 3.24.2 x86_64 oficial, kernel 6.18.52-0-virt, minirootfs oficial verificado SHA-256 e initramfs CPIO propio (shell BusyBox). Solo sistema efímero en RAM; sin BIOS ni ISO en motor nativo.
# Arquitectura actual
nativevm.exe usa API WHPX; bootea bzImage+initramfs en RAM, CPU x86 modo protegido, UART 16550 por trampas de E/S, mapa E820. Solo soporte experimental de puerto COM1, sin PIC/IOAPIC/PIT/APIC, red o almacenamiento.
# Librerías usadas
Win32, WHvPlatform (carga dinámica), C++20 stdlib, Alpine minirootfs, BusyBox, musl, kernel oficial Alpine.
# Archivos importantes modificados
src/native_main.cpp, native_core.hpp, native_whpx.hpp, native_uart.hpp; tests/test_native.cpp, tests/test_uart.cpp; scripts/preparar-initramfs.sh, scripts/compilar-nativo.sh, README-NATIVO.md.
# Problemas encontrados
Entorno Linux no expone /dev/kvm ni WHPX: imposible validar directamente aceleración Windows. Linux puede hacer HLT como espera de IRQ, no como finalización; cambió el criterio. Alpine ISO original requiere modloop, por eso se hizo initramfs propio.
# Soluciones implementadas
UART 16550 con DLAB, TX/RX FIFO básica, LSR; validación independiente de regiones, kernel e initramfs; CLI validate; creación de initramfs con /init; separación éxito HLT solo para smoke.
# Estado de tests
Pruebas test_native y test_uart pasan; compilación PE64 MinGW correcta; Wine help/list/validate/doctor (WHPX ausente) correctamente; arranque de kernel+initramfs llega a ~ # en VM de referencia QEMU TCG. Esto NO verifica motor WHPX en Windows.
# Estado de seguridad
QEMU no forma parte del motor nativo, NO hay dispositivo de red, no se distribuyen credenciales; límites de RAM e imágenes; output UART limitado en memoria; descarga Alpine validada SHA-256.
# Pendientes
Prueba smoke y boot en Windows WHPX físico; registrar la primera salida no implementada; controlador PIC/PIT/APIC, entrada de consola con IRQ; almacenamiento virtio y red; subsistemas Docker/ISO/QCOW2 nativos.
# Próximos pasos
Probar nativevm.exe smoke en Windows con Windows Hypervisor Platform habilitado; después nativevm.exe run alpine --ram 256 --timeout 120 y compartir logs. Implementar PIC/PIT e input UART tras diagnóstico.
