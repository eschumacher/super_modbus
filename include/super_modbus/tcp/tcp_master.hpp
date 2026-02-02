#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../common/address_span.hpp"
#include "../common/exception_code.hpp"
#include "../common/function_code.hpp"
#include "../common/wire_format_options.hpp"
#include "../transport/byte_reader.hpp"
#include "../transport/byte_writer.hpp"
#include "tcp_frame.hpp"
#include "tcp_request.hpp"
#include "tcp_response.hpp"

namespace supermb {

/** Custom hasher for std::pair<uint16_t, uint16_t> (avoids undefined behavior of specializing std::hash). */
struct PairU16Hasher {
  size_t operator()(const std::pair<uint16_t, uint16_t> &p) const noexcept {
    return std::hash<uint16_t>{}(p.first) ^ (std::hash<uint16_t>{}(p.second) << 1);
  }
};

/**
 * @brief Modbus TCP Master/Client implementation
 *
 * This class provides master/client functionality for Modbus TCP communication.
 * It uses the transport abstraction layer to read/write bytes without knowing
 * the underlying communication mechanism.
 */
class TcpMaster {
 public:
  /**
   * @brief Construct a Modbus TCP Master
   * @param transport Transport layer for byte I/O
   * @param options Wire format options (byte/word order); default is standard Modbus (BigEndian)
   */
  explicit TcpMaster(ByteTransport &transport, WireFormatOptions options = {})
      : transport_(transport),
        options_(options),
        next_transaction_id_{1} {}

  /**
   * @brief Read holding registers from a unit
   * @param unit_id Target unit ID
   * @param start_address Starting register address
   * @param count Number of registers to read
   * @return Vector of register values, or empty if error
   */
  [[nodiscard]] std::optional<std::vector<int16_t>> ReadHoldingRegisters(uint8_t unit_id, uint16_t start_address,
                                                                         uint16_t count);

  /**
   * @brief Read 32-bit floats from holding registers (Enron-style; two registers per float).
   * @param unit_id Target unit ID
   * @param start_address Starting register address
   * @param count Number of floats to read (if float_count_semantics is CountIsFloatCount) or number of registers (if
   * CountIsRegisterCount)
   * @return Vector of float values, or empty if error
   */
  [[nodiscard]] std::optional<std::vector<float>> ReadFloats(uint8_t unit_id, uint16_t start_address, uint16_t count);

  /**
   * @brief Write 32-bit floats to holding registers (two registers per float).
   * @param unit_id Target unit ID
   * @param start_address Starting register address
   * @param values Float values to write
   * @return true on success, false on error
   */
  [[nodiscard]] bool WriteFloats(uint8_t unit_id, uint16_t start_address, std::span<const float> values);

  /**
   * @brief Read input registers from a unit
   * @param unit_id Target unit ID
   * @param start_address Starting register address
   * @param count Number of registers to read
   * @return Vector of register values, or empty if error
   */
  [[nodiscard]] std::optional<std::vector<int16_t>> ReadInputRegisters(uint8_t unit_id, uint16_t start_address,
                                                                       uint16_t count);

  /**
   * @brief Write a single holding register
   * @param unit_id Target unit ID
   * @param address Register address
   * @param value Register value to write
   * @return true on success, false on error
   */
  [[nodiscard]] bool WriteSingleRegister(uint8_t unit_id, uint16_t address, int16_t value);

  /**
   * @brief Read coils from a unit
   * @param unit_id Target unit ID
   * @param start_address Starting coil address
   * @param count Number of coils to read
   * @return Vector of coil values, or empty if error
   */
  [[nodiscard]] std::optional<std::vector<bool>> ReadCoils(uint8_t unit_id, uint16_t start_address, uint16_t count);

  /**
   * @brief Read discrete inputs from a unit
   * @param unit_id Target unit ID
   * @param start_address Starting discrete input address
   * @param count Number of discrete inputs to read
   * @return Vector of discrete input values, or empty if error
   */
  [[nodiscard]] std::optional<std::vector<bool>> ReadDiscreteInputs(uint8_t unit_id, uint16_t start_address,
                                                                    uint16_t count);

  /**
   * @brief Write a single coil
   * @param unit_id Target unit ID
   * @param address Coil address
   * @param value Coil value to write (true = ON, false = OFF)
   * @return true on success, false on error
   */
  [[nodiscard]] bool WriteSingleCoil(uint8_t unit_id, uint16_t address, bool value);

  /**
   * @brief Write multiple holding registers
   * @param unit_id Target unit ID
   * @param start_address Starting register address
   * @param values Register values to write
   * @return true on success, false on error
   */
  [[nodiscard]] bool WriteMultipleRegisters(uint8_t unit_id, uint16_t start_address, std::span<const int16_t> values);

  /**
   * @brief Write multiple coils
   * @param unit_id Target unit ID
   * @param start_address Starting coil address
   * @param values Coil values to write
   * @return true on success, false on error
   */
  [[nodiscard]] bool WriteMultipleCoils(uint8_t unit_id, uint16_t start_address, std::span<const bool> values);

