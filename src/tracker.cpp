#include "tracker.hpp"

#include <algorithm>

namespace {

struct MatchCandidate {
  std::size_t track_idx = 0;
  std::size_t detection_idx = 0;
  float distance_sq = 0.0f;
};

}  // namespace

Tracker::Tracker(const AppConfig& config) : config_(config) {}

void Tracker::Update(std::uint64_t now_ms, const std::vector<Detection>& detections) {
  const float gate_sq = config_.tracking.track_match_distance * config_.tracking.track_match_distance;

  std::vector<MatchCandidate> candidates;
  for (std::size_t track_idx = 0; track_idx < tracks_.size(); ++track_idx) {
    for (std::size_t detection_idx = 0; detection_idx < detections.size(); ++detection_idx) {
      const float distance_sq = DistanceSquaredXY(tracks_[track_idx].state.position, detections[detection_idx].centroid);
      if (distance_sq <= gate_sq) {
        candidates.push_back(MatchCandidate{track_idx, detection_idx, distance_sq});
      }
    }
  }

  std::sort(candidates.begin(), candidates.end(), [](const MatchCandidate& lhs, const MatchCandidate& rhs) {
    return lhs.distance_sq < rhs.distance_sq;
  });

  std::vector<bool> track_assigned(tracks_.size(), false);
  std::vector<bool> detection_assigned(detections.size(), false);

  for (const auto& candidate : candidates) {
    if (track_assigned[candidate.track_idx] || detection_assigned[candidate.detection_idx]) {
      continue;
    }

    auto& track = tracks_[candidate.track_idx];
    const auto& detection = detections[candidate.detection_idx];
    const float dt = std::max<std::uint64_t>(1, now_ms - track.state.last_seen_ms) / 1000.0f;
    const Vec3f new_velocity = (detection.centroid - track.state.position) / dt;

    track.state.velocity = track.state.velocity * 0.35f + new_velocity * 0.65f;
    track.state.position = detection.centroid;
    track.state.min_bounds = detection.min_bounds;
    track.state.max_bounds = detection.max_bounds;
    track.state.point_count = detection.point_count;
    track.state.last_seen_ms = now_ms;
    track.missed_updates = 0;

    track_assigned[candidate.track_idx] = true;
    detection_assigned[candidate.detection_idx] = true;
  }

  for (std::size_t track_idx = 0; track_idx < tracks_.size(); ++track_idx) {
    if (!track_assigned[track_idx]) {
      tracks_[track_idx].missed_updates += 1;
    }
  }

  for (std::size_t detection_idx = 0; detection_idx < detections.size(); ++detection_idx) {
    if (detection_assigned[detection_idx]) {
      continue;
    }

    const auto& detection = detections[detection_idx];
    Track new_track;
    new_track.state.id = next_track_id_++;
    new_track.state.position = detection.centroid;
    new_track.state.velocity = {};
    new_track.state.min_bounds = detection.min_bounds;
    new_track.state.max_bounds = detection.max_bounds;
    new_track.state.point_count = detection.point_count;
    new_track.state.created_ms = now_ms;
    new_track.state.last_seen_ms = now_ms;
    tracks_.push_back(new_track);
  }

  tracks_.erase(
      std::remove_if(tracks_.begin(), tracks_.end(), [&](const Track& track) {
        return now_ms > track.state.last_seen_ms &&
               (now_ms - track.state.last_seen_ms) > static_cast<std::uint64_t>(config_.tracking.track_timeout_ms);
      }),
      tracks_.end());
}

std::vector<PersonState> Tracker::ActiveStates(std::uint64_t now_ms) const {
  std::vector<PersonState> active;
  for (const auto& track : tracks_) {
    if (now_ms >= track.state.last_seen_ms &&
        (now_ms - track.state.last_seen_ms) <= static_cast<std::uint64_t>(config_.tracking.track_timeout_ms)) {
      active.push_back(track.state);
    }
  }
  return active;
}
