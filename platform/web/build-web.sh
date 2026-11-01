#!/usr/bin/env bash
#
#  build-web.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-12.
#

set -euo pipefail

PRESET="web-wasm32"
TARGET="synesthesia-vibe-web"
WASM_NAME="synesthesia-vibe-wasm"
ZIP=0
SKIP_CONFIGURE=0

print_help() {
  cat <<'EOF'
build-web.sh -- builds the SynesthesiaVibe web library into build/web/dist/

USAGE

    platform/web/build-web.sh [--zip] [--skip-configure]

  Runs from any working directory. Sources $EMSDK/emsdk_env.sh, configures the web-wasm32
  CMake preset into build/web-wasm32, builds synesthesia-vibe-web with ninja, then assembles
  build/web/dist/: synesthesia-vibe-wasm.js, synesthesia-vibe-wasm.wasm, every
  platform/web/js/*.js and a VERSION stamp. The ES modules and the wasm glue sit in one
  folder because they import each other relative to import.meta.url; the public entry is
  synesthesia-vibe.js. No package.json, no bundler; resources are the integrator's.

PARAMETERS

  --zip             Also write build/web/synesthesia-vibe-web-<version>.zip (the dist files).
  --skip-configure  Reuse the existing build/web-wasm32 tree.
  --help            This text.

NOTES

  EMSDK must point at an activated emsdk checkout (e.g. ~/emsdk). VCPKG_ROOT defaults to
  ~/vcpkg when unset and present. The cmake configure runs from /tmp with PWD unset (cmake
  can hang on macOS when PWD is the repository root). The version is the root VERSION file
  when it exists, otherwise 0.0.1.

EXIT CODE

  0   dist (and zip) written to build/web/.
  1   Configure/build failed, an expected output is missing, or EMSDK is unusable.
  2   Usage error.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --zip)
      ZIP=1
      shift
      ;;
    --skip-configure)
      SKIP_CONFIGURE=1
      shift
      ;;
    --help|-h)
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/$PRESET"
OUT_ROOT="$REPO_ROOT/build/web"
DIST="$OUT_ROOT/dist"
LOG_FILE="$OUT_ROOT/build-web.log"

log() {
  printf '[build-web] %s\n' "$*"
}

fail() {
  printf '[build-web] ERROR: %s\n' "$*" >&2
  exit 1
}

if [[ -z "${EMSDK:-}" ]]; then
  fail "EMSDK is not set; export EMSDK=<path to an activated emsdk checkout> (e.g. ~/emsdk)"
fi
[[ -f "$EMSDK/emsdk_env.sh" ]] || fail "missing $EMSDK/emsdk_env.sh (EMSDK=$EMSDK)"
source "$EMSDK/emsdk_env.sh" > /dev/null 2>&1 || fail "sourcing $EMSDK/emsdk_env.sh failed"
command -v emcc > /dev/null || fail "emcc not on PATH after sourcing $EMSDK/emsdk_env.sh"
command -v ninja > /dev/null || fail "ninja not on PATH"

if [[ -z "${VCPKG_ROOT:-}" && -d "$HOME/vcpkg" ]]; then
  export VCPKG_ROOT="$HOME/vcpkg"
fi

VERSION="0.0.1"
if [[ -f "$REPO_ROOT/VERSION" ]]; then
  VERSION="$(tr -d '[:space:]' < "$REPO_ROOT/VERSION")"
fi
ZIP_PATH="$OUT_ROOT/synesthesia-vibe-web-$VERSION.zip"

run_cmake() {
  (cd /tmp && env -u PWD cmake "$@")
}

mkdir -p "$OUT_ROOT"
: > "$LOG_FILE"

log "version $VERSION"
log "emcc $(emcc -dumpversion)"
if [[ "$SKIP_CONFIGURE" -eq 0 ]]; then
  log "configure $PRESET -> $BUILD_DIR"
  if ! run_cmake -S "$REPO_ROOT" --preset "$PRESET" >> "$LOG_FILE" 2>&1; then
    tail -n 20 "$LOG_FILE" >&2 || true
    fail "configure $PRESET failed, see $LOG_FILE"
  fi
elif [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  fail "--skip-configure: $BUILD_DIR has no CMakeCache.txt"
fi

log "build $TARGET"
if ! ninja -C "$BUILD_DIR" "$TARGET" >> "$LOG_FILE" 2>&1; then
  tail -n 20 "$LOG_FILE" >&2 || true
  fail "build $TARGET failed, see $LOG_FILE"
fi

WASM_DIR="$BUILD_DIR/platform/web"
for EXT in js wasm; do
  [[ -f "$WASM_DIR/$WASM_NAME.$EXT" ]] || fail "missing $WASM_DIR/$WASM_NAME.$EXT"
done

rm -rf "$DIST"
mkdir -p "$DIST"
cp "$WASM_DIR/$WASM_NAME.js" "$WASM_DIR/$WASM_NAME.wasm" "$DIST/"
cp "$SCRIPT_DIR"/js/*.js "$DIST/"
printf '%s\n' "$VERSION" > "$DIST/VERSION"
[[ -f "$DIST/synesthesia-vibe.js" ]] || fail "missing $DIST/synesthesia-vibe.js"

if grep -l -F -e "$REPO_ROOT" -e "$HOME/" "$DIST"/*.js > /dev/null 2>&1; then
  fail "an absolute local path is baked into $(grep -l -F -e "$REPO_ROOT" -e "$HOME/" "$DIST"/*.js | head -n 1)"
fi

log "dist -> $DIST ($(ls "$DIST" | wc -l | tr -d ' ') files)"

if [[ "$ZIP" -eq 1 ]]; then
  command -v zip > /dev/null || fail "zip not on PATH"
  rm -f "$ZIP_PATH"
  (cd "$DIST" && zip -q -X -r "$ZIP_PATH" .)
  log "zip -> $ZIP_PATH"
fi
