#pragma once

#include "config.hpp"
#include "point_processing.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

struct PersonState {
  std::uint64_t id = 0;
  Vec3f position{};
  Vec3f velocity{};
  Vec3f min_bounds{};
  Vec3f max_bounds{};
  std::size_t point_count = 0;
  std::uint64_t created_ms = 0;
  std::uint64_t last_seen_ms = 0;

  float Width() const { return max_bounds.x - min_bounds.x; }
  float Depth() const { return max_bounds.y - min_bounds.y; }
  float Height() const { return max_bounds.z - min_bounds.z; }
};

class Tracker {
 public:
  explicit Tracker(const AppConfig& config);

  void Update(std::uint64_t now_ms, const std::vector<Detection>& detections);
  std::vector<PersonState> ActiveStates(std::uint64_t now_ms) const;

 private:
  struct Track {
    PersonState state{};
    std::uint32_t missed_updates = 0;
  };

  AppConfig config_;
  std::vector<Track> tracks_;
  std::uint64_t next_track_id_ = 1;
};
