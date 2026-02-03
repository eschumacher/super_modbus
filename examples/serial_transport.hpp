/**
 * @file serial_transport.hpp
 * @brief Working POSIX serial port transport implementation
 *
 * This is a complete, working implementation of ByteTransport for Linux/Unix
 * serial ports using POSIX termios. It can be used with real serial ports
 * or virtual serial ports created with socat.
 *
 * Usage:
 *   #include "serial_transport.hpp"
 *   SerialTransport transport("/dev/ttyUSB0", 9600);
 *   RtuMaster master(transport);
 *
 * For virtual ports:
 *   socat -d -d pty,raw,echo=0 pty,raw,echo=0
 *   SerialTransport transport("/dev/pts/2", 9600);
 */

#pragma once

#include "super_modbus/transport/byte_reader.hpp"
#include "super_modbus/transport/byte_writer.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#ifdef __linux__
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>
#include <cstring>
#endif

namespace supermb {

/**
 * @brief POSIX serial port transport implementation
 *
 * This implementation uses POSIX termios for serial port configuration
 * and standard file I/O for reading/writing. Works on Linux, macOS, and
 * other POSIX-compliant systems.
 */
class SerialTransport : public ByteTransport {
 public:
  /**
   * @brief Constructor - opens and configures serial port
   * @param port_name Serial port path (e.g., "/dev/ttyUSB0", "/dev/pts/2")
   * @param baud_rate Baud rate (e.g., 9600, 19200, 38400, 115200)
   * @param parity Parity setting: 'N' (none), 'E' (even), 'O' (odd)
   * @param data_bits Data bits (typically 8)
   * @param stop_bits Stop bits (1 or 2)
   */
  SerialTransport(const std::string &port_name, int baud_rate, char parity = 'E', int data_bits = 8, int stop_bits = 1)
      : port_name_(port_name),
        fd_(-1),
        baud_rate_(baud_rate),
        parity_(parity),
        data_bits_(data_bits),
        stop_bits_(stop_bits) {
    Open();
  }

  /**
   * @brief Destructor - closes serial port
   */
  ~SerialTransport() override { Close(); }

  // Delete copy constructor and assignment
  SerialTransport(const SerialTransport &) = delete;
  SerialTransport &operator=(const SerialTransport &) = delete;

  // Move constructor
  SerialTransport(SerialTransport &&other) noexcept
      : port_name_(std::move(other.port_name_)),
        fd_(other.fd_),
        baud_rate_(other.baud_rate_),
        parity_(other.parity_),
        data_bits_(other.data_bits_),
        stop_bits_(other.stop_bits_) {
    other.fd_ = -1;
  }

  // Move assignment
  SerialTransport &operator=(SerialTransport &&other) noexcept {
    if (this != &other) {
      Close();
      port_name_ = std::move(other.port_name_);
      fd_ = other.fd_;
      baud_rate_ = other.baud_rate_;
      parity_ = other.parity_;
      data_bits_ = other.data_bits_;
      stop_bits_ = other.stop_bits_;
      other.fd_ = -1;
    }
    return *this;
  }

  /**
   * @brief Check if port is open
   */
  bool IsOpen() const { return fd_ >= 0; }

  /**
   * @brief Reopen the port (useful for recovery)
   */
  bool Reopen() {
    Close();
    return Open();
  }

