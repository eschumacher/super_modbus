#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <ranges>
#include <span>
#include <tuple>
#include <vector>
#include "common/address_span.hpp"
#include "common/byte_helpers.hpp"
#include "common/function_code.hpp"
#include "rtu/rtu_request.hpp"

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
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
  // std::ranges::any_of correctly handles std::array without decay
  return std::ranges::any_of(kAddressSpanValidFunctions,
                             [function_code](FunctionCode valid_code) { return valid_code == function_code; });
}

std::optional<AddressSpan> RtuRequest::GetAddressSpan() const {
  if ((data_.size() < kAddressSpanMinDataSize) || !IsAddressSpanValidFunction(header_.function_code)) {
    return {};
  }

  // Modbus uses big-endian (high byte first, low byte second)
  AddressSpan address_span;
  address_span.start_address =
      MakeInt16(data_[kAddressSpanStartAddressIndex + 1], data_[kAddressSpanStartAddressIndex]);
  address_span.reg_count = MakeInt16(data_[kAddressSpanRegCountIndex + 1], data_[kAddressSpanRegCountIndex]);
  return address_span;
}

bool RtuRequest::SetAddressSpan(AddressSpan address_span) {
  if (!IsAddressSpanValidFunction(header_.function_code)) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  data_.clear();
  data_.emplace_back(GetHighByte(address_span.start_address));
  data_.emplace_back(GetLowByte(address_span.start_address));
  data_.emplace_back(GetHighByte(address_span.reg_count));
  data_.emplace_back(GetLowByte(address_span.reg_count));
  return true;
}

bool RtuRequest::SetWriteSingleRegisterData(uint16_t register_address, int16_t register_value) {
  if (header_.function_code != FunctionCode::kWriteSingleReg) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  data_.clear();
  data_.emplace_back(GetHighByte(register_address));
  data_.emplace_back(GetLowByte(register_address));
  data_.emplace_back(GetHighByte(register_value));
  data_.emplace_back(GetLowByte(register_value));
  return true;
}

bool RtuRequest::SetWriteSingleCoilData(uint16_t coil_address, bool coil_value) {
  if (header_.function_code != FunctionCode::kWriteSingleCoil) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  data_.clear();
  data_.emplace_back(GetHighByte(coil_address));
  data_.emplace_back(GetLowByte(coil_address));
  // Modbus spec: 0x0000 = OFF, 0xFF00 = ON
  uint16_t value = coil_value ? kCoilOnValue : 0x0000;
  data_.emplace_back(GetHighByte(value));
  data_.emplace_back(GetLowByte(value));
  return true;
}

bool RtuRequest::SetWriteMultipleRegistersData(uint16_t start_address, uint16_t count,
                                               std::span<int16_t const> values) {
  if (header_.function_code != FunctionCode::kWriteMultRegs) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  if (values.size() != count) {
    return false;
  }

  data_.clear();
  // Address
  data_.emplace_back(GetHighByte(start_address));
  data_.emplace_back(GetLowByte(start_address));
  // Count
  data_.emplace_back(GetHighByte(count));
  data_.emplace_back(GetLowByte(count));
  // Byte count
  data_.emplace_back(static_cast<uint8_t>(count * 2));
  // Modbus RTU uses big-endian: high byte first, then low byte
  for (int16_t value : values) {
    data_.emplace_back(GetHighByte(value));
    data_.emplace_back(GetLowByte(value));
  }
  return true;
}

bool RtuRequest::SetWriteMultipleCoilsData(uint16_t start_address, uint16_t count, std::span<bool const> values) {
  if (header_.function_code != FunctionCode::kWriteMultCoils) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  if (values.size() != count) {
    return false;
  }

  data_.clear();
  // Address
  data_.emplace_back(GetHighByte(start_address));
  data_.emplace_back(GetLowByte(start_address));
  // Count
  data_.emplace_back(GetHighByte(count));
  data_.emplace_back(GetLowByte(count));
  // Byte count (8 coils per byte, rounded up)
  auto byte_count = static_cast<uint8_t>((count + supermb::kCoilByteCountRoundingOffset) / supermb::kCoilsPerByte);
  data_.emplace_back(byte_count);
  // Pack coils into bytes
  uint8_t current_byte = 0;
  uint8_t bit_position = 0;
  for (size_t i = 0; i < count; ++i) {
    if (values[i]) {
      current_byte |= (1 << bit_position);
    }
    ++bit_position;
    if (bit_position == supermb::kCoilsPerByte || i == static_cast<size_t>(count - 1)) {
      data_.emplace_back(current_byte);
      current_byte = 0;
      bit_position = 0;
    }
  }
  return true;
}

bool RtuRequest::SetDiagnosticsData(uint16_t sub_function_code, std::span<uint8_t const> data) {
  if (header_.function_code != FunctionCode::kDiagnostics) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);
    return false;
  }

  data_.clear();
  data_.emplace_back(GetHighByte(sub_function_code));
  data_.emplace_back(GetLowByte(sub_function_code));
  data_.insert(data_.end(), data.begin(), data.end());
  return true;
}

