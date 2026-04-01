#include "net.hpp"

#include <mutex>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

std::mutex g_socket_mutex;
int g_socket_runtime_refcount = 0;

int LastSocketError() {
#ifdef _WIN32
  return WSAGetLastError();
#else
  return errno;
#endif
}

}  // namespace

bool AcquireSocketRuntime() {
  std::lock_guard<std::mutex> lock(g_socket_mutex);
  if (g_socket_runtime_refcount++ > 0) {
    return true;
  }

#ifdef _WIN32
  WSADATA wsa_data{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    g_socket_runtime_refcount = 0;
    return false;
  }
#endif

  return true;
}

void ReleaseSocketRuntime() {
  std::lock_guard<std::mutex> lock(g_socket_mutex);
  if (g_socket_runtime_refcount <= 0) {
    g_socket_runtime_refcount = 0;
    return;
  }
  if (--g_socket_runtime_refcount > 0) {
    return;
  }

#ifdef _WIN32
  WSACleanup();
#endif
}

void CloseSocket(socket_t socket_fd) {
  if (socket_fd == kInvalidSocket) {
    return;
  }

#ifdef _WIN32
  closesocket(socket_fd);
#else
  close(socket_fd);
#endif
}

bool SetNonBlocking(socket_t socket_fd) {
#ifdef _WIN32
  u_long enabled = 1;
  return ioctlsocket(socket_fd, FIONBIO, &enabled) == 0;
#else
  const int flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool BuildIpv4Address(const std::string& ip, std::uint16_t port, sockaddr_in& out_address) {
  out_address = {};
  out_address.sin_family = AF_INET;
  out_address.sin_port = htons(port);
  return inet_pton(AF_INET, ip.c_str(), &out_address.sin_addr) == 1;
}

bool WouldBlockSocketError() {
#ifdef _WIN32
  const int error = LastSocketError();
  return error == WSAEWOULDBLOCK || error == WSAEINTR;
#else
  const int error = LastSocketError();
  return error == EWOULDBLOCK || error == EAGAIN || error == EINTR;
#endif
}

std::string LastSocketErrorString() {
  return std::to_string(LastSocketError());
}
