#include "viewer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

constexpr float kPi = 3.14159265358979323846f;

std::vector<float> make_axis_vertices(float size) {
  return {
      0.0f, 0.0f, 0.0f, size, 0.0f, 0.0f,  // x
      0.0f, 0.0f, 0.0f, 0.0f, size, 0.0f,  // y
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, size,  // z
  };
}

std::vector<float> make_axis_colors() {
  return {
      1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // x
      0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,  // y
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // z
  };
}

std::vector<float> make_grid_vertices(float half, float step) {
  std::vector<float> vertices;
  if (step <= 0.0f || half <= 0.0f) {
    return vertices;
  }

  const int line_count = static_cast<int>(std::floor((half * 2.0f) / step)) + 1;
  vertices.reserve(static_cast<std::size_t>(line_count) * 12);

  const float start = -half;
  for (int i = 0; i < line_count; ++i) {
    const float p = start + step * static_cast<float>(i);
    // Vertical line.
    vertices.push_back(p);
    vertices.push_back(-half);
    vertices.push_back(0.0f);
    vertices.push_back(p);
    vertices.push_back(half);
    vertices.push_back(0.0f);
    // Horizontal line.
    vertices.push_back(-half);
    vertices.push_back(p);
    vertices.push_back(0.0f);
    vertices.push_back(half);
    vertices.push_back(p);
    vertices.push_back(0.0f);
  }
  return vertices;
}

}  // namespace

Viewer::Viewer(int width, int height, std::string title)
    : width_(width), height_(height), title_(std::move(title)) {
  camera_dirty_ = true;
}

Viewer::~Viewer() { stop(); }

void Viewer::start() {
  if (render_thread_.joinable()) {
    return;
  }
  stop_requested_.store(false, std::memory_order_release);
  render_thread_ = std::thread(&Viewer::render_loop, this);
}

void Viewer::stop() {
  stop_requested_.store(true, std::memory_order_release);
  command_cv_.notify_all();
  if (render_thread_.joinable()) {
    render_thread_.join();
  }
}

bool Viewer::is_running() const noexcept { return running_.load(std::memory_order_acquire); }

void Viewer::set_camera(const std::array<float, 3>& eye,
                        const std::array<float, 3>& lookat,
                        const std::array<float, 3>& up) {
  enqueue([this, eye, lookat, up]() {
    camera_.eye = eye;
    camera_.lookat = lookat;
    camera_.up = up;
    camera_dirty_ = true;
  });
}

void Viewer::set_projection(float fov_deg, float near_clip, float far_clip) {
  enqueue([this, fov_deg, near_clip, far_clip]() {
    camera_.fov_deg = std::clamp(fov_deg, 10.0f, 140.0f);
    camera_.near_clip = std::max(near_clip, 0.0001f);
    camera_.far_clip = std::max(far_clip, camera_.near_clip + 0.01f);
    camera_dirty_ = true;
  });
}

void Viewer::add_points(std::string name,
                        std::vector<float> xyz,
                        std::optional<std::vector<float>> rgb,
                        float point_size,
                        bool dynamic) {
  enqueue([this, name = std::move(name), xyz = std::move(xyz), rgb = std::move(rgb), point_size,
           dynamic]() mutable {
    SceneObject obj;
    obj.type = ObjectType::Points;
    obj.dynamic = dynamic;
    obj.point_size = std::max(point_size, 1.0f);
    obj.pose = identity_matrix();
    obj.vertices = std::move(xyz);
    if (rgb.has_value() && !rgb->empty()) {
      obj.colors = std::move(*rgb);
    } else {
      fill_color(obj.colors, obj.vertices.size() / 3, 1.0f, 1.0f, 1.0f);
    }

    auto it = objects_.find(name);
    if (it != objects_.end()) {
      delete_gl_buffers(it->second);
    }
    objects_[name] = std::move(obj);
  });
}

