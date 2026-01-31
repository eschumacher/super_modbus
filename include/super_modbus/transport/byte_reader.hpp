#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace supermb {

/**
 * @brief Abstract interface for reading bytes from a transport layer
 *
 * This interface allows the Modbus library to work with any byte source
 * (serial port, TCP socket, memory buffer, etc.) without knowing the details.
 */
class ByteReader {
 public:
  virtual ~ByteReader() = default;

  /**
   * @brief Read bytes from the transport layer
   * @param buffer Buffer to store read bytes
   * @param max_bytes Maximum number of bytes to read
   * @return Number of bytes actually read (0 if no data available, -1 on error)
   */
  [[nodiscard]] virtual int Read(std::span<uint8_t> buffer) = 0;

  /**
   * @brief Check if data is available to read
   * @return true if data is available, false otherwise
   */
  [[nodiscard]] virtual bool HasData() const = 0;

  /**
   * @brief Get the number of bytes available to read
   * @return Number of bytes available (0 if unknown/unavailable)
   */
  [[nodiscard]] virtual size_t AvailableBytes() const = 0;
};

}  // namespace supermb
