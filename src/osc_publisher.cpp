#include "osc_publisher.hpp"

#include <cstring>
#include <iostream>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace {

void AppendOscString(std::vector<std::uint8_t>& buffer, const std::string& value) {
  buffer.insert(buffer.end(), value.begin(), value.end());
  buffer.push_back('\0');
  while (buffer.size() % 4 != 0) {
    buffer.push_back('\0');
  }
}

void AppendOscInt(std::vector<std::uint8_t>& buffer, std::int32_t value) {
  const std::uint32_t network_value = htonl(static_cast<std::uint32_t>(value));
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&network_value);
  buffer.insert(buffer.end(), bytes, bytes + sizeof(network_value));
}

void AppendOscFloat(std::vector<std::uint8_t>& buffer, float value) {
  std::uint32_t raw = 0;
  static_assert(sizeof(raw) == sizeof(value), "Unexpected float size");
  std::memcpy(&raw, &value, sizeof(raw));
  raw = htonl(raw);
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&raw);
  buffer.insert(buffer.end(), bytes, bytes + sizeof(raw));
}

std::vector<std::uint8_t> BuildPersonMessage(const std::string& address,
                                             std::uint64_t timestamp_ms,
                                             const PersonState& person) {
  std::vector<std::uint8_t> message;
  message.reserve(64);
  AppendOscString(message, address);
  AppendOscString(message, ",ifffffiii");
  AppendOscInt(message, static_cast<std::int32_t>(person.id));
  AppendOscFloat(message, person.position.x);
  AppendOscFloat(message, person.position.y);
  AppendOscFloat(message, person.position.z);
  AppendOscFloat(message, person.velocity.x);
  AppendOscFloat(message, person.velocity.y);
  AppendOscInt(message, static_cast<std::int32_t>(person.point_count));
  AppendOscInt(message, static_cast<std::int32_t>(timestamp_ms & 0x7fffffff));
  AppendOscInt(message, static_cast<std::int32_t>(person.last_seen_ms & 0x7fffffff));
  return message;
}

}  // namespace

OscPublisher::OscPublisher(OscOutputConfig config) : config_(std::move(config)) {}

OscPublisher::~OscPublisher() {
  Stop();
}

bool OscPublisher::Start() {
  if (!config_.enabled) {
    return true;
  }
  if (socket_fd_ != kInvalidSocket) {
    return true;
  }

  runtime_acquired_ = AcquireSocketRuntime();
  if (!runtime_acquired_) {
    std::cerr << "OSC publisher failed to initialize socket runtime\n";
    return false;
  }

  socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_fd_ == kInvalidSocket) {
    std::cerr << "OSC publisher socket() failed: " << LastSocketErrorString() << '\n';
    Stop();
    return false;
  }

  if (!BuildIpv4Address(config_.target_ip, config_.port, target_address_)) {
    std::cerr << "OSC publisher invalid target " << config_.target_ip << ':' << config_.port << '\n';
    Stop();
    return false;
  }

  return true;
}

void OscPublisher::Stop() {
  if (socket_fd_ != kInvalidSocket) {
    CloseSocket(socket_fd_);
    socket_fd_ = kInvalidSocket;
  }

  if (runtime_acquired_) {
    ReleaseSocketRuntime();
    runtime_acquired_ = false;
  }
}

void OscPublisher::Publish(std::uint64_t timestamp_ms, const std::vector<PersonState>& people) {
  if (!config_.enabled || socket_fd_ == kInvalidSocket) {
    return;
  }

  for (const auto& person : people) {
    const auto message = BuildPersonMessage(config_.address, timestamp_ms, person);
    const auto sent = ::sendto(socket_fd_,
                               reinterpret_cast<const char*>(message.data()),
                               static_cast<int>(message.size()),
                               0,
                               reinterpret_cast<const sockaddr*>(&target_address_),
                               sizeof(target_address_));
    if (sent < 0) {
      std::cerr << "OSC publisher sendto() failed: " << LastSocketErrorString() << '\n';
      return;
    }
  }
}
