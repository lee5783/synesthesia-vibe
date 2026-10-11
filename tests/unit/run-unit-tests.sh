#!/usr/bin/env bash
#
#  run-unit-tests.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-09-30.
#

set -euo pipefail

print_help() {
  cat <<'EOF'
run-unit-tests.sh -- P2-019 synesthesia-vibe-unit-tests runner (Mac wrapper)

USAGE

  Thin wrapper around ctest (C2-020: ctest itself is the tool - no logic lives
  here). Runs the synesthesia-vibe-unit-tests suite registered by tests/unit/CMakeLists.txt
  against an already-configured/built macos-arm64 build tree.

    ./run-unit-tests.sh [--build-dir <path>] [--config <cfg>] [--filter <ctest -R regex>]

PARAMETERS

  --build-dir  Path to the CMake build directory. Default: build/macos-arm64.
  --config     Optional build configuration passed to ctest -C (not needed for the
               single-config Ninja macos-arm64 preset; omitted by default).
  --filter     Optional ctest -R regex to run a subset of tests.

  Build first if the tree isn't configured/built yet:

    cmake --preset macos-arm64
    cmake --build build/macos-arm64

EXIT CODE

  Forwards ctest's exit code: 0 all tests passed, non-zero otherwise.
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BUILD_DIR="$REPO_ROOT/build/macos-arm64"
CONFIG=""
FILTER=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --config)
      CONFIG="$2"
      shift 2
      ;;
    --filter)
      FILTER="$2"
      shift 2
      ;;
    --help)
      print_help
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      print_help
      exit 2
      ;;
  esac
done

if [[ "$BUILD_DIR" != /* ]]; then
  BUILD_DIR="$REPO_ROOT/$BUILD_DIR"
fi

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "Build directory not found: $BUILD_DIR"
  echo "Configure and build it first (see --help)."
  exit 2
fi

CTEST_ARGS=(--test-dir "$BUILD_DIR" --output-on-failure)
if [[ -n "$CONFIG" ]]; then
  CTEST_ARGS+=(-C "$CONFIG")
fi
if [[ -n "$FILTER" ]]; then
  CTEST_ARGS+=(-R "$FILTER")
fi

ctest "${CTEST_ARGS[@]}"
exit $?
