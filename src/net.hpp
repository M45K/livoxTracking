#pragma once

#include <cstdint>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
#include <netinet/in.h>
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
#endif

bool AcquireSocketRuntime();
void ReleaseSocketRuntime();

void CloseSocket(socket_t socket_fd);
bool SetNonBlocking(socket_t socket_fd);
bool BuildIpv4Address(const std::string& ip, std::uint16_t port, sockaddr_in& out_address);
bool WouldBlockSocketError();
std::string LastSocketErrorString();
