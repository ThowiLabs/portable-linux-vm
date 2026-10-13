# Fecha
2026-10-13

# Objetivo
Resolver la incidencia del usuario en Windows: `apk add bash`, `apk add vim` y `apk add htop` terminan en `WARNING: updating and opening https://dl-cdn.alpinelinux.org/alpine/v3.24/{main,community}/x86_64/APKINDEX.tar.gz: Bad message`, con paquetes no encontrados.

# Decisiones tomadas
- No confundir el mensaje `bash: not found` (Alpine minirootfs no incluye Bash por defecto) con el error de `apk update`.
- Mantener repositorios HTTPS **por defecto**, no desactivar verificación TLS ni firmas.
- Probar HTTP **solo como diagnóstico temporal** para distinguir un posible problema de TLS/transferencia de datos en Blink Windows, con restauración posterior a HTTPS.
- Entregar una revisión de Alpine con `bash`, `vim`, `htop`, `curl` preinstalados mediante el gestor oficial APK en entorno Linux de build, para disponer de herramientas aun si la red Windows sigue defectuosa.
- No introducir puentes de red ni parches en Blink sin haber recogido una traza específica de Windows; Windows físico no está accesible desde Kaggle.
- Mantener los builds **en Kaggle**, publicar únicamente código y contexto en GitHub.

# Arquitectura actual
PortableBox C++20 Windows x64 lanza Blink APE con rootfs Alpine 3.24.2 en `distros/alpine-3.24/`. La distribución se prepara desde minirootfs Alpine oficial con SHA256 validado, se instalan paquetes verificados por las claves APK incluidas, luego se convierten enlaces simbólicos a copias físicas para NTFS. El script PowerShell `scripts/diagnostico-apk.ps1` ejecuta `blink.exe -s -C . /sbin/apk update` en dos modos (HTTPS y HTTP); conserva los bytes originales de `etc/apk/repositories` para restaurarlos en un bloque `finally`.

# Librerías usadas
Alpine apk-tools 3.0.8 del minirootfs, Blink 1.1.0 parcheado (Linux pruebas), PowerShell 5.1+/.NET de Windows, MinGW-w64 C++20, Bash y utilidades de empaquetado locales (tar/zip/7z). Sin dependencias nuevas para el producto Windows.

# Archivos importantes modificados
- `scripts/diagnostico-apk.ps1` (nuevo diagnóstico Win32).
- `scripts/empaquetar-alpine.sh` (instalación `bash vim htop curl` en chroot Linux de build y copia del diagnóstico).
- `contexto/15-apk-bad-message-windows.md` (este documento).
- `README-PORTABLEBOX-FINAL.md` (nuevas instrucciones del modo diagnóstico y del paquete local).

# Problemas encontrados
- Captura real Windows: la shell Alpine, `ls` y `ping` DNS funcionan al menos parcialmente. `ping google.com` selecciona IPv6 y advierte `setsockopt(ICMP6_FILTER)`, una limitación adicional; no prueba que el socket HTTPS funcione.
- Los índices HTTPS de `apk` fallan con `Bad message`. No se ha confirmado todavía el syscall o librería responsable; puede ser fallo TLS/IO o validación de index, no presuponer causa.
- El `apk` del Linux de pruebas obtiene índices por HTTPS **y también por HTTP**, utilizando Blink Linux parcheado. Esto NO reproduce necesariamente Windows.

# Soluciones implementadas
- Instalados paquetes oficiales firmados en rootfs previo a la conversión Windows: `bash 5.3.9-r1`, `vim 9.2.1091-r0`, `htop 3.5.3-r0`, `curl 8.22.0-r0`, con dependencias. `apk add --no-cache` informó `OK: 45.5 MiB in 34 packages`.
- `scripts/diagnostico-apk.ps1`: compara HTTPS nativo Windows (HEAD) y `apk update` guest HTTPS/HTTP con `blink -s`, registro de stdout/stderr, límites por proceso, copia de seguridad y restauración exacta en `finally`. Se debe ejecutar con PortableBox/Alpine cerrado. El modo HTTP es solo prueba, no cambio persistente.
- Para probar inmediatamente en la versión anterior, dentro de Alpine: `cp /etc/apk/repositories /etc/apk/repositories.bak; sed -i 's#https://#http://#g' /etc/apk/repositories; apk update`. Restaurar después con `cp /etc/apk/repositories.bak /etc/apk/repositories`.

# Estado de tests
- Rootfs Alpine: `chroot ... apk add --no-cache bash vim htop curl`: OK.
- Blink Linux ejecutando `bash --version`, `vim --version`, `htop --version`, `curl --version`: OK.
- `apk update` con URLs HTTP bajo Blink Linux: OK, 28652 paquetes reportados; HTTPS previamente también OK.
- Parser tree-sitter PowerShell de `scripts/diagnostico-apk.ps1`: sin errores.
- Bash `bash -n scripts/empaquetar-alpine.sh`: correcto.
- `apk update` dentro de Windows físico y `blink.exe` sobre red HTTPS: **no corregido ni validado**.

# Estado de seguridad
Mantener HTTPS de forma predeterminada y validación de firmas. HTTP optativo reduce la autenticación/confidencialidad del transporte y no se debe activar sin informar al usuario. El diagnóstico restaura las fuentes originales después de pruebas. Los registros pueden contener rutas privadas del host. No desactivar Windows Defender ni pedir admin. Blink no es sandbox.

# Pendientes
- Recibir resultados de `apk update` tras cambio temporal HTTPS→HTTP en Windows.
- Recibir las últimas syscalls de `apk-https.stderr.log` y `apk-http.stderr.log` (script diagnóstico).
- Si HTTP funciona pero HTTPS falla, investigar en el backend sockets, TLS o lectura/transferencia mediante Win32, sin abandonar verificaciones de TLS; solo después implementar puente TLS nativo si está justificado.
- Si HTTP también falla, identificar error de lectura/escritura o parsing APKINDEX con traza precisa antes de parchear Blink.

# Próximos pasos
1. Probar `apk update` por HTTP temporalmente como se indica en la respuesta.
2. Descargar paquete local actualizado de Kaggle con bash/vim/htop/curl y ejecutar diagnóstico PowerShell sin Alpine en ejecución.
3. Comparar resultados y corregir la syscall o ruta concreta de red Windows; construir un nuevo blink.exe solo al contar con evidencia reproducible.
