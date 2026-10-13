# Parche Blink para Windows: ELF sin bit ejecutable POSIX

Contexto: PortableBox 1.0.1 consigue iniciar Blink pero al cargar Debian 12
desde su rootfs en una carpeta de Windows informa:
  command not found: /bin/bash

Causa sospechada: Commandv utiliza VfsAccess(X_OK) para encontrar ejecutables;
los modos Unix no se conservan al extraer en Windows y faccessat puede negar X_OK.

Cambio: en el backend Cosmopolitan sobre Windows, si X_OK falla, consultar
VfsStat y aceptar archivos regulares existentes. El cargador ELF sigue verificando
el formato de archivo; esto no constituye aislamiento de seguridad.

Aplicación (sobre checkout de Blink 1.1.0 ya preparado con Cosmopolitan):

    python3 scripts/patch_blink_windows_xok.py build/blink-src/blink/commandv.c
    PATH=.../cosmocc/bin:$PATH make -C build/blink-src MODE=rel o/rel/blink/blink

Importante: es una hipótesis del fallo, todavía no probada en Windows físico.
No se modifica la estructura del rootfs ni se convierte a VHDX.
