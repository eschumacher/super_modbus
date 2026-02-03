#include "ascii/ascii_frame.hpp"
#include "common/address_span.hpp"
#include "common/byte_helpers.hpp"
#include "common/function_code.hpp"
#include "common/lrc8.hpp"
#include "rtu/rtu_request.hpp"
#include "rtu/rtu_response.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace supermb {

std::string AsciiFrame::BytesToHex(std::span<const uint8_t> bytes) {
  static constexpr char kHexChars[] = "0123456789ABCDEF";
  std::string result;
  result.reserve(bytes.size() * 2);
  for (uint8_t b : bytes) {
    result += kHexChars[(b >> 4) & 0x0F];
    result += kHexChars[b & 0x0F];
  }
  return result;
}

std::optional<std::vector<uint8_t>> AsciiFrame::HexToBytes(std::string_view hex) {
  if (hex.size() % 2 != 0) {
    return {};
  }
  std::vector<uint8_t> result;
  result.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    auto hexCharToNibble = [](char c) -> std::optional<int> {
      if (c >= '0' && c <= '9') {
        return c - '0';
      }
      char uc = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      if (uc >= 'A' && uc <= 'F') {
        return uc - 'A' + 10;
      }
      return {};
    };
    auto hi = hexCharToNibble(hex[i]);
    auto lo = hexCharToNibble(hex[i + 1]);
    if (!hi.has_value() || !lo.has_value()) {
      return {};
    }
    result.push_back(static_cast<uint8_t>((*hi << 4) | *lo));
  }
  return result;
}

std::string AsciiFrame::EncodeRequest(const RtuRequest &request) {
  std::vector<uint8_t> pdu;
  pdu.push_back(request.GetSlaveId());
  pdu.push_back(static_cast<uint8_t>(request.GetFunctionCode()));
  const auto &data = request.GetData();
  pdu.insert(pdu.end(), data.begin(), data.end());
  uint8_t lrc = CalculateLrc8(pdu);
  pdu.push_back(lrc);

  std::string frame;
  frame.reserve(1 + pdu.size() * 2 + 2);  // ':' + hex + CRLF
  frame += kStartByte;
  frame += BytesToHex(pdu);
  frame += kCr;
  frame += kLf;
  return frame;
}

std::string AsciiFrame::EncodeResponse(const RtuResponse &response) {
  std::vector<uint8_t> pdu;
  pdu.push_back(response.GetSlaveId());
  FunctionCode function_code = response.GetFunctionCode();
  if (response.GetExceptionCode() != ExceptionCode::kInvalidExceptionCode &&
      response.GetExceptionCode() != ExceptionCode::kAcknowledge) {
    function_code = static_cast<FunctionCode>(static_cast<uint8_t>(function_code) | kExceptionFunctionCodeMask);
  }
  pdu.push_back(static_cast<uint8_t>(function_code));
  if (response.GetExceptionCode() != ExceptionCode::kInvalidExceptionCode &&
      response.GetExceptionCode() != ExceptionCode::kAcknowledge) {
    pdu.push_back(static_cast<uint8_t>(response.GetExceptionCode()));
  } else {
    const auto data = response.GetData();
    pdu.insert(pdu.end(), data.begin(), data.end());
  }
  uint8_t lrc = CalculateLrc8(pdu);
  pdu.push_back(lrc);

  std::string frame;
  frame.reserve(1 + pdu.size() * 2 + 2);
  frame += kStartByte;
  frame += BytesToHex(pdu);
  frame += kCr;
  frame += kLf;
  return frame;
}

std::optional<RtuRequest> AsciiFrame::DecodeRequest(std::string_view frame, ByteOrder byte_order) {
  if (frame.empty() || frame.front() != kStartByte) {
    return {};
  }
  std::string_view hex = frame.substr(1);
  size_t end_pos = hex.find(kCr);
  if (end_pos != std::string_view::npos) {
    hex = hex.substr(0, end_pos);
  }
  auto pdu = HexToBytes(hex);
  if (!pdu.has_value() || pdu->size() < 3) {  // slave_id + function_code + LRC
    return {};
  }
  std::vector<uint8_t> &vec = *pdu;
  if (!VerifyLrc8(vec)) {
    return {};
  }
  vec.pop_back();  // Remove LRC for parsing
  uint8_t slave_id = vec[0];
  auto function_code = static_cast<FunctionCode>(vec[1]);
  std::vector<uint8_t> data(vec.begin() + 2, vec.end());

  RtuRequest request({slave_id, function_code}, byte_order);
  if (!data.empty()) {
    if (function_code == FunctionCode::kReadHR || function_code == FunctionCode::kReadIR ||
        function_code == FunctionCode::kReadCoils || function_code == FunctionCode::kReadDI) {
      if (data.size() >= 4) {
        AddressSpan span;
        span.start_address = static_cast<uint16_t>(DecodeU16(data[0], data[1], byte_order));
        span.reg_count = static_cast<uint16_t>(DecodeU16(data[2], data[3], byte_order));
        request.SetAddressSpan(span);
      } else {
        request.SetRawData(data);
      }
    } else if (function_code == FunctionCode::kWriteSingleReg) {
      if (data.size() >= 4) {
        uint16_t address = static_cast<uint16_t>(DecodeU16(data[0], data[1], byte_order));
        int16_t value = static_cast<int16_t>(DecodeU16(data[2], data[3], byte_order));
        request.SetWriteSingleRegisterData(address, value);
      } else {
        request.SetRawData(data);
      }
    } else if (function_code == FunctionCode::kWriteSingleCoil) {
      if (data.size() >= 4) {
        uint16_t address = static_cast<uint16_t>(DecodeU16(data[0], data[1], byte_order));
        uint16_t value = static_cast<uint16_t>(DecodeU16(data[2], data[3], byte_order));
        bool coil_value = (value == kCoilOnValue);
        request.SetWriteSingleCoilData(address, coil_value);
      } else {
        request.SetRawData(data);
      }
    } else {
      request.SetRawData(data);
    }
  }
  return request;
}

std::optional<RtuResponse> AsciiFrame::DecodeResponse(std::string_view frame) {
  if (frame.empty() || frame.front() != kStartByte) {
    return {};
  }
  std::string_view hex = frame.substr(1);
  size_t end_pos = hex.find(kCr);
  if (end_pos != std::string_view::npos) {
    hex = hex.substr(0, end_pos);
  }
  auto pdu = HexToBytes(hex);
  if (!pdu.has_value() || pdu->size() < 3) {
    return {};
  }
  std::vector<uint8_t> &vec = *pdu;
  if (!VerifyLrc8(vec)) {
    return {};
  }
  vec.pop_back();
  uint8_t slave_id = vec[0];
  uint8_t function_code_byte = vec[1];
  bool is_exception = (function_code_byte & kExceptionFunctionCodeMask) != 0;
  auto function_code = static_cast<FunctionCode>(function_code_byte & kFunctionCodeMask);

  RtuResponse response(slave_id, function_code);
  if (is_exception && vec.size() >= 3) {
    response.SetExceptionCode(static_cast<ExceptionCode>(vec[2]));
  } else {
    if (vec.size() > 2) {
      std::vector<uint8_t> data(vec.begin() + 2, vec.end());
      response.SetData(data);
    }
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
  }
  return response;
}

}  // namespace supermb
