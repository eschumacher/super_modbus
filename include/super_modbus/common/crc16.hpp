#pragma once

#include <cstdint>
#include <span>

namespace supermb {

/**
 * @brief Calculate CRC-16 for Modbus RTU frames
 *
 * Modbus RTU uses CRC-16 with polynomial 0x8005 (reversed: 0xA001)
 * Initial value: 0xFFFF
 *
 * @param data Data to calculate CRC for
 * @return 16-bit CRC value (little-endian order for Modbus)
 */
[[nodiscard]] inline uint16_t CalculateCrc16(std::span<uint8_t const> data) {
  constexpr uint16_t kPolynomial = 0xA001;  // Reversed polynomial
  constexpr uint16_t kInitialValue = 0xFFFF;

  uint16_t crc = kInitialValue;

  for (uint8_t byte : data) {
    crc ^= static_cast<uint16_t>(byte);

    for (int i = 0; i < 8; ++i) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ kPolynomial;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

/**
 * @brief Verify CRC-16 of a Modbus RTU frame
 *
 * @param frame Complete frame including CRC bytes (last 2 bytes)
 * @return true if CRC is valid, false otherwise
 */
[[nodiscard]] inline bool VerifyCrc16(std::span<uint8_t const> frame) {
  if (frame.size() < 2) {
    return false;
  }

  // Calculate CRC for all bytes except the last 2 (which are the CRC)
  auto data_span = frame.subspan(0, frame.size() - 2);
  uint16_t calculated_crc = CalculateCrc16(data_span);

  // Extract CRC from frame (little-endian: low byte first, then high byte)
  uint16_t frame_crc =
      static_cast<uint16_t>(frame[frame.size() - 2]) | (static_cast<uint16_t>(frame[frame.size() - 1]) << 8);

  return calculated_crc == frame_crc;
}

}  // namespace supermb
