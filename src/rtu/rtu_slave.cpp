#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>
#include <optional>
#include <thread>
#include <vector>
#include "common/address_map.hpp"
#include "common/byte_helpers.hpp"
#include "common/function_code.hpp"
#include "rtu/rtu_frame.hpp"
#include "rtu/rtu_request.hpp"
#include "rtu/rtu_response.hpp"
#include "rtu/rtu_slave.hpp"
#include "common/exception_code.hpp"
#include "transport/byte_reader.hpp"

using supermb::IsBroadcastableWrite;

namespace supermb {

RtuResponse RtuSlave::Process(RtuRequest const &request) {
  RtuResponse response{request.GetSlaveId(), request.GetFunctionCode()};
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

void RtuSlave::AddHoldingRegisters(AddressSpan span) {
  holding_registers_.AddAddressSpan(span);
}

void RtuSlave::AddInputRegisters(AddressSpan span) {
  input_registers_.AddAddressSpan(span);
}

void RtuSlave::AddCoils(AddressSpan span) {
  coils_.AddAddressSpan(span);
}

void RtuSlave::AddDiscreteInputs(AddressSpan span) {
  discrete_inputs_.AddAddressSpan(span);
}

void RtuSlave::ProcessReadRegisters(AddressMap<int16_t> const &address_map, RtuRequest const &request,
                                    RtuResponse &response) {
  bool exception_hit = false;
  auto const maybe_address_span = request.GetAddressSpan();
  if (!maybe_address_span.has_value()) {
    assert(false);  // likely a library defect if hit - create ticket in github
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  AddressSpan const &address_span = maybe_address_span.value();
  // Calculate byte count (2 bytes per register)
  uint8_t byte_count = static_cast<uint8_t>(address_span.reg_count * 2);
  response.EmplaceBack(byte_count);

  for (int i = 0; i < address_span.reg_count; ++i) {
    auto const reg_value = address_map[address_span.start_address + i];
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

void RtuSlave::ProcessWriteSingleRegister(AddressMap<int16_t> &address_map, RtuRequest const &request,
                                          RtuResponse &response) {
  if (request.GetData().size() < 4) {
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  // SetWriteSingleRegisterData writes: [high_addr, low_addr, high_val, low_val]
  // Modbus uses low-byte-first, so we need to read as MakeInt16(low_byte, high_byte)
  // For [high, low] in data, we need MakeInt16(data[1], data[0])
  uint16_t const address = MakeInt16(request.GetData()[1], request.GetData()[0]);
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

void RtuSlave::ProcessReadCoils(AddressMap<bool> const &address_map, RtuRequest const &request,
                                 RtuResponse &response) {
  bool exception_hit = false;
  auto const maybe_address_span = request.GetAddressSpan();
  if (!maybe_address_span.has_value()) {
    assert(false);  // likely a library defect if hit - create ticket in github
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  AddressSpan const &address_span = maybe_address_span.value();

  // Calculate number of bytes needed (8 coils per byte, rounded up)
  uint8_t byte_count = static_cast<uint8_t>((address_span.reg_count + 7) / 8);
  response.EmplaceBack(byte_count);  // First byte is the byte count

  uint8_t current_byte = 0;
  uint8_t bit_position = 0;

  for (int i = 0; i < address_span.reg_count; ++i) {
    auto const coil_value = address_map[address_span.start_address + i];
    if (coil_value.has_value()) {
      if (coil_value.value()) {
        current_byte |= (1 << bit_position);
      }
      ++bit_position;

      // If we've filled a byte or reached the end, emit it
      if (bit_position == 8 || i == address_span.reg_count - 1) {
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

void RtuSlave::ProcessWriteSingleCoil(AddressMap<bool> &address_map, RtuRequest const &request,
                                       RtuResponse &response) {
  if (request.GetData().size() < 4) {
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  uint16_t const address = MakeInt16(request.GetData()[1], request.GetData()[0]);
  uint16_t const value = MakeInt16(request.GetData()[3], request.GetData()[2]);

  // Modbus spec: 0x0000 = OFF, 0xFF00 = ON
  bool coil_value = (value == 0xFF00);

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

void RtuSlave::ProcessWriteMultipleRegisters(AddressMap<int16_t> &address_map, RtuRequest const &request,
                                              RtuResponse &response) {
  auto const &data = request.GetData();

  // Data format: address (2), count (2), byte_count (1), values (count * 2)
  if (data.size() < 5) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint16_t start_address = MakeInt16(data[1], data[0]);
  uint16_t count = MakeInt16(data[3], data[2]);
  uint8_t byte_count = data[4];

  if (byte_count != static_cast<uint8_t>(count * 2) || data.size() < 5 + byte_count) {
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
  size_t data_offset = 5;  // Skip address, count, byte_count
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

void RtuSlave::ProcessWriteMultipleCoils(AddressMap<bool> &address_map, RtuRequest const &request,
                                          RtuResponse &response) {
  auto const &data = request.GetData();

  // Data format: address (2), count (2), byte_count (1), coil values (packed)
  if (data.size() < 5) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint16_t start_address = MakeInt16(data[1], data[0]);
  uint16_t count = MakeInt16(data[3], data[2]);
  uint8_t byte_count = data[4];

  uint8_t expected_byte_count = static_cast<uint8_t>((count + 7) / 8);
  if (byte_count != expected_byte_count || data.size() < 5 + byte_count) {
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
  size_t data_offset = 5;  // Skip address, count, byte_count
  for (int i = 0; i < count; ++i) {
    uint8_t byte_index = i / 8;
    uint8_t bit_index = i % 8;
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

bool RtuSlave::ProcessIncomingFrame(ByteTransport &transport, uint32_t timeout_ms) {
  auto frame = ReadFrame(transport, timeout_ms);
  if (!frame.has_value()) {
    return false;
  }

  // Decode the request frame
  auto request = RtuFrame::DecodeRequest(std::span<uint8_t const>(frame->data(), frame->size()));
  if (!request.has_value()) {
    return false;  // Invalid frame
  }

  // Check if this request is for this slave
  // Accept broadcast (slave ID 0) for write operations only
  uint8_t request_slave_id = request->GetSlaveId();
  bool is_broadcast = (request_slave_id == 0);
  bool is_broadcastable_write = IsBroadcastableWrite(request->GetFunctionCode());

  if (is_broadcast) {
    // Broadcast messages are only valid for write operations
    if (!is_broadcastable_write) {
      return false;  // Invalid broadcast (read operations cannot be broadcast)
    }
    // Process broadcast write, but don't send response
  } else if (request_slave_id != id_) {
    return false;  // Not for this slave
  }

  // Increment communication event counter and add to log
  ++com_event_counter_;
  ++message_count_;
  if (com_event_log_.size() >= 64) {
    com_event_log_.erase(com_event_log_.begin());  // Remove oldest entry
  }
  ComEventLogEntry new_entry{static_cast<uint16_t>(request->GetFunctionCode()), com_event_counter_};
  com_event_log_.push_back(new_entry);

  // Process the request
  RtuResponse response = Process(*request);

  // Broadcast messages don't receive responses (Modbus spec)
  if (is_broadcast) {
    return true;  // Successfully processed broadcast, but no response sent
  }

  // Encode and send response for non-broadcast messages
  std::vector<uint8_t> response_frame = RtuFrame::EncodeResponse(response);
  int bytes_written = transport.Write(std::span<uint8_t const>(response_frame.data(), response_frame.size()));
  if (bytes_written != static_cast<int>(response_frame.size())) {
    return false;
  }

  transport.Flush();
  return true;
}

bool RtuSlave::Poll(ByteTransport &transport) {
  // Check if data is available
  if (!transport.HasData()) {
    return false;
  }

  // Try to process a frame (with minimal timeout since we know data is available)
  return ProcessIncomingFrame(transport, 100);
}

void RtuSlave::ProcessReadExceptionStatus(RtuRequest const & /*request*/, RtuResponse &response) {
  // FC 7: Read Exception Status - Returns 1 byte exception status
  // For simplicity, we'll return a configurable exception status byte
  response.EmplaceBack(exception_status_);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessDiagnostics(RtuRequest const &request, RtuResponse &response) {
  // FC 8: Diagnostics - Sub-function code based
  // Returns the data sent in the request (echo)
  auto const &data = request.GetData();
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

void RtuSlave::ProcessGetComEventCounter(RtuRequest const &request, RtuResponse &response) {
  // FC 11: Get Com Event Counter - Returns 2 bytes: status (1) + event count (2)
  response.EmplaceBack(0x00);  // Status: 0x00 = no error, 0xFF = error
  response.EmplaceBack(GetLowByte(com_event_counter_));
  response.EmplaceBack(GetHighByte(com_event_counter_));
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessGetComEventLog(RtuRequest const & /*request*/, RtuResponse &response) {
  // FC 12: Get Com Event Log - Returns status, event count, message count, and events
  response.EmplaceBack(0x00);  // Status (0x00 = no error, 0xFF = error)
  response.EmplaceBack(GetLowByte(com_event_counter_));
  response.EmplaceBack(GetHighByte(com_event_counter_));
  response.EmplaceBack(GetLowByte(message_count_));
  response.EmplaceBack(GetHighByte(message_count_));

  // Add event log entries (up to 64 events max per Modbus spec)
  size_t event_count = std::min(com_event_log_.size(), size_t{64});
  for (size_t i = 0; i < event_count; ++i) {
    const auto &entry = com_event_log_[i];
    response.EmplaceBack(GetLowByte(entry.event_id));
    response.EmplaceBack(GetHighByte(entry.event_id));
    response.EmplaceBack(GetLowByte(entry.event_count));
    response.EmplaceBack(GetHighByte(entry.event_count));
  }

  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessReportSlaveID(RtuRequest const & /*request*/, RtuResponse &response) {
  // FC 17: Report Slave ID - Returns slave ID, run indicator, and additional data
  response.EmplaceBack(id_);           // Slave ID
  response.EmplaceBack(0xFF);         // Run indicator (0xFF = run, 0x00 = stop)
  // Additional data (optional, device-specific)
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessReadFileRecord(RtuRequest const &request, RtuResponse &response) {
  // FC 20: Read File Record - Read records from files
  // Request format: byte_count (1) + [file_number (2) + record_number (2) + record_length (2)] * N
  auto const &data = request.GetData();
  if (data.empty() || data.size() < 1) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint8_t byte_count = data[0];
  if (byte_count == 0 || data.size() < 1 + byte_count) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Response format: response_length (1) + [reference_type (1) + data_length (1) + file_number (2) + record_number (2) + record_data (N*2)] * N
  std::vector<uint8_t> response_data;
  size_t data_offset = 1;  // Skip byte_count

  while (data_offset < data.size()) {
    if (data_offset + 6 > data.size()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }

    uint16_t file_number = MakeInt16(data[data_offset + 1], data[data_offset]);
    uint16_t record_number = MakeInt16(data[data_offset + 3], data[data_offset + 2]);
    uint16_t record_length = MakeInt16(data[data_offset + 5], data[data_offset + 4]);
    data_offset += 6;

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

    // Add record to response: reference_type (1) + data_length (1) + file_number (2) + record_number (2) + data (record_length * 2)
    response_data.emplace_back(0x06);  // Reference type (0x06 = file record)
    response_data.emplace_back(static_cast<uint8_t>(record_length * 2 + 4));  // Data length (record data + file/record numbers)
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

void RtuSlave::ProcessWriteFileRecord(RtuRequest const &request, RtuResponse &response) {
  // FC 21: Write File Record - Write records to files
  // Request format: byte_count (1) + [reference_type (1) + file_number (2) + record_number (2) + record_length (2) + record_data (N*2)] * N
  auto const &data = request.GetData();
  if (data.empty() || data.size() < 1) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint8_t byte_count = data[0];
  if (byte_count == 0 || data.size() < 1 + byte_count) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  size_t data_offset = 1;  // Skip byte_count

  while (data_offset < data.size()) {
    if (data_offset + 7 > data.size()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }

    uint8_t reference_type = data[data_offset];
    if (reference_type != 0x06) {  // Only 0x06 (file record) is supported
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }

    uint16_t file_number = MakeInt16(data[data_offset + 2], data[data_offset + 1]);
    uint16_t record_number = MakeInt16(data[data_offset + 4], data[data_offset + 3]);
    uint16_t record_length = MakeInt16(data[data_offset + 6], data[data_offset + 5]);
    data_offset += 7;

    if (data_offset + record_length * 2 > data.size()) {
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

void RtuSlave::ProcessMaskWriteRegister(AddressMap<int16_t> &address_map, RtuRequest const &request,
                                         RtuResponse &response) {
  // FC 22: Mask Write Register - Write with AND mask and OR mask
  auto const &data = request.GetData();
  if (data.size() < 6) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint16_t address = MakeInt16(data[1], data[0]);
  uint16_t and_mask = MakeInt16(data[3], data[2]);
  uint16_t or_mask = MakeInt16(data[5], data[4]);

  auto current_value = address_map[address];
  if (!current_value.has_value()) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
    return;
  }

  // Apply mask: (current & and_mask) | or_mask
  int16_t new_value = static_cast<int16_t>((static_cast<uint16_t>(current_value.value()) & and_mask) | or_mask);
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

void RtuSlave::ProcessReadWriteMultipleRegisters(AddressMap<int16_t> &address_map, RtuRequest const &request,
                                                  RtuResponse &response) {
  // FC 23: Read/Write Multiple Registers - Read from one area, write to another
  auto const &data = request.GetData();
  if (data.size() < 10) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Parse read request: read_start (2), read_count (2)
  uint16_t read_start = MakeInt16(data[1], data[0]);
  uint16_t read_count = MakeInt16(data[3], data[2]);

  // Parse write request: write_start (2), write_count (2), write_byte_count (1), write_values (N*2)
  uint16_t write_start = MakeInt16(data[5], data[4]);
  uint16_t write_count = MakeInt16(data[7], data[6]);
  uint8_t write_byte_count = data[8];

  if (write_byte_count != static_cast<uint8_t>(write_count * 2) || data.size() < 9 + write_byte_count) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // First, perform the write operation
  size_t data_offset = 9;
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

void RtuSlave::ProcessReadFIFOQueue(RtuRequest const &request, RtuResponse &response) {
  // FC 24: Read FIFO Queue - Read from FIFO queue at specified address
  auto const &data = request.GetData();
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
  uint16_t fifo_count = static_cast<uint16_t>(fifo_queue.size());
  uint16_t byte_count = fifo_count * 2;  // Each register is 2 bytes

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

std::optional<std::vector<uint8_t>> RtuSlave::ReadFrame(ByteReader &transport, uint32_t timeout_ms) {
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

    // Try to read bytes
    uint8_t temp_buffer[64];
    std::span<uint8_t> read_span(temp_buffer, sizeof(temp_buffer));
    int bytes_read = transport.Read(read_span);

    if (bytes_read > 0) {
      buffer.insert(buffer.end(), temp_buffer, temp_buffer + bytes_read);
      frame_started = true;

      // Check if we have a complete frame (slave reads requests)
      std::span<uint8_t const> frame_span(buffer.data(), buffer.size());
      if (RtuFrame::IsRequestFrameComplete(frame_span)) {
        return buffer;
      }
    } else if (frame_started && bytes_read == 0) {
      // Frame started but no more data available - might be complete (slave reads requests)
      std::span<uint8_t const> frame_span(buffer.data(), buffer.size());
      if (RtuFrame::IsRequestFrameComplete(frame_span)) {
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
