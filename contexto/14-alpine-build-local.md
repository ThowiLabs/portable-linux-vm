# Fecha
2026-10-13

# Objetivo
Entregar un PortableBox Windows x64 descargable que incluya una distribución funcional, compilado **en Kaggle**. Conservar GitHub solo como repositorio de código; no crear releases ni subir binarios nuevos.

# Decisiones tomadas
- Entregar Alpine Linux 3.24.2 dentro del paquete, compatible con el comando ya implementado `portablebox.exe run alpine`.
- Conservar `portablebox.exe` del código actual y `blink.exe` 1.0.9 verificado; no inventar una corrección de red Windows antes de probarla.
- Usar `scripts/make_windows_rootfs.py` para convertir symlinks a archivos regulares que no requieren privilegios para extraerse en Windows.
- Empaquetar en ZIP estándar y 7z sólido, ambos generados y verificados **localmente en Kaggle**, no en GitHub Actions ni Releases.
- Excluir Debian de este paquete: su rootfs incluye 246 nombres con `:` prohibidos en Windows y una colisión de nombre que difiere solo por mayúsculas/minúsculas. No romper dpkg omitiendo/renombrando esos archivos.

# Arquitectura actual
`portablebox.exe` Windows x64 (MinGW C++20) inicia `blink.exe` 1.0.9 (Cosmopolitan APE), con `-C .` y cwd `distros/alpine-3.24`. Rootfs Alpine x86_64 incluido offline, con repositorios apk oficiales y `etc/resolv.conf` con DNS públicos. Alpine usa `apk`, no `apt`. No es una VM, ni un contenedor o sandbox.

# Librerías usadas
MinGW-w64, Blink 1.1 + parches propios ya publicados, Python 3 para preparar rootfs, ZIP estándar y 7z, rootfs oficial de Alpine. Se incorpora `scripts/empaquetar-alpine.sh` que reconstruye el paquete local; no ejecuta ninguna acción GitHub.

# Archivos importantes modificados
- `scripts/empaquetar-alpine.sh`: script local de empaquetado reproducible, descarga con SHA-256 fijo, construcción del EXE, validación Windows de nombres de archivos, creación ZIP.
- `contexto/14-alpine-build-local.md`: este contexto.
- Los ejecutables, directorios `distros/` y paquetes de distribución quedaron en `/kaggle/working/portable-linux-vm/dist` y no se incorporaron al repo GitHub.

# Problemas encontrados
- Paquete anterior 1.0.9 solo incluía motor/gestor/diagnóstico, sin distribución Linux.
- Alpine minirootfs no trae `etc/resolv.conf`; sin DNS las operaciones de red pueden fallar. Se configura DNS público para permitir pruebas (susceptible de bloqueo por red; se puede editar después de extraer).
- Al convertir symlinks de BusyBox en copias físicas, Alpine ocupa unos 250 MB extraídos y el ZIP ~150 MB.
- Debian no es portable sin capa VFS adicional o método de metadatos para sus nombres incompatibles con NTFS, por lo que se excluye sin modificaciones.
- Persisten limitaciones de pruebas de Blink APE en Windows físico; Wine no lo reproduce fielmente.

# Soluciones implementadas
- Descarga oficial: `alpine-minirootfs-3.24.2-x86_64.tar.gz` desde Alpine v3.24.
- SHA256 comprobado: `c5ca053cfe1d85c5b96dff8b9bc57045f7f184a30ffb6b65776409ca90388677`.
- 335 enlaces simbólicos convertidos en archivos físicos; cero symlinks remanentes y cero rutas invalidas/collisions bajo Alpine.
- `etc/resolv.conf` incluye 1.1.1.1 y 1.0.0.1, timeouts DNS de 2 segundos y 2 intentos.
- ZIP Kaggle: `dist/PortableBox-Alpine-3.24.2-Windows-x64.zip`, SHA256 `2ad7af6de549c25aeedf9d59b092675c9631acdf4b5e40de03e58235c2e5a00c`.
- 7z sólido Kaggle: `dist/PortableBox-Alpine-3.24.2-Windows-x64.7z`, SHA256 `8e55d2276c66a280c98c0395f157849048d62d64d43272185a96c3068b7d83f2` (~9.6 MB); conserva el mismo contenido, extracción requiere herramienta compatible 7z.
- Ambos archivos se sirven desde un servidor temporal HTTPS de solo descarga apuntando a Kaggle, con ruta aleatoria, sin subir binarios a GitHub. El URL es efímero.

# Estado de tests
- Binario `portablebox.exe` compilado en Kaggle como PE32+ x86-64: correcto.
- `portablebox.exe --help` bajo Wine64: correcto.
- `tests/soft/test_portablebox_launch.sh` bajo Wine con Blink simulado: OK para alias, cwd, PATH, HOME, LANG, LC_ALL.
- Alpine shell y `apk --version` en chroot Linux: correctos.
- Alpine `/bin/sh` con Blink Linux parcheado: imprime `3.24.2`.
- Alpine DNS con Blink Linux parcheado: `nslookup dl-cdn.alpinelinux.org` exitoso.
- Alpine `apk update` con Blink Linux parcheado: devuelve `OK: 28652 distinct packages available`, código 0.
- ZIP: `unzip -t` OK. 7z: `7z t` OK. Descarga HTTPS remota del 7z: SHA256 idéntico al local.
- **Windows físico**: el boot real y `apk update` con Blink.exe todavía requieren confirmación del usuario.

# Estado de seguridad
No requiere Windows admin ni desactivar seguridad. Rootfs sin symlinks de extracción privilegiada; checksums oficiales verificados. Blink NO es aislamiento: Linux guest puede acceder al host según capacidades de Blink/usuario Windows. Los DNS públicos pueden no ser accesibles en todas las redes.

# Pendientes
- Validación final en Windows: ejecutar `portablebox.exe images`, `portablebox.exe run alpine`, dentro del invitado `apk update`.
- Si falla red Windows, recopilar trazas específicas de `apk`/Blink.
- Desarrollar una representación Windows-compatible del metadata dpkg Debian sin eliminar nombres y confirmar APT antes de incluir Debian.

# Próximos pasos
1. Extraer ZIP con el Explorador de Windows o 7z usando descompresor compatible, preservando toda la estructura.
2. En PowerShell ejecutar `.\portablebox.exe images` y `.\portablebox.exe run alpine`.
3. En Alpine ejecutar `cat /etc/alpine-release`, `apk update`, `apk add curl`.
4. Mantener binarios y datasets fuera de GitHub: versionar solo los scripts y la documentación del proceso.