void Viewer::update_points(std::string name,
                           std::optional<std::vector<float>> xyz,
                           std::optional<std::vector<float>> rgb) {
  enqueue([this, name = std::move(name), xyz = std::move(xyz), rgb = std::move(rgb)]() mutable {
    auto it = objects_.find(name);
    if (it == objects_.end() || it->second.type != ObjectType::Points) {
      return;
    }
    SceneObject& obj = it->second;

    if (xyz.has_value()) {
      obj.vertices = std::move(*xyz);
      obj.vertices_dirty = true;
      if (!rgb.has_value() && obj.colors.size() != obj.vertices.size()) {
        fill_color(obj.colors, obj.vertices.size() / 3, 1.0f, 1.0f, 1.0f);
        obj.colors_dirty = true;
      }
    }
    if (rgb.has_value()) {
      if (!obj.vertices.empty() && rgb->size() == obj.vertices.size()) {
        obj.colors = std::move(*rgb);
        obj.colors_dirty = true;
      }
    }
  });
}

void Viewer::set_point_size(std::string name, float point_size) {
  enqueue([this, name = std::move(name), point_size]() {
    auto it = objects_.find(name);
    if (it == objects_.end() || it->second.type != ObjectType::Points) {
      return;
    }
    it->second.point_size = std::max(point_size, 1.0f);
  });
}

void Viewer::set_visible(std::string name, bool visible) {
  enqueue([this, name = std::move(name), visible]() {
    auto it = objects_.find(name);
    if (it == objects_.end()) {
      return;
    }
    it->second.visible = visible;
  });
}

void Viewer::remove(std::string name) {
  enqueue([this, name = std::move(name)]() {
    auto it = objects_.find(name);
    if (it == objects_.end()) {
      return;
    }
    delete_gl_buffers(it->second);
    objects_.erase(it);
  });
}

void Viewer::add_lines(std::string name,
                       std::vector<float> vertices,
                       std::optional<std::vector<float>> rgb,
                       float width) {
  enqueue([this, name = std::move(name), vertices = std::move(vertices), rgb = std::move(rgb),
           width]() mutable {
    SceneObject obj;
    obj.type = ObjectType::Lines;
    obj.dynamic = true;
    obj.line_width = std::max(width, 1.0f);
    obj.pose = identity_matrix();
    obj.vertices = std::move(vertices);
    if (rgb.has_value() && !rgb->empty()) {
      obj.colors = std::move(*rgb);
    } else {
      fill_color(obj.colors, obj.vertices.size() / 3, 1.0f, 1.0f, 1.0f);
    }

    auto it = objects_.find(name);
    if (it != objects_.end()) {
      delete_gl_buffers(it->second);
    }
    objects_[name] = std::move(obj);
  });
}

void Viewer::add_axis(std::string name, float size) {
  add_lines(std::move(name), make_axis_vertices(std::max(size, 0.01f)), make_axis_colors(), 3.0f);
}

void Viewer::add_grid(std::string name, float half, float step) {
  std::vector<float> vertices = make_grid_vertices(std::max(half, 0.1f), std::max(step, 0.01f));
  std::vector<float> colors;
  fill_color(colors, vertices.size() / 3, 0.8f, 0.8f, 0.8f);
  add_lines(std::move(name), std::move(vertices), std::move(colors), 1.0f);
}

void Viewer::add_mesh(std::string name,
                      std::vector<float> vertices,
                      std::vector<std::uint32_t> faces,
                      std::optional<std::vector<float>> normals,
                      std::optional<std::vector<float>> rgb) {
  enqueue([this, name = std::move(name), vertices = std::move(vertices), faces = std::move(faces),
           normals = std::move(normals), rgb = std::move(rgb)]() mutable {
    SceneObject obj;
    obj.type = ObjectType::Mesh;
    obj.dynamic = true;
    obj.pose = identity_matrix();
    obj.vertices = std::move(vertices);
    obj.indices = std::move(faces);
    if (normals.has_value() && normals->size() == obj.vertices.size()) {
      obj.normals = std::move(*normals);
    }
    if (rgb.has_value() && rgb->size() == obj.vertices.size()) {
      obj.colors = std::move(*rgb);
    } else {
      fill_color(obj.colors, obj.vertices.size() / 3, 0.8f, 0.8f, 0.8f);
    }

    auto it = objects_.find(name);
    if (it != objects_.end()) {
      delete_gl_buffers(it->second);
    }
    objects_[name] = std::move(obj);
  });
}

