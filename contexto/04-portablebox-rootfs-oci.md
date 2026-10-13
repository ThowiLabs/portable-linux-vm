# Fecha
2026-10-13
# Objetivo
PortableBox offline Debian12/Alpine sin instalar Hyper-V, WSL o Docker Desktop en Windows.
# Decisiones tomadas
Blink 1.1 ISC compilado con Cosmopolitan Libc 4.0.2 como ejecutor x86-64 Linux usermode, no VM. Gestor C++20 acepta run debian:12 y run alpine:3.24. NO es Docker Engine.
# Arquitectura actual
Gestor PE64 Win32 CreateProcessW + Blink APE (1.9MB) con VFS -C rootfs + dos rootfs Linux offline. Debian oficial Docker Hub amd64 manifest sha256:bc49dc1918ee1a47a93e65b5e4676e8680fb754b133197b92ca52bfe6731d5f0, capa verificada sha256:8cb71386cfcfc55b27b559f73c6d5c0cdcb93b83be8281314116e271cc42cea6; Alpine minirootfs 3.24.2.
# Librerías usadas
C++20 STL, Win32, Blink 1.1 ISC, Cosmopolitan Libc 4.0.2.
# Archivos importantes modificados
src/portablebox_app.cpp, scripts/make_windows_rootfs.py, README-PORTABLEBOX-FINAL.md, contexto/04-portablebox-rootfs-oci.md.
# Problemas encontrados
Wine no ejecuta APE Cosmopolitan y cuelga incluso hello mínimo (limitación de validación). Motor Blink NO probado en Windows físico. Symlinks Unix requieren permisos elevados en Windows; se convierten a copias físicas.
# Soluciones implementadas
Gestor Windows con run debian:12, run alpine:3.24, images, doctor. Offline rootfs sin symlinks. Blink adaptado en build no versionado: _COSMO_SOURCE, libc/dce.h, renombrar endswith/startswith, buffer 4096 log, mkfifoat fallback ENOSYS, metadata strings omitidas.
# Estado de tests
Blink en Linux ejecuta Debian12 /bin/bash y responde 12.15; Alpine /bin/sh responde 3.24.2. También funcionan rootfs flattened. Gestor compila MinGW y acepta comandos bajo Wine; CreateProcessW validado con motor simulado. Blink PE Windows pendiente.
# Estado de seguridad
No es sandbox ni Docker. Aplicaciones Linux pueden acceder al anfitrión; no ejecutar software malicioso. Digest de capa OCI verificado SHA256; tar revisado contra rutas absolutas y dispositivos. Licencia Blink ISC incluida.
# Pendientes
Prueba Blink en Windows real, import OCI adicionales, soporte avanzado de procesos, redes y filesystem.
# Próximos pasos
Extraer ZIP en Windows. Ejecutar portablebox.exe doctor; si pasa, portablebox.exe run debian:12 -- /bin/bash -c "cat /etc/debian_version". Compartir resultado.
