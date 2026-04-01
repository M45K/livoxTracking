#include "zones.hpp"

std::vector<ZoneSnapshot> BuildZoneSnapshots(const std::vector<ZoneConfig>& zones,
                                             const std::vector<PersonState>& people) {
  std::vector<ZoneSnapshot> snapshots;
  snapshots.reserve(zones.size());

  for (const auto& zone : zones) {
    ZoneSnapshot snapshot;
    snapshot.name = zone.name;
    for (const auto& person : people) {
      if (zone.Contains(person.position)) {
        snapshot.occupant_ids.push_back(person.id);
      }
    }
    snapshots.push_back(std::move(snapshot));
  }

  return snapshots;
}
