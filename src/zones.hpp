#pragma once

#include "config.hpp"
#include "tracker.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ZoneSnapshot {
  std::string name;
  std::vector<std::uint64_t> occupant_ids;

  std::size_t Occupancy() const {
    return occupant_ids.size();
  }
};

std::vector<ZoneSnapshot> BuildZoneSnapshots(const std::vector<ZoneConfig>& zones,
                                             const std::vector<PersonState>& people);
