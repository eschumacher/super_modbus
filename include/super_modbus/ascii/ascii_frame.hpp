#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "../common/exception_code.hpp"
#include "../common/function_code.hpp"
#include "../common/wire_format_options.hpp"
#include "../rtu/rtu_request.hpp"
#include "../rtu/rtu_response.hpp"

namespace supermb {

/**
 * @brief Modbus ASCII frame encoder/decoder
 *
 * Modbus ASCII uses human-readable hex encoding. Each byte is transmitted as two ASCII hex
 * characters. Frames start with ':' (0x3A), end with CRLF (0x0D 0x0A), and use LRC for
 * error checking instead of CRC-16.
 *
 * The PDU (slave_id, function_code, data) is identical to Modbus RTU; only the framing differs.
 */
class AsciiFrame {
 public:
  static constexpr char kStartByte = ':';
  static constexpr char kCr = '\r';
  static constexpr char kLf = '\n';

  /**
   * @brief Encode a request into an ASCII frame
   * @param request The Modbus request to encode (same PDU as RTU)
   * @return ASCII string: ":<hex chars><LRC>\\r\\n"
   */
  [[nodiscard]] static std::string EncodeRequest(const RtuRequest &request);

  /**
   * @brief Encode a response into an ASCII frame
   * @param response The Modbus response to encode (same PDU as RTU)
   * @return ASCII string: ":<hex chars><LRC>\\r\\n"
   */
  [[nodiscard]] static std::string EncodeResponse(const RtuResponse &response);

  /**
   * @brief Decode an ASCII frame into a request
   * @param frame ASCII frame (":" through CRLF)
   * @param byte_order Byte order for 16-bit values in PDU
   * @return Parsed request if frame is valid, empty optional otherwise
   */
  [[nodiscard]] static std::optional<RtuRequest> DecodeRequest(std::string_view frame,
                                                               ByteOrder byte_order = ByteOrder::BigEndian);

  /**
   * @brief Decode an ASCII frame into a response
   * @param frame ASCII frame (":" through CRLF)
   * @return Parsed response if frame is valid, empty optional otherwise
   */
  [[nodiscard]] static std::optional<RtuResponse> DecodeResponse(std::string_view frame);

  /**
   * @brief Convert binary bytes to ASCII hex string (e.g., {0x01, 0x03} -> "0103")
   */
  [[nodiscard]] static std::string BytesToHex(std::span<const uint8_t> bytes);

  /**
   * @brief Convert ASCII hex string to binary bytes
   * @return Binary bytes, or empty if invalid hex
   */
  [[nodiscard]] static std::optional<std::vector<uint8_t>> HexToBytes(std::string_view hex);
};

}  // namespace supermb
