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
#include "tcp/tcp_frame.hpp"
#include "tcp/tcp_master.hpp"
#include "tcp/tcp_request.hpp"
#include "tcp/tcp_response.hpp"

namespace supermb {

std::optional<std::vector<int16_t>> TcpMaster::ReadHoldingRegisters(uint8_t unit_id, uint16_t start_address,
                                                                    uint16_t count) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kReadHR});
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
  for (uint16_t i = 0; i < count; ++i) {
    size_t offset = 1 + i * 2;
    // Modbus uses big-endian: high byte first, then low byte
    int16_t value = MakeInt16(data[offset + 1], data[offset]);
    registers.push_back(value);
  }

  return registers;
}

std::optional<std::vector<int16_t>> TcpMaster::ReadInputRegisters(uint8_t unit_id, uint16_t start_address,
                                                                  uint16_t count) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kReadIR});
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

  uint8_t byte_count = data[0];
  if (byte_count != static_cast<uint8_t>(count * 2)) {
    return {};
  }

  std::vector<int16_t> registers;
  registers.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    size_t offset = 1 + i * 2;
    int16_t value = MakeInt16(data[offset + 1], data[offset]);
    registers.push_back(value);
  }

  return registers;
}

bool TcpMaster::WriteSingleRegister(uint8_t unit_id, uint16_t address, int16_t value) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kWriteSingleReg});
  if (!request.SetWriteSingleRegisterData(address, value)) {
    return false;
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }

  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

std::optional<std::vector<bool>> TcpMaster::ReadCoils(uint8_t unit_id, uint16_t start_address, uint16_t count) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kReadCoils});
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
  if (data.empty() || data.size() < 2) {
    return {};
  }

  uint8_t byte_count = data[0];
  if (byte_count != static_cast<uint8_t>((count + 7) / 8)) {
    return {};
  }

  std::vector<bool> coils;
  coils.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    uint8_t byte_index = i / 8;
    uint8_t bit_index = i % 8;
    if (byte_index >= byte_count || static_cast<size_t>(1 + byte_index) >= data.size()) {
      return {};
    }
    bool value = (data[1 + byte_index] & (1U << bit_index)) != 0;
    coils.push_back(value);
  }

  return coils;
}

std::optional<std::vector<bool>> TcpMaster::ReadDiscreteInputs(uint8_t unit_id, uint16_t start_address,
                                                               uint16_t count) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kReadDI});
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
  if (data.empty() || data.size() < 2) {
    return {};
  }

  uint8_t byte_count = data[0];
  if (byte_count != static_cast<uint8_t>((count + 7) / 8)) {
    return {};
  }

  std::vector<bool> inputs;
  inputs.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    uint8_t byte_index = i / 8;
    uint8_t bit_index = i % 8;
    if (byte_index >= byte_count || static_cast<size_t>(1 + byte_index) >= data.size()) {
      return {};
    }
    bool value = (data[1 + byte_index] & (1U << bit_index)) != 0;
    inputs.push_back(value);
  }

  return inputs;
}

bool TcpMaster::WriteSingleCoil(uint8_t unit_id, uint16_t address, bool value) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kWriteSingleCoil});
  if (!request.SetWriteSingleCoilData(address, value)) {
    return false;
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }

  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

bool TcpMaster::WriteMultipleRegisters(uint8_t unit_id, uint16_t start_address, std::span<const int16_t> values) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kWriteMultRegs});
  if (!request.SetWriteMultipleRegistersData(start_address, static_cast<uint16_t>(values.size()), values)) {
    return false;
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }

  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

bool TcpMaster::WriteMultipleCoils(uint8_t unit_id, uint16_t start_address, std::span<const bool> values) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kWriteMultCoils});
  if (!request.SetWriteMultipleCoilsData(start_address, static_cast<uint16_t>(values.size()), values)) {
    return false;
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }

  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

std::optional<uint8_t> TcpMaster::ReadExceptionStatus(uint8_t unit_id) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kReadExceptionStatus});
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

std::optional<std::vector<uint8_t>> TcpMaster::Diagnostics(uint8_t unit_id, uint16_t sub_function_code,
                                                           std::span<const uint8_t> data) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kDiagnostics});
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

