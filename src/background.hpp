#pragma once

#include "config.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class BackgroundModel {
 public:
  BackgroundModel() = default;
  explicit BackgroundModel(float voxel_leaf);

  bool Enabled() const;
  float VoxelLeaf() const;
  std::size_t VoxelCount() const;
  const std::vector<std::int64_t>& PackedVoxels() const;

  void AddVoxelIndex(int x, int y, int z);
  bool Contains(const Vec3f& point) const;

 private:
  float voxel_leaf_ = 0.0f;
  std::size_t voxel_count_ = 0;
  std::vector<std::int64_t> packed_voxels_;

  std::int64_t Pack(int x, int y, int z) const;
  bool ContainsPacked(std::int64_t packed) const;
};

class BackgroundAccumulator {
 public:
  explicit BackgroundAccumulator(float voxel_leaf);

  void AddFrame(const std::vector<PointXYZI>& points);
  BackgroundModel Finalize(int min_frame_hits) const;

  std::size_t FrameCount() const;
  std::size_t ObservedVoxelCount() const;

 private:
  float voxel_leaf_ = 0.0f;
  std::size_t frame_count_ = 0;
  std::unordered_map<std::int64_t, std::int32_t> hits_;
};

BackgroundModel LoadBackgroundModel(const std::string& path);
void SaveBackgroundModel(const std::string& path, const BackgroundModel& model);
std::vector<PointXYZI> RemoveBackgroundPoints(const BackgroundModel& model,
                                             const std::vector<PointXYZI>& points);
