#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "../common/address_span.hpp"
#include "../common/wire_format_options.hpp"
#include "../transport/byte_reader.hpp"
#include "../transport/byte_writer.hpp"
#include "../rtu/rtu_request.hpp"
#include "../rtu/rtu_response.hpp"
#include "../rtu/rtu_slave.hpp"

namespace supermb {

/**
 * @brief Modbus ASCII Slave/Server implementation
 *
 * Same PDU and function codes as Modbus RTU; uses ASCII hex framing with LRC.
 * Delegates request processing to RtuSlave (shared PDU logic).
 */
class AsciiSlave {
 public:
  explicit AsciiSlave(uint8_t slave_id, WireFormatOptions options = {})
      : rtu_slave_(slave_id, options) {}

  [[nodiscard]] uint8_t GetId() const noexcept { return rtu_slave_.GetId(); }
  void SetId(uint8_t slave_id) noexcept { rtu_slave_.SetId(slave_id); }

  /**
   * @brief Process a Modbus ASCII request and return response
   */
  RtuResponse Process(const RtuRequest &request) { return rtu_slave_.Process(request); }

  /**
   * @brief Process incoming ASCII frame from transport and send response
   */
  bool ProcessIncomingFrame(ByteTransport &transport, uint32_t timeout_ms = 1000);

  /**
   * @brief Poll transport for incoming frames and process them
   */
  bool Poll(ByteTransport &transport);

  void AddHoldingRegisters(AddressSpan span) { rtu_slave_.AddHoldingRegisters(span); }
  void AddInputRegisters(AddressSpan span) { rtu_slave_.AddInputRegisters(span); }
  void AddCoils(AddressSpan span) { rtu_slave_.AddCoils(span); }
  void AddDiscreteInputs(AddressSpan span) { rtu_slave_.AddDiscreteInputs(span); }
  void AddFloatRange(uint16_t start_register, uint16_t register_count) {
    rtu_slave_.AddFloatRange(start_register, register_count);
  }
  bool SetFloat(size_t float_index, float value) { return rtu_slave_.SetFloat(float_index, value); }
  void SetFIFOQueue(uint16_t fifo_address, const std::vector<int16_t> &queue_data) {
    rtu_slave_.SetFIFOQueue(fifo_address, queue_data);
  }

 private:
  [[nodiscard]] static std::optional<std::string> ReadAsciiFrame(ByteReader &transport, uint32_t timeout_ms);

  RtuSlave rtu_slave_;
};

}  // namespace supermb
