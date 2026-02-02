#pragma once

#include <cstdint>
#include <optional>
#include <utility>

namespace supermb {

/**
 * @brief Byte order for 16-bit values on the wire (per-register).
 */
enum class ByteOrder {
  /** High byte first (standard Modbus, big-endian) */
  BigEndian,
  /** Low byte first (e.g. Enron Modbus, little-endian) */
  LittleEndian
};

/**
 * @brief Word order for 32-bit values (two consecutive 16-bit registers).
 */
enum class WordOrder {
  /** High 16-bit word first (register N = high word, N+1 = low word) */
  HighWordFirst,
  /** Low 16-bit word first (register N = low word, N+1 = high word) */
  LowWordFirst
};

/**
 * @brief Meaning of "count" in float read/write API.
 */
enum class FloatCountSemantics {
  /** Count = number of 32-bit floats; wire quantity = 2*count registers */
  CountIsFloatCount,
  /** Count = number of 16-bit registers; return count/2 floats */
  CountIsRegisterCount
};

/**
 * @brief Optional float range for validation and/or slave float storage (start_register, register_count).
 */
using FloatRange = std::pair<uint16_t, uint16_t>;

/**
 * @brief Wire format options for Modbus (byte/word order and optional float semantics).
 */
struct WireFormatOptions {
  ByteOrder byte_order{ByteOrder::BigEndian};
  WordOrder word_order{WordOrder::HighWordFirst};
  /** When using ReadFloats/WriteFloats: meaning of count (default CountIsFloatCount for Enron-style). */
  std::optional<FloatCountSemantics> float_count_semantics{};
  /** Optional: only allow float read/write in [start_register, start_register + register_count). */
  std::optional<FloatRange> float_range{};
};

}  // namespace supermb
