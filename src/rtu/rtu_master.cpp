#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include "common/byte_helpers.hpp"
#include "common/exception_code.hpp"
#include "common/function_code.hpp"
#include "rtu/rtu_frame.hpp"
#include "rtu/rtu_master.hpp"
#include "rtu/rtu_request.hpp"
#include "rtu/rtu_response.hpp"

using supermb::IsBroadcastableWrite;

namespace supermb {

std::optional<std::vector<int16_t>> RtuMaster::ReadHoldingRegisters(uint8_t slave_id, uint16_t start_address,
                                                                    uint16_t count) {
  RtuRequest request({slave_id, FunctionCode::kReadHR});
  AddressSpan span{start_address, count};
  if (!request.SetAddressSpan(span)) {
    return {};
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }

  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }

  auto data = response->GetData();
  if (data.empty() || data.size() < static_cast<size_t>(1 + count * 2)) {
    return {};
  }

  // First byte is byte_count, should be count * 2
  uint8_t byte_count = data[0];
  if (byte_count != static_cast<uint8_t>(count * 2)) {
    return {};
  }

  std::vector<int16_t> registers;
  registers.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    size_t byte_idx = 1 + i * 2;  // Skip byte_count field
    // Modbus RTU uses big-endian: high byte first, then low byte
    int16_t value = MakeInt16(data[byte_idx + 1], data[byte_idx]);
    registers.push_back(value);
  }

  return registers;
}

std::optional<std::vector<int16_t>> RtuMaster::ReadInputRegisters(uint8_t slave_id, uint16_t start_address,
                                                                  uint16_t count) {
  RtuRequest request({slave_id, FunctionCode::kReadIR});
  AddressSpan span{start_address, count};
  if (!request.SetAddressSpan(span)) {
    return {};
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }

  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }

  auto data = response->GetData();
  if (data.empty() || data.size() < static_cast<size_t>(1 + count * 2)) {
    return {};
  }

  // First byte is byte_count, should be count * 2
  uint8_t byte_count = data[0];
  if (byte_count != static_cast<uint8_t>(count * 2)) {
    return {};
  }

  std::vector<int16_t> registers;
  registers.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    size_t byte_idx = 1 + i * 2;  // Skip byte_count field
    // Modbus RTU uses big-endian: high byte first, then low byte
    int16_t value = MakeInt16(data[byte_idx + 1], data[byte_idx]);
    registers.push_back(value);
  }

  return registers;
}

bool RtuMaster::WriteSingleRegister(uint8_t slave_id, uint16_t address, int16_t value) {
  RtuRequest request({slave_id, FunctionCode::kWriteSingleReg});
  if (!request.SetWriteSingleRegisterData(address, value)) {
    return false;
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }

  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

std::optional<std::vector<bool>> RtuMaster::ReadCoils(uint8_t slave_id, uint16_t start_address, uint16_t count) {
  RtuRequest request({slave_id, FunctionCode::kReadCoils});
  AddressSpan span{start_address, count};
  if (!request.SetAddressSpan(span)) {
    return {};
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }

  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }

  auto data = response->GetData();
  if (data.empty() || data[0] != static_cast<uint8_t>((count + 7) / 8)) {
    return {};
  }

  std::vector<bool> coils;
  coils.reserve(count);

  // Unpack coils from bytes (skip first byte which is byte count)
  for (uint16_t i = 0; i < count; ++i) {
    uint8_t byte_index = 1 + (i / 8);
    uint8_t bit_index = i % 8;
    if (byte_index >= data.size()) {
      return {};
    }
    bool coil_value = (data[byte_index] & (1 << bit_index)) != 0;
    coils.push_back(coil_value);
  }

  return coils;
}

