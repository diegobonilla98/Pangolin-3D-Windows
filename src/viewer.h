#pragma once

#include <pangolin/pangolin.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class Viewer {
 public:
  struct UiValue {
    bool is_bool = false;
    double value = 0.0;
  };

  Viewer(int width = 1280, int height = 720, std::string title = "pangolin_fast");
  ~Viewer();

  Viewer(const Viewer&) = delete;
  Viewer& operator=(const Viewer&) = delete;

  void start();
  void stop();
  bool is_running() const noexcept;

  void set_camera(const std::array<float, 3>& eye,
                  const std::array<float, 3>& lookat,
                  const std::array<float, 3>& up);
  void set_projection(float fov_deg, float near_clip, float far_clip);

  void add_points(std::string name,
                  std::vector<float> xyz,
                  std::optional<std::vector<float>> rgb,
                  float point_size,
                  bool dynamic);
  void update_points(std::string name,
                     std::optional<std::vector<float>> xyz,
                     std::optional<std::vector<float>> rgb);
  void set_point_size(std::string name, float point_size);
  void set_visible(std::string name, bool visible);
  void remove(std::string name);

  void add_lines(std::string name,
                 std::vector<float> vertices,
                 std::optional<std::vector<float>> rgb,
                 float width);
  void add_axis(std::string name, float size);
  void add_grid(std::string name, float half, float step);

  void add_mesh(std::string name,
                std::vector<float> vertices,
                std::vector<std::uint32_t> faces,
                std::optional<std::vector<float>> normals,
                std::optional<std::vector<float>> rgb);
  void set_pose(std::string name, const std::array<float, 16>& T_world_object);

  void add_bool(std::string name, bool default_value);
  void add_float(std::string name, float default_value, float min_value, float max_value);
  std::unordered_map<std::string, UiValue> get_ui() const;

 private:
  enum class ObjectType { Points, Lines, Mesh };

  struct SceneObject {
    ObjectType type = ObjectType::Points;
    bool dynamic = true;
    bool visible = true;
    float point_size = 2.0f;
    float line_width = 2.0f;
    std::array<float, 16> pose{};
    std::vector<float> vertices;
    std::vector<float> colors;
    std::vector<float> normals;
    std::vector<std::uint32_t> indices;
    bool vertices_dirty = true;
    bool colors_dirty = true;
    bool normals_dirty = true;
    bool indices_dirty = true;
    GLuint vbo = 0;
    GLuint cbo = 0;
    GLuint nbo = 0;
    GLuint ebo = 0;
  };

  struct CameraConfig {
    std::array<float, 3> eye{3.0f, -3.0f, 2.0f};
    std::array<float, 3> lookat{0.0f, 0.0f, 0.0f};
    std::array<float, 3> up{0.0f, 0.0f, 1.0f};
    float fov_deg = 60.0f;
    float near_clip = 0.05f;
    float far_clip = 1000.0f;
  };

  struct UiWidget {
    enum class Type { Bool, Float };
    Type type = Type::Bool;
    std::unique_ptr<pangolin::Var<bool>> bool_var;
    std::unique_ptr<pangolin::Var<double>> float_var;
  };

  using Command = std::function<void()>;

  static std::array<float, 16> identity_matrix();
  static void fill_color(std::vector<float>& colors, std::size_t count, float r, float g, float b);

  void enqueue(Command cmd);
  void render_loop();
  void drain_commands();
  void upload_if_dirty(SceneObject& obj);
  void draw_object(SceneObject& obj);
  void delete_gl_buffers(SceneObject& obj);
  void update_camera_matrices(pangolin::OpenGlRenderState& cam_state);
  void snapshot_ui();

  int width_ = 1280;
  int height_ = 720;
  std::string title_ = "pangolin_fast";

  std::thread render_thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};

  std::mutex command_mutex_;
  std::condition_variable command_cv_;
  std::queue<Command> command_queue_;

  CameraConfig camera_;
  bool camera_dirty_ = true;
  std::unordered_map<std::string, SceneObject> objects_;
  std::unordered_map<std::string, UiWidget> ui_widgets_;

  mutable std::mutex ui_mutex_;
  std::unordered_map<std::string, UiValue> ui_snapshot_;
};
