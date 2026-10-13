# Fecha
2026-10-13

# Objetivo
Investigar por qué `apt update` en PortableBox para Windows deja de producir salida tras las correcciones de identidad virtual de Blink 1.0.9.

# Decisiones tomadas
- No atribuir sin pruebas el bloqueo a DNS, sockets o fork.
- Mantener intactos `blink.exe`, `portablebox.exe`, los rootfs y el resto de motores; no publicar un binario nuevo sin prueba Windows.
- Añadir un diagnóstico PowerShell **auxiliar y optativo** con trazas nativas de Blink (`-s`) y límite de espera.
- Permitir sincronizar DNS de Windows solo con `-SyncDns`, respaldando `distros/debian-12/etc/resolv.conf` en `resolv.conf.portablebox.bak`.

# Arquitectura actual
PortableBox 1.0.7 inicia Blink 1.0.9 dentro del rootfs; Debian 12 y Alpine se ejecutan como procesos Linux emulados en Windows, sin VM ni sandbox. La herramienta nueva invoca directamente `blink.exe -s -C .` desde el directorio del rootfs para aislar pruebas de la implementación de DNS, sockets y APT sin cambiar el lanzador.

# Librerías usadas
PowerShell 5.1+, cmdlets de Windows, clases de .NET `System.Net`, Blink ya incluido, y parser Tree-sitter PowerShell para validar sintaxis en Linux. Sin dependencias adicionales en el producto.

# Archivos importantes modificados
- `scripts/diagnostico-apt.ps1` (nuevo).
- `README-PORTABLEBOX-FINAL.md` (instrucciones diagnósticas).
- `contexto/13-apt-bloqueo-diagnostico.md` (este documento).

# Problemas encontrados
- El usuario reporta `apt update` sin salida, pero todavía no hay trazas del punto exacto donde se bloquea en Windows.
- El rootfs Debian oficial de la capa OCI verificada (SHA256 8cb71386cfcfc55b27b559f73c6d5c0cdcb93b83be8281314116e271cc42cea6) incluye `/etc/resolv.conf` con `1.1.1.1` y `1.0.0.1`, lo cual no necesariamente coincide con DNS del equipo Windows. **No está demostrado** que sea la causa.
- Blink 1.1.0 original compilado en Linux en el nuevo Kaggle experimenta un segfault al invocar `apt-get --version` dentro de ese Debian sin aplicar los parches del proyecto. No extrapolarlo a la versión parcheada Windows: el registro anterior confirmó esa misma consulta como funcional allí.
- No existe actualmente entorno Windows real conectado para reproducir el atasco de sockets/apt; Wine tampoco validó previamente Blink APE.

# Soluciones implementadas
El diagnóstico comprueba DNS IPv4 y TCP puerto 80 desde Windows; luego ejecuta en el invitado y registra trazas de `apt-get --version`, `getent ahostsv4 deb.debian.org` y `apt-get update` con reintentos desactivados y timeouts de transporte. Por etapa guarda stdout, stderr y las últimas líneas; si vence el plazo, termina el árbol de procesos de esa prueba para no dejar procesos colgados.
El flag `-SyncDns` escribe hasta tres servidores IPv4 configurados por Windows en el rootfs Debian, conservando el archivo anterior si existía. **No modifica los DNS del anfitrión**. Es optativo.

# Estado de tests
- Se leyó íntegra `contexto/` (13 archivos existentes al inicio).
- Se descargó y verificó SHA256 de la capa Debian oficial y se inspeccionaron sus archivos de DNS y APT.
- Se compiló Blink 1.1.0 Linux upstream (OK) y se reprodujo segfault en ese baseline sin parches (resultado NO equivalente a Windows).
- Se recuperaron los parches 001 a 007 y todos aplicaron sobre Blink 1.1.0. La recompilación nativa Linux pasó tras adaptar el nombre de función `blinkendswith` (introducido en adaptación Cosmopolitan) a `endswith` original en el checkout local de pruebas. `apt-get --version` sigue haciendo segfault en Blink Linux parcheado, antes de generar trazas; la causa aún no está diagnosticada y no equivale a demostrar que el build Cosmopolitan de Windows tenga ese fallo.
- Parser Tree-sitter PowerShell: script nuevo sin errores de sintaxis.
- Comprobaciones estáticas de comando `-s -C .`, límites y copia DNS: OK.
- **Pendiente**: ejecución PowerShell y prueba APT en un Windows real. No se puede afirmar que APT ya funciona.

# Estado de seguridad
Las pruebas externas usan límites temporales y no ejecutan comandos como administrador. `apt-get update` modifica únicamente índices del sistema Debian invitado; por timeout pueden quedar descargas parciales. El modo `-SyncDns` altera solo `resolv.conf` dentro del rootfs con respaldo y no modifica DNS, registro ni privilegios de Windows. Los logs podrían contener rutas/datos del anfitrión: revisarlos antes de compartir. Blink no es sandbox.

# Pendientes
Obtener las últimas líneas de `getent.stderr.log` y `apt-update.stderr.log`, verificar si el último syscall está en DNS, `socket`, `connect`, `recv`, `poll`, `clone` o `wait`; implementar un parche acotado **solo después** de identificar la causa; reconstruir y probar Blink en Windows físico.

# Próximos pasos
1. Ejecutar `./scripts/diagnostico-apt.ps1 -PortableBoxPath 'RUTA_DE_PORTABLEBOX'` desde PowerShell en Windows (si se descargó el script desde GitHub); observar los tres resultados.
2. Si Windows resuelve DNS pero `getent` no, repetir opcionalmente con `-SyncDns` y comparar.
3. Si `getent` funciona pero `apt-get update` no, usar la traza de APT para investigar el backend Blink específico sin tocar DNS.
