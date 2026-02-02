#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "common/address_span.hpp"
#include "common/byte_helpers.hpp"
#include "common/exception_code.hpp"
#include "common/function_code.hpp"
#include "tcp/tcp_frame.hpp"
#include "tcp/tcp_request.hpp"
#include "tcp/tcp_response.hpp"

namespace supermb {

uint16_t TcpFrame::ExtractTransactionId(std::span<const uint8_t> frame) {
  if (frame.size() < 2) {
    return 0;
  }
  // Big-endian: high byte first, then low byte
  return static_cast<uint16_t>((static_cast<uint16_t>(frame[0]) << 8) | static_cast<uint16_t>(frame[1]));
}

uint16_t TcpFrame::ExtractLength(std::span<const uint8_t> frame) {
  if (frame.size() < 6) {
    return 0;
  }
  // Length is at offset 4-5 (big-endian)
  return static_cast<uint16_t>((static_cast<uint16_t>(frame[4]) << 8) | static_cast<uint16_t>(frame[5]));
}

uint8_t TcpFrame::ExtractUnitId(std::span<const uint8_t> frame) {
  if (frame.size() < 7) {
    return 0;
  }
  return frame[6];
}

void TcpFrame::WriteTransactionId(std::vector<uint8_t> &frame, uint16_t transaction_id) {
  frame.push_back(GetHighByte(transaction_id));
  frame.push_back(GetLowByte(transaction_id));
}

void TcpFrame::WriteProtocolId(std::vector<uint8_t> &frame) {
  frame.push_back(GetHighByte(kProtocolId));
  frame.push_back(GetLowByte(kProtocolId));
}

void TcpFrame::WriteUnitId(std::vector<uint8_t> &frame, uint8_t unit_id) {
  frame.push_back(unit_id);
}

std::vector<uint8_t> TcpFrame::EncodeRequest(const TcpRequest &request) {
  std::vector<uint8_t> frame;

  // MBAP Header
  // Transaction ID (big-endian)
  WriteTransactionId(frame, request.GetTransactionId());

  // Protocol ID (always 0x0000, big-endian)
  WriteProtocolId(frame);

  // Length will be written after we know the PDU size
  size_t length_offset = frame.size();
  frame.resize(frame.size() + 2);  // Reserve space for length

  // Unit ID
  WriteUnitId(frame, request.GetUnitId());

  // PDU: Function code
  frame.push_back(static_cast<uint8_t>(request.GetFunctionCode()));

  // PDU: Data
  const auto &data = request.GetData();
  frame.insert(frame.end(), data.begin(), data.end());

  // Calculate and write length (Unit ID + PDU size)
  uint16_t length = static_cast<uint16_t>(1 + 1 + data.size());  // Unit ID(1) + Function Code(1) + Data
  frame[length_offset] = GetHighByte(length);
  frame[length_offset + 1] = GetLowByte(length);

  return frame;
}

std::vector<uint8_t> TcpFrame::EncodeResponse(const TcpResponse &response) {
  std::vector<uint8_t> frame;

  // MBAP Header
  // Transaction ID (big-endian)
  WriteTransactionId(frame, response.GetTransactionId());

  // Protocol ID (always 0x0000, big-endian)
  WriteProtocolId(frame);

  // Length will be written after we know the PDU size
  size_t length_offset = frame.size();
  frame.resize(frame.size() + 2);  // Reserve space for length

  // Unit ID
  WriteUnitId(frame, response.GetUnitId());

  // PDU: Function code (or exception function code if error)
  FunctionCode function_code = response.GetFunctionCode();
  if (response.GetExceptionCode() != ExceptionCode::kInvalidExceptionCode &&
      response.GetExceptionCode() != ExceptionCode::kAcknowledge) {
    // Exception response: function code with MSB set
    function_code = static_cast<FunctionCode>(static_cast<uint8_t>(function_code) | kExceptionFunctionCodeMask);
  }
  frame.push_back(static_cast<uint8_t>(function_code));

  // PDU: Exception code or data
  if (response.GetExceptionCode() != ExceptionCode::kInvalidExceptionCode &&
      response.GetExceptionCode() != ExceptionCode::kAcknowledge) {
    // Exception response: include exception code
    frame.push_back(static_cast<uint8_t>(response.GetExceptionCode()));
  } else {
    // Normal response: include data
    const auto data = response.GetData();
    frame.insert(frame.end(), data.begin(), data.end());
  }

  // Calculate and write length (Unit ID + PDU size)
  size_t pdu_size = 1;  // Function code
  if (response.GetExceptionCode() != ExceptionCode::kInvalidExceptionCode &&
      response.GetExceptionCode() != ExceptionCode::kAcknowledge) {
    pdu_size += 1;  // Exception code
  } else {
    pdu_size += response.GetData().size();  // Data
  }
  uint16_t length = static_cast<uint16_t>(1 + pdu_size);  // Unit ID(1) + PDU
  frame[length_offset] = GetHighByte(length);
  frame[length_offset + 1] = GetLowByte(length);

  return frame;
}

std::optional<TcpRequest> TcpFrame::DecodeRequest(std::span<const uint8_t> frame, ByteOrder byte_order) {
  // Minimum frame: MBAP header (7) + function_code (1) = 8 bytes
  if (frame.size() < kMinFrameSize) {
    return {};
  }

  // Verify protocol ID (must be 0x0000)
  uint16_t protocol_id =
      static_cast<uint16_t>((static_cast<uint16_t>(frame[2]) << 8) | static_cast<uint16_t>(frame[3]));
  if (protocol_id != kProtocolId) {
    return {};
  }

  // Extract MBAP header fields
  uint16_t transaction_id = ExtractTransactionId(frame);
  uint16_t length = ExtractLength(frame);
  uint8_t unit_id = ExtractUnitId(frame);

  // Verify length matches frame size
  if (frame.size() < static_cast<size_t>(6 + length)) {
    return {};
  }

  if (length < 1) {
    return {};
  }

  size_t pdu_start = kMbapHeaderSize;
  if (frame.size() < pdu_start + 1) {
    return {};
  }

  auto function_code = static_cast<FunctionCode>(frame[pdu_start]);

  std::vector<uint8_t> data;
  if (length > 1) {
    size_t data_size = length - 1;
    if (data_size > 1) {
      data_size -= 1;
      if (pdu_start + 1 + data_size <= frame.size()) {
        data.assign(frame.begin() + pdu_start + 1, frame.begin() + pdu_start + 1 + static_cast<ssize_t>(data_size));
      }
    }
  }

  TcpRequest request({transaction_id, unit_id, function_code}, byte_order);
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

std::optional<TcpResponse> TcpFrame::DecodeResponse(std::span<const uint8_t> frame) {
  // Minimum frame: MBAP header (7) + function_code (1) = 8 bytes
  if (frame.size() < kMinFrameSize) {
    return {};
  }

  // Verify protocol ID (must be 0x0000)
  uint16_t protocol_id =
      static_cast<uint16_t>((static_cast<uint16_t>(frame[2]) << 8) | static_cast<uint16_t>(frame[3]));
  if (protocol_id != kProtocolId) {
    return {};
  }

  // Extract MBAP header fields
  uint16_t transaction_id = ExtractTransactionId(frame);
  uint16_t length = ExtractLength(frame);
  uint8_t unit_id = ExtractUnitId(frame);

  // Verify length matches frame size
  // MBAP header = Transaction ID(2) + Protocol ID(2) + Length(2) + Unit ID(1) = 7 bytes
  // Length field value = Unit ID(1) + PDU size
  // PDU = Function Code + Data = (length - 1) bytes (since length includes Unit ID)
  // Total frame size = 7 + (length - 1) = 6 + length
  if (frame.size() < static_cast<size_t>(6 + length)) {
    return {};
  }

  // Extract PDU
  size_t pdu_start = kMbapHeaderSize;
  if (frame.size() < pdu_start + 1) {
    return {};
  }

  uint8_t function_code_byte = frame[pdu_start];

  // Check if this is an exception response (MSB set)
  bool is_exception = (function_code_byte & kExceptionFunctionCodeMask) != 0;
  auto function_code = static_cast<FunctionCode>(function_code_byte & kFunctionCodeMask);

  TcpResponse response(transaction_id, unit_id, function_code);

  if (is_exception) {
    // Exception response: next byte is exception code
    // Length must be at least 3: Unit ID(1) + Function Code(1) + Exception Code(1)
    if (length < 3 || frame.size() < pdu_start + 2) {
      return {};  // Invalid exception response - insufficient data
    }
    auto exception_code = static_cast<ExceptionCode>(frame[pdu_start + 1]);
    response.SetExceptionCode(exception_code);
  } else {
    // Normal response: extract data
    if (length > 1) {
      size_t data_size = length - 1;  // Subtract Unit ID
      if (data_size > 1) {            // More than just function code
        data_size -= 1;               // Subtract function code
        if (pdu_start + 1 + data_size <= frame.size()) {
          std::vector<uint8_t> data(frame.begin() + pdu_start + 1,
                                    frame.begin() + pdu_start + 1 + static_cast<ssize_t>(data_size));
          response.SetData(data);
        }
      }
      response.SetExceptionCode(ExceptionCode::kAcknowledge);
    } else {
      response.SetExceptionCode(ExceptionCode::kAcknowledge);
    }
  }

  return response;
}

size_t TcpFrame::GetMinFrameSize(FunctionCode function_code) {
  // Base size: MBAP header (7) + function_code (1) = 8 bytes
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

bool TcpFrame::IsRequestFrameComplete(std::span<const uint8_t> frame) {
  if (frame.size() < kMbapHeaderSize) {
    return false;  // Need at least MBAP header
  }

  // Check if we have enough bytes to read the length field
  uint16_t length = ExtractLength(frame);
  // MBAP header = Transaction ID(2) + Protocol ID(2) + Length(2) + Unit ID(1) = 7 bytes
  // Length field value = Unit ID(1) + PDU size
  // Total frame size = 7 + (length - 1) = 6 + length
  size_t expected_size = 6 + length;
  return frame.size() >= expected_size;
}

bool TcpFrame::IsResponseFrameComplete(std::span<const uint8_t> frame) {
  // Same logic as request frame
  return IsRequestFrameComplete(frame);
}

}  // namespace supermb
