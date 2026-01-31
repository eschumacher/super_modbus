/**
 * @file example_serial_transport.cpp
 * @brief Example implementation of ByteTransport for serial port
 *
 * This example shows how to implement the ByteTransport interface for a serial port.
 * This is a template/example - you need to adapt it to your specific serial port library.
 *
 * Common serial port libraries:
 * - Linux: termios (POSIX)
 * - Windows: CreateFile/ReadFile/WriteFile
 * - Cross-platform: boost::asio, libserial, etc.
 */

#include "super_modbus/transport/byte_reader.hpp"
#include "super_modbus/transport/byte_writer.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace supermb {

/**
 * @brief Example serial port transport implementation
 *
 * This is a template showing the interface. You need to implement
 * the actual serial port operations using your preferred library.
 */
class ExampleSerialTransport : public ByteTransport {
 public:
  /**
   * @brief Constructor
   * @param port_name Serial port name (e.g., "/dev/ttyUSB0" on Linux, "COM3" on Windows)
   * @param baud_rate Baud rate (e.g., 9600, 19200, 38400, 115200)
   */
  ExampleSerialTransport(const char* port_name, int baud_rate) {
    // TODO: Open serial port
    // Example (pseudo-code):
    //   serial_fd_ = open(port_name, O_RDWR | O_NOCTTY);
    //   configure_serial_port(serial_fd_, baud_rate);
  }

  ~ExampleSerialTransport() override {
    // TODO: Close serial port
    // Example (pseudo-code):
    //   if (serial_fd_ >= 0) {
    //     close(serial_fd_);
    //   }
  }

  // ByteReader interface
  int Read(std::span<uint8_t> buffer) override {
    // TODO: Read bytes from serial port
    // Example (pseudo-code using POSIX read):
    //   ssize_t bytes_read = read(serial_fd_, buffer.data(), buffer.size());
    //   return (bytes_read > 0) ? static_cast<int>(bytes_read) : 0;

    // For now, return 0 (no data)
    return 0;
  }

  bool HasData() const override {
    // TODO: Check if data is available
    // Example (pseudo-code using select/poll):
    //   fd_set read_fds;
    //   FD_ZERO(&read_fds);
    //   FD_SET(serial_fd_, &read_fds);
    //   struct timeval timeout = {0, 0};
    //   return select(serial_fd_ + 1, &read_fds, nullptr, nullptr, &timeout) > 0;

    return false;
  }

  size_t AvailableBytes() const override {
    // TODO: Return number of bytes available
    // Example (pseudo-code using ioctl FIONREAD on Linux):
    //   int bytes_available = 0;
    //   ioctl(serial_fd_, FIONREAD, &bytes_available);
    //   return static_cast<size_t>(bytes_available);

    return 0;
  }

  // ByteWriter interface
  int Write(std::span<uint8_t const> data) override {
    // TODO: Write bytes to serial port
    // Example (pseudo-code using POSIX write):
    //   ssize_t bytes_written = write(serial_fd_, data.data(), data.size());
    //   return (bytes_written > 0) ? static_cast<int>(bytes_written) : -1;

    return static_cast<int>(data.size());  // Stub: assume all written
  }

  bool Flush() override {
    // TODO: Flush serial port output buffer
    // Example (pseudo-code using tcdrain):
    //   return tcdrain(serial_fd_) == 0;

    return true;  // Stub
  }

 private:
  // TODO: Add your serial port handle/file descriptor
  // Examples:
  //   int serial_fd_;           // POSIX file descriptor
  //   HANDLE serial_handle_;    // Windows handle
  //   boost::asio::serial_port serial_port_;  // boost::asio
};

}  // namespace supermb

/**
 * @brief Example usage
 */
int main() {
  using supermb::ExampleSerialTransport;
  using supermb::RtuMaster;
  using supermb::RtuSlave;

  // Example 1: Using as Master with serial transport
  {
    // Open serial port at 9600 baud
    ExampleSerialTransport serial("/dev/ttyUSB0", 9600);
    RtuMaster master(serial);

    // Read holding registers from slave ID 1
    auto registers = master.ReadHoldingRegisters(1, 0, 10);
    if (registers.has_value()) {
      // Process registers...
    }
  }

  // Example 2: Using as Slave with serial transport
  {
    // Open serial port at 19200 baud
    ExampleSerialTransport serial("/dev/ttyUSB1", 19200);
    RtuSlave slave(1);

    // Configure slave
    slave.AddHoldingRegisters({0, 100});
    slave.AddCoils({0, 100});

    // Poll for incoming requests
    // In a real application, this would be in a loop:
    // while (running) {
    //   if (slave.Poll(serial)) {
    //     // Request processed
    //   }
    //   std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // }
  }

  return 0;
}
