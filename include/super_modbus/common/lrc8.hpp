#pragma once

#include <cstdint>
#include <span>

namespace supermb {

/**
 * @brief Calculate LRC-8 (Longitudinal Redundancy Check) for Modbus ASCII frames
 *
 * Modbus ASCII uses LRC instead of CRC-16. The LRC is calculated as:
 * LRC = 256 - (sum of all data bytes) & 0xFF
 *
 * @param data Data to calculate LRC for (slave_id, function_code, data)
 * @return 8-bit LRC value
 */
[[nodiscard]] inline uint8_t CalculateLrc8(std::span<const uint8_t> data) {
  uint16_t sum = 0;
  for (uint8_t byte : data) {
    sum += byte;
  }
  return static_cast<uint8_t>((256 - (sum & 0xFF)) & 0xFF);
}

/**
 * @brief Verify LRC-8 of a Modbus ASCII PDU (data including LRC byte)
 *
 * The last byte of the data is the LRC. Verification: sum of all bytes (including LRC) should be 0.
 *
 * @param pdu PDU including LRC as last byte
 * @return true if LRC is valid, false otherwise
 */
[[nodiscard]] inline bool VerifyLrc8(std::span<const uint8_t> pdu) {
  if (pdu.empty()) {
    return false;
  }
  uint16_t sum = 0;
  for (uint8_t byte : pdu) {
    sum += byte;
  }
  return (sum & 0xFF) == 0;
}

}  // namespace supermb
