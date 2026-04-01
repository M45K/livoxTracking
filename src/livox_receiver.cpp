#include "livox_receiver.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

std::string BoundedCString(const char* data, std::size_t max_size) {
  std::size_t length = 0;
  while (length < max_size && data[length] != '\0') {
    ++length;
  }
  return std::string(data, length);
}

}  // namespace

LivoxReceiver::LivoxReceiver(AppConfig config) : config_(std::move(config)) {
  lidars_.reserve(config_.lidars.size());
  for (std::size_t i = 0; i < config_.lidars.size(); ++i) {
    lidars_.push_back(std::make_unique<LidarBuffer>(config_.lidars[i]));
    if (!config_.lidars[i].ip.empty()) {
      ip_to_idx_[config_.lidars[i].ip] = i;
    }
    if (!config_.lidars[i].code.empty()) {
      code_to_idx_[config_.lidars[i].code] = i;
    }
  }
}

LivoxReceiver::~LivoxReceiver() {
  Stop();
}

bool LivoxReceiver::Start() {
  if (started_) {
    return true;
  }

#ifndef WITH_LIVOX
  std::cerr << "Livox SDK2 support is disabled at build time.\n";
  return false;
#else
  if (!LivoxLidarSdkInit(config_.sdk_config_path.c_str(), "")) {
    std::cerr << "LivoxLidarSdkInit failed for " << config_.sdk_config_path << '\n';
    return false;
  }

  SetLivoxLidarInfoChangeCallback(&LivoxReceiver::HandleInfoChangeProxy, this);
  SetLivoxLidarPointCloudCallBack(&LivoxReceiver::HandlePointDataProxy, this);

  if (!LivoxLidarSdkStart()) {
    std::cerr << "LivoxLidarSdkStart failed\n";
    LivoxLidarSdkUninit();
    return false;
  }

  started_ = true;
  return true;
#endif
}

void LivoxReceiver::Stop() {
  if (!started_) {
    return;
  }

#ifdef WITH_LIVOX
  LivoxLidarSdkUninit();
#endif
  started_ = false;
}

void LivoxReceiver::CollectPendingPoints(std::vector<PointXYZI>& out_points) {
  out_points.clear();

  std::size_t total_points = 0;
  for (const auto& lidar : lidars_) {
    std::lock_guard<std::mutex> lock(lidar->mutex);
    total_points += lidar->pending_points.size();
  }
  out_points.reserve(total_points);

  for (const auto& lidar : lidars_) {
    std::lock_guard<std::mutex> lock(lidar->mutex);
    out_points.insert(out_points.end(), lidar->pending_points.begin(), lidar->pending_points.end());
    lidar->pending_points.clear();
  }
}

std::vector<LidarStats> LivoxReceiver::SnapshotStats() const {
  std::vector<LidarStats> stats;
  stats.reserve(lidars_.size());

  for (const auto& lidar : lidars_) {
    stats.push_back(LidarStats{
        lidar->cfg.name,
        lidar->cfg.ip,
        lidar->connected.load(std::memory_order_relaxed),
        lidar->packets.load(std::memory_order_relaxed),
        lidar->points.load(std::memory_order_relaxed),
    });
  }
  return stats;
}

#ifdef WITH_LIVOX

void LivoxReceiver::HandleInfoChangeProxy(std::uint32_t handle, const LivoxLidarInfo* info, void* client_data) {
  auto* receiver = static_cast<LivoxReceiver*>(client_data);
  if (receiver != nullptr) {
    receiver->HandleInfoChange(handle, info);
  }
}

void LivoxReceiver::HandlePointDataProxy(std::uint32_t handle, std::uint8_t /*dev_type*/, LivoxLidarEthernetPacket* data, void* client_data) {
  auto* receiver = static_cast<LivoxReceiver*>(client_data);
  if (receiver != nullptr) {
    receiver->HandlePointData(handle, data);
  }
}

void LivoxReceiver::HandleAsyncControlResult(livox_status status,
                                             std::uint32_t handle,
                                             LivoxLidarAsyncControlResponse* response,
                                             void* client_data) {
  const char* label = client_data != nullptr ? static_cast<const char*>(client_data) : "control";
  if (response == nullptr) {
    std::cerr << "[LIVOX] " << label << " handle=" << handle << " status=" << status
              << " response=null\n";
    return;
  }

  std::cerr << "[LIVOX] " << label << " handle=" << handle
            << " status=" << status
            << " ret_code=" << static_cast<int>(response->ret_code)
            << " error_key=" << response->error_key << '\n';
}

