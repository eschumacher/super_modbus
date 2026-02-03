#include "ascii/ascii_master.hpp"
#include "common/address_span.hpp"
#include "common/wire_format_options.hpp"

namespace std {
template <>
struct hash<std::pair<uint16_t, uint16_t>> {
  size_t operator()(const std::pair<uint16_t, uint16_t> &p) const noexcept {
    return std::hash<uint16_t>{}(p.first) ^ (std::hash<uint16_t>{}(p.second) << 1);
  }
};
}  // namespace std
#include "common/byte_helpers.hpp"
#include "common/exception_code.hpp"
#include "common/function_code.hpp"
#include "rtu/rtu_request.hpp"
#include "rtu/rtu_response.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

using supermb::IsBroadcastableWrite;

namespace supermb {

std::optional<RtuResponse> AsciiMaster::SendRequest(const RtuRequest &request, uint32_t timeout_ms) {
  std::string frame = AsciiFrame::EncodeRequest(request);
  std::span<const uint8_t> frame_bytes(reinterpret_cast<const uint8_t *>(frame.data()), frame.size());

  int bytes_written = transport_.Write(frame_bytes);
  if (bytes_written != static_cast<int>(frame.size())) {
    return {};
  }
  if (!transport_.Flush()) {
    return {};
  }

  if (request.GetSlaveId() == 0 && IsBroadcastableWrite(request.GetFunctionCode())) {
    RtuResponse broadcast_response{0, request.GetFunctionCode()};
    broadcast_response.SetExceptionCode(ExceptionCode::kAcknowledge);
    return broadcast_response;
  }

  auto ascii_frame = ReadAsciiFrame(timeout_ms);
  if (!ascii_frame.has_value()) {
    return {};
  }
  auto response = AsciiFrame::DecodeResponse(*ascii_frame);
  if (!response.has_value()) {
    return {};
  }
  if (response->GetSlaveId() != request.GetSlaveId()) {
    return {};
  }
  return response;
}

std::optional<std::string> AsciiMaster::ReadAsciiFrame(uint32_t timeout_ms) {
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
    int n = transport_.Read(std::span<uint8_t>(temp, sizeof(temp)));
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

std::optional<std::vector<int16_t>> AsciiMaster::ReadHoldingRegisters(uint8_t slave_id, uint16_t start_address,
                                                                      uint16_t count) {
  RtuRequest request({slave_id, FunctionCode::kReadHR}, options_.byte_order);
  if (!request.SetAddressSpan({start_address, count})) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.size() < 1 + static_cast<size_t>(count) * 2) {
    return {};
  }
  std::vector<int16_t> regs;
  regs.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    size_t idx = 1 + i * 2;
    regs.push_back(static_cast<int16_t>(DecodeU16(data[idx], data[idx + 1], options_.byte_order)));
  }
  return regs;
}

std::optional<std::vector<float>> AsciiMaster::ReadFloats(uint8_t slave_id, uint16_t start_address, uint16_t count) {
  const auto semantics = options_.float_count_semantics.value_or(FloatCountSemantics::CountIsFloatCount);
  uint16_t num_registers;
  size_t num_floats;
  if (semantics == FloatCountSemantics::CountIsFloatCount) {
    num_registers = count * 2;
    num_floats = count;
  } else {
    num_registers = count;
    num_floats = count / 2;
  }
  if (num_floats == 0 || num_registers < 2) {
    return {};
  }
  if (options_.float_range.has_value()) {
    const auto [range_start, range_count] = *options_.float_range;
    if (start_address < range_start || start_address + num_registers > range_start + range_count) {
      return {};
    }
  }
  auto regs = ReadHoldingRegisters(slave_id, start_address, num_registers);
  if (!regs.has_value() || regs->size() < num_registers) {
    return {};
  }
  std::vector<float> out;
  out.reserve(num_floats);
  for (size_t i = 0; i + 1 < regs->size(); i += 2) {
    uint8_t buf[4];
    EncodeU16(static_cast<uint16_t>(regs->at(i) & 0xFFFF), options_.byte_order, buf);
    EncodeU16(static_cast<uint16_t>(regs->at(i + 1) & 0xFFFF), options_.byte_order, buf + 2);
    out.push_back(DecodeFloat(buf, options_.byte_order, options_.word_order));
  }
  return out;
}