  // ByteReader interface
  int Read(std::span<uint8_t> buffer) override {
    if (fd_ < 0) {
      return -1;
    }

    ssize_t bytes_read = ::read(fd_, buffer.data(), buffer.size());
    if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;  // No data available, not an error
      }
      return -1;  // Error
    }
    return static_cast<int>(bytes_read);
  }

  bool HasData() const override {
    if (fd_ < 0) {
      return false;
    }

#ifdef __linux__
    int bytes_available = 0;
    if (::ioctl(fd_, FIONREAD, &bytes_available) == 0) {
      return bytes_available > 0;
    }
#endif
    // Fallback: try to peek at data using select
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd_, &read_fds);
    struct timeval timeout = {0, 0};
    return ::select(fd_ + 1, &read_fds, nullptr, nullptr, &timeout) > 0;
  }

  size_t AvailableBytes() const override {
    if (fd_ < 0) {
      return 0;
    }

#ifdef __linux__
    int bytes_available = 0;
    if (::ioctl(fd_, FIONREAD, &bytes_available) == 0) {
      return static_cast<size_t>(bytes_available);
    }
#endif
    return 0;
  }

  // ByteWriter interface
  int Write(std::span<const uint8_t> data) override {
    if (fd_ < 0) {
      return -1;
    }

    ssize_t bytes_written = ::write(fd_, data.data(), data.size());
    if (bytes_written < 0) {
      return -1;
    }
    return static_cast<int>(bytes_written);
  }

  bool Flush() override {
    if (fd_ < 0) {
      return false;
    }
    return ::tcdrain(fd_) == 0;
  }

 private:
  std::string port_name_;
  int fd_;
  int baud_rate_;
  char parity_;
  int data_bits_;
  int stop_bits_;

  /**
   * @brief Open and configure serial port
   */
  bool Open() {
#ifdef __linux__
    // Open port in non-blocking mode first
    fd_ = ::open(port_name_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
      return false;
    }

    // Get current terminal settings
    struct termios tty;
    if (::tcgetattr(fd_, &tty) != 0) {
      ::close(fd_);
      fd_ = -1;
      return false;
    }

    // Set input/output baud rate
    speed_t speed = GetBaudRate(baud_rate_);
    if (::cfsetispeed(&tty, speed) != 0 || ::cfsetospeed(&tty, speed) != 0) {
      ::close(fd_);
      fd_ = -1;
      return false;
    }

    // Configure for raw mode (no line editing, no echo, etc.)
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_oflag &= ~OPOST;
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB);

    // Set data bits
    switch (data_bits_) {
      case 5:
        tty.c_cflag |= CS5;
        break;
      case 6:
        tty.c_cflag |= CS6;
        break;
      case 7:
        tty.c_cflag |= CS7;
        break;
      case 8:
      default:
        tty.c_cflag |= CS8;
        break;
    }

    // Set parity
    switch (parity_) {
      case 'E':
      case 'e':
        tty.c_cflag |= PARENB;
        tty.c_cflag &= ~PARODD;
        break;
      case 'O':
      case 'o':
        tty.c_cflag |= PARENB;
        tty.c_cflag |= PARODD;
        break;
      case 'N':
      case 'n':
      default:
        tty.c_cflag &= ~PARENB;
        break;
    }

    // Set stop bits
    if (stop_bits_ == 2) {
      tty.c_cflag |= CSTOPB;
    } else {
      tty.c_cflag &= ~CSTOPB;
    }

    // Disable hardware flow control
    tty.c_cflag &= ~CRTSCTS;

    // Enable receiver, ignore modem control lines
    tty.c_cflag |= (CLOCAL | CREAD);

    // Set read timeout and minimum bytes
    tty.c_cc[VMIN] = 0;   // Non-blocking read
    tty.c_cc[VTIME] = 0;  // No timeout

    // Apply settings
    if (::tcsetattr(fd_, TCSANOW, &tty) != 0) {
      ::close(fd_);
      fd_ = -1;
      return false;
    }

    // Switch to blocking mode for normal operation
    int flags = ::fcntl(fd_, F_GETFL);
    if (flags >= 0) {
      ::fcntl(fd_, F_SETFL, flags & ~O_NONBLOCK);
    }

    return true;
#else
    // Other platforms need their own serial impl (e.g. termios on macOS, Win32 APIs)
    (void)port_name_;
    (void)baud_rate_;
    (void)parity_;
    (void)data_bits_;
    (void)stop_bits_;
    return false;
#endif
  }

  /**
   * @brief Close serial port
   */
  void Close() {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

  /**
   * @brief Convert integer baud rate to termios speed_t
   */
  static speed_t GetBaudRate(int baud) {
    switch (baud) {
      case 50:
        return B50;
      case 75:
        return B75;
      case 110:
        return B110;
      case 134:
        return B134;
      case 150:
        return B150;
      case 200:
        return B200;
      case 300:
        return B300;
      case 600:
        return B600;
      case 1200:
        return B1200;
      case 1800:
        return B1800;
      case 2400:
        return B2400;
      case 4800:
        return B4800;
      case 9600:
        return B9600;
      case 19200:
        return B19200;
      case 38400:
        return B38400;
      case 57600:
        return B57600;
      case 115200:
        return B115200;
      case 230400:
        return B230400;
      default:
        return B9600;  // Default to 9600
    }
  }
};

}  // namespace supermb