std::optional<std::vector<bool>> RtuMaster::ReadDiscreteInputs(uint8_t slave_id, uint16_t start_address,
                                                               uint16_t count) {
  RtuRequest request({slave_id, FunctionCode::kReadDI});
  AddressSpan span{start_address, count};
  if (!request.SetAddressSpan(span)) {
    return {};
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }

  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }

  auto data = response->GetData();
  if (data.empty() || data[0] != static_cast<uint8_t>((count + 7) / 8)) {
    return {};
  }

  std::vector<bool> discrete_inputs;
  discrete_inputs.reserve(count);

  // Unpack discrete inputs from bytes (skip first byte which is byte count)
  for (uint16_t i = 0; i < count; ++i) {
    uint8_t byte_index = 1 + (i / 8);
    uint8_t bit_index = i % 8;
    if (byte_index >= data.size()) {
      return {};
    }
    bool di_value = (data[byte_index] & (1 << bit_index)) != 0;
    discrete_inputs.push_back(di_value);
  }

  return discrete_inputs;
}

bool RtuMaster::WriteSingleCoil(uint8_t slave_id, uint16_t address, bool value) {
  RtuRequest request({slave_id, FunctionCode::kWriteSingleCoil});
  if (!request.SetWriteSingleCoilData(address, value)) {
    return false;
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }

  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

bool RtuMaster::WriteMultipleRegisters(uint8_t slave_id, uint16_t start_address, std::span<int16_t const> values) {
  RtuRequest request({slave_id, FunctionCode::kWriteMultRegs});
  if (!request.SetWriteMultipleRegistersData(start_address, static_cast<uint16_t>(values.size()), values)) {
    return false;
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }

  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

bool RtuMaster::WriteMultipleCoils(uint8_t slave_id, uint16_t start_address, std::span<bool const> values) {
  RtuRequest request({slave_id, FunctionCode::kWriteMultCoils});
  if (!request.SetWriteMultipleCoilsData(start_address, static_cast<uint16_t>(values.size()), values)) {
    return false;
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }

  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

std::optional<uint8_t> RtuMaster::ReadExceptionStatus(uint8_t slave_id) {
  RtuRequest request({slave_id, FunctionCode::kReadExceptionStatus});
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.empty()) {
    return {};
  }
  return data[0];
}

std::optional<std::vector<uint8_t>> RtuMaster::Diagnostics(uint8_t slave_id, uint16_t sub_function_code,
                                                           std::span<uint8_t const> data) {
  RtuRequest request({slave_id, FunctionCode::kDiagnostics});
  if (!request.SetDiagnosticsData(sub_function_code, data)) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  return response->GetData();
}

std::optional<std::pair<uint8_t, uint16_t>> RtuMaster::GetComEventCounter(uint8_t slave_id) {
  RtuRequest request({slave_id, FunctionCode::kGetComEventCounter});
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto response_data = response->GetData();
  if (response_data.size() < 3) {
    return {};
  }
  uint8_t status = response_data[0];
  uint16_t event_count = MakeInt16(response_data[1], response_data[2]);
  return std::make_pair(status, event_count);
}

std::optional<std::vector<uint8_t>> RtuMaster::GetComEventLog(uint8_t slave_id) {
  RtuRequest request({slave_id, FunctionCode::kGetComEventLog});
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  return response->GetData();
}

std::optional<std::vector<uint8_t>> RtuMaster::ReportSlaveID(uint8_t slave_id) {
  RtuRequest request({slave_id, FunctionCode::kReportSlaveID});
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  return response->GetData();
}

bool RtuMaster::MaskWriteRegister(uint8_t slave_id, uint16_t address, uint16_t and_mask, uint16_t or_mask) {
  RtuRequest request({slave_id, FunctionCode::kMaskWriteReg});
  if (!request.SetMaskWriteRegisterData(address, and_mask, or_mask)) {
    return false;
  }
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return false;
  }
  // Verify response echoes back the same values
  auto response_data = response->GetData();
  if (response_data.size() < 6) {
    return false;
  }
  uint16_t resp_address = MakeInt16(response_data[1], response_data[0]);
  uint16_t resp_and = MakeInt16(response_data[3], response_data[2]);
  uint16_t resp_or = MakeInt16(response_data[5], response_data[4]);
  return (resp_address == address && resp_and == and_mask && resp_or == or_mask);
}

std::optional<std::vector<int16_t>> RtuMaster::ReadWriteMultipleRegisters(uint8_t slave_id, uint16_t read_start,
                                                                          uint16_t read_count, uint16_t write_start,
                                                                          std::span<int16_t const> write_values) {
  RtuRequest request({slave_id, FunctionCode::kReadWriteMultRegs});
  if (!request.SetReadWriteMultipleRegistersData(read_start, read_count, write_start,
                                                 static_cast<uint16_t>(write_values.size()), write_values)) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.size() < read_count * 2) {
    return {};
  }
  std::vector<int16_t> registers;
  registers.reserve(read_count);
  for (size_t i = 0; i < read_count; ++i) {
    size_t byte_idx = i * 2;
    // Modbus RTU uses big-endian: high byte first, then low byte
    int16_t value = MakeInt16(data[byte_idx + 1], data[byte_idx]);
    registers.push_back(value);
  }
  return registers;
}

