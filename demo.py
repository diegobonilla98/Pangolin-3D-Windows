import time

import numpy as np

from pangolin_fast import Viewer


def main() -> None:
    v = Viewer(width=1280, height=720, title="pangolin_fast demo")
    v.add_bool("show_grid", True)
    v.add_float("point_size", 2.0, 1.0, 8.0)
    v.set_camera([3.5, -4.5, 2.8], [0.0, 0.0, 0.0], [0.0, 0.0, 1.0])
    v.set_projection(60.0, 0.05, 1000.0)

    v.add_axis("axis", size=1.0)
    v.add_grid("grid", half=10.0, step=1.0)

    a = np.array(
        [
            [0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0],
        ],
        dtype=np.float32,
    )
    b = np.array(
        [
            [1.5, 0.0, 0.0],
            [0.0, 1.5, 0.0],
            [0.0, 0.0, 1.5],
        ],
        dtype=np.float32,
    )
    colors = np.array(
        [
            [255, 64, 64],
            [64, 255, 64],
            [64, 64, 255],
        ],
        dtype=np.uint8,
    )
    v.add_lines("basis_lines", a, b, rgb=colors, width=3.0)

    n = 120_000
    xyz = np.random.uniform(-2.5, 2.5, size=(n, 3)).astype(np.float32)
    rgb = np.clip((xyz + 2.5) / 5.0, 0.0, 1.0)
    v.add_points("cloud", xyz, rgb=rgb, point_size=2.0, dynamic=True)

    v.start()
    deadline = time.time() + 5.0
    while not v.is_running() and time.time() < deadline:
        time.sleep(0.01)
    if not v.is_running():
        raise RuntimeError("Viewer render thread did not start within 5 seconds.")

    t0 = time.time()
    indices = np.arange(n, dtype=np.float32)
    last_point_size = 2.0
    last_show_grid = True
    try:
        while v.is_running():
            t = time.time() - t0
            xyz[:, 2] = 0.35 * np.sin(1.5 * t + 0.01 * indices)

            ui = v.get_ui()
            psize = float(ui.get("point_size", last_point_size))
            show_grid = bool(ui.get("show_grid", last_show_grid))
            v.update_points("cloud", xyz=xyz)

            if abs(psize - last_point_size) > 1e-3:
                v.set_point_size("cloud", psize)
                last_point_size = psize

            if show_grid != last_show_grid:
                v.set_visible("grid", show_grid)
                last_show_grid = show_grid

            time.sleep(1.0 / 30.0)
    finally:
        v.stop()


if __name__ == "__main__":
    main()