void Viewer::set_pose(std::string name, const std::array<float, 16>& T_world_object) {
  enqueue([this, name = std::move(name), T_world_object]() {
    auto it = objects_.find(name);
    if (it == objects_.end()) {
      return;
    }
    it->second.pose = T_world_object;
  });
}

void Viewer::add_bool(std::string name, bool default_value) {
  enqueue([this, name = std::move(name), default_value]() {
    auto it = ui_widgets_.find(name);
    if (it != ui_widgets_.end() && it->second.type == UiWidget::Type::Bool && it->second.bool_var) {
      *(it->second.bool_var) = default_value;
      return;
    }

    UiWidget widget;
    widget.type = UiWidget::Type::Bool;
    widget.bool_var = std::make_unique<pangolin::Var<bool>>("ui." + name, default_value, true);
    ui_widgets_[name] = std::move(widget);
  });
}

void Viewer::add_float(std::string name, float default_value, float min_value, float max_value) {
  enqueue([this, name = std::move(name), default_value, min_value, max_value]() {
    const double min_v = static_cast<double>(std::min(min_value, max_value));
    const double max_v = static_cast<double>(std::max(min_value, max_value));
    const double init_v = std::clamp(static_cast<double>(default_value), min_v, max_v);

    auto it = ui_widgets_.find(name);
    if (it != ui_widgets_.end() && it->second.type == UiWidget::Type::Float && it->second.float_var) {
      *(it->second.float_var) = init_v;
      return;
    }

    UiWidget widget;
    widget.type = UiWidget::Type::Float;
    widget.float_var = std::make_unique<pangolin::Var<double>>("ui." + name, init_v, min_v, max_v);
    ui_widgets_[name] = std::move(widget);
  });
}

std::unordered_map<std::string, Viewer::UiValue> Viewer::get_ui() const {
  std::lock_guard<std::mutex> lock(ui_mutex_);
  return ui_snapshot_;
}

