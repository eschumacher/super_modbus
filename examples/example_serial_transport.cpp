/**
 * @file example_serial_transport.cpp
 * @brief Template/skeleton for implementing ByteTransport for serial port
 *
 * This is a STUB showing the interface. For a working implementation, use
 * serial_transport.hpp which provides a complete POSIX termios-based transport.
 *
 * This file is not compiled by default. Adapt this template for:
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
  ExampleSerialTransport(const char *port_name, int baud_rate) {
    (void)port_name;
    (void)baud_rate;
    // open(port_name, O_RDWR | O_NOCTTY); configure termios; store fd_
  }

  ~ExampleSerialTransport() override {
    // if (fd_ >= 0) close(fd_);
  }

  // ByteReader interface
  int Read(std::span<uint8_t> buffer) override {
    (void)buffer;
    // read(fd_, buffer.data(), buffer.size());
    return 0;
  }

  bool HasData() const override {
    // select/poll or ioctl(FIONREAD)
    return false;
  }

  size_t AvailableBytes() const override {
    // ioctl(fd_, FIONREAD, &n) on Linux
    return 0;
  }

  // ByteWriter interface
  int Write(std::span<const uint8_t> data) override {
    // write(fd_, data.data(), data.size());
    return static_cast<int>(data.size());
  }

  bool Flush() override {
    // tcdrain(fd_);
    return true;
  }

 private:
  // int fd_; or HANDLE, or boost::asio::serial_port, etc.
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

    // while (running) { if (slave.Poll(serial)) { ... } sleep(10ms); }
  }

  return 0;
}
