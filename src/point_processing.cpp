#include "point_processing.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>

namespace {

struct VoxelKey {
  int x = 0;
  int y = 0;
  int z = 0;

  bool operator==(const VoxelKey& other) const {
    return x == other.x && y == other.y && z == other.z;
  }
};

struct VoxelKeyHash {
  std::size_t operator()(const VoxelKey& key) const {
    std::size_t seed = static_cast<std::size_t>(key.x) * 73856093u;
    seed ^= static_cast<std::size_t>(key.y) * 19349663u;
    seed ^= static_cast<std::size_t>(key.z) * 83492791u;
    return seed;
  }
};

struct BucketKey {
  int x = 0;
  int y = 0;

  bool operator==(const BucketKey& other) const {
    return x == other.x && y == other.y;
  }
};

struct BucketKeyHash {
  std::size_t operator()(const BucketKey& key) const {
    std::size_t seed = static_cast<std::size_t>(key.x) * 73856093u;
    seed ^= static_cast<std::size_t>(key.y) * 19349663u;
    return seed;
  }
};

struct VoxelAccumulator {
  Vec3f sum{};
  float intensity_sum = 0.0f;
  std::size_t count = 0;
  std::uint32_t source_index = 0;
};

BucketKey MakeBucketKey(const Vec3f& position, float cell_size) {
  return {
      static_cast<int>(std::floor(position.x / cell_size)),
      static_cast<int>(std::floor(position.y / cell_size)),
  };
}

bool IsPointUsable(const AppConfig& config, const PointXYZI& point) {
  const float range_xy = std::sqrt(point.position.x * point.position.x + point.position.y * point.position.y);
  if (range_xy > config.tracking.max_range_m) {
    return false;
  }
  if (point.position.z < config.tracking.detection_z_min || point.position.z > config.tracking.detection_z_max) {
    return false;
  }
  if (!config.roi.Contains(point.position)) {
    return false;
  }
  return true;
}

Detection BuildDetection(const std::vector<PointXYZI>& points, const std::vector<std::size_t>& indices) {
  Detection detection;
  detection.point_count = indices.size();
  detection.min_bounds = {
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
  };
  detection.max_bounds = {
      std::numeric_limits<float>::lowest(),
      std::numeric_limits<float>::lowest(),
      std::numeric_limits<float>::lowest(),
  };

  Vec3f centroid_sum{};
  float intensity_sum = 0.0f;
  for (const std::size_t idx : indices) {
    const auto& point = points[idx];
    centroid_sum += point.position;
    intensity_sum += point.intensity;
    detection.min_bounds.x = std::min(detection.min_bounds.x, point.position.x);
    detection.min_bounds.y = std::min(detection.min_bounds.y, point.position.y);
    detection.min_bounds.z = std::min(detection.min_bounds.z, point.position.z);
    detection.max_bounds.x = std::max(detection.max_bounds.x, point.position.x);
    detection.max_bounds.y = std::max(detection.max_bounds.y, point.position.y);
    detection.max_bounds.z = std::max(detection.max_bounds.z, point.position.z);
  }

  const float inv_count = indices.empty() ? 0.0f : 1.0f / static_cast<float>(indices.size());
  detection.centroid = centroid_sum * inv_count;
  detection.average_intensity = intensity_sum * inv_count;
  return detection;
}

bool LooksLikePerson(const AppConfig& config, const Detection& detection) {
  if (detection.point_count < config.tracking.min_cluster_points) {
    return false;
  }
  if (detection.Height() < config.tracking.person_min_height ||
      detection.Height() > config.tracking.person_max_height) {
    return false;
  }
  if (detection.Width() > config.tracking.person_max_width ||
      detection.Depth() > config.tracking.person_max_depth) {
    return false;
  }
  return true;
}

}  // namespace

std::vector<PointXYZI> FilterAndDownsamplePoints(const AppConfig& config,
                                                 const std::vector<PointXYZI>& raw_points) {
  if (raw_points.empty()) {
    return {};
  }

  const float leaf = std::max(config.tracking.voxel_leaf, 0.01f);
  std::unordered_map<VoxelKey, VoxelAccumulator, VoxelKeyHash> voxels;
  voxels.reserve(raw_points.size());

  for (const auto& point : raw_points) {
    if (!IsPointUsable(config, point)) {
      continue;
    }

    const VoxelKey key{
        static_cast<int>(std::floor(point.position.x / leaf)),
        static_cast<int>(std::floor(point.position.y / leaf)),
        static_cast<int>(std::floor(point.position.z / leaf)),
    };

    auto& voxel = voxels[key];
    voxel.sum += point.position;
    voxel.intensity_sum += point.intensity;
    voxel.count += 1;
    voxel.source_index = point.source_index;
  }

  std::vector<PointXYZI> downsampled;
  downsampled.reserve(voxels.size());
  for (const auto& [key, voxel] : voxels) {
    (void)key;
    const float inv_count = 1.0f / static_cast<float>(voxel.count);
    downsampled.push_back(PointXYZI{
        voxel.sum * inv_count,
        voxel.intensity_sum * inv_count,
        voxel.source_index,
    });
  }

  return downsampled;
}

std::vector<Detection> DetectPeopleClusters(const AppConfig& config,
                                            const std::vector<PointXYZI>& filtered_points) {
  if (filtered_points.empty()) {
    return {};
  }

  const float tolerance = std::max(config.tracking.cluster_tolerance, 0.05f);
  const float tolerance_sq = tolerance * tolerance;

  std::unordered_map<BucketKey, std::vector<std::size_t>, BucketKeyHash> buckets;
  buckets.reserve(filtered_points.size());
  for (std::size_t i = 0; i < filtered_points.size(); ++i) {
    buckets[MakeBucketKey(filtered_points[i].position, tolerance)].push_back(i);
  }

  std::vector<bool> visited(filtered_points.size(), false);
  std::vector<Detection> detections;

  for (std::size_t start_idx = 0; start_idx < filtered_points.size(); ++start_idx) {
    if (visited[start_idx]) {
      continue;
    }

    std::queue<std::size_t> queue;
    std::vector<std::size_t> cluster_indices;
    queue.push(start_idx);
    visited[start_idx] = true;

    while (!queue.empty()) {
      const std::size_t current_idx = queue.front();
      queue.pop();
      cluster_indices.push_back(current_idx);

      const BucketKey bucket = MakeBucketKey(filtered_points[current_idx].position, tolerance);
      for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
          const BucketKey neighbor{bucket.x + dx, bucket.y + dy};
          const auto bucket_it = buckets.find(neighbor);
          if (bucket_it == buckets.end()) {
            continue;
          }

          for (const std::size_t candidate_idx : bucket_it->second) {
            if (visited[candidate_idx]) {
              continue;
            }
            if (DistanceSquaredXY(filtered_points[current_idx].position,
                                  filtered_points[candidate_idx].position) > tolerance_sq) {
              continue;
            }
            visited[candidate_idx] = true;
            queue.push(candidate_idx);
          }
        }
      }
    }

    Detection detection = BuildDetection(filtered_points, cluster_indices);
    if (LooksLikePerson(config, detection)) {
      detections.push_back(detection);
    }
  }

  return detections;
}