bool AsciiMaster::WriteFloats(uint8_t slave_id, uint16_t start_address, std::span<const float> values) {
  const size_t num_floats = values.size();
  if (num_floats == 0) {
    return true;
  }
  const uint16_t num_registers = static_cast<uint16_t>(num_floats * 2);
  if (options_.float_range.has_value()) {
    const auto [range_start, range_count] = *options_.float_range;
    if (start_address < range_start || start_address + num_registers > range_start + range_count) {
      return false;
    }
  }
  std::vector<int16_t> regs;
  regs.reserve(num_registers);
  for (float f : values) {
    uint8_t buf[4];
    EncodeFloat(f, options_.byte_order, options_.word_order, buf);
    regs.push_back(static_cast<int16_t>(DecodeU16(buf[0], buf[1], options_.byte_order)));
    regs.push_back(static_cast<int16_t>(DecodeU16(buf[2], buf[3], options_.byte_order)));
  }
  return WriteMultipleRegisters(slave_id, start_address, regs);
}

std::optional<std::vector<int16_t>> AsciiMaster::ReadInputRegisters(uint8_t slave_id, uint16_t start_address,
                                                                    uint16_t count) {
  RtuRequest request({slave_id, FunctionCode::kReadIR}, options_.byte_order);
  if (!request.SetAddressSpan({start_address, count})) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.size() < 1 + static_cast<size_t>(count) * 2) {
    return {};
  }
  std::vector<int16_t> regs;
  regs.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    size_t idx = 1 + i * 2;
    regs.push_back(static_cast<int16_t>(DecodeU16(data[idx], data[idx + 1], options_.byte_order)));
  }
  return regs;
}

std::optional<std::vector<bool>> AsciiMaster::ReadCoils(uint8_t slave_id, uint16_t start_address, uint16_t count) {
  RtuRequest request({slave_id, FunctionCode::kReadCoils}, options_.byte_order);
  if (!request.SetAddressSpan({start_address, count})) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.empty() || data[0] != static_cast<uint8_t>((count + 7) / 8)) {
    return {};
  }
  std::vector<bool> coils;
  coils.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    uint8_t byte_idx = 1 + (i / 8);
    uint8_t bit_idx = i % 8;
    if (byte_idx >= data.size()) {
      return {};
    }
    coils.push_back((data[byte_idx] & (1 << bit_idx)) != 0);
  }
  return coils;
}

std::optional<std::vector<bool>> AsciiMaster::ReadDiscreteInputs(uint8_t slave_id, uint16_t start_address,
                                                                 uint16_t count) {
  RtuRequest request({slave_id, FunctionCode::kReadDI}, options_.byte_order);
  if (!request.SetAddressSpan({start_address, count})) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.empty() || data[0] != static_cast<uint8_t>((count + 7) / 8)) {
    return {};
  }
  std::vector<bool> dis;
  dis.reserve(count);
  for (uint16_t i = 0; i < count; ++i) {
    uint8_t byte_idx = 1 + (i / 8);
    uint8_t bit_idx = i % 8;
    if (byte_idx >= data.size()) {
      return {};
    }
    dis.push_back((data[byte_idx] & (1 << bit_idx)) != 0);
  }
  return dis;
}