std::array<float, 16> Viewer::identity_matrix() {
  return {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
          0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
}

void Viewer::fill_color(std::vector<float>& colors, std::size_t count, float r, float g, float b) {
  colors.resize(count * 3);
  for (std::size_t i = 0; i < count; ++i) {
    colors[3 * i + 0] = r;
    colors[3 * i + 1] = g;
    colors[3 * i + 2] = b;
  }
}

void Viewer::enqueue(Command cmd) {
  {
    std::lock_guard<std::mutex> lock(command_mutex_);
    command_queue_.push(std::move(cmd));
  }
  command_cv_.notify_one();
}

void Viewer::drain_commands() {
  std::queue<Command> pending;
  {
    std::lock_guard<std::mutex> lock(command_mutex_);
    std::swap(pending, command_queue_);
  }

  while (!pending.empty()) {
    pending.front()();
    pending.pop();
  }
}

void Viewer::upload_if_dirty(SceneObject& obj) {
  if (obj.vertices_dirty) {
    if (obj.vertices.empty()) {
      if (obj.vbo != 0) {
        glDeleteBuffers(1, &obj.vbo);
        obj.vbo = 0;
      }
    } else {
      if (obj.vbo == 0) {
        glGenBuffers(1, &obj.vbo);
      }
      glBindBuffer(GL_ARRAY_BUFFER, obj.vbo);
      glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(obj.vertices.size() * sizeof(float)),
                   obj.vertices.data(), obj.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    }
    obj.vertices_dirty = false;
  }

  if (obj.colors_dirty) {
    if (obj.colors.empty()) {
      if (obj.cbo != 0) {
        glDeleteBuffers(1, &obj.cbo);
        obj.cbo = 0;
      }
    } else {
      if (obj.cbo == 0) {
        glGenBuffers(1, &obj.cbo);
      }
      glBindBuffer(GL_ARRAY_BUFFER, obj.cbo);
      glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(obj.colors.size() * sizeof(float)),
                   obj.colors.data(), obj.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    }
    obj.colors_dirty = false;
  }

  if (obj.normals_dirty) {
    if (obj.normals.empty()) {
      if (obj.nbo != 0) {
        glDeleteBuffers(1, &obj.nbo);
        obj.nbo = 0;
      }
    } else {
      if (obj.nbo == 0) {
        glGenBuffers(1, &obj.nbo);
      }
      glBindBuffer(GL_ARRAY_BUFFER, obj.nbo);
      glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(obj.normals.size() * sizeof(float)),
                   obj.normals.data(), obj.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    }
    obj.normals_dirty = false;
  }

  if (obj.indices_dirty) {
    if (obj.indices.empty()) {
      if (obj.ebo != 0) {
        glDeleteBuffers(1, &obj.ebo);
        obj.ebo = 0;
      }
    } else {
      if (obj.ebo == 0) {
        glGenBuffers(1, &obj.ebo);
      }
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj.ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(obj.indices.size() * sizeof(std::uint32_t)),
                   obj.indices.data(), obj.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    }
    obj.indices_dirty = false;
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Viewer::draw_object(SceneObject& obj) {
  upload_if_dirty(obj);
  if (!obj.visible || obj.vertices.empty() || obj.vbo == 0) {
    return;
  }

  glPushMatrix();
  glMultMatrixf(obj.pose.data());

  if (obj.type == ObjectType::Points) {
    glPointSize(obj.point_size);
    glEnableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, obj.vbo);
    glVertexPointer(3, GL_FLOAT, 0, nullptr);

    if (obj.cbo != 0) {
      glEnableClientState(GL_COLOR_ARRAY);
      glBindBuffer(GL_ARRAY_BUFFER, obj.cbo);
      glColorPointer(3, GL_FLOAT, 0, nullptr);
    } else {
      glColor3f(1.0f, 1.0f, 1.0f);
    }

    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(obj.vertices.size() / 3));

    if (obj.cbo != 0) {
      glDisableClientState(GL_COLOR_ARRAY);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
  } else if (obj.type == ObjectType::Lines) {
    glLineWidth(obj.line_width);
    glEnableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, obj.vbo);
    glVertexPointer(3, GL_FLOAT, 0, nullptr);

    if (obj.cbo != 0) {
      glEnableClientState(GL_COLOR_ARRAY);
      glBindBuffer(GL_ARRAY_BUFFER, obj.cbo);
      glColorPointer(3, GL_FLOAT, 0, nullptr);
    } else {
      glColor3f(1.0f, 1.0f, 1.0f);
    }

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(obj.vertices.size() / 3));

    if (obj.cbo != 0) {
      glDisableClientState(GL_COLOR_ARRAY);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
  } else {
    glEnableClientState(GL_VERTEX_ARRAY);
    glBindBuffer(GL_ARRAY_BUFFER, obj.vbo);
    glVertexPointer(3, GL_FLOAT, 0, nullptr);

    if (obj.cbo != 0) {
      glEnableClientState(GL_COLOR_ARRAY);
      glBindBuffer(GL_ARRAY_BUFFER, obj.cbo);
      glColorPointer(3, GL_FLOAT, 0, nullptr);
    } else {
      glColor3f(0.8f, 0.8f, 0.8f);
    }

    if (obj.ebo != 0 && !obj.indices.empty()) {
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj.ebo);
      glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(obj.indices.size()), GL_UNSIGNED_INT, nullptr);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    if (obj.cbo != 0) {
      glDisableClientState(GL_COLOR_ARRAY);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glPopMatrix();
}

void Viewer::delete_gl_buffers(SceneObject& obj) {
  if (obj.vbo != 0) {
    glDeleteBuffers(1, &obj.vbo);
    obj.vbo = 0;
  }
  if (obj.cbo != 0) {
    glDeleteBuffers(1, &obj.cbo);
    obj.cbo = 0;
  }
  if (obj.nbo != 0) {
    glDeleteBuffers(1, &obj.nbo);
    obj.nbo = 0;
  }
  if (obj.ebo != 0) {
    glDeleteBuffers(1, &obj.ebo);
    obj.ebo = 0;
  }
}

void Viewer::update_camera_matrices(pangolin::OpenGlRenderState& cam_state) {
  if (!camera_dirty_) {
    return;
  }

  const float fov_rad = std::clamp(camera_.fov_deg, 10.0f, 140.0f) * (kPi / 180.0f);
  const double fy = (static_cast<double>(height_) * 0.5) / std::tan(static_cast<double>(fov_rad) * 0.5);
  const double fx = fy;
  const double cx = static_cast<double>(width_) * 0.5;
  const double cy = static_cast<double>(height_) * 0.5;

  cam_state.SetProjectionMatrix(
      pangolin::ProjectionMatrix(width_, height_, fx, fy, cx, cy, camera_.near_clip, camera_.far_clip));
  cam_state.SetModelViewMatrix(pangolin::ModelViewLookAt(
      camera_.eye[0], camera_.eye[1], camera_.eye[2], camera_.lookat[0], camera_.lookat[1], camera_.lookat[2],
      camera_.up[0], camera_.up[1], camera_.up[2]));
  camera_dirty_ = false;
}

void Viewer::snapshot_ui() {
  std::lock_guard<std::mutex> lock(ui_mutex_);
  for (const auto& [name, widget] : ui_widgets_) {
    if (widget.type == UiWidget::Type::Bool && widget.bool_var) {
      ui_snapshot_[name] = UiValue{true, *(widget.bool_var) ? 1.0 : 0.0};
    } else if (widget.type == UiWidget::Type::Float && widget.float_var) {
      ui_snapshot_[name] = UiValue{false, *(widget.float_var)};
    }
  }
}

void Viewer::render_loop() {
  try {
    pangolin::CreateWindowAndBind(title_, width_, height_);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    pangolin::OpenGlRenderState cam_state(
        pangolin::ProjectionMatrix(width_, height_, 700.0, 700.0, width_ / 2.0, height_ / 2.0, camera_.near_clip,
                                   camera_.far_clip),
        pangolin::ModelViewLookAt(camera_.eye[0], camera_.eye[1], camera_.eye[2], camera_.lookat[0],
                                  camera_.lookat[1], camera_.lookat[2], camera_.up[0], camera_.up[1],
                                  camera_.up[2]));
    pangolin::Handler3D handler(cam_state);

    constexpr int kPanelWidthPx = 240;
    pangolin::CreatePanel("ui").SetBounds(0.0, 1.0, 0.0, pangolin::Attach::Pix(kPanelWidthPx));
    pangolin::View& display =
        pangolin::CreateDisplay()
            .SetBounds(0.0, 1.0, pangolin::Attach::Pix(kPanelWidthPx), 1.0,
                       -static_cast<float>(width_) / static_cast<float>(height_))
            .SetHandler(&handler);

    running_.store(true, std::memory_order_release);
    camera_dirty_ = true;

    const auto target_frame_time = std::chrono::milliseconds(8);
    while (!stop_requested_.load(std::memory_order_acquire)) {
      if (pangolin::ShouldQuit()) {
        break;
      }

      const auto frame_start = std::chrono::steady_clock::now();
      drain_commands();
      update_camera_matrices(cam_state);

      glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      display.Activate(cam_state);

      for (auto& [name, obj] : objects_) {
        (void)name;
        draw_object(obj);
      }

      snapshot_ui();
      pangolin::FinishFrame();

      const auto elapsed = std::chrono::steady_clock::now() - frame_start;
      if (elapsed < target_frame_time) {
        std::unique_lock<std::mutex> lock(command_mutex_);
        command_cv_.wait_for(lock, target_frame_time - elapsed, [this]() {
          return stop_requested_.load(std::memory_order_acquire) || !command_queue_.empty();
        });
      }
    }

    for (auto& [name, obj] : objects_) {
      (void)name;
      delete_gl_buffers(obj);
    }
    objects_.clear();
    ui_widgets_.clear();
  } catch (...) {
    // Keep destructor/stop stable even if Pangolin or GL init fails.
  }

  running_.store(false, std::memory_order_release);
}
