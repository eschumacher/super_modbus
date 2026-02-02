#include <algorithm>
#include <array>
#include <cassert>
#include <numeric>
#include <span>
#include <tuple>
#include <vector>
#include "common/address_span.hpp"
#include "common/byte_helpers.hpp"
#include "common/function_code.hpp"
#include "tcp/tcp_request.hpp"

namespace supermb {

static constexpr std::array<FunctionCode, 10> kAddressSpanValidFunctions{
    FunctionCode::kReadCoils,        FunctionCode::kReadDI,          FunctionCode::kReadHR,
    FunctionCode::kReadIR,           FunctionCode::kWriteSingleCoil, FunctionCode::kWriteSingleReg,
    FunctionCode::kWriteMultCoils,   FunctionCode::kWriteMultRegs,   FunctionCode::kMaskWriteReg,
    FunctionCode::kReadWriteMultRegs};

static constexpr uint8_t kAddressSpanStartAddressIndex{0};
static constexpr uint8_t kAddressSpanRegCountIndex{2};
static constexpr uint8_t kAddressSpanMinDataSize{4};
static constexpr uint8_t kMaxFileRecords{255};

// Helper function to check if function code is in valid functions array
static constexpr bool IsAddressSpanValidFunction(FunctionCode function_code) {
  return std::any_of(std::begin(kAddressSpanValidFunctions), std::end(kAddressSpanValidFunctions),
                     [function_code](FunctionCode valid_code) { return valid_code == function_code; });
}

std::optional<AddressSpan> TcpRequest::GetAddressSpan() const {
  if ((data_.size() < kAddressSpanMinDataSize) || !IsAddressSpanValidFunction(header_.function_code)) {
    return {};
  }

  // Modbus uses big-endian (high byte first, low byte second)
  // MakeInt16 takes (low_byte, high_byte), so data[0] is high byte, data[1] is low byte
  AddressSpan address_span;
  address_span.start_address =
      MakeInt16(data_[kAddressSpanStartAddressIndex + 1], data_[kAddressSpanStartAddressIndex]);
  address_span.reg_count = MakeInt16(data_[kAddressSpanRegCountIndex + 1], data_[kAddressSpanRegCountIndex]);
  return address_span;
}

bool TcpRequest::SetAddressSpan(AddressSpan address_span) {
  if (!IsAddressSpanValidFunction(header_.function_code)) {
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  data_.clear();
  data_.resize(kAddressSpanMinDataSize);

  // Modbus uses big-endian (high byte first, low byte second)
  data_[kAddressSpanStartAddressIndex] = GetHighByte(address_span.start_address);
  data_[kAddressSpanStartAddressIndex + 1] = GetLowByte(address_span.start_address);
  data_[kAddressSpanRegCountIndex] = GetHighByte(address_span.reg_count);
  data_[kAddressSpanRegCountIndex + 1] = GetLowByte(address_span.reg_count);

  return true;
}

bool TcpRequest::SetWriteSingleRegisterData(uint16_t register_address, int16_t register_value) {
  if (header_.function_code != FunctionCode::kWriteSingleReg) {
    assert(false);
    return false;
  }

  data_.clear();
  data_.resize(4);

  // Address (big-endian: high byte first)
  data_[0] = GetHighByte(register_address);
  data_[1] = GetLowByte(register_address);

  // Value (big-endian: high byte first)
  data_[2] = GetHighByte(register_value);
  data_[3] = GetLowByte(register_value);

  return true;
}

bool TcpRequest::SetWriteSingleCoilData(uint16_t coil_address, bool coil_value) {
  if (header_.function_code != FunctionCode::kWriteSingleCoil) {
    assert(false);
    return false;
  }

  data_.clear();
  data_.resize(4);

  // Address (big-endian: high byte first)
  data_[0] = GetHighByte(coil_address);
  data_[1] = GetLowByte(coil_address);

  // Value: 0xFF00 for ON, 0x0000 for OFF (big-endian: high byte first)
  uint16_t value = coil_value ? kCoilOnValue : 0x0000;
  data_[2] = GetHighByte(value);
  data_[3] = GetLowByte(value);

  return true;
}

bool TcpRequest::SetWriteMultipleRegistersData(uint16_t start_address, uint16_t count,
                                               std::span<const int16_t> values) {
  if (header_.function_code != FunctionCode::kWriteMultRegs) {
    assert(false);
    return false;
  }

  if (values.size() != count) {
    return false;
  }

  data_.clear();
  data_.resize(5 + count * 2);  // address(2) + count(2) + byte_count(1) + values(count*2)

  // Start address (big-endian: high byte first)
  data_[0] = GetHighByte(start_address);
  data_[1] = GetLowByte(start_address);

  // Count (big-endian: high byte first)
  data_[2] = GetHighByte(count);
  data_[3] = GetLowByte(count);

  // Byte count
  data_[4] = static_cast<uint8_t>(count * 2);

  // Values (big-endian: high byte first)
  for (size_t i = 0; i < values.size(); ++i) {
    data_[5 + i * 2] = GetHighByte(values[i]);
    data_[5 + i * 2 + 1] = GetLowByte(values[i]);
  }

  return true;
}

bool TcpRequest::SetWriteMultipleCoilsData(uint16_t start_address, uint16_t count, std::span<const bool> values) {
  if (header_.function_code != FunctionCode::kWriteMultCoils) {
    assert(false);
    return false;
  }

  if (values.size() != count) {
    return false;
  }

  // Calculate byte count for coils
  uint8_t byte_count = static_cast<uint8_t>((count + kCoilByteCountRoundingOffset) / kCoilsPerByte);

  data_.clear();
  data_.resize(5 + byte_count);  // address(2) + count(2) + byte_count(1) + coil_bytes

  // Start address (big-endian: high byte first)
  data_[0] = GetHighByte(start_address);
  data_[1] = GetLowByte(start_address);

  // Count (big-endian: high byte first)
  data_[2] = GetHighByte(count);
  data_[3] = GetLowByte(count);

  // Byte count
  data_[4] = byte_count;

  // Pack coils into bytes
  for (size_t i = 0; i < static_cast<size_t>(byte_count); ++i) {
    uint8_t byte_value = 0;
    for (size_t bit = 0; bit < kCoilsPerByte && (i * kCoilsPerByte + bit) < static_cast<size_t>(count); ++bit) {
      if (values[i * kCoilsPerByte + bit]) {
        byte_value |= (1U << bit);
      }
    }
    data_[5 + i] = byte_value;
  }

  return true;
}

bool TcpRequest::SetDiagnosticsData(uint16_t sub_function_code, std::span<const uint8_t> data) {
  if (header_.function_code != FunctionCode::kDiagnostics) {
    assert(false);
    return false;
  }

  this->data_.clear();
  this->data_.resize(2 + data.size());

  // Sub-function code (big-endian: high byte first)
  this->data_[0] = GetHighByte(sub_function_code);
  this->data_[1] = GetLowByte(sub_function_code);

  // Diagnostic data
  std::copy(data.begin(), data.end(), this->data_.begin() + 2);

  return true;
}

bool TcpRequest::SetMaskWriteRegisterData(uint16_t address, uint16_t and_mask, uint16_t or_mask) {
  if (header_.function_code != FunctionCode::kMaskWriteReg) {
    assert(false);
    return false;
  }

  data_.clear();
  data_.resize(6);

  // Address (big-endian: high byte first)
  data_[0] = GetHighByte(address);
  data_[1] = GetLowByte(address);

  // AND mask (big-endian: high byte first)
  data_[2] = GetHighByte(and_mask);
  data_[3] = GetLowByte(and_mask);

  // OR mask (big-endian: high byte first)
  data_[4] = GetHighByte(or_mask);
  data_[5] = GetLowByte(or_mask);

  return true;
}

bool TcpRequest::SetReadWriteMultipleRegistersData(uint16_t read_start, uint16_t read_count, uint16_t write_start,
                                                   uint16_t write_count, std::span<const int16_t> write_values) {
  if (header_.function_code != FunctionCode::kReadWriteMultRegs) {
    assert(false);
    return false;
  }

  if (write_values.size() != write_count) {
    return false;
  }

  data_.clear();
  data_.resize(9 + write_count * 2);  // read_start(2) + read_count(2) + write_start(2) + write_count(2) +
                                      // write_byte_count(1) + write_values(write_count*2)

  // Read start address (big-endian: high byte first)
  data_[0] = GetHighByte(read_start);
  data_[1] = GetLowByte(read_start);

  // Read count (big-endian: high byte first)
  data_[2] = GetHighByte(read_count);
  data_[3] = GetLowByte(read_count);

  // Write start address (big-endian: high byte first)
  data_[4] = GetHighByte(write_start);
  data_[5] = GetLowByte(write_start);

  // Write count (big-endian: high byte first)
  data_[6] = GetHighByte(write_count);
  data_[7] = GetLowByte(write_count);

  // Write byte count
  data_[8] = static_cast<uint8_t>(write_count * 2);

  // Write values (big-endian: high byte first)
  for (size_t i = 0; i < write_values.size(); ++i) {
    data_[9 + i * 2] = GetHighByte(write_values[i]);
    data_[9 + i * 2 + 1] = GetLowByte(write_values[i]);
  }

  return true;
}

bool TcpRequest::SetReadFIFOQueueData(uint16_t fifo_address) {
  if (header_.function_code != FunctionCode::kReadFIFOQueue) {
    assert(false);
    return false;
  }

  data_.clear();
  data_.resize(2);

  // FIFO address (big-endian: high byte first)
  data_[0] = GetHighByte(fifo_address);
  data_[1] = GetLowByte(fifo_address);

  return true;
}

bool TcpRequest::SetReadFileRecordData(std::span<const std::tuple<uint16_t, uint16_t, uint16_t>> file_records) {
  if (header_.function_code != FunctionCode::kReadFileRecord) {
    assert(false);
    return false;
  }

  if (file_records.empty() || file_records.size() > kMaxFileRecords) {
    return false;
  }

  // Calculate total size: byte_count(1) + file_records * (reference_type(1) + file_number(2) + record_number(2) +
  // record_length(2))
  size_t total_size = 1 + file_records.size() * 7;

  data_.clear();
  data_.resize(total_size);

  // Byte count (number of bytes following)
  data_[0] = static_cast<uint8_t>(file_records.size() * 7);

  // File records
  size_t offset = 1;
  for (const auto &record : file_records) {
    // Reference type
    data_[offset++] = kFileRecordReferenceType;

    // File number (big-endian: high byte first)
    uint16_t file_number = std::get<0>(record);
    data_[offset++] = GetHighByte(file_number);
    data_[offset++] = GetLowByte(file_number);

    // Record number (big-endian: high byte first)
    uint16_t record_number = std::get<1>(record);
    data_[offset++] = GetHighByte(record_number);
    data_[offset++] = GetLowByte(record_number);

    // Record length (big-endian: high byte first)
    uint16_t record_length = std::get<2>(record);
    data_[offset++] = GetHighByte(record_length);
    data_[offset++] = GetLowByte(record_length);
  }

  return true;
}

bool TcpRequest::SetWriteFileRecordData(
    std::span<const std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records) {
  if (header_.function_code != FunctionCode::kWriteFileRecord) {
    assert(false);
    return false;
  }

  if (file_records.empty() || file_records.size() > kMaxFileRecords) {
    return false;
  }

  // Calculate total size: byte_count(1) + sum of (reference_type(1) + file_number(2) + record_number(2) +
  // record_length(2) + record_data(record_length*2))
  size_t total_size =
      std::accumulate(file_records.begin(), file_records.end(), size_t{1},
                      [](size_t sum, const auto &record) { return sum + 7 + std::get<2>(record).size() * 2; });

  data_.clear();
  data_.resize(total_size);

  // Byte count (number of bytes following)
  data_[0] = static_cast<uint8_t>(total_size - 1);

  // File records
  size_t offset = 1;
  for (const auto &record : file_records) {
    // Reference type
    data_[offset++] = kFileRecordReferenceType;

    // File number (big-endian: high byte first)
    uint16_t file_number = std::get<0>(record);
    data_[offset++] = GetHighByte(file_number);
    data_[offset++] = GetLowByte(file_number);

    // Record number (big-endian: high byte first)
    uint16_t record_number = std::get<1>(record);
    data_[offset++] = GetHighByte(record_number);
    data_[offset++] = GetLowByte(record_number);

    // Record length (big-endian: high byte first)
    const auto &record_data = std::get<2>(record);
    uint16_t record_length = static_cast<uint16_t>(record_data.size());
    data_[offset++] = GetHighByte(record_length);
    data_[offset++] = GetLowByte(record_length);

    // Record data (big-endian: high byte first)
    for (int16_t value : record_data) {
      data_[offset++] = GetHighByte(value);
      data_[offset++] = GetLowByte(value);
    }
  }

  return true;
}

}  // namespace supermb
