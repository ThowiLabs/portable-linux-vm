# Fecha
2026-10-13
# Incidencia
PortableBox 1.0.2 en Windows real: doctor imprime versión Blink; run debian:12 -- /bin/bash -c ... inicializa VFS y falla "/bin/bash: failed to load executable (errno 5)". Test-Path .\\distros\\debian-12\\bin\\bash = True.
# Evaluación
errno 5=EIO, no es una prueba de que el archivo Linux esté dañado. El loader Blink 1.1 agrupaba VfsOpen, VfsFstat, CheckExecutableFile y Mmap en una sola condición; no se podía identificar la causa exacta. La búsqueda de comando ya funciona tras parche anterior (X_OK). VHDX no resolvería este error.
# Cambios
build/blink-src/blink/loader.c: separar etapas y emitir PortableBox diagnostic: loader stage=open|fstat|permissions|mmap|read-after-mmap errno=NUM. Si mmap de archivo falla, intentar mapping anónimo y cargar con VfsPread. build/blink-src/blink/hostfs.c: en Windows y tras fallo openat con dirfd intentar open sobre ruta absoluta derivada del HostfsGetHostPath. Guardar parches reproducibles patches/001-blink-loader-diagnostics-and-mmap.patch y patches/002-blink-windows-openat-fallback.patch.
# Resultados
Blink recompilado en Cosmopolitan 4.0.2; Debian 12.15 /bin/bash -c 'cat /etc/debian_version' funciona en Linux; Alpine 3.24.2 /bin/sh funciona en Linux. El ZIP del motor corregido está verificado, pero Windows real no se ha vuelto a probar todavía; falta confirmación del usuario. No afirmar que ya se solucionó.
# Producto
dist/releases/PortableBox-1.0.3-Parche-Blink-Windows-x64.zip (~878 KiB), sustituye solo blink.exe; no descargar de nuevo rootfs. Enlace de pruebas HTTP 200 en el servidor temporal.
# Seguridad
No es Docker Engine, WSL, VM ni sandbox. La funcionalidad permite acceso host vía VFS; no ejecutar código no confiable. No desactivar Windows Defender. Guardar diagnóstico sin datos privados de equipo.
# Próxima acción
Pedir ejecutar comando con el EXE 1.0.3; si falla recuperar mensaje 'PortableBox diagnostic: loader stage=... errno=...' para arreglar causa concreta. No pedir más parches sin diagnóstico.
