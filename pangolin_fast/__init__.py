from __future__ import annotations

import os
from pathlib import Path

_DLL_DIR_HANDLES = []


def _add_dll_dir(path: Path) -> None:
    if not hasattr(os, "add_dll_directory"):
        return
    if not path.exists():
        return
    _DLL_DIR_HANDLES.append(os.add_dll_directory(str(path)))


_PKG_DIR = Path(__file__).resolve().parent
_add_dll_dir(_PKG_DIR)

_vcpkg_roots: list[Path] = []
for _env_name in ("VCPKG_ROOT", "VCPKG_INSTALLATION_ROOT"):
    _env_value = os.environ.get(_env_name)
    if _env_value:
        _vcpkg_roots.append(Path(_env_value))

# Common default location for manual vcpkg installs on Windows.
_vcpkg_roots.append(Path.home() / "vcpkg")

for _root in _vcpkg_roots:
    _add_dll_dir(_root / "installed" / "x64-windows" / "bin")

from ._pangolin_fast import Viewer

__all__ = ["Viewer"]
