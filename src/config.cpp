#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

enum class Section {
  kRoot,
  kLidars,
  kBackground,
  kZones,
  kDebugView,
  kTcp,
  kOsc,
  kRoi,
};

std::string Trim(const std::string& input) {
  const char* whitespace = " \t\r\n";
  const auto start = input.find_first_not_of(whitespace);
  if (start == std::string::npos) {
    return "";
  }
  const auto end = input.find_last_not_of(whitespace);
  return input.substr(start, end - start + 1);
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

bool ParseBool(const std::string& value) {
  const std::string normalized = ToLower(Trim(value));
  if (normalized == "true" || normalized == "on" || normalized == "yes" || normalized == "1") {
    return true;
  }
  if (normalized == "false" || normalized == "off" || normalized == "no" || normalized == "0") {
    return false;
  }
  throw std::runtime_error("Invalid boolean value: " + value);
}

std::vector<std::string> Split(const std::string& input, char delimiter) {
  std::vector<std::string> tokens;
  std::stringstream stream(input);
  std::string token;
  while (std::getline(stream, token, delimiter)) {
    token = Trim(token);
    if (!token.empty()) {
      tokens.push_back(token);
    }
  }
  return tokens;
}

Mat4f ParseMatrix4(std::istream& stream, std::string matrix_text) {
  while (matrix_text.find(']') == std::string::npos) {
    std::string next_line;
    if (!std::getline(stream, next_line)) {
      throw std::runtime_error("Matrix definition must end with ']'");
    }
    matrix_text += " " + Trim(next_line);
  }

  const auto start = matrix_text.find('[');
  const auto end = matrix_text.find(']');
  if (start == std::string::npos || end == std::string::npos || end <= start) {
    throw std::runtime_error("Invalid matrix definition: " + matrix_text);
  }

  const auto values = Split(matrix_text.substr(start + 1, end - start - 1), ',');
  if (values.size() != 16) {
    throw std::runtime_error("Transform matrix must contain 16 values");
  }

  Mat4f matrix;
  for (std::size_t i = 0; i < values.size(); ++i) {
    matrix.data[i] = std::stof(values[i]);
  }
  return matrix;
}

void AssignRootKey(AppConfig& config, const std::string& key, const std::string& value) {
  if (key == "sdk_config") {
    config.sdk_config_path = value;
  } else if (key == "voxel_leaf") {
    config.tracking.voxel_leaf = std::stof(value);
  } else if (key == "detection_z_min") {
    config.tracking.detection_z_min = std::stof(value);
  } else if (key == "detection_z_max") {
    config.tracking.detection_z_max = std::stof(value);
  } else if (key == "max_range_m") {
    config.tracking.max_range_m = std::stof(value);
  } else if (key == "cluster_tolerance") {
    config.tracking.cluster_tolerance = std::stof(value);
  } else if (key == "min_cluster_points") {
    config.tracking.min_cluster_points = static_cast<std::size_t>(std::stoul(value));
  } else if (key == "person_min_height") {
    config.tracking.person_min_height = std::stof(value);
  } else if (key == "person_max_height") {
    config.tracking.person_max_height = std::stof(value);
  } else if (key == "person_max_width") {
    config.tracking.person_max_width = std::stof(value);
  } else if (key == "person_max_depth") {
    config.tracking.person_max_depth = std::stof(value);
  } else if (key == "track_match_distance") {
    config.tracking.track_match_distance = std::stof(value);
  } else if (key == "track_timeout_ms") {
    config.tracking.track_timeout_ms = std::stoi(value);
  } else if (key == "processing_hz") {
    config.tracking.processing_hz = std::stoi(value);
  } else if (key == "verbose_logs") {
    config.verbose_logs = ParseBool(value);
  } else if (key == "host_ip" || key == "point_port") {
    // Legacy keys kept for backward compatibility with the previous skeleton.
  } else {
    throw std::runtime_error("Unknown root config key: " + key);
  }
}

void AssignTcpKey(TcpOutputConfig& config, const std::string& key, const std::string& value) {
  if (key == "enabled") {
    config.enabled = ParseBool(value);
  } else if (key == "bind_ip") {
    config.bind_ip = value;
  } else if (key == "port") {
    config.port = static_cast<std::uint16_t>(std::stoul(value));
  } else {
    throw std::runtime_error("Unknown tcp config key: " + key);
  }
}

void AssignOscKey(OscOutputConfig& config, const std::string& key, const std::string& value) {
  if (key == "enabled") {
    config.enabled = ParseBool(value);
  } else if (key == "target_ip") {
    config.target_ip = value;
  } else if (key == "port") {
    config.port = static_cast<std::uint16_t>(std::stoul(value));
  } else if (key == "address") {
    config.address = value;
  } else {
    throw std::runtime_error("Unknown osc config key: " + key);
  }
}

void AssignRoiKey(RoiConfig& config, const std::string& key, const std::string& value) {
  if (key == "enabled") {
    config.enabled = ParseBool(value);
  } else if (key == "x_min") {
    config.x_min = std::stof(value);
  } else if (key == "x_max") {
    config.x_max = std::stof(value);
  } else if (key == "y_min") {
    config.y_min = std::stof(value);
  } else if (key == "y_max") {
    config.y_max = std::stof(value);
  } else {
    throw std::runtime_error("Unknown roi config key: " + key);
  }
}

void AssignBackgroundKey(BackgroundConfig& config, const std::string& key, const std::string& value) {
  if (key == "enabled") {
    config.enabled = ParseBool(value);
  } else if (key == "file") {
    config.file = value;
  } else if (key == "capture_seconds") {
    config.capture_seconds = std::stoi(value);
  } else if (key == "min_frame_hits") {
    config.min_frame_hits = std::stoi(value);
  } else if (key == "voxel_leaf") {
    config.voxel_leaf = std::stof(value);
  } else {
    throw std::runtime_error("Unknown background config key: " + key);
  }
}

void AssignZoneKey(ZoneConfig& config, const std::string& key, const std::string& value) {
  if (key == "name") {
    config.name = value;
  } else if (key == "x_min") {
    config.x_min = std::stof(value);
  } else if (key == "x_max") {
    config.x_max = std::stof(value);
  } else if (key == "y_min") {
    config.y_min = std::stof(value);
  } else if (key == "y_max") {
    config.y_max = std::stof(value);
  } else {
    throw std::runtime_error("Unknown zone config key: " + key);
  }
}

void AssignDebugViewKey(DebugViewConfig& config, const std::string& key, const std::string& value) {
  if (key == "enabled") {
    config.enabled = ParseBool(value);
  } else if (key == "file") {
    config.file = value;
  } else if (key == "width") {
    config.width = std::stoi(value);
  } else if (key == "height") {
    config.height = std::stoi(value);
  } else if (key == "x_min") {
    config.x_min = std::stof(value);
  } else if (key == "x_max") {
    config.x_max = std::stof(value);
  } else if (key == "y_min") {
    config.y_min = std::stof(value);
  } else if (key == "y_max") {
    config.y_max = std::stof(value);
  } else if (key == "max_points") {
    config.max_points = static_cast<std::size_t>(std::stoul(value));
  } else {
    throw std::runtime_error("Unknown debug_view config key: " + key);
  }
}

void AssignLidarKey(LidarConfig& config, const std::string& key, const std::string& value) {
  if (key == "name") {
    config.name = value;
  } else if (key == "ip") {
    config.ip = value;
  } else if (key == "code") {
    config.code = value;
  } else {
    throw std::runtime_error("Unknown lidar config key: " + key);
  }
}

void ParseKeyValue(const std::string& line, std::string& key, std::string& value) {
  const auto colon = line.find(':');
  if (colon == std::string::npos) {
    throw std::runtime_error("Expected key:value pair, got: " + line);
  }
  key = Trim(line.substr(0, colon));
  value = Trim(line.substr(colon + 1));
}

}  // namespace

