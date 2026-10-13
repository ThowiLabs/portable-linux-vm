#!/usr/bin/env python3
"""Modifica Blink 1.1 commandv.c para interpretar ELF en Windows sin bit X_OK.

No modifica los archivos de rootfs ni activa características de Windows.
"""
import pathlib
import sys
if len(sys.argv)!=2:
    raise SystemExit("Uso: patch_blink_windows_xok.py PATH/commandv.c")
p=pathlib.Path(sys.argv[1])
s=p.read_text()
if 'Windows archives don\'t preserve Unix executable bits' in s:
    print("Parche ya aplicado")
    raise SystemExit(0)
original='  return !VfsAccess(AT_FDCWD, ps->path, X_OK, 0);'
if original not in s:
    raise SystemExit("Código Blink inesperado, parche no aplicado")
s=s.replace('#include <unistd.h>','#include <unistd.h>\n#include <sys/stat.h>\n#ifdef __COSMOPOLITAN__\n#include <libc/dce.h>\n#endif',1)
s=s.replace(original,'''  if (!VfsAccess(AT_FDCWD, ps->path, X_OK, 0)) return 1;
#ifdef __COSMOPOLITAN__
  // Windows archives don't preserve Unix executable bits. Verify that
  // a regular file exists and let the ELF loader validate the format.
  if (IsWindows()) {
    struct stat st;
    if (!VfsStat(AT_FDCWD, ps->path, &st, 0) && S_ISREG(st.st_mode))
      return 1;
  }
#endif
  return 0;''',1)
p.write_text(s)
print("Parche Blink/X_OK aplicado")
