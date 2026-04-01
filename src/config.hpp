#pragma once

#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct LidarConfig {
  std::string name;
  std::string code;
  std::string ip;
  Mat4f T{};
};

struct RoiConfig {
  bool enabled = false;
  float x_min = -100.0f;
  float x_max = 100.0f;
  float y_min = -100.0f;
  float y_max = 100.0f;

  bool Contains(const Vec3f& point) const {
    return !enabled || (point.x >= x_min && point.x <= x_max &&
                        point.y >= y_min && point.y <= y_max);
  }
};

struct BackgroundConfig {
  bool enabled = false;
  std::string file = "data/background.vox";
  int capture_seconds = 15;
  int min_frame_hits = 5;
  float voxel_leaf = 0.12f;
};

struct ZoneConfig {
  std::string name;
  float x_min = 0.0f;
  float x_max = 0.0f;
  float y_min = 0.0f;
  float y_max = 0.0f;

  bool Contains(const Vec3f& point) const {
    return point.x >= x_min && point.x <= x_max &&
           point.y >= y_min && point.y <= y_max;
  }
};

struct DebugViewConfig {
  bool enabled = false;
  std::string file = "debug/feedback.html";
  int width = 1280;
  int height = 900;
  float x_min = -2.0f;
  float x_max = 4.0f;
  float y_min = -2.0f;
  float y_max = 4.0f;
  std::size_t max_points = 4000;
};

struct TcpOutputConfig {
  bool enabled = true;
  std::string bind_ip = "0.0.0.0";
  std::uint16_t port = 9100;
};

struct OscOutputConfig {
  bool enabled = false;
  std::string target_ip = "127.0.0.1";
  std::uint16_t port = 9000;
  std::string address = "/livox/person";
};

struct TrackingConfig {
  float voxel_leaf = 0.12f;
  float detection_z_min = 0.20f;
  float detection_z_max = 2.20f;
  float max_range_m = 25.0f;
  float cluster_tolerance = 0.45f;
  std::size_t min_cluster_points = 10;
  float person_min_height = 0.45f;
  float person_max_height = 2.40f;
  float person_max_width = 1.20f;
  float person_max_depth = 1.20f;
  float track_match_distance = 0.90f;
  int track_timeout_ms = 1200;
  int processing_hz = 20;
};

struct AppConfig {
  std::string sdk_config_path;
  std::vector<LidarConfig> lidars;
  RoiConfig roi;
  BackgroundConfig background;
  std::vector<ZoneConfig> zones;
  DebugViewConfig debug_view;
  TcpOutputConfig tcp;
  OscOutputConfig osc;
  TrackingConfig tracking;
  bool verbose_logs = true;
};

AppConfig load_config(const std::string& path);
std::string summarize_config(const AppConfig& config);
