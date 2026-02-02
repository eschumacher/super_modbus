#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
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
  // Use std::any_of with iterators for compiler compatibility (works with all C++11+ compilers)
  // std::ranges::any_of is not available in libc++ for clang-14/15
  return std::any_of(std::begin(kAddressSpanValidFunctions), std::end(kAddressSpanValidFunctions),
                     [function_code](FunctionCode valid_code) { return valid_code == function_code; });
}

std::optional<AddressSpan> RtuRequest::GetAddressSpan() const {
  if ((data_.size() < kAddressSpanMinDataSize) || !IsAddressSpanValidFunction(header_.function_code)) {
    return {};
  }

  AddressSpan address_span;
  address_span.start_address = static_cast<uint16_t>(
      DecodeU16(data_[kAddressSpanStartAddressIndex], data_[kAddressSpanStartAddressIndex + 1], byte_order_));
  address_span.reg_count = static_cast<uint16_t>(
      DecodeU16(data_[kAddressSpanRegCountIndex], data_[kAddressSpanRegCountIndex + 1], byte_order_));
  return address_span;
}

bool RtuRequest::SetAddressSpan(AddressSpan address_span) {
  if (!IsAddressSpanValidFunction(header_.function_code)) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  data_.clear();
  uint8_t buf[2];
  EncodeU16(address_span.start_address, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  EncodeU16(address_span.reg_count, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  return true;
}

bool RtuRequest::SetWriteSingleRegisterData(uint16_t register_address, int16_t register_value) {
  if (header_.function_code != FunctionCode::kWriteSingleReg) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  data_.clear();
  uint8_t buf[2];
  EncodeU16(register_address, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  EncodeU16(static_cast<uint16_t>(register_value), byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  return true;
}

bool RtuRequest::SetWriteSingleCoilData(uint16_t coil_address, bool coil_value) {
  if (header_.function_code != FunctionCode::kWriteSingleCoil) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  data_.clear();
  uint8_t buf[2];
  EncodeU16(coil_address, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  // Modbus spec: 0x0000 = OFF, 0xFF00 = ON
  uint16_t value = coil_value ? kCoilOnValue : 0x0000;
  EncodeU16(value, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  return true;
}

bool RtuRequest::SetWriteMultipleRegistersData(uint16_t start_address, uint16_t count,
                                               std::span<const int16_t> values) {
  if (header_.function_code != FunctionCode::kWriteMultRegs) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  if (values.size() != count) {
    return false;
  }

  data_.clear();
  uint8_t buf[2];
  EncodeU16(start_address, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  EncodeU16(count, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  data_.emplace_back(static_cast<uint8_t>(count * 2));
  for (int16_t value : values) {
    EncodeU16(static_cast<uint16_t>(value), byte_order_, buf);
    data_.emplace_back(buf[0]);
    data_.emplace_back(buf[1]);
  }
  return true;
}

bool RtuRequest::SetWriteMultipleCoilsData(uint16_t start_address, uint16_t count, std::span<const bool> values) {
  if (header_.function_code != FunctionCode::kWriteMultCoils) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  if (values.size() != count) {
    return false;
  }

  data_.clear();
  uint8_t buf[2];
  EncodeU16(start_address, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  EncodeU16(count, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
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

bool RtuRequest::SetDiagnosticsData(uint16_t sub_function_code, std::span<const uint8_t> data) {
  if (header_.function_code != FunctionCode::kDiagnostics) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);
    return false;
  }

  data_.clear();
  uint8_t buf[2];
  EncodeU16(sub_function_code, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
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
  uint8_t buf[2];
  EncodeU16(address, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  EncodeU16(and_mask, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  EncodeU16(or_mask, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  return true;
}

bool RtuRequest::SetReadWriteMultipleRegistersData(uint16_t read_start, uint16_t read_count, uint16_t write_start,
                                                   uint16_t write_count, std::span<const int16_t> write_values) {
  if (header_.function_code != FunctionCode::kReadWriteMultRegs) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);
    return false;
  }

  if (write_values.size() != write_count) {
    return false;
  }

  data_.clear();
  uint8_t buf[2];
  EncodeU16(read_start, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  EncodeU16(read_count, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  EncodeU16(write_start, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  EncodeU16(write_count, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  data_.emplace_back(static_cast<uint8_t>(write_count * 2));
  for (int16_t value : write_values) {
    EncodeU16(static_cast<uint16_t>(value), byte_order_, buf);
    data_.emplace_back(buf[0]);
    data_.emplace_back(buf[1]);
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
  uint8_t buf[2];
  EncodeU16(fifo_address, byte_order_, buf);
  data_.emplace_back(buf[0]);
  data_.emplace_back(buf[1]);
  return true;
}

bool RtuRequest::SetReadFileRecordData(std::span<const std::tuple<uint16_t, uint16_t, uint16_t>> file_records) {
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

  uint8_t buf[2];
  for (const auto &[file_number, record_number, record_length] : file_records) {
    EncodeU16(file_number, byte_order_, buf);
    data_.emplace_back(buf[0]);
    data_.emplace_back(buf[1]);
    EncodeU16(record_number, byte_order_, buf);
    data_.emplace_back(buf[0]);
    data_.emplace_back(buf[1]);
    EncodeU16(record_length, byte_order_, buf);
    data_.emplace_back(buf[0]);
    data_.emplace_back(buf[1]);
  }

  return true;
}

bool RtuRequest::SetWriteFileRecordData(
    std::span<const std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records) {
  if (header_.function_code != FunctionCode::kWriteFileRecord) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    assert(false);
    return false;
  }

  if (file_records.empty() || file_records.size() > kMaxFileRecords) {
    return false;
  }

  data_.clear();

  uint8_t buf[2];
  std::vector<uint8_t> temp_data;
  for (const auto &[file_number, record_number, record_data] : file_records) {
    temp_data.emplace_back(supermb::kFileRecordReferenceType);
    EncodeU16(file_number, byte_order_, buf);
    temp_data.emplace_back(buf[0]);
    temp_data.emplace_back(buf[1]);
    EncodeU16(record_number, byte_order_, buf);
    temp_data.emplace_back(buf[0]);
    temp_data.emplace_back(buf[1]);
    EncodeU16(static_cast<uint16_t>(record_data.size()), byte_order_, buf);
    temp_data.emplace_back(buf[0]);
    temp_data.emplace_back(buf[1]);
    for (int16_t value : record_data) {
      EncodeU16(static_cast<uint16_t>(value), byte_order_, buf);
      temp_data.emplace_back(buf[0]);
      temp_data.emplace_back(buf[1]);
    }
  }

  // Prepend byte_count as first byte
  data_.emplace_back(static_cast<uint8_t>(temp_data.size()));
  // Append the rest of the data
  data_.insert(data_.end(), temp_data.begin(), temp_data.end());

  return true;
}

}  // namespace supermb
