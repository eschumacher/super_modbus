/**
 * @file tcp_transport.hpp
 * @brief POSIX TCP socket transport implementation for Modbus TCP
 *
 * Provides ByteTransport over a connected TCP socket. Used by the testable
 * TCP slave example. Works on Linux, WSL, and other POSIX-like systems.
 *
 * Usage (server): accept(2) a client, then wrap the client fd:
 *   int client_fd = accept(listen_fd, ...);
 *   TcpConnectionTransport transport(client_fd);
 *   tcp_slave.Poll(transport);
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
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#endif

namespace supermb {

/**
 * @brief ByteTransport over a connected TCP socket (client fd from accept)
 *
 * Takes ownership of the file descriptor; closes it in the destructor.
 */
class TcpConnectionTransport : public ByteTransport {
 public:
  /**
   * @brief Wrap an already-connected socket fd (e.g. from accept(2))
   * @param fd Connected socket file descriptor (ownership taken)
   */
  explicit TcpConnectionTransport(int fd)
      : fd_(fd) {}

  ~TcpConnectionTransport() override { Close(); }

  TcpConnectionTransport(const TcpConnectionTransport &) = delete;
  TcpConnectionTransport &operator=(const TcpConnectionTransport &) = delete;

  TcpConnectionTransport(TcpConnectionTransport &&other) noexcept
      : fd_(other.fd_) {
    other.fd_ = -1;
  }

  TcpConnectionTransport &operator=(TcpConnectionTransport &&other) noexcept {
    if (this != &other) {
      Close();
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  [[nodiscard]] bool IsOpen() const { return fd_ >= 0; }

  // ByteReader interface
  int Read(std::span<uint8_t> buffer) override {
#ifdef __linux__
    if (fd_ < 0) {
      return -1;
    }
    ssize_t n = ::read(fd_, buffer.data(), buffer.size());
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
      }
      Close();
      return -1;
    }
    if (n == 0) {
      Close();  // EOF / client disconnected
    }
    return static_cast<int>(n);
#else
    (void)buffer;
    return -1;
#endif
  }

  [[nodiscard]] bool HasData() const override {
#ifdef __linux__
    if (fd_ < 0) {
      return false;
    }
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    struct timeval tv = {0, 0};
    return ::select(fd_ + 1, &fds, nullptr, nullptr, &tv) > 0;
#else
    return false;
#endif
  }

  [[nodiscard]] size_t AvailableBytes() const override {
#ifdef __linux__
    if (fd_ < 0) {
      return 0;
    }
    int n = 0;
    if (::ioctl(fd_, FIONREAD, &n) == 0 && n > 0) {
      return static_cast<size_t>(n);
    }
#endif
    return 0;
  }

  // ByteWriter interface
  int Write(std::span<const uint8_t> data) override {
#ifdef __linux__
    if (fd_ < 0) {
      return -1;
    }
    ssize_t n = ::write(fd_, data.data(), data.size());
    if (n < 0) {
      return -1;
    }
    return static_cast<int>(n);
#else
    (void)data;
    return -1;
#endif
  }

  bool Flush() override {
#ifdef __linux__
    if (fd_ < 0) {
      return false;
    }
    return true;  // TCP stream has no application-level flush
#else
    return false;
#endif
  }

 private:
  int fd_;

  void Close() {
#ifdef __linux__
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
#endif
  }
};

/**
 * @brief Create a listening TCP socket bound to the given address and port
 * @param bind_address Address to bind (e.g. "0.0.0.0" or "127.0.0.1")
 * @param port Port number (e.g. 502)
 * @return Listening socket fd, or -1 on error
 */
inline int TcpListen(const std::string &bind_address, uint16_t port) {
#ifdef __linux__
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }

  int opt = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
    ::close(fd);
    return -1;
  }

  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (bind_address.empty() || bind_address == "0.0.0.0") {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else if (::inet_pton(AF_INET, bind_address.c_str(), &addr.sin_addr) <= 0) {
    ::close(fd);
    return -1;
  }

  if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
    ::close(fd);
    return -1;
  }

  if (::listen(fd, 1) != 0) {
    ::close(fd);
    return -1;
  }

  return fd;
#else
  (void)bind_address;
  (void)port;
  return -1;
#endif
}

}  // namespace supermb
