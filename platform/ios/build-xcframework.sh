#!/usr/bin/env bash
#
#  build-xcframework.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

ALL_SLICES=(ios-arm64 ios-sim-arm64 ios-sim-x64 macos-arm64 macos-x64)
OPTIONAL_SLICES=(ios-sim-x64 macos-x64)
SLICES=("${ALL_SLICES[@]}")
SKIP_CONFIGURE=0

FRAMEWORK_NAME="SynesthesiaVibeCore"
BUNDLE_ID="com.lee5783.synesthesiavibe.core"
IOS_MIN="13.0"
MACOS_MIN="12.0"

print_help() {
  cat <<'EOF'
build-xcframework.sh -- builds build/xcframework/SynesthesiaVibeCore.xcframework (Darwin only)

USAGE

    platform/ios/build-xcframework.sh [--slices <p>[,<p>...]] [--skip-configure]

  Runs from any working directory. For every slice preset (ios-arm64, ios-sim-arm64,
  ios-sim-x64, macos-arm64, macos-x64) it configures CMake into
  build/xcframework/slices/<p>/build, builds SynesthesiaVibeCore and installs it into
  build/xcframework/slices/<p>/install. The two simulator archives and the two macOS archives
  are merged with lipo, one static SynesthesiaVibeCore.framework is assembled per platform
  (iOS device, iOS simulator, macOS) under build/xcframework/frameworks/, and
  xcodebuild -create-xcframework writes build/xcframework/SynesthesiaVibeCore.xcframework.
  The xcframework is zipped to build/xcframework/SynesthesiaVibeCore.xcframework.zip and its
  swift package compute-checksum is printed.

PARAMETERS

  --slices          Comma-separated subset of the slice presets, e.g.
                    --slices ios-sim-arm64,macos-arm64 for a quick dev build.
  --skip-configure  Reuse the existing build/xcframework/slices/<p>/build trees.
  --help            This text.

NOTES

  ios-sim-x64 and macos-x64 are skipped with a warning when they cannot be configured or
  built (e.g. vcpkg lacks the x64 triplet install); the release build runs all five.
  Every cmake call runs from /tmp with PWD unset (cmake can hang on macOS when PWD is
  the repository root). The version printed is the root VERSION file when it
  exists, otherwise 0.0.1.

EXIT CODE

  0   XCFramework written.
  1   A required slice failed, or no slice was built.
  2   Usage error.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --slices)
      IFS=',' read -r -a SLICES <<< "$2"
      shift 2
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

for S in "${SLICES[@]}"; do
  case " ${ALL_SLICES[*]} " in
    *" $S "*) ;;
    *)
      echo "Unknown slice: $S (expected one of: ${ALL_SLICES[*]})" >&2
      exit 2
      ;;
  esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "build-xcframework.sh runs on macOS only." >&2
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUT_ROOT="$REPO_ROOT/build/xcframework"
SLICES_ROOT="$OUT_ROOT/slices"
LIPO_ROOT="$OUT_ROOT/lipo"
FRAMEWORKS_ROOT="$OUT_ROOT/frameworks"
XCFRAMEWORK="$OUT_ROOT/$FRAMEWORK_NAME.xcframework"
ZIP_PATH="$OUT_ROOT/$FRAMEWORK_NAME.xcframework.zip"

if [[ -z "${VCPKG_ROOT:-}" && -d "$HOME/vcpkg" ]]; then
  export VCPKG_ROOT="$HOME/vcpkg"
fi

VERSION="0.0.1"
if [[ -f "$REPO_ROOT/VERSION" ]]; then
  VERSION="$(tr -d '[:space:]' < "$REPO_ROOT/VERSION")"
fi
SHORT_VERSION="${VERSION%%-*}"

log() {
  printf '[build-xcframework] %s\n' "$*"
}

warn() {
  printf '[build-xcframework] WARNING: %s\n' "$*" >&2
}

run_cmake() {
  (cd /tmp && env -u PWD cmake "$@")
}

is_optional() {
  case " ${OPTIONAL_SLICES[*]} " in
    *" $1 "*) return 0 ;;
  esac
  return 1
}

