# 2026-10-13 — Bash y utilidades Debian confirmadas en Windows

## Hallazgo del usuario
En Windows PowerShell dentro de portablebox-final con Blink 1.0.5 y gestor 1.0.1, estos comandos se ejecutaron correctamente sin hipervisor:

- `.\portablebox.exe run debian:12 -- /bin/bash -c "/bin/cat /etc/debian_version"` -> `12.15`.
- `.\portablebox.exe run debian:12 -- /bin/bash -c "/bin/ls /"` -> carpetas Linux incluyendo `bin`, `etc`, `usr`, `var`.
- Sin la ruta `/bin/`, Bash indicaba `cat: command not found` y `ls: command not found`.

## Causa y corrección
Se hereda PATH de Windows (`C:\...;...`), no el Linux de Debian (`/usr/bin:/bin...`). Se corrigió `src/portablebox_app.cpp` para crear bloque Unicode de variables de entorno de Windows durante CreateProcessW, preservando variables del anfitrión salvo PATH/HOME/LANG/LC_ALL, y pasándolas a Blink con CREATE_UNICODE_ENVIRONMENT:
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
HOME=/root
LANG=C
LC_ALL=C

No se modifica el entorno del proceso padre ni registro de Windows; `doctor` sigue usando entorno host normal. Se conserva la ruta relativa del rootfs en `-C`.

## Pruebas
`tests/soft/mock_blink.cpp` imprime las variables recibidas y argumentos. `tests/soft/test_portablebox_launch.sh` compila el Blink simulado con MinGW y ejecuta el gestor bajo Wine, verificando PATH/Linux, HOME/root, LANG/LC_ALL y argumentos Debian/Alpine. Resultado: `OK: entorno Linux y rutas relativas sin modificar el entorno de Windows`.

## Entrega
`dist/releases/PortableBox-1.0.6-PATH-Windows-x64.zip` contiene unicamente `portablebox.exe` nuevo y LEEME, tamaño ~379 KiB. Reemplazar solo portablebox.exe; **conservar blink.exe 1.0.5 confirmado funcional en Windows** y `distros/`.

## Estado y límites
La corrección de entorno se probó con motor simulado, no se ha probado el código nuevo en Windows físico; el usuario debe validar `cat` y `ls` sin `/bin/`. La ejecución Linux mediante Blink no equivale a Docker Engine, VM o sandbox, y puede exponer archivos anfitrión.
