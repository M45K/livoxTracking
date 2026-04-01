#include "background.hpp"
#include "config.hpp"
#include "debug_view.hpp"
#include "livox_receiver.hpp"
#include "osc_publisher.hpp"
#include "point_processing.hpp"
#include "tcp_json_publisher.hpp"
#include "tracker.hpp"
#include "zones.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
#include <sstream>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

void HandleSignal(int) {
  g_running.store(false, std::memory_order_relaxed);
}

std::uint64_t WallClockMillis() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

void PrintUsage() {
  std::cout
      << "Usage:\n"
      << "  livox_tracking_service [config_path]\n"
      << "  livox_tracking_service --validate-config [config_path]\n"
      << "  livox_tracking_service --calibrate-background [config_path]\n";
}

std::string FormatStats(const std::vector<LidarStats>& stats) {
  std::ostringstream stream;
  for (std::size_t i = 0; i < stats.size(); ++i) {
    if (i != 0) {
      stream << " | ";
    }
    stream << stats[i].name
           << " connected=" << (stats[i].connected ? "yes" : "no")
           << " packets=" << stats[i].packets
           << " points=" << stats[i].points;
  }
  return stream.str();
}

std::string FormatZones(const std::vector<ZoneSnapshot>& zones) {
  if (zones.empty()) {
    return "zones=off";
  }

  std::ostringstream stream;
  stream << "zones=";
  for (std::size_t i = 0; i < zones.size(); ++i) {
    if (i != 0) {
      stream << ',';
    }
    stream << zones[i].name << ':' << zones[i].Occupancy();
  }
  return stream.str();
}

}  // namespace

int main(int argc, char** argv) {
  std::string config_path = "data/config.yaml";
  bool validate_only = false;
  bool calibrate_background = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return 0;
    }
    if (arg == "--validate-config") {
      validate_only = true;
      continue;
    }
    if (arg == "--calibrate-background") {
      calibrate_background = true;
      continue;
    }
    config_path = arg;
  }

  try {
    const AppConfig config = load_config(config_path);
    std::cout << "[CONFIG] " << summarize_config(config) << '\n';
    if (validate_only) {
      return 0;
    }

    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    LivoxReceiver receiver(config);
    if (!receiver.Start()) {
      return 1;
    }
    DebugViewRenderer debug_view(config);

    const auto tick = std::chrono::milliseconds(std::max(1, 1000 / config.tracking.processing_hz));

    if (calibrate_background) {
      BackgroundAccumulator background_accumulator(config.background.voxel_leaf);
      auto next_tick = std::chrono::steady_clock::now();
      auto last_log = std::chrono::steady_clock::now();
      const auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(config.background.capture_seconds);

      std::cout << "[CAL] capturing empty-room background for "
                << config.background.capture_seconds
                << "s into " << config.background.file << '\n';

      while (g_running.load(std::memory_order_relaxed) && std::chrono::steady_clock::now() < end_time) {
        next_tick += tick;

        std::vector<PointXYZI> raw_points;
        receiver.CollectPendingPoints(raw_points);
        const std::vector<PointXYZI> filtered_points = FilterAndDownsamplePoints(config, raw_points);
        background_accumulator.AddFrame(filtered_points);
        debug_view.Render(WallClockMillis(),
                          filtered_points,
                          {},
                          {},
                          {},
                          receiver.SnapshotStats());

        const auto now = std::chrono::steady_clock::now();
        if (now - last_log >= std::chrono::seconds(2)) {
          const auto remaining =
              std::chrono::duration_cast<std::chrono::seconds>(end_time - now).count();
          std::cout << "[CAL] frames=" << background_accumulator.FrameCount()
                    << " filtered=" << filtered_points.size()
                    << " voxels=" << background_accumulator.ObservedVoxelCount()
                    << " remaining_s=" << std::max<long long>(0, remaining) << '\n';
          last_log = now;
        }

        std::this_thread::sleep_until(next_tick);
        if (next_tick < std::chrono::steady_clock::now()) {
          next_tick = std::chrono::steady_clock::now();
        }
      }

      const BackgroundModel model =
          background_accumulator.Finalize(config.background.min_frame_hits);
      SaveBackgroundModel(config.background.file, model);
      std::cout << "[CAL] saved background voxels=" << model.VoxelCount()
                << " file=" << config.background.file << '\n';
      receiver.Stop();
      return 0;
    }

    BackgroundModel background_model;
    if (config.background.enabled) {
      background_model = LoadBackgroundModel(config.background.file);
      std::cout << "[BACKGROUND] loaded voxels=" << background_model.VoxelCount()
                << " leaf=" << background_model.VoxelLeaf() << '\n';
    }

    TcpJsonPublisher tcp_publisher(config.tcp);
    if (!tcp_publisher.Start()) {
      receiver.Stop();
      return 1;
    }

    OscPublisher osc_publisher(config.osc);
    if (!osc_publisher.Start()) {
      tcp_publisher.Stop();
      receiver.Stop();
      return 1;
    }

    Tracker tracker(config);
    auto next_tick = std::chrono::steady_clock::now();
    auto last_log = std::chrono::steady_clock::now();

    std::cout << "[RUN] tracking loop started\n";
    while (g_running.load(std::memory_order_relaxed)) {
      next_tick += tick;

      std::vector<PointXYZI> raw_points;
      receiver.CollectPendingPoints(raw_points);
      std::vector<PointXYZI> filtered_points = FilterAndDownsamplePoints(config, raw_points);
      const std::size_t pre_background_count = filtered_points.size();
      if (config.background.enabled) {
        filtered_points = RemoveBackgroundPoints(background_model, filtered_points);
      }
      const std::vector<Detection> detections = DetectPeopleClusters(config, filtered_points);

      const std::uint64_t now_ms = WallClockMillis();
      tracker.Update(now_ms, detections);
      const std::vector<PersonState> active_people = tracker.ActiveStates(now_ms);
      const std::vector<ZoneSnapshot> zones = BuildZoneSnapshots(config.zones, active_people);

      tcp_publisher.Publish(now_ms, active_people, zones);
      osc_publisher.Publish(now_ms, active_people);
      debug_view.Render(now_ms,
                        filtered_points,
                        detections,
                        active_people,
                        zones,
                        receiver.SnapshotStats());

      const auto now = std::chrono::steady_clock::now();
      if (now - last_log >= std::chrono::seconds(2)) {
        std::cout << "[STAT] raw=" << raw_points.size()
                  << " filtered=" << pre_background_count
                  << " dynamic=" << filtered_points.size()
                  << " detections=" << detections.size()
                  << " active_tracks=" << active_people.size()
                  << " " << FormatZones(zones)
                  << " | " << FormatStats(receiver.SnapshotStats()) << '\n';
        last_log = now;
      }

      std::this_thread::sleep_until(next_tick);
      if (next_tick < std::chrono::steady_clock::now()) {
        next_tick = std::chrono::steady_clock::now();
      }
    }

    std::cout << "[EXIT] stopping tracking service\n";
    osc_publisher.Stop();
    tcp_publisher.Stop();
    receiver.Stop();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "[ERR] " << error.what() << '\n';
    return 1;
  }
}