build_slice() {
  local P="$1"
  local B="$SLICES_ROOT/$P/build"
  local I="$SLICES_ROOT/$P/install"
  local L="$SLICES_ROOT/$P/build-slice.log"
  mkdir -p "$SLICES_ROOT/$P"
  : > "$L"
  if [[ "$SKIP_CONFIGURE" -eq 0 ]]; then
    log "$P: configure -> $(realpath "$B" 2>/dev/null || echo "$B")"
    if ! run_cmake -S "$REPO_ROOT" --preset "$P" -B "$B" \
        -DCMAKE_INSTALL_PREFIX="$I" \
        -DSYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS=OFF >> "$L" 2>&1; then
      return 1
    fi
  elif [[ ! -f "$B/CMakeCache.txt" ]]; then
    echo "--skip-configure: $B has no CMakeCache.txt" >> "$L"
    return 1
  fi
  log "$P: build SynesthesiaVibeCore + install -> $I"
  rm -rf "$I"
  if ! run_cmake --build "$B" --target SynesthesiaVibeCore --target install >> "$L" 2>&1; then
    return 1
  fi
  [[ -f "$I/lib/lib$FRAMEWORK_NAME.a" ]]
}

BUILT=()
for P in "${SLICES[@]}"; do
  if build_slice "$P"; then
    BUILT+=("$P")
    log "$P: OK ($(xcrun lipo -archs "$SLICES_ROOT/$P/install/lib/lib$FRAMEWORK_NAME.a"))"
  elif is_optional "$P"; then
    warn "$P: skipped (configure/build failed, see $SLICES_ROOT/$P/build-slice.log)"
  else
    echo "[build-xcframework] ERROR: $P failed, see $SLICES_ROOT/$P/build-slice.log" >&2
    tail -n 20 "$SLICES_ROOT/$P/build-slice.log" >&2 || true
    exit 1
  fi
done