  /**
   * @brief Read exception status (FC 7)
   * @param unit_id Target unit ID
   * @return Exception status byte, or empty if error
   */
  [[nodiscard]] std::optional<uint8_t> ReadExceptionStatus(uint8_t unit_id);

  /**
   * @brief Diagnostics (FC 8)
   * @param unit_id Target unit ID
   * @param sub_function_code Sub-function code
   * @param data Diagnostic data
   * @return Response data, or empty if error
   */
  [[nodiscard]] std::optional<std::vector<uint8_t>> Diagnostics(uint8_t unit_id, uint16_t sub_function_code,
                                                                std::span<const uint8_t> data);

  /**
   * @brief Get communication event counter (FC 11)
   * @param unit_id Target unit ID
   * @return Pair of (status, event_count), or empty if error
   */
  [[nodiscard]] std::optional<std::pair<uint8_t, uint16_t>> GetComEventCounter(uint8_t unit_id);

  /**
   * @brief Get communication event log (FC 12)
   * @param unit_id Target unit ID
   * @return Event log data, or empty if error
   */
  [[nodiscard]] std::optional<std::vector<uint8_t>> GetComEventLog(uint8_t unit_id);

  /**
   * @brief Report slave ID (FC 17)
   * @param unit_id Target unit ID
   * @return Slave ID info, or empty if error
   */
  [[nodiscard]] std::optional<std::vector<uint8_t>> ReportSlaveID(uint8_t unit_id);

  /**
   * @brief Mask write register (FC 22)
   * @param unit_id Target unit ID
   * @param address Register address
   * @param and_mask AND mask
   * @param or_mask OR mask
   * @return true on success, false on error
   */
  [[nodiscard]] bool MaskWriteRegister(uint8_t unit_id, uint16_t address, uint16_t and_mask, uint16_t or_mask);

  /**
   * @brief Read/Write multiple registers (FC 23)
   * @param unit_id Target unit ID
   * @param read_start Starting address for read
   * @param read_count Number of registers to read
   * @param write_start Starting address for write
   * @param write_values Values to write
   * @return Read register values, or empty if error
   */
  [[nodiscard]] std::optional<std::vector<int16_t>> ReadWriteMultipleRegisters(uint8_t unit_id, uint16_t read_start,
                                                                               uint16_t read_count,
                                                                               uint16_t write_start,
                                                                               std::span<const int16_t> write_values);

  /**
   * @brief Read FIFO queue (FC 24)
   * @param unit_id Target unit ID
   * @param fifo_address FIFO address
   * @return FIFO data, or empty if error
   */
  [[nodiscard]] std::optional<std::vector<int16_t>> ReadFIFOQueue(uint8_t unit_id, uint16_t fifo_address);

  /**
   * @brief Read file record (FC 20)
   * @param unit_id Target unit ID
   * @param file_records Vector of (file_number, record_number, record_length) tuples
   * @return Map of (file_number, record_number) -> record_data, or empty if error
   */
  [[nodiscard]] std::optional<std::unordered_map<std::pair<uint16_t, uint16_t>, std::vector<int16_t>, PairU16Hasher>>
  ReadFileRecord(uint8_t unit_id, std::span<const std::tuple<uint16_t, uint16_t, uint16_t>> file_records);

  /**
   * @brief Write file record (FC 21)
   * @param unit_id Target unit ID
   * @param file_records Vector of (file_number, record_number, record_data) tuples
   * @return true on success, false on error
   */
  [[nodiscard]] bool WriteFileRecord(
      uint8_t unit_id, std::span<const std::tuple<uint16_t, uint16_t, std::vector<int16_t>>> file_records);

  /**
   * @brief Send a custom request and receive response
   * @param request The request to send
   * @param timeout_ms Timeout in milliseconds (0 = no timeout)
   * @return Response if successful, empty optional on error/timeout
   */
  [[nodiscard]] std::optional<TcpResponse> SendRequest(const TcpRequest &request, uint32_t timeout_ms = 1000);

  /**
   * @brief Receive a response frame
   * @param expected_transaction_id Expected transaction ID in response
   * @param timeout_ms Timeout in milliseconds (0 = no timeout)
   * @return Response if successful, empty optional on error/timeout
   */
  [[nodiscard]] std::optional<TcpResponse> ReceiveResponse(uint16_t expected_transaction_id,
                                                           uint32_t timeout_ms = 1000);

  /**
   * @brief Get the next transaction ID (auto-increments)
   * @return Next transaction ID
   */
  [[nodiscard]] uint16_t GetNextTransactionId() { return next_transaction_id_++; }

 private:
  /**
   * @brief Read a complete TCP frame from transport
   * @param timeout_ms Timeout in milliseconds
   * @return Frame bytes if successful, empty optional on error/timeout
   */
  [[nodiscard]] std::optional<std::vector<uint8_t>> ReadFrame(uint32_t timeout_ms = 1000);

  ByteTransport &transport_;
  WireFormatOptions options_;
  uint16_t next_transaction_id_{1};
};

}  // namespace supermb
