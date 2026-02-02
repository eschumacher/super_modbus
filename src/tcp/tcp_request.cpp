#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
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
// byte_count is uint8_t (0-255); each Read File Record sub-request is 7 bytes
static constexpr size_t kMaxReadFileRecordByteCount{255};
static constexpr size_t kBytesPerReadFileRecord{7};

// Helper function to check if function code is in valid functions array
static constexpr bool IsAddressSpanValidFunction(FunctionCode function_code) {
  return std::any_of(std::begin(kAddressSpanValidFunctions), std::end(kAddressSpanValidFunctions),
                     [function_code](FunctionCode valid_code) { return valid_code == function_code; });
}

std::optional<AddressSpan> TcpRequest::GetAddressSpan() const {
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

bool TcpRequest::SetAddressSpan(AddressSpan address_span) {
  if (!IsAddressSpanValidFunction(header_.function_code)) {
    assert(false);  // likely a library defect if hit - create ticket in github
    return false;
  }

  data_.clear();
  data_.resize(kAddressSpanMinDataSize);
  EncodeU16(address_span.start_address, byte_order_, &data_[kAddressSpanStartAddressIndex]);
  EncodeU16(address_span.reg_count, byte_order_, &data_[kAddressSpanRegCountIndex]);

  return true;
}

bool TcpRequest::SetWriteSingleRegisterData(uint16_t register_address, int16_t register_value) {
  if (header_.function_code != FunctionCode::kWriteSingleReg) {
    assert(false);
    return false;
  }

  data_.clear();
  data_.resize(4);
  EncodeU16(register_address, byte_order_, &data_[0]);
  EncodeU16(static_cast<uint16_t>(register_value), byte_order_, &data_[2]);

  return true;
}

bool TcpRequest::SetWriteSingleCoilData(uint16_t coil_address, bool coil_value) {
  if (header_.function_code != FunctionCode::kWriteSingleCoil) {
    assert(false);
    return false;
  }

  data_.clear();
  data_.resize(4);
  EncodeU16(coil_address, byte_order_, &data_[0]);
  uint16_t value = coil_value ? kCoilOnValue : 0x0000;
  EncodeU16(value, byte_order_, &data_[2]);

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
  EncodeU16(start_address, byte_order_, &data_[0]);
  EncodeU16(count, byte_order_, &data_[2]);
  data_[4] = static_cast<uint8_t>(count * 2);
  for (size_t i = 0; i < values.size(); ++i) {
    EncodeU16(static_cast<uint16_t>(values[i]), byte_order_, &data_[5 + i * 2]);
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

  EncodeU16(start_address, byte_order_, &data_[0]);
  EncodeU16(count, byte_order_, &data_[2]);
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

  constexpr size_t kMaxSafeDataSize = SIZE_MAX - 2;
  if (data.size() > kMaxSafeDataSize) {
    return false;
  }
  const size_t total_size = 2 + data.size();
  this->data_.clear();
  this->data_.resize(total_size);

  // Sub-function code (big-endian: high byte first)
  EncodeU16(sub_function_code, byte_order_, &this->data_[0]);

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

  EncodeU16(address, byte_order_, &data_[0]);
  EncodeU16(and_mask, byte_order_, &data_[2]);
  EncodeU16(or_mask, byte_order_, &data_[4]);

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

  EncodeU16(read_start, byte_order_, &data_[0]);
  EncodeU16(read_count, byte_order_, &data_[2]);
  EncodeU16(write_start, byte_order_, &data_[4]);
  EncodeU16(write_count, byte_order_, &data_[6]);
  data_[8] = static_cast<uint8_t>(write_count * 2);
  for (size_t i = 0; i < write_values.size(); ++i) {
    EncodeU16(static_cast<uint16_t>(write_values[i]), byte_order_, &data_[9 + i * 2]);
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
  EncodeU16(fifo_address, byte_order_, &data_[0]);

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

  // byte_count is uint8_t (0-255); avoid overflow when casting to uint8_t
  const size_t byte_count_value = file_records.size() * kBytesPerReadFileRecord;
  if (byte_count_value > kMaxReadFileRecordByteCount) {
    return false;
  }

  // Calculate total size: byte_count(1) + file_records * (reference_type(1) + file_number(2) + record_number(2) +
  // record_length(2))
  size_t total_size = 1 + byte_count_value;

  data_.clear();
  data_.resize(total_size);

  // Byte count (number of bytes following)
  data_[0] = static_cast<uint8_t>(byte_count_value);

  // File records
  size_t offset = 1;
  for (const auto &record : file_records) {
    // Reference type
    data_[offset++] = kFileRecordReferenceType;

    uint16_t file_number = std::get<0>(record);
    EncodeU16(file_number, byte_order_, &data_[offset]);
    offset += 2;

    uint16_t record_number = std::get<1>(record);
    EncodeU16(record_number, byte_order_, &data_[offset]);
    offset += 2;

    uint16_t record_length = std::get<2>(record);
    EncodeU16(record_length, byte_order_, &data_[offset]);
    offset += 2;
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

  if (total_size == 0 || total_size > 252) {  // Modbus PDU max ~253 bytes for data
    return false;
  }
  data_.clear();
  data_.resize(total_size);

  // Byte count (number of bytes following)
  data_[0] = static_cast<uint8_t>(total_size - 1);

  // File records
  size_t offset = 1;
  for (const auto &record : file_records) {
    // Reference type
    data_[offset++] = kFileRecordReferenceType;

    uint16_t file_number = std::get<0>(record);
    EncodeU16(file_number, byte_order_, &data_[offset]);
    offset += 2;

    uint16_t record_number = std::get<1>(record);
    EncodeU16(record_number, byte_order_, &data_[offset]);
    offset += 2;

    const auto &record_data = std::get<2>(record);
    uint16_t record_length = static_cast<uint16_t>(record_data.size());
    EncodeU16(record_length, byte_order_, &data_[offset]);
    offset += 2;

    for (int16_t value : record_data) {
      EncodeU16(static_cast<uint16_t>(value), byte_order_, &data_[offset]);
      offset += 2;
    }
  }

  return true;
}

}  // namespace supermb