if [[ ${#BUILT[@]} -eq 0 ]]; then
  echo "[build-xcframework] ERROR: no slice was built." >&2
  exit 1
fi

has_built() {
  case " ${BUILT[*]} " in
    *" $1 "*) return 0 ;;
  esac
  return 1
}

write_info_plist() {
  local PLIST="$1"
  local PLATFORM="$2"
  local MIN_KEY="$3"
  local MIN_VALUE="$4"
  cat > "$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleExecutable</key>
	<string>$FRAMEWORK_NAME</string>
	<key>CFBundleIdentifier</key>
	<string>$BUNDLE_ID</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>$FRAMEWORK_NAME</string>
	<key>CFBundlePackageType</key>
	<string>FMWK</string>
	<key>CFBundleShortVersionString</key>
	<string>$SHORT_VERSION</string>
	<key>CFBundleVersion</key>
	<string>$VERSION</string>
	<key>CFBundleSupportedPlatforms</key>
	<array>
		<string>$PLATFORM</string>
	</array>
	<key>$MIN_KEY</key>
	<string>$MIN_VALUE</string>
</dict>
</plist>
EOF
  plutil -lint "$PLIST" > /dev/null
}

merge_archives() {
  local NAME="$1"
  shift
  local OUT="$LIPO_ROOT/$NAME/lib$FRAMEWORK_NAME.a"
  local INPUTS=()
  for P in "$@"; do
    if has_built "$P"; then
      INPUTS+=("$SLICES_ROOT/$P/install/lib/lib$FRAMEWORK_NAME.a")
    fi
  done
  [[ ${#INPUTS[@]} -gt 0 ]] || return 1
  mkdir -p "$LIPO_ROOT/$NAME"
  rm -f "$OUT"
  xcrun lipo -create "${INPUTS[@]}" -output "$OUT"
  echo "$OUT"
}

copy_headers() {
  local PREFIX="$1"
  local DEST="$2"
  mkdir -p "$DEST"
  cp "$PREFIX"/include/synesthesia-vibe/*.h "$DEST/"
  cp "$PREFIX"/include/$FRAMEWORK_NAME/*.h "$DEST/"
}

assemble_shallow_framework() {
  local NAME="$1"
  local ARCHIVE="$2"
  local PREFIX="$3"
  local PLATFORM="$4"
  local FW="$FRAMEWORKS_ROOT/$NAME/$FRAMEWORK_NAME.framework"
  rm -rf "$FW"
  mkdir -p "$FW/Modules"
  copy_headers "$PREFIX" "$FW/Headers"
  cp "$PREFIX/include/$FRAMEWORK_NAME/module.modulemap" "$FW/Modules/module.modulemap"
  xcrun libtool -static -no_warning_for_no_symbols -o "$FW/$FRAMEWORK_NAME" "$ARCHIVE"
  write_info_plist "$FW/Info.plist" "$PLATFORM" MinimumOSVersion "$IOS_MIN"
  echo "$FW"
}

assemble_macos_framework() {
  local ARCHIVE="$1"
  local PREFIX="$2"
  local FW="$FRAMEWORKS_ROOT/macos/$FRAMEWORK_NAME.framework"
  local V="$FW/Versions/A"
  rm -rf "$FW"
  mkdir -p "$V/Modules" "$V/Resources"
  copy_headers "$PREFIX" "$V/Headers"
  cp "$PREFIX/include/$FRAMEWORK_NAME/module.modulemap" "$V/Modules/module.modulemap"
  xcrun libtool -static -no_warning_for_no_symbols -o "$V/$FRAMEWORK_NAME" "$ARCHIVE"
  write_info_plist "$V/Resources/Info.plist" MacOSX LSMinimumSystemVersion "$MACOS_MIN"
  ln -s A "$FW/Versions/Current"
  ln -s "Versions/Current/$FRAMEWORK_NAME" "$FW/$FRAMEWORK_NAME"
  ln -s Versions/Current/Headers "$FW/Headers"
  ln -s Versions/Current/Modules "$FW/Modules"
  ln -s Versions/Current/Resources "$FW/Resources"
  echo "$FW"
}

first_built_of() {
  for P in "$@"; do
    if has_built "$P"; then
      echo "$SLICES_ROOT/$P/install"
      return 0
    fi
  done
  return 1
}

XC_ARGS=()

if has_built ios-arm64; then
  A="$(merge_archives ios ios-arm64)"
  FW="$(assemble_shallow_framework ios "$A" "$SLICES_ROOT/ios-arm64/install" iPhoneOS)"
  log "framework ios: $FW ($(xcrun lipo -archs "$FW/$FRAMEWORK_NAME"))"
  XC_ARGS+=(-framework "$FW")
fi

if has_built ios-sim-arm64 || has_built ios-sim-x64; then
  A="$(merge_archives ios-simulator ios-sim-arm64 ios-sim-x64)"
  FW="$(assemble_shallow_framework ios-simulator "$A" "$(first_built_of ios-sim-arm64 ios-sim-x64)" iPhoneSimulator)"
  log "framework ios-simulator: $FW ($(xcrun lipo -archs "$FW/$FRAMEWORK_NAME"))"
  XC_ARGS+=(-framework "$FW")
fi

if has_built macos-arm64 || has_built macos-x64; then
  A="$(merge_archives macos macos-arm64 macos-x64)"
  FW="$(assemble_macos_framework "$A" "$(first_built_of macos-arm64 macos-x64)")"
  log "framework macos: $FW ($(xcrun lipo -archs "$FW/$FRAMEWORK_NAME"))"
  XC_ARGS+=(-framework "$FW")
fi

rm -rf "$XCFRAMEWORK"
(cd /tmp && xcodebuild -create-xcframework "${XC_ARGS[@]}" -output "$XCFRAMEWORK")

rm -f "$ZIP_PATH"
(cd "$OUT_ROOT" && ditto -c -k --sequesterRsrc --keepParent "$FRAMEWORK_NAME.xcframework" "$ZIP_PATH")
CHECKSUM="$(cd /tmp && swift package compute-checksum "$ZIP_PATH")"

log "version: $VERSION"
log "slices built: ${BUILT[*]}"
for D in "$XCFRAMEWORK"/*/; do
  log "xcframework library: $(basename "$D")"
done
log "xcframework: $XCFRAMEWORK"
log "zip: $ZIP_PATH"
log "checksum: $CHECKSUM"
