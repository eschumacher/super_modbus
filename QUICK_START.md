# Quick Start Guide

## Build the Library

```bash
# Quick build (recommended)
./scripts/build.sh

# Or manual build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON
make -j$(nproc)
```

## Build Options

```bash
# Debug build with tests
./scripts/build.sh --debug --with-tests

# Release build without examples
./scripts/build.sh --no-examples

# Custom number of jobs
./scripts/build.sh --jobs 8
```

## Test with Modbus Poll

### Step 1: Set Up Virtual Serial Ports (Linux)

```bash
# Install socat
sudo apt-get install socat

# Create virtual port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Note the two /dev/pts/X paths shown
```

### Step 2: Implement Serial Transport

See `examples/example_serial_transport.cpp` for template. You need to implement:
- `Read()` - read bytes from serial port
- `Write()` - write bytes to serial port
- `Flush()` - flush output buffer
- `HasData()` - check if data available
- `AvailableBytes()` - get available byte count

### Step 3: Build Testable Slave

```bash
cd build
make testable_slave
```

### Step 4: Run Slave

```bash
# With real serial port
./testable_slave /dev/ttyUSB0 9600 1

# With virtual port
./testable_slave /dev/pts/2 9600 1
```

### Step 5: Configure Modbus Poll

- **Connection**: RTU over Serial Port
- **Port**: COM3 (or your port)
- **Baud Rate**: 9600
- **Data Bits**: 8
- **Parity**: Even (or None)
- **Stop Bits**: 1
- **Slave ID**: 1

### Step 6: Test Operations

1. **Read Holding Registers (FC 3)**: Address 0, Quantity 10
2. **Write Single Register (FC 6)**: Address 0, Value 1234
3. **Read Coils (FC 1)**: Address 0, Quantity 8
4. **Write Single Coil (FC 5)**: Address 0, Value ON

## Common Commands

```bash
# Clean build
rm -rf build && mkdir build && cd build && cmake .. && make

# Build specific target
cd build && make testable_slave

# Run tests
cd build && ctest --output-on-failure

# Check serial ports (Linux)
ls -l /dev/ttyUSB* /dev/ttyACM* /dev/ttyS*

# Check port permissions
ls -l /dev/ttyUSB0
```

## Troubleshooting

**Build fails**: Check CMake version (3.15+) and C++20 compiler

**No serial port**: Install drivers for USB-to-serial adapter

**Permission denied**: Add user to dialout group:
```bash
sudo usermod -a -G dialout $USER
# Then logout and login
```

**Modbus Poll timeout**: Check baud rate and serial settings match

**CRC errors**: Verify serial port settings exactly match

## Next Steps

1. See `BUILD_AND_TEST.md` for detailed build instructions
2. See `scripts/test_with_modbus_poll.md` for Modbus Poll setup
3. See `examples/README.md` for example code documentation