std::optional<std::vector<int16_t>> RtuMaster::ReadFIFOQueue(uint8_t slave_id, uint16_t fifo_address) {
  RtuRequest request({slave_id, FunctionCode::kReadFIFOQueue});
  if (!request.SetReadFIFOQueueData(fifo_address)) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.size() < 4) {
    return {};
  }
  // Skip byte count (2 bytes) and FIFO count (2 bytes)
  // Modbus RTU uses big-endian: high byte first, then low byte
  // Slave sends [high_byte_count, low_byte_count, high_fifo_count, low_fifo_count]
  // MakeInt16 takes (low_byte, high_byte), so we need MakeInt16(low_fifo_count, high_fifo_count)
  uint16_t fifo_count = MakeInt16(data[3], data[2]);
  if (data.size() < static_cast<size_t>(4 + fifo_count * 2)) {
    return {};
  }
  std::vector<int16_t> fifo_data;
  fifo_data.reserve(fifo_count);
  for (size_t i = 0; i < fifo_count; ++i) {
    size_t byte_idx = 4 + i * 2;
    // Modbus RTU uses big-endian: high byte first, then low byte
    int16_t value = MakeInt16(data[byte_idx + 1], data[byte_idx]);
    fifo_data.push_back(value);
  }
  return fifo_data;
}

std::optional<std::unordered_map<std::pair<uint16_t, uint16_t>, std::vector<int16_t>>> RtuMaster::ReadFileRecord(
    uint8_t slave_id, std::span<std::tuple<uint16_t, uint16_t, uint16_t> const> file_records) {
  RtuRequest request({slave_id, FunctionCode::kReadFileRecord});
  if (!request.SetReadFileRecordData(file_records)) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.empty() || data.size() < 1) {
    return {};
  }

  uint8_t response_length = data[0];
  if (data.size() < static_cast<size_t>(1 + response_length)) {
    return {};
  }

  std::unordered_map<std::pair<uint16_t, uint16_t>, std::vector<int16_t>> result;
  size_t offset = 1;                        // Skip response_length
  size_t end_offset = 1 + response_length;  // End of response data

  while (offset < end_offset) {
    if (offset + 8 > end_offset) {
      return {};  // Not enough data for a record header
    }

    uint8_t reference_type = data[offset];
    if (reference_type != 0x06) {
      return {};  // Invalid reference type
    }

    uint8_t data_length = data[offset + 1];
    // Slave writes: high byte first, then low byte
    // MakeInt16 takes (low_byte, high_byte)
    uint16_t file_number = MakeInt16(data[offset + 3], data[offset + 2]);
    uint16_t record_number = MakeInt16(data[offset + 5], data[offset + 4]);
    offset += 6;  // Skip header

    // Extract record data
    uint16_t record_data_length = (data_length - 4) / 2;  // Subtract file/record numbers (4 bytes)
    std::vector<int16_t> record_data;
    record_data.reserve(record_data_length);

    for (uint16_t i = 0; i < record_data_length && offset + 1 < end_offset; ++i) {
      // Modbus RTU uses big-endian: high byte first, then low byte
      int16_t value = MakeInt16(data[offset + 1], data[offset]);
      record_data.push_back(value);
      offset += 2;
    }

    result[{file_number, record_number}] = std::move(record_data);
  }

  return result;
}

