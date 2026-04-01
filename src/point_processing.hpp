#pragma once

#include "config.hpp"

#include <cstddef>
#include <vector>

struct Detection {
  Vec3f centroid{};
  Vec3f min_bounds{};
  Vec3f max_bounds{};
  std::size_t point_count = 0;
  float average_intensity = 0.0f;

  float Width() const { return max_bounds.x - min_bounds.x; }
  float Depth() const { return max_bounds.y - min_bounds.y; }
  float Height() const { return max_bounds.z - min_bounds.z; }
};

std::vector<PointXYZI> FilterAndDownsamplePoints(const AppConfig& config,
                                                 const std::vector<PointXYZI>& raw_points);
std::vector<Detection> DetectPeopleClusters(const AppConfig& config,
                                            const std::vector<PointXYZI>& filtered_points);
