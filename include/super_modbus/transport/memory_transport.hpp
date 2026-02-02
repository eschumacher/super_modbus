#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "byte_reader.hpp"
#include "byte_writer.hpp"

namespace supermb {

/**
 * @brief Memory-based transport implementation for testing and simple use cases
 *
 * This implementation uses in-memory buffers for reading and writing bytes.
 * Useful for testing without actual hardware.
 */
class MemoryTransport : public ByteTransport {
 public:
  static constexpr size_t kDefaultInitialCapacity = 256;
  explicit MemoryTransport(size_t initial_capacity = kDefaultInitialCapacity)
      : read_buffer_(initial_capacity) {}

  // ByteReader interface
  [[nodiscard]] int Read(std::span<uint8_t> buffer) override {
    if (read_pos_ >= read_buffer_.size()) {
      return 0;  // No data available
    }

    size_t bytes_to_read = std::min(buffer.size(), read_buffer_.size() - read_pos_);
    std::copy_n(read_buffer_.begin() + read_pos_, bytes_to_read, buffer.begin());
    read_pos_ += bytes_to_read;
    return static_cast<int>(bytes_to_read);
  }

  [[nodiscard]] bool HasData() const override { return read_pos_ < read_buffer_.size(); }

  [[nodiscard]] size_t AvailableBytes() const override { return read_buffer_.size() - read_pos_; }

  // ByteWriter interface
  [[nodiscard]] int Write(std::span<const uint8_t> data) override {
    write_buffer_.insert(write_buffer_.end(), data.begin(), data.end());
    return static_cast<int>(data.size());
  }

  [[nodiscard]] bool Flush() override { return true; }

  // MemoryTransport-specific methods
  /**
   * @brief Set the data that will be read by Read()
   */
  void SetReadData(std::span<const uint8_t> data) {
    read_buffer_.assign(data.begin(), data.end());
    read_pos_ = 0;
  }

  /**
   * @brief Get the data that was written via Write()
   */
  [[nodiscard]] std::span<const uint8_t> GetWrittenData() const { return {write_buffer_.data(), write_buffer_.size()}; }

  /**
   * @brief Clear the write buffer
   */
  void ClearWriteBuffer() { write_buffer_.clear(); }

  /**
   * @brief Reset read position to beginning
   */
  void ResetReadPosition() { read_pos_ = 0; }

 private:
  std::vector<uint8_t> read_buffer_;
  size_t read_pos_{0};
  std::vector<uint8_t> write_buffer_;
};

}  // namespace supermb
