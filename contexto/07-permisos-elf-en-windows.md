# Fecha
2026-10-13
# Error reproducido por usuario Windows
PortableBox 1.0.3 run debian:12 -- /bin/bash -c cat /etc/debian_version:
I... Initializing VFS
PortableBox diagnostic: loader stage=permissions errno=5
/bin/bash: failed to load executable (errno 5)
doctor Blink 1.1 funciona, Test-Path bin/bash True.
# Hallazgo
loader.c CheckExecutableFile comprueba S_ISREG y luego st_mode & 0111. Windows NTFS / ZIP extraído normalmente no preserva el bit Unix ejecutable, por tanto falla; errno=5 podría estar contaminado por fprintf previo. Esta es hipótesis causal más probable; sin modo octal todavía no se puede descartar fallo de S_ISREG o tamaño.
# Solución
Blink 1.1 compilado Cosmopolitan bajo Windows omite exclusivamente la comprobación POSIX X_OK (mantiene file regular y size >0). El validador ELF IsSupportedExecutable sigue verificando el formato y la arquitectura tras la carga. Bajo Linux conserva verificación chmod +x. Diagnóstico de loader guarda errno antes de fprintf y imprime el modo octal de archivo. Además limpia mmap temporal si VfsPread falla.
# Archivos
patches/003-blink-windows-exec-permissions.patch, contexto/07-permisos-elf-en-windows.md. Ejecutable preparado en dist/releases/PortableBox-1.0.4-Parche-Blink-Windows-x64.zip (~879 KB).
# Tests
Compilación cosmocc pasa. Blink en Linux con rootfs Debian12 imprime 12.15 y Alpine imprime 3.24.2. Script regresión Wine para lanzador Windows pasa. No ejecución Blink APE con Windows real en este entorno, pendiente usuario.
# Próximo
Usuario debe sustituir solo blink.exe, conservar distros y portablebox.exe. Ejecutar run debian:12 -- /bin/bash -c "cat /etc/debian_version"; si falla recoger stage, errno y mode. Sin necesidad de VHDX ni Hyper-V.