std::optional<std::pair<uint8_t, uint16_t>> TcpMaster::GetComEventCounter(uint8_t unit_id) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kGetComEventCounter});
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto response_data = response->GetData();
  if (response_data.size() < 4) {
    return {};
  }
  // Per Modbus spec: status is 2 bytes (0x0000 = ready, 0xFFFF = busy)
  // Modbus uses big-endian: data[0] is high byte, data[1] is low byte
  uint16_t status = MakeInt16(response_data[1], response_data[0]);
  // Event count: data[2] is high byte, data[3] is low byte
  uint16_t event_count = MakeInt16(response_data[3], response_data[2]);
  // Return status as uint8_t for backward compatibility (0x00 or 0xFF)
  uint8_t status_byte = (status == 0xFFFF) ? 0xFF : 0x00;
  return std::make_pair(status_byte, event_count);
}

std::optional<std::vector<uint8_t>> TcpMaster::GetComEventLog(uint8_t unit_id) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kGetComEventLog});
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  return response->GetData();
}

std::optional<std::vector<uint8_t>> TcpMaster::ReportSlaveID(uint8_t unit_id) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kReportSlaveID});
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  return response->GetData();
}

bool TcpMaster::MaskWriteRegister(uint8_t unit_id, uint16_t address, uint16_t and_mask, uint16_t or_mask) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kMaskWriteReg});
  if (!request.SetMaskWriteRegisterData(address, and_mask, or_mask)) {
    return false;
  }

  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }

  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

std::optional<std::vector<int16_t>> TcpMaster::ReadWriteMultipleRegisters(uint8_t unit_id, uint16_t read_start,
                                                                          uint16_t read_count, uint16_t write_start,
                                                                          std::span<const int16_t> write_values) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kReadWriteMultRegs});
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
  if (data.empty() || data.size() < static_cast<size_t>(1 + read_count * 2)) {
    return {};
  }

  uint8_t byte_count = data[0];
  if (byte_count != static_cast<uint8_t>(read_count * 2)) {
    return {};
  }

  std::vector<int16_t> registers;
  registers.reserve(read_count);
  for (uint16_t i = 0; i < read_count; ++i) {
    size_t offset = 1 + i * 2;
    int16_t value = MakeInt16(data[offset + 1], data[offset]);
    registers.push_back(value);
  }

  return registers;
}

std::optional<std::vector<int16_t>> TcpMaster::ReadFIFOQueue(uint8_t unit_id, uint16_t fifo_address) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kReadFIFOQueue});
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

  // First 2 bytes: byte count (big-endian) - Modbus FC24 response format
  uint16_t byte_count = MakeInt16(data[1], data[0]);
  // Next 2 bytes: FIFO count (big-endian). Zero is valid (empty queue per Modbus spec).
  uint16_t fifo_count = MakeInt16(data[3], data[2]);
  if (fifo_count > 31) {
    return {};  // Invalid FIFO count (max 31 per Modbus)
  }
  // Per Modbus spec: byte_count includes the 2-byte FIFO count field + data
  if (byte_count != 2 + (fifo_count * 2)) {
    return {};
  }

  // Data size check: byte_count(2) + fifo_count(2) + fifo_data(fifo_count*2)
  if (data.size() < static_cast<size_t>(4 + fifo_count * 2)) {
    return {};
  }

  std::vector<int16_t> fifo_data;
  fifo_data.reserve(fifo_count);
  for (uint16_t i = 0; i < fifo_count; ++i) {
    size_t offset = 4 + i * 2;
    int16_t value = MakeInt16(data[offset + 1], data[offset]);
    fifo_data.push_back(value);
  }

  return fifo_data;
}

std::optional<std::unordered_map<std::pair<uint16_t, uint16_t>, std::vector<int16_t>, PairU16Hasher>>
TcpMaster::ReadFileRecord(uint8_t unit_id, std::span<const std::tuple<uint16_t, uint16_t, uint16_t>> file_records) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kReadFileRecord});
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

  std::unordered_map<std::pair<uint16_t, uint16_t>, std::vector<int16_t>, PairU16Hasher> result;
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
    uint16_t file_number = MakeInt16(data[offset + 3], data[offset + 2]);
    uint16_t record_number = MakeInt16(data[offset + 5], data[offset + 4]);
    offset += 6;  // Skip header

    // Extract record data
    uint16_t record_data_length = (data_length - 4) / 2;  // Subtract file/record numbers (4 bytes)
    std::vector<int16_t> record_data;
    record_data.reserve(record_data_length);

    for (uint16_t i = 0; i < record_data_length && offset + 1 < end_offset; ++i) {
      int16_t value = MakeInt16(data[offset + 1], data[offset]);
      record_data.push_back(value);
      offset += 2;
    }

    result[{file_number, record_number}] = std::move(record_data);
  }

  return result;
}

