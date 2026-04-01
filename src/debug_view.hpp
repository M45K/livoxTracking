#pragma once

#include "config.hpp"
#include "livox_receiver.hpp"
#include "point_processing.hpp"
#include "tracker.hpp"
#include "zones.hpp"

#include <cstdint>
#include <vector>

class DebugViewRenderer {
 public:
  explicit DebugViewRenderer(const AppConfig& config);

  bool Enabled() const;
  void Render(std::uint64_t timestamp_ms,
              const std::vector<PointXYZI>& points,
              const std::vector<Detection>& detections,
              const std::vector<PersonState>& people,
              const std::vector<ZoneSnapshot>& zones,
              const std::vector<LidarStats>& lidar_stats);

 private:
  AppConfig config_;
  std::uint64_t last_render_ms_ = 0;
};
