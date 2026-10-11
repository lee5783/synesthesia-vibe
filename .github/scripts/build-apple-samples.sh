#!/usr/bin/env bash
#
#  build-apple-samples.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

ALL_SAMPLES=(ios-uikit ios-swiftui macos-appkit macos-swiftui)
SAMPLES=("${ALL_SAMPLES[@]}")

print_help() {
  cat <<'EOF'
build-apple-samples.sh -- builds the four Apple sample apps and zips each .app for a release

USAGE

    .github/scripts/build-apple-samples.sh [--samples <s>[,<s>...]]

  Runs from any working directory (macOS only). Needs
  build/xcframework/SynesthesiaVibeCore.xcframework (platform/ios/build-xcframework.sh or the
  release-xcframework artifact) and resources/beatdrop. Each sample is built in
  Release with xcodebuild from its committed .xcodeproj, into
  build/samples/apple/derived-data/<s>:

    ios-uikit, ios-swiftui        iphonesimulator, arm64 + x86_64, CODE_SIGNING_ALLOWED=NO
    macos-appkit, macos-swiftui   macOS universal (arm64 + x86_64), ad-hoc signed as configured

  Each .app is checked with lipo and zipped alone with ditto to
  build/samples/synesthesiavibe-sample-<s>-<version>.zip.

PARAMETERS

  --samples  Comma-separated subset of ios-uikit, ios-swiftui, macos-appkit, macos-swiftui.
  --help     This text.

NOTES

  The version is the root VERSION file when it exists, otherwise 0.0.1. xcodebuild runs from
  /tmp with PWD unset (cmake and xcodebuild can hang when PWD is the repo root). Build logs go
  to build/samples/apple/<s>.log.

EXIT CODE

  0   Every requested sample zip written.
  1   A prerequisite is missing, a build failed, or an app lacks an architecture.
  2   Usage error.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --samples)
      [[ $# -ge 2 ]] || { echo "Missing value for --samples" >&2; print_help; exit 2; }
      IFS=',' read -r -a SAMPLES <<< "$2"
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

for S in "${SAMPLES[@]}"; do
  case " ${ALL_SAMPLES[*]} " in
    *" $S "*) ;;
    *)
      echo "Unknown sample: $S (expected one of: ${ALL_SAMPLES[*]})" >&2
      exit 2
      ;;
  esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "build-apple-samples.sh runs on macOS only." >&2
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUT_ROOT="$REPO_ROOT/build/samples"
WORK_ROOT="$OUT_ROOT/apple"
XCFRAMEWORK="$REPO_ROOT/build/xcframework/SynesthesiaVibeCore.xcframework"
BEATDROP_RESOURCES="$REPO_ROOT/resources/beatdrop"

VERSION="0.0.1"
if [[ -f "$REPO_ROOT/VERSION" ]]; then
  VERSION="$(tr -d '[:space:]' < "$REPO_ROOT/VERSION")"
fi

log() {
  printf '[build-apple-samples] %s\n' "$*"
}

fail() {
  printf '[build-apple-samples] ERROR: %s\n' "$*" >&2
  exit 1
}

[[ -d "$XCFRAMEWORK" ]] || fail "missing $XCFRAMEWORK; run platform/ios/build-xcframework.sh or unzip the release-xcframework artifact into build/xcframework/"
[[ -d "$BEATDROP_RESOURCES/capture-presets" && -d "$BEATDROP_RESOURCES/textures" && -f "$BEATDROP_RESOURCES/capture-set.json" ]] \
  || fail "missing $BEATDROP_RESOURCES (capture-presets/, textures/ and capture-set.json); restore it from the repository"

build_sample() {
  local ID="$1"
  local PROJECT SCHEME PLATFORM PRODUCTS_DIR BINARY_SUBPATH
  case "$ID" in
    ios-uikit)
      PROJECT="samples/ios/uikit/SynesthesiaVibeUIKit.xcodeproj"
      SCHEME="SynesthesiaVibeUIKit"
      PLATFORM=ios
      ;;
    ios-swiftui)
      PROJECT="samples/ios/swiftui/SynesthesiaVibeSwiftUI.xcodeproj"
      SCHEME="SynesthesiaVibeSwiftUI"
      PLATFORM=ios
      ;;
    macos-appkit)
      PROJECT="samples/macos/appkit/SynesthesiaVibeAppKit.xcodeproj"
      SCHEME="SynesthesiaVibeAppKit"
      PLATFORM=macos
      ;;
    macos-swiftui)
      PROJECT="samples/macos/swiftui/SynesthesiaVibeMacSwiftUI.xcodeproj"
      SCHEME="SynesthesiaVibeMacSwiftUI"
      PLATFORM=macos
      ;;
  esac

  local DERIVED="$WORK_ROOT/derived-data/$ID"
  local LOG="$WORK_ROOT/$ID.log"
  local ARGS=(-project "$REPO_ROOT/$PROJECT" -scheme "$SCHEME" -configuration Release
    -derivedDataPath "$DERIVED" ARCHS="arm64 x86_64" ONLY_ACTIVE_ARCH=NO)
  if [[ "$PLATFORM" == ios ]]; then
    ARGS+=(-sdk iphonesimulator -destination "generic/platform=iOS Simulator" CODE_SIGNING_ALLOWED=NO)
    PRODUCTS_DIR="$DERIVED/Build/Products/Release-iphonesimulator"
    BINARY_SUBPATH="$SCHEME"
  else
    ARGS+=(-destination "generic/platform=macOS")
    PRODUCTS_DIR="$DERIVED/Build/Products/Release"
    BINARY_SUBPATH="Contents/MacOS/$SCHEME"
  fi

  log "$ID: xcodebuild $SCHEME (Release, $PLATFORM) -> $DERIVED"
  rm -rf "$DERIVED"
  if ! (cd /tmp && env -u PWD xcodebuild "${ARGS[@]}" build) > "$LOG" 2>&1; then
    tail -n 40 "$LOG" >&2 || true
    fail "$ID: xcodebuild failed, see $LOG"
  fi

  local APP="$PRODUCTS_DIR/$SCHEME.app"
  [[ -d "$APP" ]] || fail "$ID: missing $APP"
  local BINARY="$APP/$BINARY_SUBPATH"
  [[ -f "$BINARY" ]] || fail "$ID: missing $BINARY"
  local ARCHS_FOUND
  ARCHS_FOUND="$(xcrun lipo -archs "$BINARY" | tr ' ' '\n' | sort | xargs)"
  [[ "$ARCHS_FOUND" == "arm64 x86_64" ]] || fail "$ID: $BINARY has '$ARCHS_FOUND', expected 'arm64 x86_64'"
  log "$ID: $APP ($ARCHS_FOUND)"

  local ZIP="$OUT_ROOT/synesthesiavibe-sample-$ID-$VERSION.zip"
  rm -f "$ZIP"
  (cd "$PRODUCTS_DIR" && ditto -c -k --norsrc --noextattr --keepParent "$SCHEME.app" "$ZIP")
  log "$ID: zip -> $ZIP"
}

mkdir -p "$WORK_ROOT/derived-data"
log "version $VERSION"
for S in "${SAMPLES[@]}"; do
  build_sample "$S"
done
