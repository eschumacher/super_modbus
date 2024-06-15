#include <limits>
#include <vector>
#include "rtu/rtu_slave.hpp"
#include "common/exception_code.hpp"

namespace supermb {

RtuResponse RtuSlave::Process(RtuRequest const &request) {
  RtuResponse response{request.GetSlaveId(), request.GetFunctionCode()};
  switch (request.GetFunctionCode()) {
    case FunctionCode::kReadHR: {
      // TODO: replace dummy data
      std::vector<uint8_t> dummy_data{};
      for (int i = 0; i < request.GetAddressSpan().reg_count_; ++i) {
        // two bytes per register for now (no enron modbus 32-bit register
        // support yet)
        dummy_data.push_back(i);
        dummy_data.push_back(std::numeric_limits<uint8_t>::max() - i);
      }
      response.SetData(dummy_data);
      response.SetExceptionCode(ExceptionCode::kAcknowledge);
      break;
    }
    default: {
      response.SetExceptionCode(ExceptionCode::kIllegalFunction);
      break;
    }
  }

  return response;
}

}  // namespace supermb
