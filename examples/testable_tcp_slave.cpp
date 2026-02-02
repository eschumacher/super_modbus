/**
 * @file testable_tcp_slave.cpp
 * @brief Runnable Modbus TCP slave for use with mbpoll or other Modbus TCP masters
 *
 * Listens on a configurable address/port and serves Modbus TCP requests.
 * Test with: mbpoll -m tcp -a 1 -0 -r 0 -c 10 -1 127.0.0.1 -p 5502
 *
 * Usage:
 *   ./testable_tcp_slave [bind_address] [port] [unit_id]
 *
 * Example:
 *   ./testable_tcp_slave 0.0.0.0 502 1
 *   ./testable_tcp_slave 127.0.0.1 5502 1
 */

#include "tcp_transport.hpp"
#include "super_modbus/common/function_code.hpp"
#include "super_modbus/tcp/tcp_request.hpp"
#include "super_modbus/tcp/tcp_slave.hpp"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#ifdef __linux__
#include <unistd.h>
#endif

using supermb::FunctionCode;
using supermb::TcpConnectionTransport;
using supermb::TcpListen;
using supermb::TcpRequest;
using supermb::TcpSlave;

volatile bool g_running = true;

void signal_handler([[maybe_unused]] int signal) {
  g_running = false;
}

int main(int argc, const char *argv[]) {
  if (argc >= 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    std::cerr << "Usage: " << argv[0] << " [bind_address] [port] [unit_id]\n";
    std::cerr << "\n";
    std::cerr << "Arguments:\n";
    std::cerr << "  bind_address  Address to bind (default: 0.0.0.0)\n";
    std::cerr << "  port          TCP port (default: 502)\n";
    std::cerr << "  unit_id       Modbus unit ID (default: 1)\n";
    std::cerr << "\n";
    std::cerr << "Examples:\n";
    std::cerr << "  " << argv[0] << " 0.0.0.0 502 1\n";
    std::cerr << "  " << argv[0] << " 127.0.0.1 5502 1\n";
    std::cerr << "\n";
    std::cerr << "Test with mbpoll:\n";
    std::cerr << "  mbpoll -m tcp -a 1 -0 -r 0 -c 10 -1 127.0.0.1 -p 502\n";
    return 0;
  }

  const std::string bind_address = (argc >= 2) ? argv[1] : "0.0.0.0";
  const uint16_t port = (argc >= 3) ? static_cast<uint16_t>(std::atoi(argv[2])) : 502;
  const uint8_t unit_id = (argc >= 4) ? static_cast<uint8_t>(std::atoi(argv[3])) : 1;

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

#ifdef __linux__
  int listen_fd = TcpListen(bind_address, port);
  if (listen_fd < 0) {
    std::cerr << "Error: Failed to listen on " << bind_address << ":" << port << "\n";
    std::cerr << "  Check that the port is not in use and you have permission (e.g. 502 may need root).\n";
    return 1;
  }

  TcpSlave slave(unit_id);

  slave.AddHoldingRegisters({0, 100});
  slave.AddInputRegisters({0, 50});
  slave.AddCoils({0, 100});
  slave.AddDiscreteInputs({0, 50});

  std::vector<int16_t> fifo_data{0x1111, 0x2222, 0x3333, 0x4444};
  slave.SetFIFOQueue(0, fifo_data);

  // Initial register values for testing
  {
    TcpRequest::Header header{0, unit_id, FunctionCode::kWriteSingleReg};
    TcpRequest req(header);
    req.SetWriteSingleRegisterData(0, 0x1234);
    slave.Process(req);
    req.SetWriteSingleRegisterData(1, 0x5678);
    slave.Process(req);
  }

  std::cout << "Modbus TCP Slave\n";
  std::cout << "  Bind: " << bind_address << ":" << port << "\n";
  std::cout << "  Unit ID: " << static_cast<int>(unit_id) << "\n";
  std::cout << "  Holding Registers: 0-99\n";
  std::cout << "  Input Registers: 0-49\n";
  std::cout << "  Coils: 0-99\n";
  std::cout << "  Discrete Inputs: 0-49\n";
  std::cout << "  FIFO Queue at address 0: " << fifo_data.size() << " entries\n";
  std::cout << "\n";
  std::cout << "Test with: mbpoll -m tcp -a " << static_cast<int>(unit_id) << " -0 -r 0 -c 10 -1 127.0.0.1 -p " << port
            << "\n";
  std::cout << "\n";
  std::cout << "Waiting for connections... Press Ctrl+C to stop\n\n";

  uint32_t total_requests = 0;

  while (g_running) {
    struct timeval timeout {};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(listen_fd, &fds);
    int ret = select(listen_fd + 1, &fds, nullptr, nullptr, &timeout);
    if (ret <= 0) {
      continue;
    }

    int client_fd = accept(listen_fd, nullptr, nullptr);
    if (client_fd < 0) {
      if (g_running) {
        std::cerr << "accept() failed\n";
      }
      continue;
    }

    std::cout << "Client connected\n";

    TcpConnectionTransport transport(client_fd);
    uint32_t client_requests = 0;

    while (g_running && transport.IsOpen()) {
      if (slave.Poll(transport)) {
        ++client_requests;
        ++total_requests;
        if (total_requests % 10 == 0) {
          std::cout << "Processed " << total_requests << " requests\n";
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "Client disconnected (processed " << client_requests << " requests)\n";
  }

  close(listen_fd);
  std::cout << "\nShutdown. Total requests: " << total_requests << "\n";
  return 0;

#else
  (void)bind_address;
  (void)port;
  (void)unit_id;
  std::cerr << "TCP slave is only implemented on Linux/POSIX.\n";
  return 1;
#endif
}