bool AsciiMaster::WriteSingleRegister(uint8_t slave_id, uint16_t address, int16_t value) {
  RtuRequest request({slave_id, FunctionCode::kWriteSingleReg}, options_.byte_order);
  if (!request.SetWriteSingleRegisterData(address, value)) {
    return false;
  }
  auto response = SendRequest(request);
  return response.has_value() && response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

bool AsciiMaster::WriteSingleCoil(uint8_t slave_id, uint16_t address, bool value) {
  RtuRequest request({slave_id, FunctionCode::kWriteSingleCoil}, options_.byte_order);
  if (!request.SetWriteSingleCoilData(address, value)) {
    return false;
  }
  auto response = SendRequest(request);
  return response.has_value() && response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

bool AsciiMaster::WriteMultipleRegisters(uint8_t slave_id, uint16_t start_address, std::span<const int16_t> values) {
  RtuRequest request({slave_id, FunctionCode::kWriteMultRegs}, options_.byte_order);
  if (!request.SetWriteMultipleRegistersData(start_address, static_cast<uint16_t>(values.size()), values)) {
    return false;
  }
  auto response = SendRequest(request);
  return response.has_value() && response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

bool AsciiMaster::WriteMultipleCoils(uint8_t slave_id, uint16_t start_address, std::span<const bool> values) {
  RtuRequest request({slave_id, FunctionCode::kWriteMultCoils}, options_.byte_order);
  if (!request.SetWriteMultipleCoilsData(start_address, static_cast<uint16_t>(values.size()), values)) {
    return false;
  }
  auto response = SendRequest(request);
  return response.has_value() && response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

std::optional<uint8_t> AsciiMaster::ReadExceptionStatus(uint8_t slave_id) {
  RtuRequest request({slave_id, FunctionCode::kReadExceptionStatus}, options_.byte_order);
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  return data.empty() ? std::optional<uint8_t>{} : data[0];
}

std::optional<std::vector<uint8_t>> AsciiMaster::Diagnostics(uint8_t slave_id, uint16_t sub_function_code,
                                                             std::span<const uint8_t> data) {
  RtuRequest request({slave_id, FunctionCode::kDiagnostics}, options_.byte_order);
  if (!request.SetDiagnosticsData(sub_function_code, data)) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  return response->GetData();
}

std::optional<std::pair<uint8_t, uint16_t>> AsciiMaster::GetComEventCounter(uint8_t slave_id) {
  RtuRequest request({slave_id, FunctionCode::kGetComEventCounter}, options_.byte_order);
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.size() < 3) {
    return {};
  }
  return std::make_pair(data[0], static_cast<uint16_t>(DecodeU16(data[1], data[2], options_.byte_order)));
}

std::optional<std::vector<uint8_t>> AsciiMaster::GetComEventLog(uint8_t slave_id) {
  RtuRequest request({slave_id, FunctionCode::kGetComEventLog}, options_.byte_order);
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  return response->GetData();
}

std::optional<std::vector<uint8_t>> AsciiMaster::ReportSlaveID(uint8_t slave_id) {
  RtuRequest request({slave_id, FunctionCode::kReportSlaveID}, options_.byte_order);
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  return response->GetData();
}

bool AsciiMaster::MaskWriteRegister(uint8_t slave_id, uint16_t address, uint16_t and_mask, uint16_t or_mask) {
  RtuRequest request({slave_id, FunctionCode::kMaskWriteReg}, options_.byte_order);
  if (!request.SetMaskWriteRegisterData(address, and_mask, or_mask)) {
    return false;
  }
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return false;
  }
  auto data = response->GetData();
  if (data.size() < 6) {
    return false;
  }
  return DecodeU16(data[0], data[1], options_.byte_order) == address &&
         DecodeU16(data[2], data[3], options_.byte_order) == and_mask &&
         DecodeU16(data[4], data[5], options_.byte_order) == or_mask;
}

std::optional<std::vector<int16_t>> AsciiMaster::ReadWriteMultipleRegisters(uint8_t slave_id, uint16_t read_start,
                                                                            uint16_t read_count, uint16_t write_start,
                                                                            std::span<const int16_t> write_values) {
  RtuRequest request({slave_id, FunctionCode::kReadWriteMultRegs}, options_.byte_order);
  if (!request.SetReadWriteMultipleRegistersData(read_start, read_count, write_start,
                                                 static_cast<uint16_t>(write_values.size()), write_values)) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.size() < read_count * 2) {
    return {};
  }
  std::vector<int16_t> regs;
  regs.reserve(read_count);
  for (uint16_t i = 0; i < read_count; ++i) {
    regs.push_back(static_cast<int16_t>(DecodeU16(data[i * 2], data[i * 2 + 1], options_.byte_order)));
  }
  return regs;
}

std::optional<std::vector<int16_t>> AsciiMaster::ReadFIFOQueue(uint8_t slave_id, uint16_t fifo_address) {
  RtuRequest request({slave_id, FunctionCode::kReadFIFOQueue}, options_.byte_order);
  if (!request.SetReadFIFOQueueData(fifo_address)) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.size() < 4) {
    return {};
  }
  uint16_t fifo_count = static_cast<uint16_t>(DecodeU16(data[2], data[3], options_.byte_order));
  if (data.size() < 4u + static_cast<size_t>(fifo_count) * 2u) {
    return {};
  }
  std::vector<int16_t> fifo;
  fifo.reserve(fifo_count);
  for (uint16_t i = 0; i < fifo_count; ++i) {
    fifo.push_back(static_cast<int16_t>(DecodeU16(data[4 + i * 2], data[4 + i * 2 + 1], options_.byte_order)));
  }
  return fifo;
}

