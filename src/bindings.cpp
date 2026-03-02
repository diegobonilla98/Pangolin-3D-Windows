#include "viewer.h"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace py = pybind11;

namespace {

constexpr int kArrayFlags = py::array::c_style | py::array::forcecast;

struct F32Nx3 {
  py::array_t<float, kArrayFlags> arr;
  py::ssize_t rows = 0;
};

F32Nx3 require_f32_nx3(const py::array& input, const char* name) {
  auto arr = py::array_t<float, kArrayFlags>(input);
  if (arr.ndim() != 2 || arr.shape(1) != 3) {
    std::ostringstream oss;
    oss << name << " must have shape (N, 3)";
    throw py::value_error(oss.str());
  }
  return F32Nx3{arr, arr.shape(0)};
}

std::vector<float> copy_f32(const py::array_t<float, kArrayFlags>& arr) {
  std::vector<float> out(static_cast<std::size_t>(arr.size()));
  {
    py::gil_scoped_release release;
    std::memcpy(out.data(), arr.data(), out.size() * sizeof(float));
  }
  return out;
}

std::vector<float> normalize_color_buffer(std::vector<float> values) {
  bool divide_255 = false;
  for (float v : values) {
    if (v > 1.0f) {
      divide_255 = true;
      break;
    }
  }
  if (divide_255) {
    for (float& v : values) {
      v /= 255.0f;
    }
  }
  for (float& v : values) {
    v = std::clamp(v, 0.0f, 1.0f);
  }
  return values;
}

std::optional<std::vector<float>> parse_rgb_for_rows(const py::object& rgb_obj,
                                                     py::ssize_t expected_rows,
                                                     const char* name) {
  if (rgb_obj.is_none()) {
    return std::nullopt;
  }
  py::array arr_any = py::array::ensure(rgb_obj);
  if (!arr_any) {
    throw py::value_error(std::string(name) + " must be a NumPy array");
  }
  if (arr_any.ndim() != 2 || arr_any.shape(1) != 3) {
    throw py::value_error(std::string(name) + " must have shape (N, 3)");
  }
  if (arr_any.shape(0) != expected_rows) {
    std::ostringstream oss;
    oss << name << " must have " << expected_rows << " rows";
    throw py::value_error(oss.str());
  }

  if (arr_any.dtype().is(py::dtype::of<std::uint8_t>())) {
    auto arr_u8 = py::array_t<std::uint8_t, kArrayFlags>(arr_any);
    std::vector<float> out(static_cast<std::size_t>(arr_u8.size()));
    {
      py::gil_scoped_release release;
      const std::uint8_t* src = arr_u8.data();
      for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<float>(src[i]) / 255.0f;
      }
    }
    return out;
  }

  auto arr_f32 = py::array_t<float, kArrayFlags>(arr_any);
  return normalize_color_buffer(copy_f32(arr_f32));
}

std::vector<std::uint32_t> parse_faces(const py::array& faces, std::size_t vertex_count) {
  auto arr = py::array_t<std::int64_t, kArrayFlags>(faces);
  if (arr.ndim() != 2 || arr.shape(1) != 3) {
    throw py::value_error("faces must have shape (M, 3)");
  }

  std::vector<std::uint32_t> out(static_cast<std::size_t>(arr.size()));
  const std::int64_t* src = arr.data();
  for (std::size_t i = 0; i < out.size(); ++i) {
    if (src[i] < 0 || static_cast<std::size_t>(src[i]) >= vertex_count) {
      throw py::value_error("faces contains out-of-range vertex index");
    }
    out[i] = static_cast<std::uint32_t>(src[i]);
  }
  return out;
}

std::array<float, 16> parse_pose_col_major(const py::array& pose) {
  auto arr = py::array_t<float, kArrayFlags>(pose);
  if (arr.ndim() != 2 || arr.shape(0) != 4 || arr.shape(1) != 4) {
    throw py::value_error("T_world_object must have shape (4, 4)");
  }

  const float* src = arr.data();
  std::array<float, 16> out{};
  // Input is standard NumPy row-major matrix; OpenGL expects column-major memory layout.
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      out[4 * c + r] = src[4 * r + c];
    }
  }
  return out;
}