bool TcpMaster::WriteFileRecord(uint8_t unit_id,
                                std::span<const std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records) {
  uint16_t transaction_id = GetNextTransactionId();
  TcpRequest request({transaction_id, unit_id, FunctionCode::kWriteFileRecord});
  if (!request.SetWriteFileRecordData(file_records)) {
    return false;
  }
  auto response = SendRequest(request);
  if (!response.has_value()) {
    return false;
  }
  return response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

std::optional<TcpResponse> TcpMaster::SendRequest(const TcpRequest &request, uint32_t timeout_ms) {
  // Encode request to frame
  std::vector<uint8_t> frame = TcpFrame::EncodeRequest(request);

  // Write frame to transport
  int bytes_written = transport_.Write(std::span<const uint8_t>(frame.data(), frame.size()));
  if (bytes_written != static_cast<int>(frame.size())) {
    return {};
  }

  // Flush to ensure data is sent
  if (!transport_.Flush()) {
    return {};
  }

  // Receive response
  return ReceiveResponse(request.GetTransactionId(), timeout_ms);
}

std::optional<TcpResponse> TcpMaster::ReceiveResponse(uint16_t expected_transaction_id, uint32_t timeout_ms) {
  auto frame = ReadFrame(timeout_ms);
  if (!frame.has_value()) {
    return {};
  }

  auto response = TcpFrame::DecodeResponse(*frame);
  if (!response.has_value()) {
    return {};
  }

  // Verify transaction ID matches
  if (response->GetTransactionId() != expected_transaction_id) {
    return {};
  }

  return response;
}

std::optional<std::vector<uint8_t>> TcpMaster::ReadFrame(uint32_t timeout_ms) {
  std::vector<uint8_t> frame;
  frame.reserve(256);  // Reserve some space

  auto start_time = std::chrono::steady_clock::now();

  // Read MBAP header first (7 bytes)
  while (frame.size() < 7) {
    if (timeout_ms > 0) {
      auto elapsed = std::chrono::steady_clock::now() - start_time;
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
      if (elapsed_ms >= static_cast<int64_t>(timeout_ms)) {
        return {};
      }
    }

    if (!transport_.HasData()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    size_t current_size = frame.size();
    size_t needed = 7 - current_size;
    frame.resize(7);
    int bytes_read = transport_.Read(std::span<uint8_t>(frame.data() + current_size, needed));
    if (bytes_read <= 0) {
      frame.resize(current_size);  // Restore original size on error
      return {};
    }
    frame.resize(current_size + static_cast<size_t>(bytes_read));
  }

  // Extract length from MBAP header (bytes 4-5, big-endian)
  uint16_t length = static_cast<uint16_t>((static_cast<uint16_t>(frame[4]) << 8) | static_cast<uint16_t>(frame[5]));

  // Read remaining bytes
  // MBAP header = Transaction ID(2) + Protocol ID(2) + Length(2) + Unit ID(1) = 7 bytes
  // Length field value = Unit ID(1) + PDU size
  // Total frame size = 7 + (length - 1) = 6 + length
  size_t total_size = 6 + length;
  while (frame.size() < total_size) {
    if (timeout_ms > 0) {
      auto elapsed = std::chrono::steady_clock::now() - start_time;
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
      if (elapsed_ms >= static_cast<int64_t>(timeout_ms)) {
        return {};
      }
    }

    if (!transport_.HasData()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    size_t current_size = frame.size();
    size_t needed = total_size - current_size;
    frame.resize(total_size);
    int bytes_read = transport_.Read(std::span<uint8_t>(frame.data() + current_size, needed));
    if (bytes_read <= 0) {
      frame.resize(current_size);  // Restore original size on error
      return {};
    }
    frame.resize(current_size + static_cast<size_t>(bytes_read));
  }

  return frame;
}

}  // namespace supermb
