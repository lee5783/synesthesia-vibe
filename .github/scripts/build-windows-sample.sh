#!/usr/bin/env bash
#
#  build-windows-sample.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-11-25.
#

set -euo pipefail

ARCH="x64"
VCVARS_VER=""
DIST_OVERRIDE=""

print_help() {
  cat <<'EOF2'
build-windows-sample.sh -- builds samples/windows against the Windows library dist (a link check)

USAGE

    .github/scripts/build-windows-sample.sh [--arch x64|arm64] [--dist DIR] [--vcvars-ver VER]

  Runs from any working directory in Git Bash. Configures and builds samples/windows with Ninja into
  build/samples/windows-<arch>/ against build/windows/dist-<arch>/ (written by
  platform/windows/build-windows.sh), then checks that the exe and the dist DLLs are side by side.
  The result is a Release console exe; the arm64 exe is cross-compiled and is not run.

PARAMETERS

  --arch ARCH       x64 (default) or arm64 (cross-compiled with x64-hosted MSVC tools).
  --dist DIR        Dist folder to link against instead of build/windows/dist-<arch>/.
  --vcvars-ver VER  MSVC toolset passed to vcvarsall.bat as -vcvars_ver (e.g. 14.44).
  --help            This text.

EXIT CODE

  0   Sample built.
  1   The dist is missing or the build failed.
  2   Usage error.
EOF2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      [[ $# -ge 2 ]] || { echo "--arch needs a value" >&2; exit 2; }
      ARCH="$2"
      shift 2
      ;;
    --dist)
      [[ $# -ge 2 ]] || { echo "--dist needs a value" >&2; exit 2; }
      DIST_OVERRIDE="$2"
      shift 2
      ;;
    --vcvars-ver)
      [[ $# -ge 2 ]] || { echo "--vcvars-ver needs a value" >&2; exit 2; }
      VCVARS_VER="$2"
      shift 2
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

case "$ARCH" in
  x64) VCVARS_ARCH="x64" ;;
  arm64) VCVARS_ARCH="x64_arm64" ;;
  *)
    echo "Unknown --arch $ARCH (x64 or arm64)" >&2
    exit 2
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SAMPLE_DIR="$REPO_ROOT/samples/windows"
DIST="${DIST_OVERRIDE:-$REPO_ROOT/build/windows/dist-$ARCH}"
BUILD_DIR="$REPO_ROOT/build/samples/windows-$ARCH"
EXE_NAME="synesthesia-vibe-windows-sample"

log() {
  printf '[build-windows-sample] %s\n' "$*"
}

fail() {
  printf '[build-windows-sample] ERROR: %s\n' "$*" >&2
  exit 1
}

import_msvc_env() {
  local vswhere="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
  [[ -x "$vswhere" ]] || fail "cl.exe is not on PATH and vswhere.exe was not found; run from an MSVC developer environment"

  local install
  install="$("$vswhere" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | tr -d '\r')"
  [[ -n "$install" ]] || fail "no Visual Studio with the C++ x64 tools was found"

  local arguments="$VCVARS_ARCH"
  if [[ -n "$VCVARS_VER" ]]; then
    arguments="$arguments -vcvars_ver=$VCVARS_VER"
  fi

  local script
  script="$(mktemp --suffix=.cmd)"
  printf '@set "PATH=%%PATH%%;%s"\r\n@call "%s\\VC\\Auxiliary\\Build\\vcvarsall.bat" %s >nul || exit /b 1\r\n@set\r\n' \
    "$(cygpath -w "$(dirname "$vswhere")")" "$install" "$arguments" > "$script"

  local dump
  dump="$(cmd.exe //c "$(cygpath -w "$script")" | tr -d '\r')" || { rm -f "$script"; fail "vcvarsall.bat $arguments failed"; }
  rm -f "$script"

  local name value
  while IFS='=' read -r name value; do
    [[ "$name" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]] || continue
    case "$name" in
      PATH|Path) export PATH="$(cygpath -u -p "$value")" ;;
      PWD|OLDPWD|HOME|SHELL|SHLVL|TERM|TMP|TEMP|VCPKG_ROOT|_) ;;
      *) export "$name=$value" ;;
    esac
  done <<< "$dump"

  command -v cl.exe > /dev/null || fail "cl.exe is still not on PATH after vcvarsall.bat $arguments"
}

[[ -f "$DIST/include/synesthesia-vibe/synesthesia-vibe-windows.h" ]] || fail "missing dist at $DIST (run platform/windows/build-windows.sh --arch $ARCH first)"

if ! command -v cl.exe > /dev/null; then
  import_msvc_env
fi
command -v cmake > /dev/null || fail "cmake not on PATH"
command -v ninja > /dev/null || fail "ninja not on PATH"

run_cmake() {
  (cd /tmp && env -u PWD cmake "$@")
}

log "arch $ARCH, dist $DIST, $(cl.exe 2>&1 | head -n 1 | tr -d '\r')"
rm -rf "$BUILD_DIR"
run_cmake -S "$(cygpath -m "$SAMPLE_DIR")" -B "$(cygpath -m "$BUILD_DIR")" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSYNESTHESIA_VIBE_DIST="$(cygpath -m "$DIST")" || fail "configure failed"
run_cmake --build "$(cygpath -m "$BUILD_DIR")" || fail "build failed"

for FILE in "$EXE_NAME.exe" synesthesia-vibe.dll libEGL.dll libGLESv2.dll; do
  [[ -f "$BUILD_DIR/$FILE" ]] || fail "missing $BUILD_DIR/$FILE"
done

log "sample -> $BUILD_DIR/$EXE_NAME.exe"
