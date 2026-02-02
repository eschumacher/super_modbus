#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "../common/exception_code.hpp"
#include "../common/function_code.hpp"
#include "../common/wire_format_options.hpp"
#include "tcp_request.hpp"
#include "tcp_response.hpp"

namespace supermb {

/**
 * @brief TCP frame encoder/decoder
 *
 * Handles conversion between byte frames and Modbus TCP request/response objects.
 * Modbus TCP uses MBAP (Modbus Application Protocol) header:
 * - Transaction ID (2 bytes)
 * - Protocol ID (2 bytes, always 0x0000)
 * - Length (2 bytes) - number of bytes following (Unit ID + PDU)
 * - Unit ID (1 byte) - similar to slave ID in RTU
 * - PDU (Protocol Data Unit) - function code + data
 */
class TcpFrame {
 public:
  /**
   * @brief Encode a request into a TCP frame with MBAP header
   * @param request The Modbus TCP request to encode
   * @return Byte vector containing the complete TCP frame (MBAP header + PDU)
   */
  [[nodiscard]] static std::vector<uint8_t> EncodeRequest(const TcpRequest &request);

  /**
   * @brief Encode a response into a TCP frame with MBAP header
   * @param response The Modbus TCP response to encode
   * @return Byte vector containing the complete TCP frame (MBAP header + PDU)
   */
  [[nodiscard]] static std::vector<uint8_t> EncodeResponse(const TcpResponse &response);

  /**
   * @brief Decode a TCP frame into a request
   * @param frame Complete TCP frame including MBAP header
   * @param byte_order Byte order used for 16-bit values in PDU (default: BigEndian)
   * @return Parsed request if frame is valid, empty optional otherwise
   */
  [[nodiscard]] static std::optional<TcpRequest> DecodeRequest(std::span<const uint8_t> frame,
                                                               ByteOrder byte_order = ByteOrder::BigEndian);

  /**
   * @brief Decode a TCP frame into a response
   * @param frame Complete TCP frame including MBAP header
   * @return Parsed response if frame is valid, empty optional otherwise
   */
  [[nodiscard]] static std::optional<TcpResponse> DecodeResponse(std::span<const uint8_t> frame);

  /**
   * @brief Get the minimum frame size for a given function code
   * @param function_code The function code
   * @return Minimum frame size in bytes (including MBAP header)
   */
  [[nodiscard]] static size_t GetMinFrameSize(FunctionCode function_code);

  /**
   * @brief Check if a request frame appears complete
   * @param frame Partial or complete frame
   * @return true if frame has minimum size for a request, false otherwise
   */
  [[nodiscard]] static bool IsRequestFrameComplete(std::span<const uint8_t> frame);

  /**
   * @brief Check if a response frame appears complete
   * @param frame Partial or complete frame
   * @return true if frame has minimum size for a response, false otherwise
   */
  [[nodiscard]] static bool IsResponseFrameComplete(std::span<const uint8_t> frame);

 private:
  static constexpr size_t kMbapHeaderSize = 7;  // Transaction ID(2) + Protocol ID(2) + Length(2) + Unit ID(1)
  static constexpr size_t kMinPduSize = 1;      // function_code (1)
  static constexpr size_t kMinFrameSize = kMbapHeaderSize + kMinPduSize;  // MBAP + function_code
  static constexpr size_t kExceptionResponsePduSize = 2;                  // function_code + exception_code
  static constexpr size_t kWriteSinglePduSize = 5;                        // function_code + address(2) + value(2)
  static constexpr uint16_t kProtocolId = 0x0000;                         // Modbus protocol ID

  /**
   * @brief Extract transaction ID from MBAP header (big-endian)
   */
  [[nodiscard]] static uint16_t ExtractTransactionId(std::span<const uint8_t> frame);

  /**
   * @brief Extract length from MBAP header (big-endian)
   */
  [[nodiscard]] static uint16_t ExtractLength(std::span<const uint8_t> frame);

  /**
   * @brief Extract unit ID from MBAP header
   */
  [[nodiscard]] static uint8_t ExtractUnitId(std::span<const uint8_t> frame);

  /**
   * @brief Write transaction ID to MBAP header (big-endian)
   */
  static void WriteTransactionId(std::vector<uint8_t> &frame, uint16_t transaction_id);

  /**
   * @brief Write protocol ID to MBAP header (big-endian, always 0x0000)
   */
  static void WriteProtocolId(std::vector<uint8_t> &frame);

  /**
   * @brief Write unit ID to MBAP header
   */
  static void WriteUnitId(std::vector<uint8_t> &frame, uint8_t unit_id);
};

}  // namespace supermb
