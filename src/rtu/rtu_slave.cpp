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
constexpr size_t kSlaveIdMaxSize = 256;
constexpr size_t kFifoQueueMaxSize = 64;
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

RtuResponse RtuSlave::Process(const RtuRequest &request) {
  // Increment communication event counter and add to log
  ++com_event_counter_;
  ++message_count_;
  if (com_event_log_.size() >= kComEventLogMaxSize) {
    com_event_log_.erase(com_event_log_.begin());  // Remove oldest entry
  }
  ComEventLogEntry new_entry{static_cast<uint16_t>(request.GetFunctionCode()), com_event_counter_};
  com_event_log_.push_back(new_entry);

  RtuResponse response{request.GetSlaveId(), request.GetFunctionCode()};
  switch (request.GetFunctionCode()) {
    case FunctionCode::kReadHR: {
      ProcessReadRegisters(holding_registers_, request, response, true);
      break;
    }
    case FunctionCode::kReadIR: {
      ProcessReadRegisters(input_registers_, request, response, false);
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

void RtuSlave::AddFloatRange(uint16_t start_register, uint16_t register_count) {
  if (register_count == 0 || (register_count % 2) != 0) {
    return;
  }
  float_range_ = {start_register, register_count};
  float_storage_.resize(register_count / 2, 0.0f);
}

bool RtuSlave::SetFloat(size_t float_index, float value) {
  if (float_index >= float_storage_.size()) {
    return false;
  }
  float_storage_[float_index] = value;
  return true;
}

void RtuSlave::ProcessReadRegisters(const AddressMap<int16_t> &address_map, const RtuRequest &request,
                                    RtuResponse &response, bool for_holding_registers) {
  bool exception_hit = false;
  const auto maybe_address_span = request.GetAddressSpan();
  if (!maybe_address_span.has_value()) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  const AddressSpan &address_span = maybe_address_span.value();

  // Float range: serve from float_storage_ when holding and span entirely within range
  if (for_holding_registers && float_range_.has_value() && (address_span.reg_count % 2) == 0) {
    const auto [range_start, range_count] = *float_range_;
    if (address_span.start_address >= range_start &&
        address_span.start_address + address_span.reg_count <= range_start + range_count) {
      const size_t float_index_start = (address_span.start_address - range_start) / 2;
      const size_t num_floats = address_span.reg_count / 2;
      if (float_index_start + num_floats <= float_storage_.size()) {
        auto byte_count = static_cast<uint8_t>(address_span.reg_count * 2);
        response.EmplaceBack(byte_count);
        uint8_t buf[4];
        for (size_t i = 0; i < num_floats; ++i) {
          EncodeFloat(float_storage_[float_index_start + i], options_.byte_order, options_.word_order, buf);
          response.EmplaceBack(buf[0]);
          response.EmplaceBack(buf[1]);
          response.EmplaceBack(buf[2]);
          response.EmplaceBack(buf[3]);
        }
        response.SetExceptionCode(ExceptionCode::kAcknowledge);
        return;
      }
    }
  }

  // Normal register path
  auto byte_count = static_cast<uint8_t>(address_span.reg_count * 2);
  response.EmplaceBack(byte_count);

  uint8_t buf[2];
  for (int i = 0; i < address_span.reg_count; ++i) {
    const auto reg_value = address_map[address_span.start_address + i];
    if (reg_value.has_value()) {
      EncodeU16(static_cast<uint16_t>(reg_value.value()), options_.byte_order, buf);
      response.EmplaceBack(buf[0]);
      response.EmplaceBack(buf[1]);
    } else {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      exception_hit = true;
    }
  }

  if (!exception_hit) {
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
  }
}

void RtuSlave::ProcessWriteSingleRegister(AddressMap<int16_t> &address_map, const RtuRequest &request,
                                          RtuResponse &response) {
  if (request.GetData().size() < 4) {
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }
  const auto &data = request.GetData();
  uint16_t address = static_cast<uint16_t>(DecodeU16(data[0], data[1], options_.byte_order));
  if (float_range_.has_value()) {
    const auto [range_start, range_count] = *float_range_;
    if (address >= range_start && address < range_start + range_count) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      return;
    }
  }
  int16_t new_value = static_cast<int16_t>(DecodeU16(data[2], data[3], options_.byte_order));
  if (address_map[address].has_value()) {
    address_map.Set(address, new_value);
    uint8_t buf[2];
    EncodeU16(address, options_.byte_order, buf);
    response.EmplaceBack(buf[0]);
    response.EmplaceBack(buf[1]);
    EncodeU16(static_cast<uint16_t>(new_value), options_.byte_order, buf);
    response.EmplaceBack(buf[0]);
    response.EmplaceBack(buf[1]);
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
  } else {
    response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
  }
}

void RtuSlave::ProcessReadCoils(const AddressMap<bool> &address_map, const RtuRequest &request, RtuResponse &response) {
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

void RtuSlave::ProcessWriteSingleCoil(AddressMap<bool> &address_map, const RtuRequest &request, RtuResponse &response) {
  if (request.GetData().size() < 4) {
    response.SetExceptionCode(ExceptionCode::kIllegalFunction);
    return;
  }

  const auto &data = request.GetData();
  const uint16_t address = static_cast<uint16_t>(DecodeU16(data[0], data[1], options_.byte_order));
  const uint16_t value = static_cast<uint16_t>(DecodeU16(data[2], data[3], options_.byte_order));

  // Modbus spec: 0x0000 = OFF, 0xFF00 = ON
  bool coil_value = (value == supermb::kCoilOnValue);

  if (address_map[address].has_value()) {
    address_map.Set(address, coil_value);
    uint8_t buf[2];
    EncodeU16(address, options_.byte_order, buf);
    response.EmplaceBack(buf[0]);
    response.EmplaceBack(buf[1]);
    EncodeU16(value, options_.byte_order, buf);
    response.EmplaceBack(buf[0]);
    response.EmplaceBack(buf[1]);
    response.SetExceptionCode(ExceptionCode::kAcknowledge);
  } else {
    response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
  }
}

void RtuSlave::ProcessWriteMultipleRegisters(AddressMap<int16_t> &address_map, const RtuRequest &request,
                                             RtuResponse &response) {
  const auto &data = request.GetData();

  // Data format: address (2), count (2), byte_count (1), values (count * 2)
  if (data.size() < kMinWriteDataSize) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint16_t start_address = static_cast<uint16_t>(DecodeU16(data[0], data[1], options_.byte_order));
  uint16_t count = static_cast<uint16_t>(DecodeU16(data[2], data[3], options_.byte_order));
  uint8_t byte_count = data[4];

  if (byte_count != static_cast<uint8_t>(count * 2) || data.size() < kMinWriteDataSize + byte_count) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Float range: decode to floats and write to float_storage_ when span entirely within range
  if (float_range_.has_value() && (count % 2) == 0) {
    const auto [range_start, range_count] = *float_range_;
    if (start_address >= range_start && start_address + count <= range_start + range_count) {
      const size_t float_index_start = (start_address - range_start) / 2;
      const size_t num_floats = count / 2;
      if (float_index_start + num_floats <= float_storage_.size()) {
        size_t data_offset = kMinWriteDataSize;
        for (size_t i = 0; i < num_floats; ++i) {
          if (data_offset + 3 >= data.size()) {
            response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
            return;
          }
          float_storage_[float_index_start + i] =
              DecodeFloat(&data[data_offset], options_.byte_order, options_.word_order);
          data_offset += 4;
        }
        uint8_t buf[2];
        EncodeU16(start_address, options_.byte_order, buf);
        response.EmplaceBack(buf[0]);
        response.EmplaceBack(buf[1]);
        EncodeU16(count, options_.byte_order, buf);
        response.EmplaceBack(buf[0]);
        response.EmplaceBack(buf[1]);
        response.SetExceptionCode(ExceptionCode::kAcknowledge);
        return;
      }
    }
    // Overlap but not entirely within: reject
    if (start_address < range_start + range_count && start_address + count > range_start) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      return;
    }
  }

  // Check if all addresses exist (normal path)
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
    int16_t value = static_cast<int16_t>(DecodeU16(data[data_offset], data[data_offset + 1], options_.byte_order));
    address_map.Set(start_address + i, value);
    data_offset += 2;
  }

  uint8_t buf[2];
  EncodeU16(start_address, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);
  EncodeU16(count, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessWriteMultipleCoils(AddressMap<bool> &address_map, const RtuRequest &request,
                                         RtuResponse &response) {
  const auto &data = request.GetData();

  // Data format: address (2), count (2), byte_count (1), coil values (packed)
  if (data.size() < kMinWriteDataSize) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint16_t start_address = static_cast<uint16_t>(DecodeU16(data[0], data[1], options_.byte_order));
  uint16_t count = static_cast<uint16_t>(DecodeU16(data[2], data[3], options_.byte_order));
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
  uint8_t buf[2];
  EncodeU16(start_address, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);
  EncodeU16(count, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

bool RtuSlave::ProcessIncomingFrame(ByteTransport &transport, uint32_t timeout_ms) {
  auto frame = ReadFrame(transport, timeout_ms);
  if (!frame.has_value()) {
    return false;
  }

  // Decode the request frame
  auto request = RtuFrame::DecodeRequest(std::span<const uint8_t>(frame->data(), frame->size()), options_.byte_order);
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
  if (com_event_log_.size() >= kComEventLogMaxSize) {
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
  int bytes_written = transport.Write(std::span<const uint8_t>(response_frame.data(), response_frame.size()));
  if (bytes_written != static_cast<int>(response_frame.size())) {
    return false;
  }

  (void)transport.Flush();  // Flush may return error, but we've already written the frame
  return true;
}

bool RtuSlave::Poll(ByteTransport &transport) {
  // Check if data is available
  if (!transport.HasData()) {
    return false;
  }

  // Try to process a frame (with minimal timeout since we know data is available)
  return ProcessIncomingFrame(transport, kDefaultPollTimeoutMs);
}

void RtuSlave::ProcessReadExceptionStatus(const RtuRequest & /*request*/, RtuResponse &response) const {
  // FC 7: Read Exception Status - Returns 1 byte exception status
  // For simplicity, we'll return a configurable exception status byte
  response.EmplaceBack(exception_status_);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessDiagnostics(const RtuRequest &request, RtuResponse &response) {
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

void RtuSlave::ProcessGetComEventCounter(const RtuRequest & /*request*/, RtuResponse &response) const {
  // FC 11: Get Com Event Counter - Returns 2 bytes: status (1) + event count (2)
  response.EmplaceBack(0x00);  // Status: 0x00 = no error, 0xFF = error
  uint8_t buf[2];
  EncodeU16(com_event_counter_, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessGetComEventLog(const RtuRequest & /*request*/, RtuResponse &response) const {
  // FC 12: Get Com Event Log - Returns status, event count, message count, and events
  response.EmplaceBack(0x00);  // Status (0x00 = no error, kMaxByteValue = error)
  uint8_t buf[2];
  EncodeU16(com_event_counter_, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);
  EncodeU16(message_count_, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);

  // Add event log entries (up to kComEventLogMaxSize events max per Modbus spec)
  size_t event_count = std::min(com_event_log_.size(), size_t{kComEventLogMaxSize});
  for (size_t i = 0; i < event_count; ++i) {
    const auto &entry = com_event_log_[i];
    EncodeU16(entry.event_id, options_.byte_order, buf);
    response.EmplaceBack(buf[0]);
    response.EmplaceBack(buf[1]);
    EncodeU16(entry.event_count, options_.byte_order, buf);
    response.EmplaceBack(buf[0]);
    response.EmplaceBack(buf[1]);
  }

  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessReportSlaveID(const RtuRequest & /*request*/, RtuResponse &response) const {
  // FC 17: Report Slave ID - Returns slave ID, run indicator, and additional data
  response.EmplaceBack(id_);            // Slave ID
  response.EmplaceBack(kMaxByteValue);  // Run indicator (kMaxByteValue = run, 0x00 = stop)
  // Additional data (optional, device-specific)
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessReadFileRecord(const RtuRequest &request, RtuResponse &response) const {
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
    uint16_t file_number =
        static_cast<uint16_t>(DecodeU16(data[data_offset + kFileRecordReadFileNumberOffset],
                                        data[data_offset + kFileRecordReadFileNumberOffset + 1], options_.byte_order));
    uint16_t record_number = static_cast<uint16_t>(DecodeU16(data[data_offset + kFileRecordReadRecordNumberOffset],
                                                             data[data_offset + kFileRecordReadRecordNumberOffset + 1],
                                                             options_.byte_order));
    uint16_t record_length = static_cast<uint16_t>(DecodeU16(data[data_offset + kFileRecordReadRecordLengthOffset],
                                                             data[data_offset + kFileRecordReadRecordLengthOffset + 1],
                                                             options_.byte_order));
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
    uint8_t buf[2];
    EncodeU16(file_number, options_.byte_order, buf);
    response_data.emplace_back(buf[0]);
    response_data.emplace_back(buf[1]);
    EncodeU16(record_number, options_.byte_order, buf);
    response_data.emplace_back(buf[0]);
    response_data.emplace_back(buf[1]);

    for (uint16_t i = 0; i < record_length && i < record_it->second.size(); ++i) {
      EncodeU16(static_cast<uint16_t>(record_it->second[i]), options_.byte_order, buf);
      response_data.emplace_back(buf[0]);
      response_data.emplace_back(buf[1]);
    }
  }

  // Prepend response length as first byte
  response_data.insert(response_data.begin(), static_cast<uint8_t>(response_data.size()));
  response.SetData(response_data);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessWriteFileRecord(const RtuRequest &request, RtuResponse &response) {
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
    uint16_t file_number =
        static_cast<uint16_t>(DecodeU16(data[data_offset + 1], data[data_offset + 2], options_.byte_order));
    uint16_t record_number =
        static_cast<uint16_t>(DecodeU16(data[data_offset + 3], data[data_offset + 4], options_.byte_order));
    uint16_t record_length =
        static_cast<uint16_t>(DecodeU16(data[data_offset + supermb::kFileRecordBytesPerRecord - 1],
                                        data[data_offset + supermb::kFileRecordBytesPerRecord], options_.byte_order));
    data_offset += kFileRecordMinHeaderSize;

    if (data_offset + static_cast<size_t>(record_length) * 2 > data.size()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
      return;
    }

    // Create or update file record
    FileRecord record_data;
    record_data.reserve(record_length);
    for (uint16_t i = 0; i < record_length; ++i) {
      int16_t value = static_cast<int16_t>(DecodeU16(data[data_offset], data[data_offset + 1], options_.byte_order));
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

void RtuSlave::ProcessMaskWriteRegister(AddressMap<int16_t> &address_map, const RtuRequest &request,
                                        RtuResponse &response) {
  // FC 22: Mask Write Register - Write with AND mask and OR mask
  const auto &data = request.GetData();
  if (data.size() < kMinMaskWriteDataSize) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Mask write format: address(2) + and_mask(2) + or_mask(2) = 6 bytes
  uint16_t address = static_cast<uint16_t>(
      DecodeU16(data[kMaskWriteAddressOffset], data[kMaskWriteAddressOffset + 1], options_.byte_order));
  uint16_t and_mask = static_cast<uint16_t>(
      DecodeU16(data[kMaskWriteAndMaskOffset], data[kMaskWriteAndMaskOffset + 1], options_.byte_order));
  uint16_t or_mask = static_cast<uint16_t>(
      DecodeU16(data[kMaskWriteOrMaskOffset], data[kMaskWriteOrMaskOffset + 1], options_.byte_order));

  auto current_value = address_map[address];
  if (!current_value.has_value()) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
    return;
  }

  // Apply mask: (current & and_mask) | or_mask
  auto new_value = static_cast<int16_t>((static_cast<uint16_t>(current_value.value()) & and_mask) | or_mask);
  address_map.Set(address, new_value);

  uint8_t buf[2];
  EncodeU16(address, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);
  EncodeU16(and_mask, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);
  EncodeU16(or_mask, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);
  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessReadWriteMultipleRegisters(AddressMap<int16_t> &address_map, const RtuRequest &request,
                                                 RtuResponse &response) {
  // FC 23: Read/Write Multiple Registers - Read from one area, write to another
  const auto &data = request.GetData();
  if (data.size() < kMinReadWriteDataSize) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  // Parse read request: read_start (2), read_count (2)
  uint16_t read_start = static_cast<uint16_t>(
      DecodeU16(data[kReadWriteReadStartOffset], data[kReadWriteReadStartOffset + 1], options_.byte_order));
  uint16_t read_count = static_cast<uint16_t>(
      DecodeU16(data[kReadWriteReadCountOffset], data[kReadWriteReadCountOffset + 1], options_.byte_order));

  // Parse write request: write_start (2), write_count (2), write_byte_count (1), write_values (N*2)
  uint16_t write_start = static_cast<uint16_t>(
      DecodeU16(data[kReadWriteWriteStartOffset], data[kReadWriteWriteStartOffset + 1], options_.byte_order));
  uint16_t write_count = static_cast<uint16_t>(
      DecodeU16(data[kReadWriteWriteCountOffset], data[kReadWriteWriteCountOffset + 1], options_.byte_order));
  uint8_t write_byte_count = data[kReadWriteByteCountOffset];

  if (write_byte_count != static_cast<uint8_t>(write_count * 2) ||
      data.size() < kMinReadWriteDataWithValues + write_byte_count) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  size_t data_offset = kMinReadWriteDataWithValues;

  // Write part: float range or address_map
  if (float_range_.has_value() && (write_count % 2) == 0) {
    const auto [range_start, range_count] = *float_range_;
    if (write_start >= range_start && write_start + write_count <= range_start + range_count) {
      const size_t float_index_start = (write_start - range_start) / 2;
      const size_t num_floats = write_count / 2;
      if (float_index_start + num_floats <= float_storage_.size()) {
        for (size_t i = 0; i < num_floats; ++i) {
          if (data_offset + 3 >= data.size()) {
            response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
            return;
          }
          float_storage_[float_index_start + i] =
              DecodeFloat(&data[data_offset], options_.byte_order, options_.word_order);
          data_offset += 4;
        }
      } else {
        response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
        return;
      }
    } else if (write_start < range_start + range_count && write_start + write_count > range_start) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      return;
    } else {
      for (int i = 0; i < write_count; ++i) {
        if (!address_map[write_start + i].has_value()) {
          response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
          return;
        }
        if (data_offset + 1 >= data.size()) {
          response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
          return;
        }
        int16_t value = static_cast<int16_t>(DecodeU16(data[data_offset], data[data_offset + 1], options_.byte_order));
        address_map.Set(write_start + i, value);
        data_offset += 2;
      }
    }
  } else {
    for (int i = 0; i < write_count; ++i) {
      if (!address_map[write_start + i].has_value()) {
        response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
        return;
      }
      if (data_offset + 1 >= data.size()) {
        response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
        return;
      }
      int16_t value = static_cast<int16_t>(DecodeU16(data[data_offset], data[data_offset + 1], options_.byte_order));
      address_map.Set(write_start + i, value);
      data_offset += 2;
    }
  }

  // Read part: float range or address_map
  if (float_range_.has_value() && (read_count % 2) == 0) {
    const auto [range_start, range_count] = *float_range_;
    if (read_start >= range_start && read_start + read_count <= range_start + range_count) {
      const size_t float_index_start = (read_start - range_start) / 2;
      const size_t num_floats = read_count / 2;
      if (float_index_start + num_floats <= float_storage_.size()) {
        uint8_t buf[4];
        for (size_t i = 0; i < num_floats; ++i) {
          EncodeFloat(float_storage_[float_index_start + i], options_.byte_order, options_.word_order, buf);
          response.EmplaceBack(buf[0]);
          response.EmplaceBack(buf[1]);
          response.EmplaceBack(buf[2]);
          response.EmplaceBack(buf[3]);
        }
        response.SetExceptionCode(ExceptionCode::kAcknowledge);
        return;
      }
    }
  }
  // Normal read from address_map
  uint8_t buf[2];
  for (int i = 0; i < read_count; ++i) {
    auto reg_value = address_map[read_start + i];
    if (!reg_value.has_value()) {
      response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
      return;
    }
    EncodeU16(static_cast<uint16_t>(reg_value.value()), options_.byte_order, buf);
    response.EmplaceBack(buf[0]);
    response.EmplaceBack(buf[1]);
  }

  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

void RtuSlave::ProcessReadFIFOQueue(const RtuRequest &request, RtuResponse &response) const {
  // FC 24: Read FIFO Queue - Read from FIFO queue at specified address
  const auto &data = request.GetData();
  if (data.size() < 2) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataValue);
    return;
  }

  uint16_t fifo_address = static_cast<uint16_t>(DecodeU16(data[0], data[1], options_.byte_order));

  // Check if FIFO queue exists (empty queue is valid per Modbus spec)
  auto fifo_it = fifo_storage_.find(fifo_address);
  if (fifo_it == fifo_storage_.end()) {
    response.SetExceptionCode(ExceptionCode::kIllegalDataAddress);
    return;
  }

  const FIFOQueue &fifo_queue = fifo_it->second;
  auto fifo_count = static_cast<uint16_t>(fifo_queue.size());
  uint16_t byte_count = fifo_count * 2;  // Each register is 2 bytes

  uint8_t buf[2];
  EncodeU16(byte_count, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);
  EncodeU16(fifo_count, options_.byte_order, buf);
  response.EmplaceBack(buf[0]);
  response.EmplaceBack(buf[1]);

  for (int16_t value : fifo_queue) {
    EncodeU16(static_cast<uint16_t>(value), options_.byte_order, buf);
    response.EmplaceBack(buf[0]);
    response.EmplaceBack(buf[1]);
  }

  response.SetExceptionCode(ExceptionCode::kAcknowledge);
}

std::optional<std::vector<uint8_t>> RtuSlave::ReadFrame(ByteReader &transport, uint32_t timeout_ms) {
  std::vector<uint8_t> buffer;
  buffer.reserve(kSlaveIdMaxSize);  // Reserve space for typical frame

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
    std::array<uint8_t, kFifoQueueMaxSize> temp_buffer{};
    std::span<uint8_t> read_span(temp_buffer.data(), temp_buffer.size());
    int bytes_read = transport.Read(read_span);

    if (bytes_read > 0) {
      buffer.insert(buffer.end(), temp_buffer.begin(), temp_buffer.begin() + bytes_read);
      frame_started = true;

      // Check if we have a complete frame (slave reads requests)
      std::span<const uint8_t> frame_span(buffer.data(), buffer.size());
      if (RtuFrame::IsRequestFrameComplete(frame_span)) {
        return buffer;
      }
    } else if (frame_started && bytes_read == 0) {
      // Frame started but no more data available - might be complete (slave reads requests)
      std::span<const uint8_t> frame_span(buffer.data(), buffer.size());
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
