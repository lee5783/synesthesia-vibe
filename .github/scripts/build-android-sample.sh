#!/usr/bin/env bash
#
#  build-android-sample.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

MODE=""

print_help() {
  cat <<'EOF'
build-android-sample.sh -- builds the SynesthesiaVibe Android sample APK for a release

USAGE

    .github/scripts/build-android-sample.sh --release | --debug

  Runs from any working directory. Calls ./gradlew :app:assembleRelease or :app:assembleDebug in
  samples/android (which includes platform/android as a composite build, so the native library is
  compiled from source), then copies the APK to build/samples/ at the repo root:

    build/samples/synesthesiavibe-sample-android-<version>.apk        (--release)
    build/samples/synesthesiavibe-sample-android-<version>-debug.apk  (--debug)

PARAMETERS

  --release  Release build signed with the keystore described by ANDROID_KEYSTORE_FILE,
             ANDROID_KEYSTORE_PASSWORD, ANDROID_KEY_ALIAS and ANDROID_KEY_PASSWORD. All four
             must be set.
  --debug    Debug build signed with the local debug key.
  --help     This text.

NOTES

  The version is the root VERSION file when it exists, otherwise 0.0.1 (the same rule as
  platform/android/build-aar.sh). samples/android/local.properties must exist (sdk.dir), and
  platform/android/local.properties needs cmake.dir (a CMake 4.4.3 install). VCPKG_ROOT defaults
  to ~/vcpkg and ANDROID_HOME to ~/Library/Android/sdk when unset and present. The presets and
  textures come from resources/beatdrop.

EXIT CODE

  0   APK written to build/samples/.
  1   Missing input, Gradle failed or the APK is missing.
  2   Usage error.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      MODE="release"
      shift
      ;;
    --debug)
      MODE="debug"
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

if [[ -z "$MODE" ]]; then
  echo "Pass --release or --debug" >&2
  print_help
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SAMPLE_DIR="$REPO_ROOT/samples/android"
OUT_ROOT="$REPO_ROOT/build/samples"

ARTIFACT_ID="synesthesiavibe-sample-android"

if [[ -z "${VCPKG_ROOT:-}" && -d "$HOME/vcpkg" ]]; then
  export VCPKG_ROOT="$HOME/vcpkg"
fi
if [[ -z "${ANDROID_HOME:-}" && -d "$HOME/Library/Android/sdk" ]]; then
  export ANDROID_HOME="$HOME/Library/Android/sdk"
fi

VERSION="0.0.1"
if [[ -f "$REPO_ROOT/VERSION" ]]; then
  VERSION="$(tr -d '[:space:]' < "$REPO_ROOT/VERSION")"
fi

log() {
  printf '[build-android-sample] %s\n' "$*"
}

fail() {
  printf '[build-android-sample] ERROR: %s\n' "$*" >&2
  exit 1
}

[[ -f "$SAMPLE_DIR/local.properties" ]] || fail "missing $SAMPLE_DIR/local.properties"

if [[ "$MODE" == "release" ]]; then
  MISSING=()
  for name in ANDROID_KEYSTORE_FILE ANDROID_KEYSTORE_PASSWORD ANDROID_KEY_ALIAS ANDROID_KEY_PASSWORD; do
    [[ -n "${!name:-}" ]] || MISSING+=("$name")
  done
  [[ ${#MISSING[@]} -eq 0 ]] || fail "--release needs signing env vars; missing: ${MISSING[*]}"
  [[ -f "$ANDROID_KEYSTORE_FILE" ]] || fail "ANDROID_KEYSTORE_FILE does not point to a file"
  TASK=":app:assembleRelease"
  APK="$SAMPLE_DIR/app/build/outputs/apk/release/app-release.apk"
  OUT="$OUT_ROOT/$ARTIFACT_ID-$VERSION.apk"
else
  TASK=":app:assembleDebug"
  APK="$SAMPLE_DIR/app/build/outputs/apk/debug/app-debug.apk"
  OUT="$OUT_ROOT/$ARTIFACT_ID-$VERSION-debug.apk"
fi

log "version $VERSION"
log "gradle $TASK"
rm -f "$APK"
(cd "$SAMPLE_DIR" && ./gradlew "$TASK") || fail "gradle $TASK failed"

[[ -f "$APK" ]] || fail "missing $APK"

mkdir -p "$OUT_ROOT"
cp "$APK" "$OUT"
log "apk -> $OUT"
