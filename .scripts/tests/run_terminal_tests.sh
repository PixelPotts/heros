#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────
# HerOS Terminal — Test Runner
# Compiles and runs the test harness for all 100 terminal commands.
# ──────────────────────────────────────────────────────────────
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
TEST_BIN="$BUILD_DIR/test_terminal"

echo ""
echo "╔═══════════════════════════════════════════════════════╗"
echo "║  HerOS Terminal Test Runner                          ║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""

# Ensure core is built
if [ ! -f "$BUILD_DIR/vfs.o" ]; then
    echo "Core not built. Running 'make' first..."
    cd "$PROJECT_DIR" && make
fi

# Compiler flags (same as Makefile)
CXX="g++"
CXXFLAGS="-std=c++17 -Wall -Wextra -fPIC $(pkg-config --cflags sdl2 SDL2_ttf SDL2_image)"
LDFLAGS="$(pkg-config --libs sdl2 SDL2_ttf SDL2_image) -ldl"
TERMINAL_LIBS="-lcurl -lssl -lcrypto -lz -larchive -lyaml-cpp"

# Core object files (everything except main.o which has main())
CORE_OBJS=$(ls "$BUILD_DIR"/*.o | grep -v main.o | tr '\n' ' ')

echo "Compiling test harness..."
$CXX $CXXFLAGS \
    "$SCRIPT_DIR/test_terminal_commands.cpp" \
    $CORE_OBJS \
    -o "$TEST_BIN" \
    $LDFLAGS $TERMINAL_LIBS

# Copy commands.yaml so man/manall can find it
YAML_SRC="$PROJECT_DIR/src/apps/commands.yaml"
export HEROS_COMMANDS_YAML="$YAML_SRC"

echo "Running tests..."
echo ""

# Run the test binary
"$TEST_BIN"
EXIT_CODE=$?

# Cleanup
rm -f "$TEST_BIN"

exit $EXIT_CODE
