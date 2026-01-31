#!/bin/bash
# Build script for Super Modbus library

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default options
BUILD_TYPE="Release"
BUILD_EXAMPLES="ON"
BUILD_TESTS="OFF"
JOBS=$(nproc 2>/dev/null || echo 4)

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --debug)
      BUILD_TYPE="Debug"
      BUILD_TESTS="ON"
      shift
      ;;
    --no-examples)
      BUILD_EXAMPLES="OFF"
      shift
      ;;
    --with-tests)
      BUILD_TESTS="ON"
      BUILD_TYPE="Debug"
      shift
      ;;
    --jobs|-j)
      JOBS="$2"
      shift 2
      ;;
    --help|-h)
      echo "Usage: $0 [options]"
      echo ""
      echo "Options:"
      echo "  --debug          Build in Debug mode (enables tests)"
      echo "  --no-examples    Don't build examples"
      echo "  --with-tests     Build and run tests"
      echo "  --jobs N, -j N   Number of parallel jobs (default: auto)"
      echo "  --help, -h       Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

echo -e "${GREEN}Building Super Modbus Library${NC}"
echo "  Build Type: $BUILD_TYPE"
echo "  Examples: $BUILD_EXAMPLES"
echo "  Tests: $BUILD_TESTS"
echo "  Jobs: $JOBS"
echo ""

# Create build directory
if [ ! -d "build" ]; then
  mkdir build
fi

cd build

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake .. \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DBUILD_EXAMPLES="$BUILD_EXAMPLES" \
  -DBUILD_TESTS="$BUILD_TESTS"

# Build
echo -e "${YELLOW}Building...${NC}"
make -j"$JOBS"

echo ""
echo -e "${GREEN}Build successful!${NC}"
echo ""

# Show what was built
if [ "$BUILD_EXAMPLES" = "ON" ]; then
  echo "Examples built:"
  ls -lh example_* testable_slave 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
  echo ""
fi

if [ "$BUILD_TESTS" = "ON" ]; then
  echo "Test executable:"
  ls -lh run_tests 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
  echo ""
  echo "To run tests:"
  echo "  cd build && ctest --output-on-failure"
  echo ""
fi

echo "Library:"
ls -lh libsuper-modbus-lib.a 2>/dev/null | awk '{print "  " $9 " (" $5 ")"}'
echo ""

echo "To use with Modbus Poll:"
echo "  1. Implement serial transport (see examples/example_serial_transport.cpp)"
echo "  2. Build testable_slave: make testable_slave"
echo "  3. Run: ./testable_slave /dev/ttyUSB0 9600 1"
echo "  4. Connect Modbus Poll to the serial port"
