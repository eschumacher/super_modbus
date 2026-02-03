#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>
#include "../common/address_map.hpp"
#include "../common/address_span.hpp"
#include "../common/wire_format_options.hpp"
#include "../transport/byte_reader.hpp"
#include "../transport/byte_writer.hpp"
#include "rtu_request.hpp"
#include "rtu_response.hpp"

namespace supermb {

// Default timeout for frame processing (1 second)
static constexpr uint32_t kDefaultFrameTimeoutMs = 1000;

class RtuSlave {
 public:
  explicit RtuSlave(uint8_t slave_id, WireFormatOptions options = {})
      : id_(slave_id),
        options_(options) {}

  [[nodiscard]] uint8_t GetId() const noexcept { return id_; }
  void SetId(uint8_t slave_id) noexcept { id_ = slave_id; }
  [[nodiscard]] ByteOrder GetByteOrder() const noexcept { return options_.byte_order; }

  /**
   * @brief Process a Modbus request and return response
   * @param request The request to process
   * @return Response to the request
   */
  RtuResponse Process(const RtuRequest &request);

  /**
   * @brief Process incoming frame from transport and send response
   * @param transport Transport layer for reading frames and writing responses
   * @param timeout_ms Timeout in milliseconds (0 = no timeout)
   * @return true if a frame was processed, false if no frame or error
   */
  bool ProcessIncomingFrame(ByteTransport &transport, uint32_t timeout_ms = kDefaultFrameTimeoutMs);

  /**
   * @brief Poll transport for incoming frames and process them
   * @param transport Transport layer for reading frames and writing responses
   * @return true if a frame was processed, false if no frame available
   */
  bool Poll(ByteTransport &transport);

  void AddHoldingRegisters(AddressSpan span);
  void AddInputRegisters(AddressSpan span);
  void AddCoils(AddressSpan span);
  void AddDiscreteInputs(AddressSpan span);

  /**
   * @brief Add a float range: holding registers [start_register, start_register + register_count) store 32-bit floats.
   * Each float uses two registers. register_count must be even. Allocates float storage of size register_count/2.
   */
  void AddFloatRange(uint16_t start_register, uint16_t register_count);

  /**
   * @brief Set one float value in the float range (float_index is 0-based within the range).
   */
  bool SetFloat(size_t float_index, float value);

  /**
   * @brief Set FIFO queue data at specified address
   * @param fifo_address FIFO address
   * @param queue_data Queue data (vector of register values)
   */
  void SetFIFOQueue(uint16_t fifo_address, const std::vector<int16_t> &queue_data) {
    fifo_storage_[fifo_address] = queue_data;
  }

 private:
  /**
   * @brief Read a complete RTU frame from transport
   * @param transport Transport to read from
   * @param timeout_ms Timeout in milliseconds
   * @return Frame bytes if successful, empty optional on error/timeout
   */
  [[nodiscard]] static std::optional<std::vector<uint8_t>> ReadFrame(ByteReader &transport,
                                                                     uint32_t timeout_ms = kDefaultFrameTimeoutMs);

  void ProcessReadRegisters(const AddressMap<int16_t> &address_map, const RtuRequest &request, RtuResponse &response,
                            bool for_holding_registers = false);
  void ProcessWriteSingleRegister(AddressMap<int16_t> &address_map, const RtuRequest &request, RtuResponse &response);
  void ProcessReadCoils(const AddressMap<bool> &address_map, const RtuRequest &request, RtuResponse &response);
  void ProcessWriteSingleCoil(AddressMap<bool> &address_map, const RtuRequest &request, RtuResponse &response);
  void ProcessWriteMultipleRegisters(AddressMap<int16_t> &address_map, const RtuRequest &request,
                                     RtuResponse &response);
  void ProcessWriteMultipleCoils(AddressMap<bool> &address_map, const RtuRequest &request, RtuResponse &response);
  void ProcessReadExceptionStatus(const RtuRequest &request, RtuResponse &response) const;
  void ProcessDiagnostics(const RtuRequest &request, RtuResponse &response);
  void ProcessGetComEventCounter(const RtuRequest &request, RtuResponse &response) const;
  void ProcessGetComEventLog(const RtuRequest &request, RtuResponse &response) const;
  void ProcessReportSlaveID(const RtuRequest &request, RtuResponse &response) const;
  void ProcessReadFileRecord(const RtuRequest &request, RtuResponse &response) const;
  void ProcessWriteFileRecord(const RtuRequest &request, RtuResponse &response);
  void ProcessMaskWriteRegister(AddressMap<int16_t> &address_map, const RtuRequest &request, RtuResponse &response);
  void ProcessReadWriteMultipleRegisters(AddressMap<int16_t> &address_map, const RtuRequest &request,
                                         RtuResponse &response);
  void ProcessReadFIFOQueue(const RtuRequest &request, RtuResponse &response) const;

  // File Record storage: file_number -> (record_number -> record_data)
  // Each record is a vector of 16-bit values (registers)
  using FileRecord = std::vector<int16_t>;
  using FileRecords = std::unordered_map<uint16_t, FileRecord>;   // record_number -> record_data
  using FileStorage = std::unordered_map<uint16_t, FileRecords>;  // file_number -> records

  // FIFO Queue storage: fifo_address -> queue of register values
  using FIFOQueue = std::vector<int16_t>;
  using FIFOStorage = std::unordered_map<uint16_t, FIFOQueue>;

  // Communication event log entry
  struct ComEventLogEntry {
    uint16_t event_id;
    uint16_t event_count;
  };
  using ComEventLog = std::vector<ComEventLogEntry>;

  uint8_t id_{1};
  WireFormatOptions options_{};
  uint8_t exception_status_{0};    // For FC 7
  uint16_t com_event_counter_{0};  // For FC 11
  uint16_t message_count_{0};      // For FC 12
  AddressMap<int16_t> holding_registers_{};
  AddressMap<int16_t> input_registers_{};
  AddressMap<bool> coils_{};
  AddressMap<bool> discrete_inputs_{};
  FileStorage file_storage_{};   // File record storage
  FIFOStorage fifo_storage_{};   // FIFO queue storage
  ComEventLog com_event_log_{};  // Communication event log

  std::optional<std::pair<uint16_t, uint16_t>> float_range_{};  // (start_register, register_count)
  std::vector<float> float_storage_{};
};

}  // namespace supermb