std::size_t LivoxReceiver::ResolveLidarIndex(const std::string& serial, const std::string& ip) const {
  auto code_it = code_to_idx_.find(serial);
  if (code_it != code_to_idx_.end()) {
    return code_it->second;
  }

  auto ip_it = ip_to_idx_.find(ip);
  if (ip_it != ip_to_idx_.end()) {
    return ip_it->second;
  }

  return std::numeric_limits<std::size_t>::max();
}

void LivoxReceiver::HandleInfoChange(std::uint32_t handle, const LivoxLidarInfo* info) {
  if (info == nullptr) {
    return;
  }

  const std::string serial = BoundedCString(info->sn, kBroadcastCodeSize);
  const std::string ip = BoundedCString(info->lidar_ip, 16);
  const std::size_t lidar_idx = ResolveLidarIndex(serial, ip);
  if (lidar_idx == std::numeric_limits<std::size_t>::max()) {
    std::cerr << "[LIVOX] Unmapped lidar handle=" << handle
              << " sn=" << serial << " ip=" << ip << '\n';
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mapping_mutex_);
    handle_to_idx_[handle] = lidar_idx;
  }
  lidars_[lidar_idx]->connected.store(true, std::memory_order_relaxed);

  if (config_.verbose_logs) {
    std::cerr << "[LIVOX] mapped handle=" << handle
              << " name=" << lidars_[lidar_idx]->cfg.name
              << " sn=" << serial << " ip=" << ip << '\n';
  }

  SetLivoxLidarWorkMode(handle, kLivoxLidarNormal, &LivoxReceiver::HandleAsyncControlResult,
                        const_cast<char*>("SetWorkMode"));
  SetLivoxLidarPclDataType(handle, kLivoxLidarCartesianCoordinateHighData,
                           &LivoxReceiver::HandleAsyncControlResult,
                           const_cast<char*>("SetPclType"));
  EnableLivoxLidarPointSend(handle, &LivoxReceiver::HandleAsyncControlResult,
                            const_cast<char*>("EnablePointSend"));
}

void LivoxReceiver::HandlePointData(std::uint32_t handle, LivoxLidarEthernetPacket* data) {
  if (data == nullptr) {
    return;
  }

  std::size_t lidar_idx = std::numeric_limits<std::size_t>::max();
  {
    std::lock_guard<std::mutex> lock(mapping_mutex_);
    auto it = handle_to_idx_.find(handle);
    if (it != handle_to_idx_.end()) {
      lidar_idx = it->second;
    }
  }
  if (lidar_idx == std::numeric_limits<std::size_t>::max()) {
    return;
  }

  auto& lidar = *lidars_[lidar_idx];
  std::vector<PointXYZI> batch;
  batch.reserve(data->dot_num);

  if (data->data_type == kLivoxLidarCartesianCoordinateHighData) {
    auto* points = reinterpret_cast<LivoxLidarCartesianHighRawPoint*>(data->data);
    for (std::uint16_t i = 0; i < data->dot_num; ++i) {
      const Vec3f lidar_point{
          points[i].x * 0.001f,
          points[i].y * 0.001f,
          points[i].z * 0.001f,
      };
      batch.push_back(PointXYZI{
          lidar.cfg.T.TransformPoint(lidar_point),
          points[i].reflectivity / 255.0f,
          static_cast<std::uint32_t>(lidar_idx),
      });
    }
  } else if (data->data_type == kLivoxLidarCartesianCoordinateLowData) {
    auto* points = reinterpret_cast<LivoxLidarCartesianLowRawPoint*>(data->data);
    for (std::uint16_t i = 0; i < data->dot_num; ++i) {
      const Vec3f lidar_point{
          points[i].x * 0.01f,
          points[i].y * 0.01f,
          points[i].z * 0.01f,
      };
      batch.push_back(PointXYZI{
          lidar.cfg.T.TransformPoint(lidar_point),
          points[i].reflectivity / 255.0f,
          static_cast<std::uint32_t>(lidar_idx),
      });
    }
  } else {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(lidar.mutex);
    lidar.pending_points.insert(lidar.pending_points.end(), batch.begin(), batch.end());
  }

  lidar.packets.fetch_add(1, std::memory_order_relaxed);
  lidar.points.fetch_add(batch.size(), std::memory_order_relaxed);
}

#endif
