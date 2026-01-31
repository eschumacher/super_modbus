#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "../common/exception_code.hpp"
#include "../common/function_code.hpp"
#include "rtu_request.hpp"
#include "rtu_response.hpp"

namespace supermb {

/**
 * @brief RTU frame encoder/decoder
 *
 * Handles conversion between byte frames and Modbus RTU request/response objects.
 * Includes CRC calculation and verification.
 */
class RtuFrame {
 public:
  /**
   * @brief Encode a request into an RTU frame with CRC
   * @param request The Modbus request to encode
   * @return Byte vector containing the complete RTU frame (slave_id, function_code, data, CRC)
   */
  [[nodiscard]] static std::vector<uint8_t> EncodeRequest(RtuRequest const &request);

  /**
   * @brief Encode a response into an RTU frame with CRC
   * @param response The Modbus response to encode
   * @return Byte vector containing the complete RTU frame (slave_id, function_code, data, CRC)
   */
  [[nodiscard]] static std::vector<uint8_t> EncodeResponse(RtuResponse const &response);

  /**
   * @brief Decode an RTU frame into a request
   * @param frame Complete RTU frame including CRC
   * @return Parsed request if frame is valid, empty optional otherwise
   */
  [[nodiscard]] static std::optional<RtuRequest> DecodeRequest(std::span<uint8_t const> frame);

  /**
   * @brief Decode an RTU frame into a response
   * @param frame Complete RTU frame including CRC
   * @return Parsed response if frame is valid, empty optional otherwise
   */
  [[nodiscard]] static std::optional<RtuResponse> DecodeResponse(std::span<uint8_t const> frame);

  /**
   * @brief Get the minimum frame size for a given function code
   * @param function_code The function code
   * @return Minimum frame size in bytes (including CRC)
   */
  [[nodiscard]] static size_t GetMinFrameSize(FunctionCode function_code);

  /**
   * @brief Check if a request frame appears complete
   * @param frame Partial or complete frame
   * @return true if frame has minimum size for a request, false otherwise
   */
  [[nodiscard]] static bool IsRequestFrameComplete(std::span<uint8_t const> frame);

  /**
   * @brief Check if a response frame appears complete
   * @param frame Partial or complete frame
   * @return true if frame has minimum size for a response, false otherwise
   */
  [[nodiscard]] static bool IsResponseFrameComplete(std::span<uint8_t const> frame);

  /**
   * @brief Check if a frame appears complete (has minimum size)
   * @deprecated Use IsRequestFrameComplete or IsResponseFrameComplete instead
   * @param frame Partial or complete frame
   * @return true if frame has minimum size, false otherwise
   */
  [[nodiscard]] static bool IsFrameComplete(std::span<uint8_t const> frame);

 private:
  static constexpr size_t kMinFrameSize = 4;  // slave_id + function_code + CRC (2 bytes)
  static constexpr size_t kCrcSize = 2;
};

}  // namespace supermb
