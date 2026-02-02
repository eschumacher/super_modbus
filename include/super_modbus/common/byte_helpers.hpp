#pragma once

#include <cstdint>
#include <cstring>
#include "wire_format_options.hpp"

namespace supermb {

static constexpr uint8_t kMaxByte = 0xFF;
static constexpr uint8_t kBitsPerByte = 8;

static inline constexpr uint8_t GetLowByte(uint16_t value) {
  return value & kMaxByte;
}

static inline constexpr uint8_t GetHighByte(uint16_t value) {
  return (value >> kBitsPerByte) & kMaxByte;
}

/** Big-endian decode: (high_byte << 8) | low_byte; wire order is high byte first. */
static inline constexpr int16_t MakeInt16(uint8_t low_byte, uint8_t high_byte) {
  return static_cast<int16_t>(static_cast<int16_t>(high_byte) << kBitsPerByte | static_cast<int16_t>(low_byte));
}

/**
 * @brief Encode 16-bit value to wire bytes (two bytes at out[0], out[1]).
 * @param value Value to encode
 * @param order Byte order (BigEndian: high byte first; LittleEndian: low byte first)
 * @param out Output buffer (at least 2 bytes)
 */
static inline void EncodeU16(uint16_t value, ByteOrder order, uint8_t *out) {
  if (order == ByteOrder::BigEndian) {
    out[0] = GetHighByte(value);
    out[1] = GetLowByte(value);
  } else {
    out[0] = GetLowByte(value);
    out[1] = GetHighByte(value);
  }
}

/**
 * @brief Decode 16-bit value from wire bytes (first_byte, second_byte in wire order).
 * @param first_byte First byte on wire
 * @param second_byte Second byte on wire
 * @param order Byte order used when encoding
 * @return Decoded uint16_t (cast to int16_t for signed if needed)
 */
static inline constexpr uint16_t DecodeU16(uint8_t first_byte, uint8_t second_byte, ByteOrder order) {
  if (order == ByteOrder::BigEndian) {
    return static_cast<uint16_t>(MakeInt16(second_byte, first_byte)) & 0xFFFFu;
  }
  return static_cast<uint16_t>(MakeInt16(first_byte, second_byte)) & 0xFFFFu;
}

/**
 * @brief Encode 32-bit value to wire bytes (four bytes) using byteOrder per word and wordOrder.
 * @param value Value to encode
 * @param byte_order Byte order for each 16-bit word
 * @param word_order Order of high/low 16-bit words (HighWordFirst: high word then low; LowWordFirst: low then high)
 * @param out Output buffer (at least 4 bytes)
 */
static inline void EncodeU32(uint32_t value, ByteOrder byte_order, WordOrder word_order, uint8_t *out) {
  const uint16_t high_word = static_cast<uint16_t>((value >> 16) & 0xFFFFu);
  const uint16_t low_word = static_cast<uint16_t>(value & 0xFFFFu);
  if (word_order == WordOrder::HighWordFirst) {
    EncodeU16(high_word, byte_order, out);
    EncodeU16(low_word, byte_order, out + 2);
  } else {
    EncodeU16(low_word, byte_order, out);
    EncodeU16(high_word, byte_order, out + 2);
  }
}

/**
 * @brief Decode 32-bit value from wire bytes (4 bytes in wire order).
 * @param in Input buffer (at least 4 bytes)
 * @param byte_order Byte order used for each 16-bit word
 * @param word_order Order of the two 16-bit words on the wire
 * @return Decoded uint32_t
 */
static inline uint32_t DecodeU32(const uint8_t *in, ByteOrder byte_order, WordOrder word_order) {
  const uint16_t first_word = DecodeU16(in[0], in[1], byte_order);
  const uint16_t second_word = DecodeU16(in[2], in[3], byte_order);
  if (word_order == WordOrder::HighWordFirst) {
    return (static_cast<uint32_t>(first_word) << 16) | second_word;
  }
  return (static_cast<uint32_t>(second_word) << 16) | first_word;
}

/**
 * @brief Encode float to wire bytes (four bytes) using byteOrder and wordOrder (same as 32-bit codec).
 * @param value Float value (bits encoded as uint32_t)
 * @param byte_order Byte order for each 16-bit word
 * @param word_order Order of high/low 16-bit words
 * @param out Output buffer (at least 4 bytes)
 */
static inline void EncodeFloat(float value, ByteOrder byte_order, WordOrder word_order, uint8_t *out) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  EncodeU32(bits, byte_order, word_order, out);
}

/**
 * @brief Decode float from wire bytes (4 bytes).
 * @param in Input buffer (at least 4 bytes)
 * @param byte_order Byte order used for each 16-bit word
 * @param word_order Order of the two 16-bit words on the wire
 * @return Decoded float
 */
static inline float DecodeFloat(const uint8_t *in, ByteOrder byte_order, WordOrder word_order) {
  uint32_t bits = DecodeU32(in, byte_order, word_order);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

}  // namespace supermb
