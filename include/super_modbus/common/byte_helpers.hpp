#pragma once

#include <cstdint>

namespace supermb {

static constexpr uint8_t kMaxByte = 0xFF;
static constexpr uint8_t kBitsPerByte = 8;

static inline constexpr uint8_t GetLowByte(uint16_t value) {
  return value & kMaxByte;
}

static inline constexpr uint8_t GetHighByte(uint16_t value) {
  return (value >> kBitsPerByte) & kMaxByte;
}

// TODO: account for endianness? (if not already handled in the transport implementation)
static inline constexpr int16_t MakeInt16(uint8_t low_byte, uint8_t high_byte) {
  return static_cast<int16_t>(static_cast<int16_t>(high_byte) << kBitsPerByte | static_cast<int16_t>(low_byte));
}

}  // namespace supermb