bool RtuRequest::SetMaskWriteRegisterData(uint16_t address, uint16_t and_mask, uint16_t or_mask) {
  if (header_.function_code != FunctionCode::kMaskWriteReg) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);
    return false;
  }

  data_.clear();
  data_.emplace_back(GetHighByte(address));
  data_.emplace_back(GetLowByte(address));
  data_.emplace_back(GetHighByte(and_mask));
  data_.emplace_back(GetLowByte(and_mask));
  data_.emplace_back(GetHighByte(or_mask));
  data_.emplace_back(GetLowByte(or_mask));
  return true;
}

bool RtuRequest::SetReadWriteMultipleRegistersData(uint16_t read_start, uint16_t read_count, uint16_t write_start,
                                                   uint16_t write_count, std::span<int16_t const> write_values) {
  if (header_.function_code != FunctionCode::kReadWriteMultRegs) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);
    return false;
  }

  if (write_values.size() != write_count) {
    return false;
  }

  data_.clear();
  // Read parameters
  data_.emplace_back(GetHighByte(read_start));
  data_.emplace_back(GetLowByte(read_start));
  data_.emplace_back(GetHighByte(read_count));
  data_.emplace_back(GetLowByte(read_count));
  // Write parameters
  data_.emplace_back(GetHighByte(write_start));
  data_.emplace_back(GetLowByte(write_start));
  data_.emplace_back(GetHighByte(write_count));
  data_.emplace_back(GetLowByte(write_count));
  // Byte count
  data_.emplace_back(static_cast<uint8_t>(write_count * 2));
  // Modbus RTU uses big-endian: high byte first, then low byte
  for (int16_t value : write_values) {
    data_.emplace_back(GetHighByte(value));
    data_.emplace_back(GetLowByte(value));
  }
  return true;
}

bool RtuRequest::SetReadFIFOQueueData(uint16_t fifo_address) {
  if (header_.function_code != FunctionCode::kReadFIFOQueue) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);
    return false;
  }

  data_.clear();
  data_.emplace_back(GetHighByte(fifo_address));
  data_.emplace_back(GetLowByte(fifo_address));
  return true;
}

bool RtuRequest::SetReadFileRecordData(std::span<std::tuple<uint16_t, uint16_t, uint16_t> const> file_records) {
  if (header_.function_code != FunctionCode::kReadFileRecord) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);
    return false;
  }

  if (file_records.empty() || file_records.size() > kMaxFileRecords) {
    return false;
  }

  data_.clear();
  // Calculate byte count: 6 bytes per record (file_number(2) + record_number(2) + record_length(2))
  auto byte_count = static_cast<uint8_t>(file_records.size() * supermb::kFileRecordBytesPerRecord);
  data_.emplace_back(byte_count);

  // Encode each record: file_number (high, low) + record_number (high, low) + record_length (high, low)
  // Slave expects: MakeInt16(data[offset+1], data[offset]) = MakeInt16(low, high) where data[offset]=high,
  // data[offset+1]=low
  for (auto const &[file_number, record_number, record_length] : file_records) {
    data_.emplace_back(GetHighByte(file_number));
    data_.emplace_back(GetLowByte(file_number));
    data_.emplace_back(GetHighByte(record_number));
    data_.emplace_back(GetLowByte(record_number));
    data_.emplace_back(GetHighByte(record_length));
    data_.emplace_back(GetLowByte(record_length));
  }

  return true;
}

bool RtuRequest::SetWriteFileRecordData(
    std::span<std::tuple<uint16_t, uint16_t, std::vector<int16_t>> const> file_records) {
  if (header_.function_code != FunctionCode::kWriteFileRecord) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);
    return false;
  }

  if (file_records.empty() || file_records.size() > kMaxFileRecords) {
    return false;
  }

  data_.clear();

  // First, build the data without byte_count to calculate it
  std::vector<uint8_t> temp_data;
  for (auto const &[file_number, record_number, record_data] : file_records) {
    // Reference type (0x06 for file record)
    temp_data.emplace_back(supermb::kFileRecordReferenceType);
    // File number (high byte, low byte)
    temp_data.emplace_back(GetHighByte(file_number));
    temp_data.emplace_back(GetLowByte(file_number));
    // Record number (high byte, low byte)
    temp_data.emplace_back(GetHighByte(record_number));
    temp_data.emplace_back(GetLowByte(record_number));
    // Record length (high byte, low byte)
    temp_data.emplace_back(GetHighByte(static_cast<uint16_t>(record_data.size())));
    temp_data.emplace_back(GetLowByte(static_cast<uint16_t>(record_data.size())));
    // Modbus RTU uses big-endian: high byte first, then low byte
    for (int16_t value : record_data) {
      temp_data.emplace_back(GetHighByte(value));
      temp_data.emplace_back(GetLowByte(value));
    }
  }

  // Prepend byte_count as first byte
  data_.emplace_back(static_cast<uint8_t>(temp_data.size()));
  // Append the rest of the data
  data_.insert(data_.end(), temp_data.begin(), temp_data.end());

  return true;
}

}  // namespace supermb