AppConfig load_config(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("Cannot open config: " + path);
  }

  AppConfig config;
  Section section = Section::kRoot;
  bool has_active_lidar = false;
  LidarConfig current_lidar;
  bool has_active_zone = false;
  ZoneConfig current_zone;
  std::string line;

  const auto flush_lidar = [&]() {
    if (!has_active_lidar) {
      return;
    }
    if (current_lidar.name.empty()) {
      current_lidar.name = current_lidar.ip.empty() ? current_lidar.code : current_lidar.ip;
    }
    config.lidars.push_back(current_lidar);
    current_lidar = LidarConfig{};
    has_active_lidar = false;
  };

  const auto flush_zone = [&]() {
    if (!has_active_zone) {
      return;
    }
    if (current_zone.name.empty()) {
      throw std::runtime_error("Zone entry is missing a name");
    }
    config.zones.push_back(current_zone);
    current_zone = ZoneConfig{};
    has_active_zone = false;
  };

  while (std::getline(file, line)) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
      continue;
    }

    if (trimmed == "lidars:") {
      flush_lidar();
      flush_zone();
      section = Section::kLidars;
      continue;
    }
    if (trimmed == "background:") {
      flush_lidar();
      flush_zone();
      section = Section::kBackground;
      continue;
    }
    if (trimmed == "zones:") {
      flush_lidar();
      flush_zone();
      section = Section::kZones;
      continue;
    }
    if (trimmed == "debug_view:") {
      flush_lidar();
      flush_zone();
      section = Section::kDebugView;
      continue;
    }
    if (trimmed == "tcp:") {
      flush_lidar();
      flush_zone();
      section = Section::kTcp;
      continue;
    }
    if (trimmed == "osc:") {
      flush_lidar();
      flush_zone();
      section = Section::kOsc;
      continue;
    }
    if (trimmed == "roi:") {
      flush_lidar();
      flush_zone();
      section = Section::kRoi;
      continue;
    }

    if (section == Section::kLidars) {
      if (trimmed.rfind("- ", 0) == 0 || trimmed == "-") {
        flush_lidar();
        has_active_lidar = true;
        const std::string rest = Trim(trimmed.substr(1));
        if (rest.empty()) {
          continue;
        }
        std::string key;
        std::string value;
        ParseKeyValue(rest, key, value);
        AssignLidarKey(current_lidar, key, value);
        continue;
      }

      if (!has_active_lidar) {
        throw std::runtime_error("Found lidar key outside of a list item: " + trimmed);
      }

      if (trimmed.rfind("T:", 0) == 0) {
        current_lidar.T = ParseMatrix4(file, trimmed.substr(2));
        continue;
      }

      std::string key;
      std::string value;
      ParseKeyValue(trimmed, key, value);
      AssignLidarKey(current_lidar, key, value);
      continue;
    }

    if (section == Section::kZones) {
      if (trimmed.rfind("- ", 0) == 0 || trimmed == "-") {
        flush_zone();
        has_active_zone = true;
        const std::string rest = Trim(trimmed.substr(1));
        if (rest.empty()) {
          continue;
        }
        std::string key;
        std::string value;
        ParseKeyValue(rest, key, value);
        AssignZoneKey(current_zone, key, value);
        continue;
      }

      if (!has_active_zone) {
        throw std::runtime_error("Found zone key outside of a list item: " + trimmed);
      }

      std::string key;
      std::string value;
      ParseKeyValue(trimmed, key, value);
      AssignZoneKey(current_zone, key, value);
      continue;
    }

    std::string key;
    std::string value;
    ParseKeyValue(trimmed, key, value);

    switch (section) {
      case Section::kRoot:
        AssignRootKey(config, key, value);
        break;
      case Section::kBackground:
        AssignBackgroundKey(config.background, key, value);
        break;
      case Section::kZones:
        break;
      case Section::kDebugView:
        AssignDebugViewKey(config.debug_view, key, value);
        break;
      case Section::kTcp:
        AssignTcpKey(config.tcp, key, value);
        break;
      case Section::kOsc:
        AssignOscKey(config.osc, key, value);
        break;
      case Section::kRoi:
        AssignRoiKey(config.roi, key, value);
        break;
      case Section::kLidars:
        break;
    }
  }

  flush_lidar();
  flush_zone();

  if (config.sdk_config_path.empty()) {
    throw std::runtime_error("Config must define sdk_config");
  }
  if (config.lidars.empty()) {
    throw std::runtime_error("Config must define at least one lidar");
  }
  if (config.tracking.processing_hz <= 0) {
    throw std::runtime_error("processing_hz must be positive");
  }
  if (config.background.capture_seconds <= 0) {
    throw std::runtime_error("background.capture_seconds must be positive");
  }
  if (config.background.min_frame_hits <= 0) {
    throw std::runtime_error("background.min_frame_hits must be positive");
  }
  if (config.background.voxel_leaf <= 0.0f) {
    throw std::runtime_error("background.voxel_leaf must be positive");
  }
  if (config.debug_view.width <= 0 || config.debug_view.height <= 0) {
    throw std::runtime_error("debug_view width/height must be positive");
  }
  if (config.debug_view.x_max <= config.debug_view.x_min ||
      config.debug_view.y_max <= config.debug_view.y_min) {
    throw std::runtime_error("debug_view bounds must be increasing");
  }
  if (config.debug_view.max_points == 0) {
    throw std::runtime_error("debug_view.max_points must be positive");
  }

  return config;
}

std::string summarize_config(const AppConfig& config) {
  std::ostringstream stream;
  stream << "sdk_config=" << config.sdk_config_path
         << " lidars=" << config.lidars.size()
         << " processing_hz=" << config.tracking.processing_hz
         << " background=" << (config.background.enabled ? config.background.file : "off")
         << " zones=" << config.zones.size()
         << " debug_view=" << (config.debug_view.enabled ? config.debug_view.file : "off")
         << " tcp=" << (config.tcp.enabled ? config.tcp.bind_ip + ":" + std::to_string(config.tcp.port) : "off")
         << " osc=" << (config.osc.enabled ? config.osc.target_ip + ":" + std::to_string(config.osc.port) : "off");
  return stream.str();
}
