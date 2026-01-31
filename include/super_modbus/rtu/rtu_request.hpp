#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "../common/address_span.hpp"
#include "../common/function_code.hpp"

namespace supermb {

class RtuRequest {
 public:
  struct Header {
    uint8_t slave_id;
    FunctionCode function_code;
  };

  explicit RtuRequest(Header header)
      : header_(header) {}

  [[nodiscard]] uint8_t GetSlaveId() const { return header_.slave_id; }
  [[nodiscard]] FunctionCode GetFunctionCode() const { return header_.function_code; }
  [[nodiscard]] std::vector<uint8_t> const &GetData() const { return data_; }
  [[nodiscard]] std::optional<AddressSpan> GetAddressSpan() const;

  bool SetAddressSpan(AddressSpan address_span);
  bool SetWriteSingleRegisterData(uint16_t register_address, int16_t register_value);
  bool SetWriteSingleCoilData(uint16_t coil_address, bool coil_value);
  bool SetWriteMultipleRegistersData(uint16_t start_address, uint16_t count, std::span<int16_t const> values);
  bool SetWriteMultipleCoilsData(uint16_t start_address, uint16_t count, std::span<bool const> values);
  bool SetDiagnosticsData(uint16_t sub_function_code, std::span<uint8_t const> data);
  bool SetMaskWriteRegisterData(uint16_t address, uint16_t and_mask, uint16_t or_mask);
  bool SetReadWriteMultipleRegistersData(uint16_t read_start, uint16_t read_count, uint16_t write_start,
                                         uint16_t write_count, std::span<int16_t const> write_values);
  bool SetReadFIFOQueueData(uint16_t fifo_address);
  bool SetReadFileRecordData(std::span<std::tuple<uint16_t, uint16_t, uint16_t> const> file_records);
  bool SetWriteFileRecordData(std::span<std::tuple<uint16_t, uint16_t, std::vector<int16_t>> const> file_records);
  void SetRawData(std::vector<uint8_t> const &data) { data_ = data; }
  void SetRawData(std::span<uint8_t const> data) {
    data_.assign(data.begin(), data.end());
  }

 private:
  Header header_;
  std::vector<uint8_t> data_{};
};

}  // namespace supermb
