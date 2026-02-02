#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "../common/address_span.hpp"
#include "../common/function_code.hpp"
#include "../common/wire_format_options.hpp"

namespace supermb {

class TcpRequest {
 public:
  struct Header {
    uint16_t transaction_id;
    uint8_t unit_id;
    FunctionCode function_code;
  };

  explicit TcpRequest(Header header, ByteOrder byte_order = ByteOrder::BigEndian)
      : header_(header),
        byte_order_(byte_order) {}

  [[nodiscard]] uint16_t GetTransactionId() const { return header_.transaction_id; }
  [[nodiscard]] uint8_t GetUnitId() const { return header_.unit_id; }
  [[nodiscard]] FunctionCode GetFunctionCode() const { return header_.function_code; }
  [[nodiscard]] const std::vector<uint8_t> &GetData() const { return data_; }
  [[nodiscard]] std::optional<AddressSpan> GetAddressSpan() const;

  bool SetAddressSpan(AddressSpan address_span);
  bool SetWriteSingleRegisterData(uint16_t register_address, int16_t register_value);
  bool SetWriteSingleCoilData(uint16_t coil_address, bool coil_value);
  bool SetWriteMultipleRegistersData(uint16_t start_address, uint16_t count, std::span<const int16_t> values);
  bool SetWriteMultipleCoilsData(uint16_t start_address, uint16_t count, std::span<const bool> values);
  bool SetDiagnosticsData(uint16_t sub_function_code, std::span<const uint8_t> data);
  bool SetMaskWriteRegisterData(uint16_t address, uint16_t and_mask, uint16_t or_mask);
  bool SetReadWriteMultipleRegistersData(uint16_t read_start, uint16_t read_count, uint16_t write_start,
                                         uint16_t write_count, std::span<const int16_t> write_values);
  bool SetReadFIFOQueueData(uint16_t fifo_address);
  bool SetReadFileRecordData(std::span<const std::tuple<uint16_t, uint16_t, uint16_t>> file_records);
  bool SetWriteFileRecordData(std::span<const std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records);
  void SetRawData(const std::vector<uint8_t> &data) { data_ = data; }
  void SetRawData(std::span<const uint8_t> data) { data_.assign(data.begin(), data.end()); }

  [[nodiscard]] ByteOrder GetByteOrder() const noexcept { return byte_order_; }

 private:
  Header header_;
  ByteOrder byte_order_;
  std::vector<uint8_t> data_{};
};

}  // namespace supermb