void bind_add_lines(Viewer& self,
                    std::string name,
                    const py::array& a,
                    const py::array& b,
                    py::object rgb,
                    float width) {
  const auto a_data = require_f32_nx3(a, "a");
  const auto b_data = require_f32_nx3(b, "b");
  if (a_data.rows != b_data.rows) {
    throw py::value_error("a and b must have the same number of rows");
  }

  const py::ssize_t n = a_data.rows;
  std::vector<float> vertices(static_cast<std::size_t>(n) * 6);
  {
    py::gil_scoped_release release;
    const float* ap = a_data.arr.data();
    const float* bp = b_data.arr.data();
    for (py::ssize_t i = 0; i < n; ++i) {
      vertices[6 * static_cast<std::size_t>(i) + 0] = ap[3 * i + 0];
      vertices[6 * static_cast<std::size_t>(i) + 1] = ap[3 * i + 1];
      vertices[6 * static_cast<std::size_t>(i) + 2] = ap[3 * i + 2];
      vertices[6 * static_cast<std::size_t>(i) + 3] = bp[3 * i + 0];
      vertices[6 * static_cast<std::size_t>(i) + 4] = bp[3 * i + 1];
      vertices[6 * static_cast<std::size_t>(i) + 5] = bp[3 * i + 2];
    }
  }

  std::optional<std::vector<float>> rgb_vec;
  if (!rgb.is_none()) {
    py::array rgb_arr = py::array::ensure(rgb);
    if (!rgb_arr) {
      throw py::value_error("rgb must be a NumPy array or None");
    }
    if (rgb_arr.ndim() != 2 || rgb_arr.shape(1) != 3) {
      throw py::value_error("rgb must have shape (N, 3) or (2N, 3) for lines");
    }
    if (rgb_arr.shape(0) == n) {
      auto per_line = parse_rgb_for_rows(rgb, n, "rgb");
      std::vector<float> expanded(static_cast<std::size_t>(n) * 6);
      for (py::ssize_t i = 0; i < n; ++i) {
        const std::size_t src = static_cast<std::size_t>(3 * i);
        const std::size_t dst = static_cast<std::size_t>(6 * i);
        expanded[dst + 0] = (*per_line)[src + 0];
        expanded[dst + 1] = (*per_line)[src + 1];
        expanded[dst + 2] = (*per_line)[src + 2];
        expanded[dst + 3] = (*per_line)[src + 0];
        expanded[dst + 4] = (*per_line)[src + 1];
        expanded[dst + 5] = (*per_line)[src + 2];
      }
      rgb_vec = std::move(expanded);
    } else if (rgb_arr.shape(0) == 2 * n) {
      rgb_vec = parse_rgb_for_rows(rgb, 2 * n, "rgb");
    } else {
      throw py::value_error("rgb must have shape (N, 3) or (2N, 3) for lines");
    }
  }

  self.add_lines(std::move(name), std::move(vertices), std::move(rgb_vec), width);
}

}  // namespace

