# Fecha
2026-10-13
# Objetivo
Diseñar solución alternativa ligera, portable USB y sin dependencias de WHPX/Hyper-V/QEMU/WSL tras error Windows 0x80073712 del usuario.
# Decisiones tomadas
Crear PortableBox como ejecutor Linux ELF64 x86-64 en modo usuario, no emulador de PC ni VM. Ejecución nativa CPU x86-64 con sustitución de instrucciones SYSCALL por INT3 y traductor mínimo a Win32. Mantener intactos otros motores del proyecto.
# Arquitectura actual
portablebox.exe C++20 PE64 estático, carga ELF ET_EXEC por VirtualAlloc a direcciones ELF; valida segmentos/permisos; intercepta syscalls Linux read/write/exit/exit_group/getpid mediante excepción VEH Win32; ejecuta hilo invitado limitado a 5 s. Dos ELF Linux reales estáticos ensamblados (saludo, eco).
# Librerías usadas
Solo Win32 Kernel32 + C runtime estándar y STL compilada estáticamente. No DLL de QEMU ni WHPX/WSL.
# Archivos importantes modificados
src/soft_elf.hpp, src/soft_main.cpp, tests/test_soft.cpp, tests/soft/hello.s, tests/soft/eco.s, scripts/compilar-portablebox.sh, README-PORTABLEBOX.md.
# Problemas encontrados
Docker debian:12 no es ejecutable ELF y requiere OCI layers, namespaces y syscall complejas. Esta edición no tiene glibc/musl dinámicas ni execve; no corre distros completas. Ejecución invitada no aislada: no se debe usar con binarios no confiables.
# Soluciones implementadas
Parser ELF64 con validación límites/tipos/cabecera/segmentos, lectura/escritura Linux vía API Windows, exit, prueba real de ELF estáticos compilados con assembler de Linux y ejecutados desde EXE bajo Wine. Binario ~1.1MB.
# Estado de tests
Compilación MinGW x64 completada; unit test ELF pasa; wine portablebox --help/inspect/run funciona; hola-linux.elf imprime texto y eco-linux.elf prueba stdin/stdout con Wine. No validado en Windows físico todavía.
# Estado de seguridad
No existe aislamiento: código Linux corre directamente en proceso Windows, potencial compromiso del anfitrión. Solo muestras ELF propias. Segmentos ELF sin WX y buffers validados; límites RAM 256MiB, programa 32MiB; timeout no sustituye sandbox.
# Pendientes
Prueba en Windows real. Para soporte Docker/OCI necesitaría loader dinámico ELF, syscalls amplias, FS virtual y gestión de procesos; probablemente un programa de gran alcance. No prometer compatibilidad general.
# Próximos pasos
Empaquetar portablebox.exe + dos ELF demos + README para USB, código fuente y hashes. Solicitar resultado de demo en Windows. Evitar ejecutar imágenes Docker externas mientras no haya aislamiento.
