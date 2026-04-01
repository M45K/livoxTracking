#include "tcp_json_publisher.hpp"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

namespace {

std::string SerializePeople(std::uint64_t timestamp_ms,
                            const std::vector<PersonState>& people,
                            const std::vector<ZoneSnapshot>& zones) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(3);
  stream << "{\"timestamp_ms\":" << timestamp_ms << ",\"people\":[";

  for (std::size_t i = 0; i < people.size(); ++i) {
    const auto& person = people[i];
    if (i != 0) {
      stream << ',';
    }
    stream << "{\"id\":" << person.id
           << ",\"x\":" << person.position.x
           << ",\"y\":" << person.position.y
           << ",\"z\":" << person.position.z
           << ",\"vx\":" << person.velocity.x
           << ",\"vy\":" << person.velocity.y
           << ",\"vz\":" << person.velocity.z
           << ",\"width\":" << person.Width()
           << ",\"depth\":" << person.Depth()
           << ",\"height\":" << person.Height()
           << ",\"points\":" << person.point_count
           << ",\"last_seen_ms\":" << person.last_seen_ms
           << '}';
  }

  stream << "],\"zones\":[";
  for (std::size_t i = 0; i < zones.size(); ++i) {
    const auto& zone = zones[i];
    if (i != 0) {
      stream << ',';
    }
    stream << "{\"name\":\"" << zone.name << "\",\"occupancy\":" << zone.Occupancy() << ",\"ids\":[";
    for (std::size_t j = 0; j < zone.occupant_ids.size(); ++j) {
      if (j != 0) {
        stream << ',';
      }
      stream << zone.occupant_ids[j];
    }
    stream << "]}";
  }
  stream << "]}\n";
  return stream.str();
}

}  // namespace

TcpJsonPublisher::TcpJsonPublisher(TcpOutputConfig config) : config_(std::move(config)) {}

TcpJsonPublisher::~TcpJsonPublisher() {
  Stop();
}

bool TcpJsonPublisher::Start() {
  if (!config_.enabled) {
    return true;
  }
  if (listen_socket_ != kInvalidSocket) {
    return true;
  }

  runtime_acquired_ = AcquireSocketRuntime();
  if (!runtime_acquired_) {
    std::cerr << "TCP publisher failed to initialize socket runtime\n";
    return false;
  }

  listen_socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket_ == kInvalidSocket) {
    std::cerr << "TCP publisher socket() failed: " << LastSocketErrorString() << '\n';
    Stop();
    return false;
  }

  int reuse_addr = 1;
  setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&reuse_addr), sizeof(reuse_addr));

  sockaddr_in bind_address{};
  if (!BuildIpv4Address(config_.bind_ip, config_.port, bind_address)) {
    std::cerr << "TCP publisher invalid bind address " << config_.bind_ip << ':' << config_.port << '\n';
    Stop();
    return false;
  }

  if (::bind(listen_socket_, reinterpret_cast<sockaddr*>(&bind_address), sizeof(bind_address)) != 0) {
    std::cerr << "TCP publisher bind() failed: " << LastSocketErrorString() << '\n';
    Stop();
    return false;
  }

  if (::listen(listen_socket_, 8) != 0) {
    std::cerr << "TCP publisher listen() failed: " << LastSocketErrorString() << '\n';
    Stop();
    return false;
  }

  if (!SetNonBlocking(listen_socket_)) {
    std::cerr << "TCP publisher failed to set listen socket non-blocking\n";
    Stop();
    return false;
  }

  return true;
}

void TcpJsonPublisher::Stop() {
  for (const socket_t client_socket : client_sockets_) {
    CloseSocket(client_socket);
  }
  client_sockets_.clear();

  if (listen_socket_ != kInvalidSocket) {
    CloseSocket(listen_socket_);
    listen_socket_ = kInvalidSocket;
  }

  if (runtime_acquired_) {
    ReleaseSocketRuntime();
    runtime_acquired_ = false;
  }
}

void TcpJsonPublisher::AcceptClients() {
  if (listen_socket_ == kInvalidSocket) {
    return;
  }

  for (;;) {
    sockaddr_in client_address{};
#ifdef _WIN32
    int client_size = sizeof(client_address);
#else
    socklen_t client_size = sizeof(client_address);
#endif
    const socket_t client_socket =
        ::accept(listen_socket_, reinterpret_cast<sockaddr*>(&client_address), &client_size);
    if (client_socket == kInvalidSocket) {
      if (!WouldBlockSocketError()) {
        std::cerr << "TCP publisher accept() failed: " << LastSocketErrorString() << '\n';
      }
      break;
    }

    if (!SetNonBlocking(client_socket)) {
      CloseSocket(client_socket);
      continue;
    }

    client_sockets_.push_back(client_socket);
  }
}

void TcpJsonPublisher::Publish(std::uint64_t timestamp_ms,
                               const std::vector<PersonState>& people,
                               const std::vector<ZoneSnapshot>& zones) {
  if (!config_.enabled || listen_socket_ == kInvalidSocket) {
    return;
  }

  AcceptClients();
  if (client_sockets_.empty()) {
    return;
  }

  const std::string payload = SerializePeople(timestamp_ms, people, zones);
  auto it = client_sockets_.begin();
  while (it != client_sockets_.end()) {
    const auto sent = ::send(*it, payload.data(), static_cast<int>(payload.size()), 0);
    if (sent < 0) {
      if (WouldBlockSocketError()) {
        ++it;
        continue;
      }
      CloseSocket(*it);
      it = client_sockets_.erase(it);
      continue;
    }

    if (static_cast<std::size_t>(sent) != payload.size()) {
      CloseSocket(*it);
      it = client_sockets_.erase(it);
      continue;
    }

    ++it;
  }
}
