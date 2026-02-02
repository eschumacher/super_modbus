#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "byte_reader.hpp"

namespace supermb {

/**
 * @brief Abstract interface for writing bytes to a transport layer
 *
 * This interface allows the Modbus library to work with any byte sink
 * (serial port, TCP socket, memory buffer, etc.) without knowing the details.
 */
class ByteWriter {
 public:
  virtual ~ByteWriter() = default;

  /**
   * @brief Write bytes to the transport layer
   * @param data Bytes to write
   * @return Number of bytes actually written (-1 on error)
   */
  [[nodiscard]] virtual int Write(std::span<const uint8_t> data) = 0;

  /**
   * @brief Flush any buffered data
   * @return true on success, false on error
   */
  [[nodiscard]] virtual bool Flush() = 0;
};

/**
 * @brief Combined interface for bidirectional byte I/O
 */
class ByteTransport : public ByteReader, public ByteWriter {
 public:
  ~ByteTransport() override = default;
};

}  // namespace supermb
