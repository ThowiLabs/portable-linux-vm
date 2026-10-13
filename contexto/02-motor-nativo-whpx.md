# Fecha
2026-10-13
# Objetivo
Reducir PortableVM eliminando QEMU y arrancar Alpine con un VMM nativo C++20 basado en WHPX, distribuible en USB.
# Decisiones tomadas
Crear ejecutable adicional nativevm.exe (independiente y sin dependencias de QEMU). Mantener v0.1 con QEMU intacto. Arranque directo bzImage+initramfs; sin BIOS ni firmware. Sólo un vCPU y UART básica. Perfiles de distros portátiles bajo distros/NOMBRE.
# Arquitectura actual
Native CLI -> WinHvPlatform.dll System32 -> WHvCreatePartition/Setup/MapGpaRange/CreateVirtualProcessor/RunVirtualProcessor; VirtualAlloc RAM invitada; configuración inicial de CPU en modo protegido 32-bit; manejo de I/O COM1 limitado; boot_params y E820 cargados según kernel.org.
# Librerías usadas
Win32, WHPX de Microsoft, C++20, MinGW-w64. Alpine 3.24.2 x86_64 vmlinuz-virt e initramfs-virt oficiales extraídos de ISO previamente verificada SHA256.
# Archivos importantes modificados
src/native_core.hpp, src/native_whpx.hpp, src/native_main.cpp, tests/test_native.cpp, README-NATIVE.md, scripts/compilar-linux.sh.
# Problemas encontrados
No hay Windows anfitrión con WHPX en Kaggle (Wine no emula WHPX), así que no se puede verificar guest smoke o boot Linux. El kernel Alpine usa dispositivos e interrupciones no implementados; no hay red, disco ni shell interactiva todavía. En consola COM1 sólo salida UART mínima.
# Soluciones implementadas
Motor PE64 nativo de ~1.1MiB, cargador bzImage, carga initramfs, mapa E820, perfil distro add/list/run, comandos doctor/smoke/inspect/boot, ejecución con timeout, validación de tamaños y límites RAM, test native_core y firma bzImage Alpine.
# Estado de tests
Compilación MinGW OK; pruebas C++ nativas de parser/boot_params y kernel real OK; help/doctor/list/add/run (graceful no WHPX) en Wine OK. smoke y arranque Linux en Windows FÍSICO no verificados.
# Estado de seguridad
WinHvPlatform.dll se carga sólo de System32, no se ejecutan comandos del invitado en host; guest I/O diferente COM1 es rechazado explícitamente; memoria acotada 128-1024MiB; watchdog limita ejecución; perfil nombres ASCII controlados; no se incluyen proveedores de kernel arbitrarios.
# Pendientes
Validación de WHPX físico con smoke; corregir salida WHPX si necesario, boot real; añadir interrupciones (PIC/APIC), timers, cmos, ACPI/PCI, entrada UART, VirtIO block/net, persistencia, múltiples CPU, QCOW2 e ISOs; Docker después de arranque Linux completo.
# Próximos pasos
Entregar ZIP nativo de tamaño ligero. Usuario prueba doctor/smoke en Windows y devuelve salida para continuar hasta Linux.
