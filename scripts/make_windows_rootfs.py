#!/usr/bin/env python3
"""Genera rootfs sin enlaces simbolicos para copiado en Windows sin permisos elevados.

NO es sandbox: solo procesa imagenes OCI verificadas, sigue enlaces virtuales
dentro de la raiz y evita resolver enlaces absolutos contra el Linux anfitrion.
"""
import os
import pathlib
import posixpath
import shutil
import stat
import sys

if len(sys.argv) != 3:
    raise SystemExit("Uso: make_windows_rootfs.py SRC_ROOTFS DEST_ROOTFS")
root = pathlib.Path(sys.argv[1]).absolute()
dest = pathlib.Path(sys.argv[2]).absolute()
if not (root / "etc" / "os-release").is_file():
    raise SystemExit("Rootfs invalido")
if dest.exists():
    raise SystemExit("La carpeta destino ya existe")
dest.mkdir(parents=True)
links_resolved = 0
omitted = 0

def safe_target(relative, followed=None):
    if followed is None:
        followed = set()
    rel = pathlib.PurePosixPath(relative)
    path = pathlib.PurePosixPath("/")
    components = list(rel.parts)
    while components:
        name = components.pop(0)
        if name in ("", ".", "/"):
            continue
        if name == "..":
            path = path.parent
            continue
        path = path / name
        file = root.joinpath(*path.parts[1:])
        if file.is_symlink():
            if str(path) in followed:
                raise ValueError(f"Circular: {path}")
            followed.add(str(path))
            link = os.readlink(file)
            start = link if link.startswith("/") else str(path.parent / link)
            resolved = posixpath.normpath(start)
            path = pathlib.PurePosixPath("/")
            components = list(pathlib.PurePosixPath(resolved).parts[1:]) + components
    return root.joinpath(*path.parts[1:])

def mirror(src, target, seen_dirs=frozenset()):
    global links_resolved, omitted
    try:
        if src.is_symlink():
            links_resolved += 1
            rel = src.relative_to(root)
            actual = safe_target(rel)
            if not actual.exists():
                omitted += 1
                return
            return mirror(actual, target, seen_dirs)
        st = src.lstat()
        if stat.S_ISDIR(st.st_mode):
            key = str(src)
            if key in seen_dirs:
                omitted += 1
                return
            target.mkdir(exist_ok=True)
            for item in src.iterdir():
                mirror(item, target / item.name, seen_dirs | {key})
        elif stat.S_ISREG(st.st_mode):
            shutil.copy2(src, target, follow_symlinks=False)
        else:
            omitted += 1
    except (ValueError, OSError, RuntimeError) as exc:
        print(f"ADVERTENCIA omitido: {src.relative_to(root)}: {exc}", file=sys.stderr)
        omitted += 1

for item in root.iterdir():
    mirror(item, dest / item.name)
print(f"Rootfs Windows creado: {dest} | enlaces convertidos: {links_resolved} | omitidos: {omitted}")
