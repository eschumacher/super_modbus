#include "ascii/ascii_frame.hpp"
#include "ascii/ascii_slave.hpp"
#include "common/function_code.hpp"
#include "rtu/rtu_request.hpp"
#include "rtu/rtu_response.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <thread>

using supermb::IsBroadcastableWrite;

namespace supermb {

std::optional<std::string> AsciiSlave::ReadAsciiFrame(ByteReader &transport, uint32_t timeout_ms) {
  std::string buffer;
  buffer.reserve(512);
  auto start_time = std::chrono::steady_clock::now();

  while (true) {
    if (timeout_ms > 0) {
      auto elapsed = std::chrono::steady_clock::now() - start_time;
      if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= static_cast<int64_t>(timeout_ms)) {
        return {};
      }
    }

    uint8_t temp[128];
    int n = transport.Read(std::span<uint8_t>(temp, sizeof(temp)));
    if (n > 0) {
      buffer.append(reinterpret_cast<char *>(temp), static_cast<size_t>(n));
      size_t start = buffer.find(AsciiFrame::kStartByte);
      if (start != std::string::npos) {
        size_t crlf = buffer.find("\r\n", start);
        if (crlf != std::string::npos) {
          return buffer.substr(start, crlf - start + 2);
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

bool AsciiSlave::ProcessIncomingFrame(ByteTransport &transport, uint32_t timeout_ms) {
  auto frame = ReadAsciiFrame(transport, timeout_ms);
  if (!frame.has_value()) {
    return false;
  }

  auto request = AsciiFrame::DecodeRequest(*frame, rtu_slave_.GetByteOrder());
  if (!request.has_value()) {
    return false;
  }

  uint8_t req_slave_id = request->GetSlaveId();
  bool is_broadcast = (req_slave_id == 0);
  bool is_broadcastable = IsBroadcastableWrite(request->GetFunctionCode());

  if (is_broadcast) {
    if (!is_broadcastable) {
      return false;
    }
  } else if (req_slave_id != rtu_slave_.GetId()) {
    return false;
  }

  RtuResponse response = rtu_slave_.Process(*request);

  if (is_broadcast) {
    return true;  // No response for broadcast
  }

  std::string response_frame = AsciiFrame::EncodeResponse(response);
  std::span<const uint8_t> response_bytes(reinterpret_cast<const uint8_t *>(response_frame.data()),
                                          response_frame.size());
  if (transport.Write(response_bytes) != static_cast<int>(response_bytes.size())) {
    return false;
  }
  if (!transport.Flush()) {
    return false;
  }
  return true;
}

bool AsciiSlave::Poll(ByteTransport &transport) {
  return ProcessIncomingFrame(transport, 100);
}

}  // namespace supermb
