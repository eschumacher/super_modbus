#pragma once

#include <cstdint>

namespace supermb {

static constexpr uint8_t kMaxByte = 0xFF;
static constexpr uint8_t kBitsPerByte = 8;

static inline uint8_t GetLowByte(uint16_t value) {
  return value & kMaxByte;
}

static inline uint8_t GetHighByte(uint16_t value) {
  return (value >> kBitsPerByte) & kMaxByte;
}

}  // namespace supermb
