#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <limits>
#include <optional>
#include <span>
#include <thread>
#include <vector>
#include "common/address_map.hpp"
#include "common/byte_helpers.hpp"
#include "common/function_code.hpp"
#include "tcp/tcp_frame.hpp"
#include "tcp/tcp_request.hpp"
#include "tcp/tcp_response.hpp"
#include "tcp/tcp_slave.hpp"
#include "common/exception_code.hpp"
#include "transport/byte_reader.hpp"
#include "transport/byte_writer.hpp"

namespace supermb {

// Local constants for magic numbers used in this file
namespace {
constexpr size_t kMinWriteDataSize = 5;       // address(2) + count(2) + byte_count(1)
constexpr size_t kMinMaskWriteDataSize = 6;   // address(2) + and_mask(2) + or_mask(2)
constexpr size_t kMinReadWriteDataSize = 10;  // read_start(2) + read_count(2) + write_start(2) + write_count(2) +
                                              // write_byte_count(1) + at least 1 write_value(2)
constexpr size_t kMinReadWriteDataWithValues =
    9;  // read_start(2) + read_count(2) + write_start(2) + write_count(2) + write_byte_count(1)
constexpr size_t kComEventLogMaxSize = 64;
constexpr uint32_t kDefaultPollTimeoutMs = 100;
constexpr uint8_t kMaxByteValue = 0xFF;
constexpr size_t kFileRecordMinHeaderSize =
    7;  // reference_type(1) + file_number(2) + record_number(2) + record_length(2)
constexpr size_t kFileRecordReadMinSize = 6;  // file_number(2) + record_number(2) + record_length(2)
// Array index offsets for ReadWriteMultipleRegisters
constexpr size_t kReadWriteReadStartOffset = 0;
constexpr size_t kReadWriteReadCountOffset = 2;
constexpr size_t kReadWriteWriteStartOffset = 4;
constexpr size_t kReadWriteWriteCountOffset = 6;
constexpr size_t kReadWriteByteCountOffset = 8;
// Array index offsets for MaskWriteRegister
constexpr size_t kMaskWriteAddressOffset = 0;
constexpr size_t kMaskWriteAndMaskOffset = 2;
constexpr size_t kMaskWriteOrMaskOffset = 4;
// Array index offsets for file record read
constexpr size_t kFileRecordReadFileNumberOffset = 0;
constexpr size_t kFileRecordReadRecordNumberOffset = 2;
constexpr size_t kFileRecordReadRecordLengthOffset = 4;
}  // namespace

TcpResponse TcpSlave::Process(const TcpRequest &request) {
  // Increment communication event counter and add to log
  ++com_event_counter_;
  ++message_count_;
  if (com_event_log_.size() >= kComEventLogMaxSize) {
    com_event_log_.erase(com_event_log_.begin());  // Remove oldest entry
  }
  ComEventLogEntry new_entry{static_cast<uint16_t>(request.GetFunctionCode()), com_event_counter_};
  com_event_log_.push_back(new_entry);

  TcpResponse response{request.GetTransactionId(), request.GetUnitId(), request.GetFunctionCode()};
  switch (request.GetFunctionCode()) {
    case FunctionCode::kReadHR: {
      ProcessReadRegisters(holding_registers_, request, response);
      break;
    }
    case FunctionCode::kReadIR: {
      ProcessReadRegisters(input_registers_, request, response);
      break;
    }
    case FunctionCode::kReadCoils: {
      ProcessReadCoils(coils_, request, response);
      break;
    }
    case FunctionCode::kReadDI: {
      ProcessReadCoils(discrete_inputs_, request, response);
      break;
    }
    case FunctionCode::kWriteSingleReg: {
      ProcessWriteSingleRegister(holding_registers_, request, response);
      break;
    }
    case FunctionCode::kWriteSingleCoil: {
      ProcessWriteSingleCoil(coils_, request, response);
      break;
    }
    case FunctionCode::kWriteMultRegs: {
      ProcessWriteMultipleRegisters(holding_registers_, request, response);
      break;
    }
    case FunctionCode::kWriteMultCoils: {
      ProcessWriteMultipleCoils(coils_, request, response);
      break;
    }
    case FunctionCode::kReadExceptionStatus: {
      ProcessReadExceptionStatus(request, response);
      break;
    }
    case FunctionCode::kDiagnostics: {
      ProcessDiagnostics(request, response);
      break;
    }
    case FunctionCode::kGetComEventCounter: {
      ProcessGetComEventCounter(request, response);
      break;
    }
    case FunctionCode::kGetComEventLog: {
      ProcessGetComEventLog(request, response);
      break;
    }
    case FunctionCode::kReportSlaveID: {
      ProcessReportSlaveID(request, response);
      break;
    }
    case FunctionCode::kReadFileRecord: {
      ProcessReadFileRecord(request, response);
      break;
    }
    case FunctionCode::kWriteFileRecord: {
      ProcessWriteFileRecord(request, response);
      break;
    }
    case FunctionCode::kMaskWriteReg: {
      ProcessMaskWriteRegister(holding_registers_, request, response);
      break;
    }
    case FunctionCode::kReadWriteMultRegs: {
      ProcessReadWriteMultipleRegisters(holding_registers_, request, response);
      break;
    }
    case FunctionCode::kReadFIFOQueue: {
      ProcessReadFIFOQueue(request, response);
      break;
    }
    default: {
      response.SetExceptionCode(ExceptionCode::kIllegalFunction);
      break;
    }
  }

  return response;
}

void TcpSlave::AddHoldingRegisters(AddressSpan span) {
  holding_registers_.AddAddressSpan(span);
}

void TcpSlave::AddInputRegisters(AddressSpan span) {
  input_registers_.AddAddressSpan(span);
}

void TcpSlave::AddCoils(AddressSpan span) {
  coils_.AddAddressSpan(span);
}

void TcpSlave::AddDiscreteInputs(AddressSpan span) {
  discrete_inputs_.AddAddressSpan(span);
}

void TcpSlave::ProcessReadRegisters(const AddressMap<int16_t> &address_map, const TcpRequest &request,
                                    TcpResponse &response) {
  bool exception_hit = false;
  const auto maybe_address_span = request.GetAddressSpan();
  if (!maybe_address_span.has_value()) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  const AddressSpan &address_span = maybe_address_span.value();
  // Calculate byte count (2 bytes per register)
  auto byte_count = static_cast<uint8_t>(address_span.reg_count * 2);
  response.EmplaceBack(byte_count);

  for (int i = 0; i < address_span.reg_count; ++i) {
    const auto reg_value = address_map[address_span.start_address + i];
    if (reg_value.has_value()) {
      // Modbus RTU uses big-endian: high byte first, then low byte
      response.EmplaceBack(GetHighByte(reg_value.value()));
      response.EmplaceBack(GetLowByte(reg_value.value()));
    } else {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      exception_hit = true;
    }
  }

  if (!exception_hit) {
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
  }
}

void TcpSlave::ProcessWriteSingleRegister(AddressMap<int16_t> &address_map, const TcpRequest &request,
                                          TcpResponse &response) {
  if (request.GetData().size() < 4) {
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  // SetWriteSingleRegisterData writes: [high_addr, low_addr, high_val, low_val]
  // Modbus uses low-byte-first, so we need to read as MakeInt16(low_byte, high_byte)
  // For [high, low] in data, we need MakeInt16(data[1], data[0])
  const uint16_t address = MakeInt16(request.GetData()[1], request.GetData()[0]);
  int16_t new_value = MakeInt16(request.GetData()[3], request.GetData()[2]);
  if (address_map[address].has_value()) {
    address_map.Set(address, new_value);
    // WriteSingleReg response should echo back address and value (Modbus spec)
    // Match request format: [high_addr, low_addr, high_val, low_val]
    response.EmplaceBack(GetHighByte(address));
    response.EmplaceBack(GetLowByte(address));
    response.EmplaceBack(GetHighByte(new_value));
    response.EmplaceBack(GetLowByte(new_value));
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
  } else {
    response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
  }
}

void TcpSlave::ProcessReadCoils(const AddressMap<bool> &address_map, const TcpRequest &request, TcpResponse &response) {
  bool exception_hit = false;
  const auto maybe_address_span = request.GetAddressSpan();
  if (!maybe_address_span.has_value()) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  const AddressSpan &address_span = maybe_address_span.value();

  // Calculate number of bytes needed (8 coils per byte, rounded up)
  auto byte_count =
      static_cast<uint8_t>((address_span.reg_count + supermb::kCoilByteCountRoundingOffset) / supermb::kCoilsPerByte);
  response.EmplaceBack(byte_count);  // First byte is the byte count

  uint8_t current_byte = 0;
  uint8_t bit_position = 0;

  for (int i = 0; i < address_span.reg_count; ++i) {
    const auto coil_value = address_map[address_span.start_address + i];
    if (coil_value.has_value()) {
      if (coil_value.value()) {
        current_byte |= (1 << bit_position);
      }
      ++bit_position;

      // If we've filled a byte or reached the end, emit it
      if (bit_position == supermb::kCoilsPerByte || i == address_span.reg_count - 1) {
        response.EmplaceBack(current_byte);
        current_byte = 0;
        bit_position = 0;
      }
    } else {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      exception_hit = true;
      break;
    }
  }

  if (!exception_hit) {
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
  }
}

void TcpSlave::ProcessWriteSingleCoil(AddressMap<bool> &address_map, const TcpRequest &request, TcpResponse &response) {
  if (request.GetData().size() < 4) {
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  const uint16_t address = MakeInt16(request.GetData()[1], request.GetData()[0]);
  const uint16_t value = MakeInt16(request.GetData()[3], request.GetData()[2]);

  // Modbus spec: 0x0000 = OFF, 0xFF00 = ON
  bool coil_value = (value == supermb::kCoilOnValue);

  if (address_map[address].has_value()) {
    address_map.Set(address, coil_value);
    // WriteSingleCoil response should echo back address and value (Modbus spec)
    // Match request format: [high_addr, low_addr, high_val, low_val]
    response.EmplaceBack(GetHighByte(address));
    response.EmplaceBack(GetLowByte(address));
    response.EmplaceBack(GetHighByte(value));
    response.EmplaceBack(GetLowByte(value));
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
  } else {
    response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
  }
}

void TcpSlave::ProcessWriteMultipleRegisters(AddressMap<int16_t> &address_map, const TcpRequest &request,
                                             TcpResponse &response) {
  const auto &data = request.GetData();

  // Data format: address (2), count (2), byte_count (1), values (count * 2)
  if (data.size() < kMinWriteDataSize) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint16_t start_address = MakeInt16(data[1], data[0]);
  uint16_t count = MakeInt16(data[3], data[2]);
  uint8_t byte_count = data[4];

  if (byte_count != static_cast<uint8_t>(count * 2) || data.size() < kMinWriteDataSize + byte_count) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Check if all addresses exist
  for (int i = 0; i < count; ++i) {
    if (!address_map[start_address + i].has_value()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      return;
    }
  }

  // Write all registers
  size_t data_offset = kMinWriteDataSize;  // Skip address, count, byte_count
  for (int i = 0; i < count; ++i) {
    if (data_offset + 1 >= data.size()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }
    // Modbus RTU uses big-endian: high byte first, then low byte
    int16_t value = MakeInt16(data[data_offset + 1], data[data_offset]);
    address_map.Set(start_address + i, value);
    data_offset += 2;
  }

  // WriteMultipleRegisters response should echo back address and count (Modbus spec)
  // Format: [high_addr, low_addr, high_count, low_count]
  response.EmplaceBack(GetHighByte(start_address));
  response.EmplaceBack(GetLowByte(start_address));
  response.EmplaceBack(GetHighByte(count));
  response.EmplaceBack(GetLowByte(count));
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void TcpSlave::ProcessWriteMultipleCoils(AddressMap<bool> &address_map, const TcpRequest &request,
                                         TcpResponse &response) {
  const auto &data = request.GetData();

  // Data format: address (2), count (2), byte_count (1), coil values (packed)
  if (data.size() < kMinWriteDataSize) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint16_t start_address = MakeInt16(data[1], data[0]);
  uint16_t count = MakeInt16(data[3], data[2]);
  uint8_t byte_count = data[4];

  auto expected_byte_count = static_cast<uint8_t>((count + kCoilByteCountRoundingOffset) / kCoilsPerByte);
  if (byte_count != expected_byte_count || data.size() < kMinWriteDataSize + byte_count) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Check if all addresses exist
  for (int i = 0; i < count; ++i) {
    if (!address_map[start_address + i].has_value()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      return;
    }
  }

  // Unpack and write coils
  size_t data_offset = kMinWriteDataSize;  // Skip address, count, byte_count
  for (int i = 0; i < count; ++i) {
    uint8_t byte_index = i / supermb::kCoilsPerByte;
    uint8_t bit_index = i % supermb::kCoilsPerByte;
    if (data_offset + byte_index >= data.size()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }
    bool coil_value = (data[data_offset + byte_index] & (1 << bit_index)) != 0;
    address_map.Set(start_address + i, coil_value);
  }

  // WriteMultipleCoils response should echo back address and count (Modbus spec)
  // Format: [high_addr, low_addr, high_count, low_count]
  response.EmplaceBack(GetHighByte(start_address));
  response.EmplaceBack(GetLowByte(start_address));
  response.EmplaceBack(GetHighByte(count));
  response.EmplaceBack(GetLowByte(count));
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

bool TcpSlave::ProcessIncomingFrame(ByteTransport &transport, uint32_t timeout_ms) {
  auto frame = ReadFrame(transport, timeout_ms);
  if (!frame.has_value()) {
    return false;
  }

  // Decode the request frame
  auto request = TcpFrame::DecodeRequest(std::span<const uint8_t>(frame->data(), frame->size()));
  if (!request.has_value()) {
    return false;  // Invalid frame
  }

  // Check if this request is for this slave
  // Accept broadcast (slave ID 0) for write operations only
  uint8_t request_unit_id = request->GetUnitId();
  if (request_unit_id != id_) {
    return false;  // Not for this slave
  }

  // Process the request (Process increments com_event_counter_, message_count_, and adds to com_event_log_)
  TcpResponse response = Process(*request);

  // Encode and send response
  std::vector<uint8_t> response_frame = TcpFrame::EncodeResponse(response);
  int bytes_written = transport.Write(std::span<const uint8_t>(response_frame.data(), response_frame.size()));
  if (bytes_written != static_cast<int>(response_frame.size())) {
    return false;
  }

  (void)transport.Flush();  // Flush may return error, but we've already written the frame
  return true;
}

bool TcpSlave::Poll(ByteTransport &transport) {
  // Check if data is available
  if (!transport.HasData()) {
    return false;
  }

  // Try to process a frame (with minimal timeout since we know data is available)
  return ProcessIncomingFrame(transport, kDefaultPollTimeoutMs);
}

void TcpSlave::ProcessReadExceptionStatus(const TcpRequest & /*request*/, TcpResponse &response) const {
  // FC 7: Read Exception Status - Returns 1 byte exception status
  // For simplicity, we'll return a configurable exception status byte
  response.EmplaceBack(exception_status_);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void TcpSlave::ProcessDiagnostics(const TcpRequest &request, TcpResponse &response) {
  // FC 8: Diagnostics - Sub-function code based
  // Returns the data sent in the request (echo)
  const auto &data = request.GetData();
  if (data.size() < 2) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Echo back the data (sub-function code + data)
  for (uint8_t byte : data) {
    response.EmplaceBack(byte);
  }
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void TcpSlave::ProcessGetComEventCounter(const TcpRequest & /*request*/, TcpResponse &response) const {
  // FC 11: Get Com Event Counter - Returns status (2 bytes) + event count (2 bytes)
  // Per Modbus spec: status is a 2-byte word (0x0000 = ready, 0xFFFF = busy)
  // Modbus uses big-endian: high byte first, then low byte
  response.EmplaceBack(0x00);  // Status high byte
  response.EmplaceBack(0x00);  // Status low byte (0x0000 = ready)
  response.EmplaceBack(GetHighByte(com_event_counter_));
  response.EmplaceBack(GetLowByte(com_event_counter_));
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void TcpSlave::ProcessGetComEventLog(const TcpRequest & /*request*/, TcpResponse &response) const {
  // FC 12: Get Com Event Log - Returns status, event count, message count, and events
  // Modbus uses big-endian: high byte first, then low byte
  response.EmplaceBack(0x00);  // Status (0x00 = no error, kMaxByteValue = error)
  response.EmplaceBack(GetHighByte(com_event_counter_));
  response.EmplaceBack(GetLowByte(com_event_counter_));
  response.EmplaceBack(GetHighByte(message_count_));
  response.EmplaceBack(GetLowByte(message_count_));

  // Add event log entries (up to kComEventLogMaxSize events max per Modbus spec)
  // Modbus uses big-endian: high byte first, then low byte
  size_t event_count = std::min(com_event_log_.size(), size_t{kComEventLogMaxSize});
  for (size_t i = 0; i < event_count; ++i) {
    const auto &entry = com_event_log_[i];
    response.EmplaceBack(GetHighByte(entry.event_id));
    response.EmplaceBack(GetLowByte(entry.event_id));
    response.EmplaceBack(GetHighByte(entry.event_count));
    response.EmplaceBack(GetLowByte(entry.event_count));
  }

  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void TcpSlave::ProcessReportSlaveID(const TcpRequest & /*request*/, TcpResponse &response) const {
  // FC 17: Report Slave ID - Returns slave ID, run indicator, and additional data
  response.EmplaceBack(id_);            // Slave ID
  response.EmplaceBack(kMaxByteValue);  // Run indicator (kMaxByteValue = run, 0x00 = stop)
  // Additional data (optional, device-specific)
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void TcpSlave::ProcessReadFileRecord(const TcpRequest &request, TcpResponse &response) const {
  // FC 20: Read File Record - Read records from files
  // Request format: byte_count (1) + [file_number (2) + record_number (2) + record_length (2)] * N
  const auto &data = request.GetData();
  if (data.empty()) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint8_t byte_count = data[0];
  if (byte_count == 0 || data.size() < static_cast<size_t>(1 + byte_count)) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Response format: response_length (1) + [reference_type (1) + data_length (1) + file_number (2) + record_number (2)
  // + record_data (N*2)] * N
  std::vector<uint8_t> response_data;
  size_t data_offset = 1;  // Skip byte_count

  while (data_offset < data.size()) {
    if (data_offset + kFileRecordReadMinSize > data.size()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }

    // File record read format: file_number(2) + record_number(2) + record_length(2) = 6 bytes
    uint16_t file_number = MakeInt16(data[data_offset + kFileRecordReadFileNumberOffset + 1],
                                     data[data_offset + kFileRecordReadFileNumberOffset]);
    uint16_t record_number = MakeInt16(data[data_offset + kFileRecordReadRecordNumberOffset + 1],
                                       data[data_offset + kFileRecordReadRecordNumberOffset]);
    uint16_t record_length = MakeInt16(data[data_offset + kFileRecordReadRecordLengthOffset + 1],
                                       data[data_offset + kFileRecordReadRecordLengthOffset]);
    data_offset += kFileRecordReadMinSize;

    // Check if file and record exist
    auto file_it = file_storage_.find(file_number);
    if (file_it == file_storage_.end()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      return;
    }

    auto record_it = file_it->second.find(record_number);
    if (record_it == file_it->second.end() || record_it->second.size() < record_length) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      return;
    }

    // Add record to response: reference_type (1) + data_length (1) + file_number (2) + record_number (2) + data
    // (record_length * 2)
    response_data.emplace_back(
        supermb::kFileRecordReferenceType);  // Reference type (kFileRecordReferenceType = file record)
    response_data.emplace_back(
        static_cast<uint8_t>(record_length * 2 + 4));  // Data length (record data + file/record numbers)
    response_data.emplace_back(GetHighByte(file_number));
    response_data.emplace_back(GetLowByte(file_number));
    response_data.emplace_back(GetHighByte(record_number));
    response_data.emplace_back(GetLowByte(record_number));

    // Add record data
    // Modbus RTU uses big-endian: high byte first, then low byte
    for (uint16_t i = 0; i < record_length && i < record_it->second.size(); ++i) {
      response_data.emplace_back(GetHighByte(record_it->second[i]));
      response_data.emplace_back(GetLowByte(record_it->second[i]));
    }
  }

  // Prepend response length as first byte
  response_data.insert(response_data.begin(), static_cast<uint8_t>(response_data.size()));
  response.SetData(response_data);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void TcpSlave::ProcessWriteFileRecord(const TcpRequest &request, TcpResponse &response) {
  // FC 21: Write File Record - Write records to files
  // Request format: byte_count (1) + [reference_type (1) + file_number (2) + record_number (2) + record_length (2) +
  // record_data (N*2)] * N
  const auto &data = request.GetData();
  if (data.empty()) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint8_t byte_count = data[0];
  if (byte_count == 0 || data.size() < static_cast<size_t>(1 + byte_count)) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  size_t data_offset = 1;  // Skip byte_count

  while (data_offset < data.size()) {
    if (data_offset + kFileRecordMinHeaderSize > data.size()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }

    uint8_t reference_type = data[data_offset];
    if (reference_type !=
        supermb::kFileRecordReferenceType) {  // Only kFileRecordReferenceType (file record) is supported
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }

    // File record write format: reference_type(1) + file_number(2) + record_number(2) + record_length(2) = 7 bytes
    uint16_t file_number = MakeInt16(data[data_offset + 2], data[data_offset + 1]);
    uint16_t record_number = MakeInt16(data[data_offset + 4], data[data_offset + 3]);
    uint16_t record_length = MakeInt16(data[data_offset + supermb::kFileRecordBytesPerRecord],
                                       data[data_offset + supermb::kFileRecordBytesPerRecord - 1]);
    data_offset += kFileRecordMinHeaderSize;

    if (data_offset + static_cast<size_t>(record_length) * 2 > data.size()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }

    // Create or update file record
    FileRecord record_data;
    record_data.reserve(record_length);
    for (uint16_t i = 0; i < record_length; ++i) {
      // Modbus RTU uses big-endian: high byte first, then low byte
      int16_t value = MakeInt16(data[data_offset + 1], data[data_offset]);
      record_data.push_back(value);
      data_offset += 2;
    }

    // Store the record
    file_storage_[file_number][record_number] = std::move(record_data);
  }

  // Response echoes back the request data (byte_count + all records)
  std::vector<uint8_t> response_data(data.begin(), data.end());
  response.SetData(response_data);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void TcpSlave::ProcessMaskWriteRegister(AddressMap<int16_t> &address_map, const TcpRequest &request,
                                        TcpResponse &response) {
  // FC 22: Mask Write Register - Write with AND mask and OR mask
  const auto &data = request.GetData();
  if (data.size() < kMinMaskWriteDataSize) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Mask write format: address(2) + and_mask(2) + or_mask(2) = 6 bytes
  uint16_t address = MakeInt16(data[kMaskWriteAddressOffset + 1], data[kMaskWriteAddressOffset]);
  uint16_t and_mask = MakeInt16(data[kMaskWriteAndMaskOffset + 1], data[kMaskWriteAndMaskOffset]);
  uint16_t or_mask = MakeInt16(data[kMaskWriteOrMaskOffset + 1], data[kMaskWriteOrMaskOffset]);

  auto current_value = address_map[address];
  if (!current_value.has_value()) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
    return;
  }

  // Apply mask: (current & and_mask) | or_mask
  auto new_value = static_cast<int16_t>((static_cast<uint16_t>(current_value.value()) & and_mask) | or_mask);
  address_map.Set(address, new_value);

  // Response echoes back address, and_mask, or_mask
  response.EmplaceBack(GetHighByte(address));
  response.EmplaceBack(GetLowByte(address));
  response.EmplaceBack(GetHighByte(and_mask));
  response.EmplaceBack(GetLowByte(and_mask));
  response.EmplaceBack(GetHighByte(or_mask));
  response.EmplaceBack(GetLowByte(or_mask));
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void TcpSlave::ProcessReadWriteMultipleRegisters(AddressMap<int16_t> &address_map, const TcpRequest &request,
                                                 TcpResponse &response) {
  // FC 23: Read/Write Multiple Registers - Read from one area, write to another
  const auto &data = request.GetData();
  if (data.size() < kMinReadWriteDataSize) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Parse read request: read_start (2), read_count (2)
  uint16_t read_start = MakeInt16(data[kReadWriteReadStartOffset + 1], data[kReadWriteReadStartOffset]);
  uint16_t read_count = MakeInt16(data[kReadWriteReadCountOffset + 1], data[kReadWriteReadCountOffset]);

  // Parse write request: write_start (2), write_count (2), write_byte_count (1), write_values (N*2)
  uint16_t write_start = MakeInt16(data[kReadWriteWriteStartOffset + 1], data[kReadWriteWriteStartOffset]);
  uint16_t write_count = MakeInt16(data[kReadWriteWriteCountOffset + 1], data[kReadWriteWriteCountOffset]);
  uint8_t write_byte_count = data[kReadWriteByteCountOffset];

  if (write_byte_count != static_cast<uint8_t>(write_count * 2) ||
      data.size() < kMinReadWriteDataWithValues + write_byte_count) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // First, perform the write operation
  size_t data_offset = kMinReadWriteDataWithValues;
  for (int i = 0; i < write_count; ++i) {
    if (!address_map[write_start + i].has_value()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      return;
    }
    if (data_offset + 1 >= data.size()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }
    // Modbus RTU uses big-endian: high byte first, then low byte
    int16_t value = MakeInt16(data[data_offset + 1], data[data_offset]);
    address_map.Set(write_start + i, value);
    data_offset += 2;
  }

  // Then, perform the read operation
  auto byte_count = static_cast<uint8_t>(read_count * 2);
  response.EmplaceBack(byte_count);  // FC23 response format: byte_count prefix, then register data

  for (int i = 0; i < read_count; ++i) {
    auto reg_value = address_map[read_start + i];
    if (!reg_value.has_value()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      return;
    }
    // Modbus RTU uses big-endian: high byte first, then low byte
    response.EmplaceBack(GetHighByte(reg_value.value()));
    response.EmplaceBack(GetLowByte(reg_value.value()));
  }

  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void TcpSlave::ProcessReadFIFOQueue(const TcpRequest &request, TcpResponse &response) const {
  // FC 24: Read FIFO Queue - Read from FIFO queue at specified address
  const auto &data = request.GetData();
  if (data.size() < 2) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint16_t fifo_address = MakeInt16(data[1], data[0]);

  // Check if FIFO queue exists
  auto fifo_it = fifo_storage_.find(fifo_address);
  if (fifo_it == fifo_storage_.end() || fifo_it->second.empty()) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
    return;
  }

  const FIFOQueue &fifo_queue = fifo_it->second;
  auto fifo_count = static_cast<uint16_t>(fifo_queue.size());
  // Per Modbus spec: byte_count includes the 2-byte FIFO count field + data
  uint16_t byte_count = 2 + (fifo_count * 2);  // 2 for fifo_count + data bytes

  // FIFO response: byte count (2), FIFO count (2), FIFO data (fifo_count * 2)
  // Modbus RTU uses big-endian: high byte first, then low byte
  response.EmplaceBack(GetHighByte(byte_count));
  response.EmplaceBack(GetLowByte(byte_count));
  response.EmplaceBack(GetHighByte(fifo_count));
  response.EmplaceBack(GetLowByte(fifo_count));

  // Add FIFO queue data
  for (int16_t value : fifo_queue) {
    // Modbus RTU uses big-endian: high byte first, then low byte
    response.EmplaceBack(GetHighByte(value));
    response.EmplaceBack(GetLowByte(value));
  }

  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

std::optional<std::vector<uint8_t>> TcpSlave::ReadFrame(ByteReader &transport, uint32_t timeout_ms) {
  std::vector<uint8_t> buffer;
  buffer.reserve(256);  // Reserve space for typical frame

  auto start_time = std::chrono::steady_clock::now();

  // Read MBAP header first (7 bytes)
  while (buffer.size() < 7) {
    if (timeout_ms > 0) {
      auto elapsed = std::chrono::steady_clock::now() - start_time;
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
      if (elapsed_ms >= static_cast<int64_t>(timeout_ms)) {
        return {};
      }
    }

    if (!transport.HasData()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    size_t current_size = buffer.size();
    size_t needed = 7 - current_size;
    buffer.resize(7);
    std::span<uint8_t> read_span(buffer.data() + current_size, needed);
    int bytes_read = transport.Read(read_span);
    if (bytes_read <= 0) {
      buffer.resize(current_size);  // Restore original size on error
      return {};
    }
    buffer.resize(current_size + static_cast<size_t>(bytes_read));
  }

  // Extract length from MBAP header (bytes 4-5, big-endian)
  uint16_t length = static_cast<uint16_t>((static_cast<uint16_t>(buffer[4]) << 8) | static_cast<uint16_t>(buffer[5]));

  // Read remaining bytes
  // MBAP header = Transaction ID(2) + Protocol ID(2) + Length(2) + Unit ID(1) = 7 bytes
  // Length field value = Unit ID(1) + PDU size
  // Total frame size = 7 + (length - 1) = 6 + length
  size_t total_size = 6 + length;
  while (buffer.size() < total_size) {
    if (timeout_ms > 0) {
      auto elapsed = std::chrono::steady_clock::now() - start_time;
      auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
      if (elapsed_ms >= static_cast<int64_t>(timeout_ms)) {
        return {};
      }
    }

    if (!transport.HasData()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    size_t current_size = buffer.size();
    size_t needed = total_size - current_size;
    buffer.resize(total_size);
    std::span<uint8_t> read_span(buffer.data() + current_size, needed);
    int bytes_read = transport.Read(read_span);
    if (bytes_read <= 0) {
      buffer.resize(current_size);  // Restore original size on error
      return {};
    }
    buffer.resize(current_size + static_cast<size_t>(bytes_read));
  }

  return buffer;
}

}  // namespace supermb
