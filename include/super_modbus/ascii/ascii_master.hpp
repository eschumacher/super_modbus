#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <unordered_map>
#include <vector>
#include "../common/address_span.hpp"
#include "../common/exception_code.hpp"
#include "../common/function_code.hpp"
#include "../common/wire_format_options.hpp"
#include "../transport/byte_reader.hpp"
#include "../transport/byte_writer.hpp"
#include "ascii_frame.hpp"
#include "../rtu/rtu_request.hpp"
#include "../rtu/rtu_response.hpp"

namespace supermb {

/**
 * @brief Modbus ASCII Master/Client implementation
 *
 * Same PDU and function codes as Modbus RTU; uses ASCII hex framing with LRC
 * instead of binary with CRC-16. Use with serial transport (ByteTransport).
 */
class AsciiMaster {
 public:
  explicit AsciiMaster(ByteTransport &transport, WireFormatOptions options = {})
      : transport_(transport),
        options_(options) {}

  [[nodiscard]] std::optional<std::vector<int16_t>> ReadHoldingRegisters(uint8_t slave_id, uint16_t start_address,
                                                                         uint16_t count);
  [[nodiscard]] std::optional<std::vector<float>> ReadFloats(uint8_t slave_id, uint16_t start_address, uint16_t count);
  [[nodiscard]] bool WriteFloats(uint8_t slave_id, uint16_t start_address, std::span<const float> values);
  [[nodiscard]] std::optional<std::vector<int16_t>> ReadInputRegisters(uint8_t slave_id, uint16_t start_address,
                                                                       uint16_t count);
  [[nodiscard]] std::optional<std::vector<bool>> ReadCoils(uint8_t slave_id, uint16_t start_address, uint16_t count);
  [[nodiscard]] std::optional<std::vector<bool>> ReadDiscreteInputs(uint8_t slave_id, uint16_t start_address,
                                                                    uint16_t count);
  [[nodiscard]] bool WriteSingleRegister(uint8_t slave_id, uint16_t address, int16_t value);
  [[nodiscard]] bool WriteSingleCoil(uint8_t slave_id, uint16_t address, bool value);
  [[nodiscard]] bool WriteMultipleRegisters(uint8_t slave_id, uint16_t start_address, std::span<const int16_t> values);
  [[nodiscard]] bool WriteMultipleCoils(uint8_t slave_id, uint16_t start_address, std::span<const bool> values);
  [[nodiscard]] std::optional<uint8_t> ReadExceptionStatus(uint8_t slave_id);
  [[nodiscard]] std::optional<std::vector<uint8_t>> Diagnostics(uint8_t slave_id, uint16_t sub_function_code,
                                                                std::span<const uint8_t> data);
  [[nodiscard]] std::optional<std::pair<uint8_t, uint16_t>> GetComEventCounter(uint8_t slave_id);
  [[nodiscard]] std::optional<std::vector<uint8_t>> GetComEventLog(uint8_t slave_id);
  [[nodiscard]] std::optional<std::vector<uint8_t>> ReportSlaveID(uint8_t slave_id);
  [[nodiscard]] bool MaskWriteRegister(uint8_t slave_id, uint16_t address, uint16_t and_mask, uint16_t or_mask);
  [[nodiscard]] std::optional<std::vector<int16_t>> ReadWriteMultipleRegisters(uint8_t slave_id, uint16_t read_start,
                                                                               uint16_t read_count,
                                                                               uint16_t write_start,
                                                                               std::span<const int16_t> write_values);
  [[nodiscard]] std::optional<std::vector<int16_t>> ReadFIFOQueue(uint8_t slave_id, uint16_t fifo_address);
  [[nodiscard]] std::optional<std::unordered_map<std::pair<uint16_t, uint16_t>, std::vector<int16_t>>> ReadFileRecord(
      uint8_t slave_id, std::span<const std::tuple<uint16_t, uint16_t, uint16_t>> file_records);
  [[nodiscard]] bool WriteFileRecord(
      uint8_t slave_id, std::span<const std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records);

  [[nodiscard]] std::optional<RtuResponse> SendRequest(const RtuRequest &request, uint32_t timeout_ms = 1000);

 private:
  [[nodiscard]] std::optional<std::string> ReadAsciiFrame(uint32_t timeout_ms = 1000);

  ByteTransport &transport_;
  WireFormatOptions options_;
};

}  // namespace supermb
