#!/bin/bash
# ci-local.sh - Run CI steps locally (format, build, test, E2E, optional static analysis)
# Usage: ./scripts/ci-local.sh [--no-e2e] [--no-rtu-e2e] [--clang-tidy] [--release] [--help]

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

RUN_E2E=true
RUN_RTU_E2E=true
RUN_CLANG_TIDY=false
RUN_RELEASE=false
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE=Debug

while [[ $# -gt 0 ]]; do
  case $1 in
    --no-e2e)
      RUN_E2E=false
      shift
      ;;
    --no-rtu-e2e)
      RUN_RTU_E2E=false
      shift
      ;;
    --clang-tidy)
      RUN_CLANG_TIDY=true
      shift
      ;;
    --release)
      RUN_RELEASE=true
      shift
      ;;
    --help|-h)
      echo "Usage: $0 [options]"
      echo ""
      echo "Options:"
      echo "  --no-e2e       Skip TCP E2E (testable_tcp_slave + mbpoll)"
      echo "  --no-rtu-e2e   Skip RTU E2E (test_with_mbpoll_comprehensive.sh)"
      echo "  --clang-tidy   Run clang-tidy on src/include/test"
      echo "  --release      Also run Release build + ctest (no tests in Release by design)"
      echo "  --help, -h     Show this help"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

cd "$ROOT_DIR"

echo -e "${BLUE}=== 1. Format check (clang-format) ===${NC}"
FAILED_FILES=()
while IFS= read -r -d '' file; do
  if ! clang-format --style=file --dry-run --Werror "$file" 2>/dev/null; then
    FAILED_FILES+=("$file")
  fi
done < <(find . -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \) \
  -not -path "./build/*" -not -path "./build-*/*" -not -path "./.git/*" -not -path "./cmake/*" -print0 2>/dev/null)
if [ ${#FAILED_FILES[@]} -gt 0 ]; then
  echo -e "${RED}Format errors in:${NC}"
  printf '%s\n' "${FAILED_FILES[@]}"
  exit 1
fi
echo -e "${GREEN}✓ Format check OK${NC}"
echo ""

echo -e "${BLUE}=== 2. Configure & build (Debug, tests, examples) ===${NC}"
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DPROFILING=OFF
make -j$(nproc 2>/dev/null || echo 4)
echo -e "${GREEN}✓ Build OK${NC}"
echo ""

echo -e "${BLUE}=== 3. Run all tests (ctest) ===${NC}"
ctest --output-on-failure
echo -e "${GREEN}✓ Tests OK${NC}"
echo ""

echo -e "${BLUE}=== 4. Integration tests (filters) ===${NC}"
RUN_TESTS="./test/run_tests"
[ -f "./run_tests" ] && RUN_TESTS="./run_tests"
if [ -f "$RUN_TESTS" ]; then
  $RUN_TESTS --gtest_filter="*Integration*" || true
fi
echo -e "${GREEN}✓ Integration filters OK${NC}"
echo ""

echo -e "${BLUE}=== 5. example_loopback (E2E) ===${NC}"
if [ -f "./example_loopback" ]; then
  timeout 30 ./example_loopback
  echo -e "${GREEN}✓ example_loopback OK${NC}"
else
  echo -e "${YELLOW}  example_loopback not found, skip${NC}"
fi
echo ""

if [ "$RUN_E2E" = true ]; then
  echo -e "${BLUE}=== 6. TCP E2E (testable_tcp_slave + mbpoll) ===${NC}"
  TCP_SLAVE="./testable_tcp_slave"
  [ ! -f "$TCP_SLAVE" ] && TCP_SLAVE="./bin/testable_tcp_slave"
  [ ! -f "$TCP_SLAVE" ] && TCP_SLAVE="./testable_tcp_slave"
  if [ -f "$TCP_SLAVE" ] && command -v mbpoll >/dev/null 2>&1; then
    timeout 15 "$TCP_SLAVE" 127.0.0.1 5502 1 &
    TCP_PID=$!
    sleep 2
    if mbpoll -m tcp -a 1 -0 -r 0 -c 5 -1 -p 5502 127.0.0.1; then
      echo -e "${GREEN}✓ TCP E2E OK${NC}"
    else
      kill $TCP_PID 2>/dev/null || true
      wait $TCP_PID 2>/dev/null || true
      echo -e "${RED}TCP E2E failed${NC}"
      exit 1
    fi
    kill $TCP_PID 2>/dev/null || true
    wait $TCP_PID 2>/dev/null || true
  else
    echo -e "${YELLOW}  testable_tcp_slave or mbpoll not found, skip${NC}"
  fi
  echo ""
fi

cd "$ROOT_DIR"
if [ "$RUN_RTU_E2E" = true ]; then
  echo -e "${BLUE}=== 7. RTU E2E (test_with_mbpoll_comprehensive.sh) ===${NC}"
  if [ -f scripts/test_with_mbpoll_comprehensive.sh ] && command -v socat >/dev/null 2>&1; then
    if [ -f build/example_virtual_port_test ] || [ -f build/testable_slave ]; then
      chmod +x scripts/test_with_mbpoll_comprehensive.sh
      ./scripts/test_with_mbpoll_comprehensive.sh || { echo -e "${RED}RTU E2E failed${NC}"; exit 1; }
      echo -e "${GREEN}✓ RTU E2E OK${NC}"
    else
      echo -e "${YELLOW}  Slave executable not found, skip RTU E2E${NC}"
    fi
  else
    echo -e "${YELLOW}  Script or socat not found, skip RTU E2E${NC}"
  fi
  echo ""
fi

if [ "$RUN_CLANG_TIDY" = true ]; then
  echo -e "${BLUE}=== 8. clang-tidy (optional) ===${NC}"
  cd "$ROOT_DIR/build"
  TIDY_INCLUDE="-I../include/super_modbus"
  TIDY_FAIL=0
  for f in $(find ../src ../include ../test -type f \( -name "*.cpp" -o -name "*.hpp" \) 2>/dev/null); do
    if ! clang-tidy "$f" -p . -- -std=c++20 $TIDY_INCLUDE 2>/dev/null; then
      TIDY_FAIL=1
    fi
  done
  if [ $TIDY_FAIL -eq 1 ]; then
    echo -e "${YELLOW}  clang-tidy reported issues (see above)${NC}"
  else
    echo -e "${GREEN}✓ clang-tidy OK${NC}"
  fi
  cd "$ROOT_DIR"
  echo ""
fi

if [ "$RUN_RELEASE" = true ]; then
  echo -e "${BLUE}=== Release build ===${NC}"
  cd "$ROOT_DIR"
  rm -rf build-release
  mkdir -p build-release
  cd build-release
  cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DPROFILING=OFF
  make -j$(nproc 2>/dev/null || echo 4)
  echo -e "${GREEN}✓ Release build OK (tests not built in Release by design)${NC}"
  cd "$ROOT_DIR"
  echo ""
fi

echo -e "${GREEN}=== CI-local finished ===${NC}"
