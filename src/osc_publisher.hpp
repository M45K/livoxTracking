#pragma once

#include "config.hpp"
#include "net.hpp"
#include "tracker.hpp"

#include <vector>

class OscPublisher {
 public:
  explicit OscPublisher(OscOutputConfig config);
  ~OscPublisher();

  bool Start();
  void Stop();
  void Publish(std::uint64_t timestamp_ms, const std::vector<PersonState>& people);

 private:
  OscOutputConfig config_;
  socket_t socket_fd_ = kInvalidSocket;
  sockaddr_in target_address_{};
  bool runtime_acquired_ = false;
};