PYBIND11_MODULE(_pangolin_fast, m) {
  m.doc() = "Fast Pangolin realtime viewer with render thread.";

  py::class_<Viewer>(m, "Viewer")
      .def(py::init<int, int, std::string>(), py::arg("width") = 1280, py::arg("height") = 720,
           py::arg("title") = "pangolin_fast")
      .def("start",
           [](Viewer& self) {
             py::gil_scoped_release release;
             self.start();
           })
      .def("stop",
           [](Viewer& self) {
             py::gil_scoped_release release;
             self.stop();
           })
      .def("is_running", &Viewer::is_running)
      .def("set_camera", &Viewer::set_camera, py::arg("eye"), py::arg("lookat"), py::arg("up"))
      .def("set_projection", &Viewer::set_projection, py::arg("fov_deg"), py::arg("near"), py::arg("far"))
      .def(
          "add_points",
          [](Viewer& self,
             std::string name,
             const py::array& xyz,
             py::object rgb,
             float point_size,
             bool dynamic) {
            const auto xyz_data = require_f32_nx3(xyz, "xyz");
            auto xyz_vec = copy_f32(xyz_data.arr);
            auto rgb_vec = parse_rgb_for_rows(rgb, xyz_data.rows, "rgb");
            self.add_points(std::move(name), std::move(xyz_vec), std::move(rgb_vec), point_size, dynamic);
          },
          py::arg("name"), py::arg("xyz"), py::arg("rgb") = py::none(), py::arg("point_size") = 2.0f,
          py::arg("dynamic") = true)
      .def(
          "update_points",
          [](Viewer& self, std::string name, py::object xyz, py::object rgb) {
            std::optional<std::vector<float>> xyz_vec;
            std::optional<std::vector<float>> rgb_vec;
            std::optional<py::ssize_t> xyz_rows;

            if (!xyz.is_none()) {
              const auto xyz_data = require_f32_nx3(py::cast<py::array>(xyz), "xyz");
              xyz_rows = xyz_data.rows;
              xyz_vec = copy_f32(xyz_data.arr);
            }

            if (!rgb.is_none()) {
              if (xyz_rows.has_value()) {
                rgb_vec = parse_rgb_for_rows(rgb, *xyz_rows, "rgb");
              } else {
                py::array rgb_arr = py::array::ensure(rgb);
                if (!rgb_arr || rgb_arr.ndim() != 2 || rgb_arr.shape(1) != 3) {
                  throw py::value_error("rgb must have shape (N, 3)");
                }
                rgb_vec = parse_rgb_for_rows(rgb, rgb_arr.shape(0), "rgb");
              }
            }

            self.update_points(std::move(name), std::move(xyz_vec), std::move(rgb_vec));
          },
          py::arg("name"), py::arg("xyz") = py::none(), py::arg("rgb") = py::none())
      .def("set_point_size", &Viewer::set_point_size, py::arg("name"), py::arg("point_size"))
      .def("set_visible", &Viewer::set_visible, py::arg("name"), py::arg("visible"))
      .def("remove", &Viewer::remove, py::arg("name"))
      .def(
          "add_lines",
          [](Viewer& self, std::string name, const py::array& a, const py::array& b, py::object rgb, float width) {
            bind_add_lines(self, std::move(name), a, b, rgb, width);
          },
          py::arg("name"), py::arg("a"), py::arg("b"), py::arg("rgb") = py::none(), py::arg("width") = 2.0f)
      .def("add_axis", &Viewer::add_axis, py::arg("name") = "axis", py::arg("size") = 1.0f)
      .def("add_grid", &Viewer::add_grid, py::arg("name") = "grid", py::arg("half") = 10.0f, py::arg("step") = 1.0f)
      .def(
          "add_mesh",
          [](Viewer& self,
             std::string name,
             const py::array& vertices,
             const py::array& faces,
             py::object normals,
             py::object rgb) {
            const auto verts = require_f32_nx3(vertices, "vertices");
            auto vtx = copy_f32(verts.arr);
            auto idx = parse_faces(faces, static_cast<std::size_t>(verts.rows));

            std::optional<std::vector<float>> nrm;
            if (!normals.is_none()) {
              const auto n = require_f32_nx3(py::cast<py::array>(normals), "normals");
              if (n.rows != verts.rows) {
                throw py::value_error("normals must match vertices row count");
              }
              nrm = copy_f32(n.arr);
            }

            auto col = parse_rgb_for_rows(rgb, verts.rows, "rgb");
            self.add_mesh(std::move(name), std::move(vtx), std::move(idx), std::move(nrm), std::move(col));
          },
          py::arg("name"), py::arg("vertices"), py::arg("faces"), py::arg("normals") = py::none(),
          py::arg("rgb") = py::none())
      .def(
          "set_pose",
          [](Viewer& self, std::string name, const py::array& T_world_object) {
            self.set_pose(std::move(name), parse_pose_col_major(T_world_object));
          },
          py::arg("name"), py::arg("T_world_object"))
      .def("add_bool", &Viewer::add_bool, py::arg("name"), py::arg("default"))
      .def("add_float", &Viewer::add_float, py::arg("name"), py::arg("default"), py::arg("min"), py::arg("max"))
      .def("get_ui", [](const Viewer& self) {
        py::dict out;
        const auto values = self.get_ui();
        for (const auto& [name, value] : values) {
          out[py::str(name)] = value.is_bool ? py::bool_(value.value > 0.5) : py::float_(value.value);
        }
        return out;
      })
      .def("on_key", [](Viewer&, py::function) {
        throw py::value_error("on_key is optional and not implemented in this build.");
      });
}
