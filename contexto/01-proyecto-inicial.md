# Fecha
2026-10-12
# Objetivo
Gestor Windows portable para USB: múltiples distribuciones Linux, ISO, QCOW2, Docker dentro del invitado y aceleración WHPX con fallback TCG. EXE compilado en Linux con MinGW.
# Decisiones tomadas
Gestor propio C++20, QEMU como backend para poder arrancar sistemas ya; API WHPX nativa en segundo ejecutable solo como PoC de partición/CPU. Paths relativos a carpeta EXE, ISO y QCOW2 en carpetas VM. Motor QEMU adquirido por usuario desde proveedor mediante PowerShell verificado con SHA-512.
# Arquitectura actual
portablevm.exe Windows crea/valida VM, invoca por CreateProcessW qemu-img y qemu-system desde engine; QEMU usa WHPX/TCG, red NAT; ISO/archivos QCOW2 propios de USB. FAT virtual read-only para carpetas Windows.
# Librerías usadas
C++20 estándar, Win32, API WinHvPlatform opcional; QEMU Windows 11.1.0 extraído del instalador 2026-08-11; Alpine 3.24.2 x86_64.
# Archivos importantes modificados
src/core.hpp, src/winproc.hpp, src/main.cpp, src/whpx_probe.cpp, scripts/preparar-motor.ps1, scripts/compilar-linux.sh, README.md, tests/test_core.cpp.
# Problemas encontrados
WHPX depende de configuración externa al USB; Docker necesita kernel Linux con cgroups; contenedores no son VM. La terminal serial puede requerir ttyS0. QEMU Windows grande y sujeto a licencias. FAT virtual solo lectura y sin Unicode fiable.
# Soluciones implementadas
Detección opcional WHPX, auto WHPX:TCG, QCOW2 real con qemu-img, ISO copiada portable, importación QCOW2, comandos list/info/eject/run/check, FAT read-only, documentación e imágenes verificadas.
# Estado de tests
MinGW compila PE64, core unit tests OK, Wine help/doctor/init/list/info/dry-run OK; QEMU Windows y qemu-img ejecutados en Wine, ISO Alpine x86_64 arrancada hasta SeaBIOS e ISOLINUX. No se ha validado instalación completa ni Docker ni WHPX en Windows físico.
# Estado de seguridad
CreateProcessW evita shell injection; nombres VM restringidos y argumentos validados; import QCOW2 revisa firma; motor QEMU extraído desde fuente oficial con SHA512 fijo; carpeta Windows expuesta read-only. VMs potencialmente no confiables: revisar vulnerabilidades del monitor.
# Pendientes
Verificar funcionamiento nativo Windows, instalación Alpine, Docker y su red; sistema integrado SSH/WINFS; snapshots y apagado administrado; motor WHPX propio de arranque Linux con chipset, BIOS, dispositivos y red.
# Próximos pasos
Empaquetar compilados y Alpine x86_64; probar en Windows físico con WHPX habilitado; verificar QEMU en USB y procesos /mnt/c equivalente.
