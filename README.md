# pangolin_fast

`pangolin_fast` is a lightweight Python wrapper around Pangolin for fast, simple realtime 3D visualization.

It is designed as a pragmatic alternative to heavier viewers when you want:

- High-FPS rendering with a dedicated C++ render thread
- Minimal Python API for points, lines, meshes, poses, and UI controls
- Low-overhead updates for dynamic scenes (e.g. SLAM, tracking, robotics, point-cloud streams)

---

## What This Repo Contains

- `pangolin_fast/` - Python package entrypoint
- `src/` - C++ viewer implementation + pybind11 bindings
- `demo.py` - runnable usage example
- `INSTALL_PANGOLIN_WINDOWS.md` - Pangolin/vcpkg/Windows setup guide

If you need help installing Pangolin itself on Windows, follow:

- [INSTALL_PANGOLIN_WINDOWS.md](INSTALL_PANGOLIN_WINDOWS.md)

---

## Quick Start (Library)

```powershell
# From this repo root:
python -m pip install -e .
python demo.py
```

Public PyPI package:

- https://pypi.org/project/pangolin-windows-easy/0.1.0/

```powershell
pip install pangolin-windows-easy==0.1.0
```

Note: the package still requires Pangolin and its Windows toolchain/runtime dependencies to be installed and configured first. Follow:

- [INSTALL_PANGOLIN_WINDOWS.md](INSTALL_PANGOLIN_WINDOWS.md)

If your Pangolin dependencies are in vcpkg and not on PATH, set:

```powershell
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"
```

`pangolin_fast` will try to load DLLs from:

- `VCPKG_ROOT\installed\x64-windows\bin`
- `VCPKG_INSTALLATION_ROOT\installed\x64-windows\bin`
- `%USERPROFILE%\vcpkg\installed\x64-windows\bin`

---

## Minimal Usage

```python
import time
import numpy as np
from pangolin_fast import Viewer

v = Viewer(width=1280, height=720, title="pangolin_fast")
v.add_bool("show_grid", True)
v.add_float("point_size", 2.0, 1.0, 8.0)
v.add_grid("grid", half=10.0, step=1.0)

xyz = np.random.uniform(-2, 2, size=(50000, 3)).astype(np.float32)
rgb = np.clip((xyz + 2.0) / 4.0, 0.0, 1.0)
v.add_points("cloud", xyz, rgb=rgb, dynamic=True)

v.start()
try:
    while v.is_running():
        ui = v.get_ui()
        v.set_visible("grid", bool(ui.get("show_grid", True)))
        v.set_point_size("cloud", float(ui.get("point_size", 2.0)))
        v.update_points("cloud", xyz=xyz)
        time.sleep(1.0 / 30.0)
finally:
    v.stop()
```

---

## API Overview

`Viewer` methods currently exposed:

- Lifecycle: `start`, `stop`, `is_running`
- Camera: `set_camera`, `set_projection`
- Geometry:
  - `add_points`, `update_points`, `set_point_size`
  - `add_lines`
  - `add_axis`, `add_grid`
  - `add_mesh`
  - `set_pose`
  - `set_visible`, `remove`
- UI vars: `add_bool`, `add_float`, `get_ui`

Input conventions:

- Coordinates are NumPy arrays of shape `(N, 3)` and float32-compatible.
- Colors accept float `[0,1]` or uint8 `[0,255]`, shape `(N, 3)`.
- Mesh faces are integer arrays of shape `(M, 3)`.
- Poses are 4x4 transform matrices.

---

## Current Scope

- Target platform: Windows (native, Pangolin + vcpkg workflow)
- Key focus: fast visualization and simple control loops from Python
- `on_key` is currently a stub in this build

---

## Installation Notes

This README is intentionally focused on using the Python library.

For Pangolin installation/toolchain setup, see:

- [INSTALL_PANGOLIN_WINDOWS.md](INSTALL_PANGOLIN_WINDOWS.md)