std::optional<std::unordered_map<std::pair<uint16_t, uint16_t>, std::vector<int16_t>>> AsciiMaster::ReadFileRecord(
    uint8_t slave_id, std::span<const std::tuple<uint16_t, uint16_t, uint16_t>> file_records) {
  RtuRequest request({slave_id, FunctionCode::kReadFileRecord}, options_.byte_order);
  if (!request.SetReadFileRecordData(file_records)) {
    return {};
  }
  auto response = SendRequest(request);
  if (!response.has_value() || response->GetExceptionCode() != ExceptionCode::kAcknowledge) {
    return {};
  }
  auto data = response->GetData();
  if (data.size() < 1) {
    return {};
  }
  size_t end_offset = 1 + data[0];
  if (data.size() < end_offset) {
    return {};
  }
  std::unordered_map<std::pair<uint16_t, uint16_t>, std::vector<int16_t>> result;
  size_t offset = 1;
  while (offset + 8 <= end_offset) {
    if (data[offset] != 0x06) {
      return {};
    }
    uint8_t data_len = data[offset + 1];
    uint16_t file_num = static_cast<uint16_t>(DecodeU16(data[offset + 2], data[offset + 3], options_.byte_order));
    uint16_t rec_num = static_cast<uint16_t>(DecodeU16(data[offset + 4], data[offset + 5], options_.byte_order));
    offset += 6;
    uint16_t rec_data_len = (data_len - 4) / 2;
    std::vector<int16_t> rec_data;
    rec_data.reserve(rec_data_len);
    for (uint16_t i = 0; i < rec_data_len && offset + 1 < end_offset; ++i) {
      rec_data.push_back(static_cast<int16_t>(DecodeU16(data[offset], data[offset + 1], options_.byte_order)));
      offset += 2;
    }
    result[{file_num, rec_num}] = std::move(rec_data);
  }
  return result;
}

bool AsciiMaster::WriteFileRecord(uint8_t slave_id,
                                  std::span<const std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records) {
  RtuRequest request({slave_id, FunctionCode::kWriteFileRecord}, options_.byte_order);
  if (!request.SetWriteFileRecordData(file_records)) {
    return false;
  }
  auto response = SendRequest(request);
  return response.has_value() && response->GetExceptionCode() == ExceptionCode::kAcknowledge;
}

}  // namespace supermb