bool RtuMaster::WriteFileRecord(uint8_t slave_id,
                                std::span<std::tuple<uint16_t, uint16_t, std::vector<int16_t>> const> file_records) {
  RtuRequest request({slave_id, FunctionCode::kWriteFileRecord});
  if (!request.SetWriteFileRecordData(file_records)) {
    return false;
  }
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }
  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

std::optional<RtuResponse> RtuMaster::SendRequest(RtuRequest const &request, uint32_t timeout_ms) {
  // Encode request to frame
  std::vector<uint8_t> frame = RtuFrame::EncodeRequest(request);

  // Write frame to transport
  int bytes_written = transport_.Write(std::span<uint8_t const>(frame.data(), frame.size()));
  if (bytes_written != static_cast<int>(frame.size())) {
    return {};
  }

  // Flush to ensure data is sent
  if (!transport_.Flush()) {
    return {};
  }

  // Broadcast messages (slave ID 0) don't receive responses
  // Only write operations can be broadcast
  if (request.GetSlaveId() == 0 && IsBroadcastableWrite(request.GetFunctionCode())) {
    // Return a dummy success response for broadcast writes
    RtuResponse broadcast_response{0, request.GetFunctionCode()};
    broadcast_response.SetExceptionCode(ExceptionCode::kAcknowledge);
    return broadcast_response;
  }

  // Receive response for non-broadcast messages
  return ReceiveResponse(request.GetSlaveId(), timeout_ms);
}

std::optional<RtuResponse> RtuMaster::ReceiveResponse(uint8_t expected_slave_id, uint32_t timeout_ms) {
  auto frame = ReadFrame(timeout_ms);
  if (!frame.has_value()) {
    return {};
  }

  auto response = RtuFrame::DecodeResponse(std::span<uint8_t const>(frame->data(), frame->size()));
  if (!response.has_value()) {
    return {};
  }

  // Verify slave ID matches
  if (response->GetSlaveId() != expected_slave_id) {
    return {};
  }

  return response;
}

std::optional<std::vector<uint8_t>> RtuMaster::ReadFrame(uint32_t timeout_ms) {
  std::vector<uint8_t> buffer;
  buffer.reserve(256);  // Reserve space for typical frame

  auto start_time = std::chrono::steady_clock::now();
  bool frame_started = false;

  while (true) {
    // Check timeout
    if (timeout_ms > 0) {
      auto elapsed = std::chrono::steady_clock::now() - start_time;
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
      if (elapsed_ms >= static_cast<int64_t>(timeout_ms)) {
        return {};
      }
    }

    // Try to read bytes - read all available data if we can determine frame size
    uint8_t temp_buffer[256];
    std::span<uint8_t> read_span(temp_buffer, sizeof(temp_buffer));
    int bytes_read = transport_.Read(read_span);

    if (bytes_read > 0) {
      buffer.insert(buffer.end(), temp_buffer, temp_buffer + bytes_read);
      frame_started = true;

      // Check if we have a complete frame (master reads responses)
      std::span<uint8_t const> frame_span(buffer.data(), buffer.size());
      if (RtuFrame::IsResponseFrameComplete(frame_span)) {
        return buffer;
      }

      // If we have some data but frame is not complete, try to read more
      // Check if more data is available
      size_t available = transport_.AvailableBytes();
      if (available > 0) {
        // More data available - continue reading immediately without sleeping
        continue;
      }

      // No more data available - check if frame is complete
      if (RtuFrame::IsResponseFrameComplete(frame_span)) {
        return buffer;
      }

      // Frame not complete and no more data - wait a bit (for real serial ports)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } else if (frame_started && bytes_read == 0) {
      // Frame started but no more data available - might be complete (master reads responses)
      std::span<uint8_t const> frame_span(buffer.data(), buffer.size());
      if (RtuFrame::IsResponseFrameComplete(frame_span)) {
        return buffer;
      }
      // Wait a bit for more data (RTU frames should arrive quickly)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } else if (!frame_started) {
      // No data yet, wait a bit
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

}  // namespace supermb
