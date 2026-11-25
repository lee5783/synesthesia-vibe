#!/usr/bin/env bash
#
#  build-windows.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-11-24.
#

set -euo pipefail

ARCH="x64"
TARGET="synesthesia-vibe-windows"
DLL_NAME="synesthesia-vibe"
ZIP=0
SKIP_CONFIGURE=0
VCVARS_VER="${SV_VCVARS_VER:-}"

print_help() {
  cat <<'EOF'
build-windows.sh -- builds the SynesthesiaVibe Windows library into build/windows/dist-<arch>/

USAGE

    platform/windows/build-windows.sh [--arch x64|arm64] [--zip] [--skip-configure] [--vcvars-ver VER]

  Runs from any working directory in Git Bash. Configures the windows-<arch> CMake preset into
  build/windows-<arch>, builds synesthesia-vibe-windows with ninja, checks that the DLL exports
  every name in platform/windows/synesthesia-vibe.def, then assembles build/windows/dist-<arch>/:
  bin/ (synesthesia-vibe.dll, libEGL.dll, libGLESv2.dll), lib/ (synesthesia-vibe.lib),
  include/synesthesia-vibe/ (synesthesia-vibe.h, synesthesia-vibe-windows.h), NOTICE and a
  VERSION stamp. Library only; the sample in samples/windows builds against the dist.

PARAMETERS

  --arch ARCH       x64 (default) or arm64 (cross-compiled with x64-hosted MSVC tools).
  --zip             Also write build/windows/synesthesia-vibe-windows-<arch>-<version>.zip.
  --skip-configure  Reuse the existing build/windows-<arch> tree.
  --vcvars-ver VER  MSVC toolset passed to vcvarsall.bat as -vcvars_ver (e.g. 14.44); also
                    read from SV_VCVARS_VER. Used only when cl.exe is not already on PATH.
  --help            This text.

NOTES

  When cl.exe is not on PATH the script loads the MSVC developer environment of the newest
  Visual Studio with the C++ tools (vswhere + vcvarsall.bat x64 or x64_arm64). VCPKG_ROOT
  defaults to the vcpkg bundled with that Visual Studio when unset. The cmake configure runs
  from /tmp with PWD unset, like the other packaging scripts. The version is the root VERSION
  file when it exists, otherwise 0.0.1.

EXIT CODE

  0   dist (and zip) written to build/windows/.
  1   Configure/build failed, an export or expected output is missing, or MSVC is unusable.
  2   Usage error.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --arch)
      [[ $# -ge 2 ]] || { print_help; exit 2; }
      ARCH="$2"
      shift 2
      ;;
    --zip)
      ZIP=1
      shift
      ;;
    --skip-configure)
      SKIP_CONFIGURE=1
      shift
      ;;
    --vcvars-ver)
      [[ $# -ge 2 ]] || { print_help; exit 2; }
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

PRESET="windows-$ARCH"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/$PRESET"
OUT_ROOT="$REPO_ROOT/build/windows"
DIST="$OUT_ROOT/dist-$ARCH"
LOG_FILE="$OUT_ROOT/build-windows-$ARCH.log"
DEF_FILE="$SCRIPT_DIR/synesthesia-vibe.def"

log() {
  printf '[build-windows] %s\n' "$*"
}

fail() {
  printf '[build-windows] ERROR: %s\n' "$*" >&2
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

if ! command -v cl.exe > /dev/null; then
  import_msvc_env
fi
command -v cmake > /dev/null || fail "cmake not on PATH"
command -v ninja > /dev/null || fail "ninja not on PATH"
command -v dumpbin.exe > /dev/null || fail "dumpbin.exe not on PATH"

if [[ -z "${VCPKG_ROOT:-}" && -n "${VSINSTALLDIR:-}" && -d "$(cygpath -u "$VSINSTALLDIR")/VC/vcpkg" ]]; then
  export VCPKG_ROOT="${VSINSTALLDIR%\\}\\VC\\vcpkg"
fi
[[ -n "${VCPKG_ROOT:-}" ]] || fail "VCPKG_ROOT is not set and no vcpkg was found in the Visual Studio install"

VERSION="0.0.1"
if [[ -f "$REPO_ROOT/VERSION" ]]; then
  VERSION="$(tr -d '[:space:]' < "$REPO_ROOT/VERSION")"
fi
ZIP_PATH="$OUT_ROOT/synesthesia-vibe-windows-$ARCH-$VERSION.zip"

run_cmake() {
  (cd /tmp && env -u PWD cmake "$@")
}

mkdir -p "$OUT_ROOT"
: > "$LOG_FILE"

log "version $VERSION, arch $ARCH, $(cl.exe 2>&1 | head -n 1 | tr -d '\r')"
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

LIB_DIR="$BUILD_DIR/platform/windows"
for FILE in "$DLL_NAME.dll" "$DLL_NAME.lib" libEGL.dll libGLESv2.dll; do
  [[ -f "$LIB_DIR/$FILE" ]] || fail "missing $LIB_DIR/$FILE"
done

EXPORTS="$(dumpbin.exe -nologo -exports "$(cygpath -w "$LIB_DIR/$DLL_NAME.dll")" | tr -d '\r')"
MISSING=0
while read -r SYMBOL; do
  [[ "$SYMBOL" =~ ^svw?_ ]] || continue
  if ! grep -q -w -- "$SYMBOL" <<< "$EXPORTS"; then
    printf '[build-windows] missing export %s\n' "$SYMBOL" >&2
    MISSING=1
  fi
done < <(tr -d '\r' < "$DEF_FILE")
[[ "$MISSING" -eq 0 ]] || fail "$DLL_NAME.dll does not export every name in $DEF_FILE"

rm -rf "$DIST"
mkdir -p "$DIST/bin" "$DIST/lib" "$DIST/include/synesthesia-vibe"
cp "$LIB_DIR/$DLL_NAME.dll" "$LIB_DIR/libEGL.dll" "$LIB_DIR/libGLESv2.dll" "$DIST/bin/"
cp "$LIB_DIR/$DLL_NAME.lib" "$DIST/lib/"
cp "$SCRIPT_DIR/include/synesthesia-vibe/synesthesia-vibe-windows.h" "$DIST/include/synesthesia-vibe/"
cp "$REPO_ROOT/include/synesthesia-vibe/synesthesia-vibe.h" "$DIST/include/synesthesia-vibe/"
if [[ -f "$REPO_ROOT/distribution/overlay/NOTICE" ]]; then
  cp "$REPO_ROOT/distribution/overlay/NOTICE" "$DIST/NOTICE"
fi
printf '%s\n' "$VERSION" > "$DIST/VERSION"

log "dist -> $DIST ($(find "$DIST" -type f | wc -l | tr -d ' ') files)"

if [[ "$ZIP" -eq 1 ]]; then
  rm -f "$ZIP_PATH"
  if command -v zip > /dev/null; then
    (cd "$DIST" && zip -q -X -r "$ZIP_PATH" .)
  else
    BSDTAR="$(cygpath -u "${SYSTEMROOT:-C:\\Windows}")/System32/tar.exe"
    [[ -x "$BSDTAR" ]] || fail "neither zip nor $BSDTAR is available"
    (cd "$DIST" && "$BSDTAR" -a -c -f "$(cygpath -w "$ZIP_PATH")" *)
  fi
  [[ -f "$ZIP_PATH" ]] || fail "zip was not written to $ZIP_PATH"
  log "zip -> $ZIP_PATH"
fi
