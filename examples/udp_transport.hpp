/**
 * @file udp_transport.hpp
 * @brief UDP transport for Modbus UDP (MBAP over UDP)
 *
 * Modbus UDP uses the same MBAP/PDU format as Modbus TCP. Use TcpMaster/TcpSlave
 * with a UDP-based ByteTransport. This header provides a reference implementation
 * for UDP client mode.
 *
 * Usage:
 *   UdpClientTransport transport("127.0.0.1", 502);
 *   TcpMaster master(transport);  // Same API as Modbus TCP
 *   auto regs = master.ReadHoldingRegisters(1, 0, 10);
 *
 * Note: Modbus UDP is connectionless; implement timeouts and retries as needed.
 */

#pragma once

#include "super_modbus/transport/byte_reader.hpp"
#include "super_modbus/transport/byte_writer.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#ifdef __linux__
#include <arpa/inet.h>
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#endif

namespace supermb {

/**
 * @brief ByteTransport over UDP (client - sends to and receives from fixed peer)
 *
 * For Modbus UDP: Use with TcpMaster. Same MBAP frame format as Modbus TCP.
 */
class UdpClientTransport : public ByteTransport {
 public:
  UdpClientTransport(const std::string &host, uint16_t port) {
#ifdef __linux__
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ >= 0) {
      peer_.sin_family = AF_INET;
      peer_.sin_port = htons(port);
      ::inet_pton(AF_INET, host.c_str(), &peer_.sin_addr);
    }
#endif
  }

  ~UdpClientTransport() override {
#ifdef __linux__
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
#endif
  }

  int Read(std::span<uint8_t> buffer) override {
#ifdef __linux__
    if (fd_ < 0) {
      return -1;
    }
    socklen_t len = sizeof(peer_);
    ssize_t n = ::recvfrom(fd_, buffer.data(), buffer.size(), 0, reinterpret_cast<struct sockaddr *>(&peer_), &len);
    return (n < 0) ? -1 : static_cast<int>(n);
#else
    (void)buffer;
    return -1;
#endif
  }

  int Write(std::span<const uint8_t> data) override {
#ifdef __linux__
    if (fd_ < 0) {
      return -1;
    }
    ssize_t n =
        ::sendto(fd_, data.data(), data.size(), 0, reinterpret_cast<const struct sockaddr *>(&peer_), sizeof(peer_));
    return (n < 0) ? -1 : static_cast<int>(n);
#else
    (void)data;
    return -1;
#endif
  }

  bool Flush() override { return true; }

  bool HasData() const override {
#ifdef __linux__
    if (fd_ < 0) return false;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    struct timeval tv = {0, 0};
    return select(fd_ + 1, &fds, nullptr, nullptr, &tv) > 0;
#else
    return false;
#endif
  }

  size_t AvailableBytes() const override {
    return HasData() ? 512U : 0U;  // UDP: typically one datagram
  }

 private:
  int fd_{-1};
#ifdef __linux__
  struct sockaddr_in peer_ {};
#endif
};

}  // namespace supermb
