#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "common/address_span.hpp"
#include "common/byte_helpers.hpp"
#include "common/crc16.hpp"
#include "common/exception_code.hpp"
#include "common/function_code.hpp"
#include "rtu/rtu_frame.hpp"
#include "rtu/rtu_request.hpp"
#include "rtu/rtu_response.hpp"

namespace supermb {

std::vector<uint8_t> RtuFrame::EncodeRequest(RtuRequest const &request) {
  std::vector<uint8_t> frame;

  // Slave ID
  frame.push_back(request.GetSlaveId());

  // Function code
  frame.push_back(static_cast<uint8_t>(request.GetFunctionCode()));

  // Data
  auto const &data = request.GetData();
  frame.insert(frame.end(), data.begin(), data.end());

  // Calculate and append CRC (little-endian: low byte first)
  uint16_t crc = CalculateCrc16(std::span<uint8_t const>(frame.data(), frame.size()));
  frame.push_back(GetLowByte(crc));
  frame.push_back(GetHighByte(crc));

  return frame;
}

std::vector<uint8_t> RtuFrame::EncodeResponse(RtuResponse const &response) {
  std::vector<uint8_t> frame;

  // Slave ID
  frame.push_back(response.GetSlaveId());

  // Function code (or exception function code if error)
  FunctionCode function_code = response.GetFunctionCode();
  if (response.GetExceptionCode() != ExceptionCode::kInvalidExceptionCode &&
      response.GetExceptionCode() != ExceptionCode::kAcknowledge) {
    // Exception response: function code with MSB set
    function_code = static_cast<FunctionCode>(static_cast<uint8_t>(function_code) | kExceptionFunctionCodeMask);
  }
  frame.push_back(static_cast<uint8_t>(function_code));

  // Exception code or data
  if (response.GetExceptionCode() != ExceptionCode::kInvalidExceptionCode &&
      response.GetExceptionCode() != ExceptionCode::kAcknowledge) {
    // Exception response: include exception code
    frame.push_back(static_cast<uint8_t>(response.GetExceptionCode()));
  } else {
    // Normal response: include data
    auto const data = response.GetData();
    frame.insert(frame.end(), data.begin(), data.end());
  }

  // Calculate and append CRC (little-endian: low byte first)
  uint16_t crc = CalculateCrc16(std::span<uint8_t const>(frame.data(), frame.size()));
  frame.push_back(GetLowByte(crc));
  frame.push_back(GetHighByte(crc));

  return frame;
}

std::optional<RtuRequest> RtuFrame::DecodeRequest(std::span<uint8_t const> frame) {
  // Minimum frame: slave_id (1) + function_code (1) + CRC (2) = 4 bytes
  if (frame.size() < kMinFrameSize) {
    return {};
  }

  // Verify CRC
  if (!VerifyCrc16(frame)) {
    return {};
  }

  // Extract slave ID and function code
  uint8_t slave_id = frame[0];
  auto function_code = static_cast<FunctionCode>(frame[1]);

  // Extract data (everything between function code and CRC)
  std::vector<uint8_t> data;
  if (frame.size() > kMinFrameSize) {
    size_t data_size = frame.size() - kMinFrameSize;
    data.assign(frame.begin() + 2, frame.begin() + 2 + static_cast<ssize_t>(data_size));
  }

  RtuRequest request({slave_id, function_code});
  if (!data.empty()) {
    // Try to parse as address span for read operations
    if (function_code == FunctionCode::kReadHR || function_code == FunctionCode::kReadIR ||
        function_code == FunctionCode::kReadCoils || function_code == FunctionCode::kReadDI) {
      if (data.size() >= 4) {
        AddressSpan span;
        span.start_address = MakeInt16(data[1], data[0]);
        span.reg_count = MakeInt16(data[3], data[2]);
        request.SetAddressSpan(span);
      } else {
        request.SetRawData(data);
      }
    } else if (function_code == FunctionCode::kWriteSingleReg) {
      if (data.size() >= 4) {
        uint16_t address = MakeInt16(data[1], data[0]);
        int16_t value = MakeInt16(data[3], data[2]);
        request.SetWriteSingleRegisterData(address, value);
      } else {
        request.SetRawData(data);
      }
    } else if (function_code == FunctionCode::kWriteSingleCoil) {
      if (data.size() >= 4) {
        uint16_t address = MakeInt16(data[1], data[0]);
        uint16_t value = MakeInt16(data[3], data[2]);
        bool coil_value = (value == kCoilOnValue);
        request.SetWriteSingleCoilData(address, coil_value);
      } else {
        request.SetRawData(data);
      }
    } else {
      // For write multiple operations and other function codes, store raw data
      // Write multiple operations will be parsed by the processing functions
      request.SetRawData(data);
    }
  }

  return request;
}

std::optional<RtuResponse> RtuFrame::DecodeResponse(std::span<uint8_t const> frame) {
  // Minimum frame: slave_id (1) + function_code (1) + CRC (2) = 4 bytes
  if (frame.size() < kMinFrameSize) {
    return {};
  }

  // Verify CRC
  if (!VerifyCrc16(frame)) {
    return {};
  }

  // Extract slave ID and function code
  uint8_t slave_id = frame[0];
  uint8_t function_code_byte = frame[1];

  // Check if this is an exception response (MSB set)
  bool is_exception = (function_code_byte & kExceptionFunctionCodeMask) != 0;
  auto function_code = static_cast<FunctionCode>(function_code_byte & kFunctionCodeMask);

  RtuResponse response(slave_id, function_code);

  if (is_exception) {
    // Exception response: next byte is exception code
    if (frame.size() >= 3) {
      auto exception_code = static_cast<ExceptionCode>(frame[2]);
      response.SetExceptionCode(exception_code);
    }
  } else {
    // Normal response: extract data
    if (frame.size() > kMinFrameSize) {
      size_t data_size = frame.size() - kMinFrameSize;
      std::vector<uint8_t> data(frame.begin() + 2, frame.begin() + 2 + static_cast<ssize_t>(data_size));
      response.SetData(data);
      response.SetExceptionCode(ExceptionCode::kAcknowledge);
    } else {
      response.SetExceptionCode(ExceptionCode::kAcknowledge);
    }
  }

  return response;
}

size_t RtuFrame::GetMinFrameSize(FunctionCode function_code) {
  // Base size: slave_id (1) + function_code (1) + CRC (2) = 4 bytes
  // Some function codes have minimum data requirements
  switch (function_code) {
    case FunctionCode::kReadHR:
    case FunctionCode::kReadIR:
    case FunctionCode::kReadCoils:
    case FunctionCode::kReadDI:
    case FunctionCode::kWriteSingleReg:
    case FunctionCode::kWriteSingleCoil:
      return kMinFrameSize + 4;  // + address (2) + count/value (2)
    default:
      return kMinFrameSize;
  }
}

bool RtuFrame::IsRequestFrameComplete(std::span<uint8_t const> frame) {
  if (frame.size() < 2) {
    return false;  // Need at least slave_id and function_code
  }

  auto function_code = static_cast<FunctionCode>(frame[1] & kFunctionCodeMask);
  size_t min_size = GetMinFrameSize(function_code);
  return frame.size() >= min_size;
}

bool RtuFrame::IsResponseFrameComplete(std::span<uint8_t const> frame) {
  if (frame.size() < 2) {
    return false;  // Need at least slave_id and function_code
  }

  uint8_t function_code_byte = frame[1];
  bool is_exception = (function_code_byte & kExceptionFunctionCodeMask) != 0;
  auto function_code = static_cast<FunctionCode>(function_code_byte & kFunctionCodeMask);

  if (is_exception) {
    // Exception response: slave_id (1) + function_code (1) + exception_code (1) + CRC (2)
    return frame.size() >= kExceptionResponseFrameSize;
  }

  // For read function codes, responses have variable length based on byte_count
  if (function_code == FunctionCode::kReadHR || function_code == FunctionCode::kReadIR ||
      function_code == FunctionCode::kReadCoils || function_code == FunctionCode::kReadDI) {
    // Response format: slave_id (1) + function_code (1) + byte_count (1) + data (byte_count) + CRC (2)
    if (frame.size() < 3) {
      return false;  // Need at least byte_count
    }
    uint8_t byte_count = frame[2];
    size_t expected_size = 1 + 1 + 1 + byte_count + 2;  // slave_id + function_code + byte_count + data + CRC
    return frame.size() >= expected_size;
  }

  // For write single operations, responses echo back address and value
  if (function_code == FunctionCode::kWriteSingleReg || function_code == FunctionCode::kWriteSingleCoil) {
    // Response format: slave_id (1) + function_code (1) + address (2) + value (2) + CRC (2)
    return frame.size() >= kWriteSingleFrameSize;
  }

  // For other function codes, use the minimum frame size
  size_t min_size = GetMinFrameSize(function_code);
  return frame.size() >= min_size;
}

bool RtuFrame::IsFrameComplete(std::span<uint8_t const> frame) {
  // Legacy function - try to determine if it's a request or response
  // This is less reliable, prefer using IsRequestFrameComplete or IsResponseFrameComplete
  if (frame.size() < 2) {
    return false;
  }

  uint8_t function_code_byte = frame[1];
  bool is_exception = (function_code_byte & kExceptionFunctionCodeMask) != 0;

  // Exception responses are always responses
  if (is_exception) {
    return IsResponseFrameComplete(frame);
  }

  // For read operations, try to determine by frame size
  auto function_code = static_cast<FunctionCode>(function_code_byte & kFunctionCodeMask);
  if (function_code == FunctionCode::kReadHR || function_code == FunctionCode::kReadIR ||
      function_code == FunctionCode::kReadCoils || function_code == FunctionCode::kReadDI) {
    // Request is always kWriteSingleFrameSize bytes, response is variable
    if (frame.size() == kWriteSingleFrameSize) {
      return IsRequestFrameComplete(frame);
    }
    // Otherwise, assume it's a response
    return IsResponseFrameComplete(frame);
  }

  // For other function codes, check as request first (most common case)
  return IsRequestFrameComplete(frame);
}

}  // namespace supermb
