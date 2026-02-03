# Testing with Modbus Poll - Step by Step Guide

## Prerequisites

- Modbus Poll (Windows) or similar Modbus master software
- Serial port (real or virtual)
- Super Modbus library built

## Step 1: Build the Library

```bash
# Quick build
./scripts/build.sh

# Or manually
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON
make -j$(nproc)
```

## Step 2: Set Up Serial Communication

### Option A: Virtual Serial Ports (Linux)

```bash
# Install socat
sudo apt-get install socat

# Create virtual port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0

# Output will show two ports, e.g.:
# /dev/pts/2 and /dev/pts/3
```

### Option B: Real Serial Hardware

- Connect USB-to-serial adapter
- Note the port name: `/dev/ttyUSB0` (Linux) or `COM3` (Windows)

### Option C: Windows Virtual Ports

1. Install [com0com](https://sourceforge.net/projects/com0com/)
2. Create COM port pair (e.g., COM3 <-> COM4)
3. Use one port for your application, other for Modbus Poll

## Step 3: Implement Serial Transport

You need to implement `ByteTransport` for your serial port. See `examples/serial_transport.hpp` for a POSIX reference implementation.

Quick example using POSIX (Linux):

```cpp
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include "super_modbus/transport/byte_reader.hpp"
#include "super_modbus/transport/byte_writer.hpp"

class PosixSerialTransport : public supermb::ByteTransport {
  int fd_;

public:
  PosixSerialTransport(const char* port, int baud_rate) {
    fd_ = open(port, O_RDWR | O_NOCTTY);
    // Configure termios settings...
  }

  int Read(std::span<uint8_t> buffer) override {
    return read(fd_, buffer.data(), buffer.size());
  }

  int Write(std::span<uint8_t const> data) override {
    return write(fd_, data.data(), data.size());
  }

  // ... implement other methods
};
```

## Step 4: Build Testable Slave

```bash
cd build
make testable_slave
```

## Step 5: Run Slave Application

```bash
# Linux
./testable_slave /dev/ttyUSB0 9600 1

# Or with virtual port
./testable_slave /dev/pts/2 9600 1
```

## Step 6: Configure Modbus Poll

1. **Open Modbus Poll**
2. **Connection Settings**:
   - Connection: RTU over Serial Port
   - Serial Port: COM3 (or your port)
   - Baud Rate: 9600
   - Data Bits: 8
   - Parity: Even (or None)
   - Stop Bits: 1
   - Slave ID: 1

3. **Add Read/Write Windows**:
   - Right-click → Add Window
   - Select function code (FC 3 for holding registers, FC 1 for coils, etc.)
   - Set address and quantity

## Step 7: Test Operations

### Read Holding Registers (FC 3)
- Address: 0
- Quantity: 10
- Should read 10 registers

### Write Single Register (FC 6)
- Address: 0
- Value: 1234
- Then read back to verify

### Read Coils (FC 1)
- Address: 0
- Quantity: 8
- Should read 8 coils

### Write Single Coil (FC 5)
- Address: 0
- Value: ON
- Then read back to verify

## Troubleshooting

### "No Response" Error
- Check baud rate matches (9600, 19200, etc.)
- Verify parity settings (Even/None)
- Check stop bits (usually 1)
- Ensure slave ID matches (default: 1)

### "Timeout" Error
- Increase timeout in Modbus Poll settings
- Check serial port is not in use by another application
- Verify cable/connection

### "CRC Error"
- Serial port settings must match exactly
- Check for electrical interference
- Verify baud rate is correct

### "Illegal Data Address"
- Check address range:
  - Holding Registers: 0-99
  - Input Registers: 0-49
  - Coils: 0-99
  - Discrete Inputs: 0-49

## Quick Test Checklist

- [ ] Library built successfully
- [ ] Serial transport implemented
- [ ] Slave application running
- [ ] Modbus Poll connected to correct port
- [ ] Serial settings match (baud, parity, etc.)
- [ ] Slave ID matches (default: 1)
- [ ] Can read holding registers
- [ ] Can write holding registers
- [ ] Can read coils
- [ ] Can write coils

## Example Test Sequence

1. **Read Holding Register 0**: Should return current value (or 0 if uninitialized)
2. **Write Holding Register 0 = 0x1234**: Should succeed
3. **Read Holding Register 0**: Should return 0x1234
4. **Read Coil 0**: Should return current state
5. **Write Coil 0 = ON**: Should succeed
6. **Read Coil 0**: Should return ON
7. **Broadcast Write** (slave ID 0): Should succeed (no response expected)

## Advanced Testing

### Test All Function Codes

| FC | Function | Test |
|----|----------|------|
| 1  | Read Coils | Read 8 coils from address 0 |
| 2  | Read Discrete Inputs | Read 8 discrete inputs from address 0 |
| 3  | Read Holding Registers | Read 10 registers from address 0 |
| 4  | Read Input Registers | Read 10 input registers from address 0 |
| 5  | Write Single Coil | Write coil 0 = ON |
| 6  | Write Single Register | Write register 0 = 0x1234 |
| 15 | Write Multiple Coils | Write 8 coils starting at 0 |
| 16 | Write Multiple Registers | Write 5 registers starting at 0 |
| 24 | Read FIFO Queue | Read FIFO queue at address 0 |

### Monitor Communication

Enable verbose logging in your slave application to see:
- Incoming requests
- Processed function codes
- Response sent
- Error conditions
