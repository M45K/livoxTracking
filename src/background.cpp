#include "background.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {

constexpr std::int64_t kAxisBias = 1LL << 20;
constexpr std::int64_t kAxisMask = (1LL << 21) - 1;

std::int64_t PackVoxel(int x, int y, int z) {
  const std::int64_t px = static_cast<std::int64_t>(x) + kAxisBias;
  const std::int64_t py = static_cast<std::int64_t>(y) + kAxisBias;
  const std::int64_t pz = static_cast<std::int64_t>(z) + kAxisBias;
  return (px << 42) | (py << 21) | pz;
}

void UnpackVoxel(std::int64_t packed, int& x, int& y, int& z) {
  x = static_cast<int>(((packed >> 42) & kAxisMask) - kAxisBias);
  y = static_cast<int>(((packed >> 21) & kAxisMask) - kAxisBias);
  z = static_cast<int>((packed & kAxisMask) - kAxisBias);
}

std::int64_t PackPoint(const Vec3f& point, float leaf) {
  const int x = static_cast<int>(std::floor(point.x / leaf));
  const int y = static_cast<int>(std::floor(point.y / leaf));
  const int z = static_cast<int>(std::floor(point.z / leaf));
  return PackVoxel(x, y, z);
}

}  // namespace

BackgroundModel::BackgroundModel(float voxel_leaf) : voxel_leaf_(voxel_leaf) {}

bool BackgroundModel::Enabled() const {
  return voxel_leaf_ > 0.0f && !packed_voxels_.empty();
}

float BackgroundModel::VoxelLeaf() const {
  return voxel_leaf_;
}

std::size_t BackgroundModel::VoxelCount() const {
  return voxel_count_;
}

const std::vector<std::int64_t>& BackgroundModel::PackedVoxels() const {
  return packed_voxels_;
}

void BackgroundModel::AddVoxelIndex(int x, int y, int z) {
  const std::int64_t packed = Pack(x, y, z);
  const auto it = std::lower_bound(packed_voxels_.begin(), packed_voxels_.end(), packed);
  if (it == packed_voxels_.end() || *it != packed) {
    packed_voxels_.insert(it, packed);
    voxel_count_ = packed_voxels_.size();
  }
}

bool BackgroundModel::Contains(const Vec3f& point) const {
  if (!Enabled()) {
    return false;
  }
  return ContainsPacked(PackPoint(point, voxel_leaf_));
}

std::int64_t BackgroundModel::Pack(int x, int y, int z) const {
  return PackVoxel(x, y, z);
}

bool BackgroundModel::ContainsPacked(std::int64_t packed) const {
  return std::binary_search(packed_voxels_.begin(), packed_voxels_.end(), packed);
}

BackgroundAccumulator::BackgroundAccumulator(float voxel_leaf) : voxel_leaf_(voxel_leaf) {}

void BackgroundAccumulator::AddFrame(const std::vector<PointXYZI>& points) {
  ++frame_count_;
  std::unordered_set<std::int64_t> frame_voxels;
  frame_voxels.reserve(points.size());

  for (const auto& point : points) {
    frame_voxels.insert(PackPoint(point.position, voxel_leaf_));
  }

  for (const auto packed : frame_voxels) {
    hits_[packed] += 1;
  }
}

BackgroundModel BackgroundAccumulator::Finalize(int min_frame_hits) const {
  BackgroundModel model(voxel_leaf_);
  std::vector<std::int64_t> voxels;
  voxels.reserve(hits_.size());

  for (const auto& [packed, hit_count] : hits_) {
    if (hit_count >= min_frame_hits) {
      voxels.push_back(packed);
    }
  }

  std::sort(voxels.begin(), voxels.end());
  for (const auto packed : voxels) {
    int x = 0;
    int y = 0;
    int z = 0;
    UnpackVoxel(packed, x, y, z);
    model.AddVoxelIndex(x, y, z);
  }
  return model;
}

std::size_t BackgroundAccumulator::FrameCount() const {
  return frame_count_;
}

std::size_t BackgroundAccumulator::ObservedVoxelCount() const {
  return hits_.size();
}

BackgroundModel LoadBackgroundModel(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("Cannot open background model: " + path);
  }

  std::string line;
  float voxel_leaf = 0.0f;
  BackgroundModel model;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::stringstream stream(line);
    if (voxel_leaf <= 0.0f) {
      std::string tag;
      stream >> tag >> voxel_leaf;
      if (tag != "leaf" || voxel_leaf <= 0.0f) {
        throw std::runtime_error("Invalid background model header in " + path);
      }
      model = BackgroundModel(voxel_leaf);
      continue;
    }

    int x = 0;
    int y = 0;
    int z = 0;
    if (!(stream >> x >> y >> z)) {
      throw std::runtime_error("Invalid background voxel line in " + path + ": " + line);
    }
    model.AddVoxelIndex(x, y, z);
  }

  if (voxel_leaf <= 0.0f) {
    throw std::runtime_error("Background model is missing its leaf header: " + path);
  }

  return model;
}

void SaveBackgroundModel(const std::string& path, const BackgroundModel& model) {
  std::ofstream file(path);
  if (!file) {
    throw std::runtime_error("Cannot write background model: " + path);
  }

  file << "leaf " << model.VoxelLeaf() << '\n';
  if (!model.Enabled()) {
    return;
  }

  for (const auto packed : model.PackedVoxels()) {
    int x = 0;
    int y = 0;
    int z = 0;
    UnpackVoxel(packed, x, y, z);
    file << x << ' ' << y << ' ' << z << '\n';
  }
}

std::vector<PointXYZI> RemoveBackgroundPoints(const BackgroundModel& model,
                                             const std::vector<PointXYZI>& points) {
  if (!model.Enabled() || points.empty()) {
    return points;
  }

  std::vector<PointXYZI> filtered;
  filtered.reserve(points.size());
  for (const auto& point : points) {
    if (!model.Contains(point.position)) {
      filtered.push_back(point);
    }
  }
  return filtered;
}
