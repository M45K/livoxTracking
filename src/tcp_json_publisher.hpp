#pragma once

#include "config.hpp"
#include "net.hpp"
#include "tracker.hpp"
#include "zones.hpp"

#include <vector>

class TcpJsonPublisher {
 public:
  explicit TcpJsonPublisher(TcpOutputConfig config);
  ~TcpJsonPublisher();

  bool Start();
  void Stop();
  void Publish(std::uint64_t timestamp_ms,
               const std::vector<PersonState>& people,
               const std::vector<ZoneSnapshot>& zones);

 private:
  void AcceptClients();

  TcpOutputConfig config_;
  socket_t listen_socket_ = kInvalidSocket;
  std::vector<socket_t> client_sockets_;
  bool runtime_acquired_ = false;
};
