#pragma once

#include "config.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef WITH_LIVOX
#include <livox_lidar_api.h>
#include <livox_lidar_def.h>
#endif

struct LidarStats {
  std::string name;
  std::string ip;
  bool connected = false;
  std::uint64_t packets = 0;
  std::uint64_t points = 0;
};

class LivoxReceiver {
 public:
  explicit LivoxReceiver(AppConfig config);
  ~LivoxReceiver();

  bool Start();
  void Stop();

  void CollectPendingPoints(std::vector<PointXYZI>& out_points);
  std::vector<LidarStats> SnapshotStats() const;

 private:
  struct LidarBuffer {
    explicit LidarBuffer(const LidarConfig& lidar_cfg) : cfg(lidar_cfg) {}

    LidarConfig cfg;
    mutable std::mutex mutex;
    std::vector<PointXYZI> pending_points;
    std::atomic<bool> connected{false};
    std::atomic<std::uint64_t> packets{0};
    std::atomic<std::uint64_t> points{0};
  };

  AppConfig config_;
  std::vector<std::unique_ptr<LidarBuffer>> lidars_;
  mutable std::mutex mapping_mutex_;
  std::unordered_map<std::uint32_t, std::size_t> handle_to_idx_;
  std::unordered_map<std::string, std::size_t> ip_to_idx_;
  std::unordered_map<std::string, std::size_t> code_to_idx_;
  bool started_ = false;

#ifdef WITH_LIVOX
  static void HandleInfoChangeProxy(std::uint32_t handle, const LivoxLidarInfo* info, void* client_data);
  static void HandlePointDataProxy(std::uint32_t handle, std::uint8_t dev_type, LivoxLidarEthernetPacket* data, void* client_data);
  static void HandleAsyncControlResult(livox_status status, std::uint32_t handle, LivoxLidarAsyncControlResponse* response, void* client_data);

  void HandleInfoChange(std::uint32_t handle, const LivoxLidarInfo* info);
  void HandlePointData(std::uint32_t handle, LivoxLidarEthernetPacket* data);
  std::size_t ResolveLidarIndex(const std::string& serial, const std::string& ip) const;
#endif
};
