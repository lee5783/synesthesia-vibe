#!/usr/bin/env bash
#
#  build-android-sample.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

MODE=""
PRESET_SET=""

print_help() {
  cat <<'EOF'
build-android-sample.sh -- builds the SynesthesiaVibe Android sample APK for a release

USAGE

    .github/scripts/build-android-sample.sh --release | --debug [--preset-set <dir>]

  Runs from any working directory. Calls ./gradlew :app:assembleRelease or :app:assembleDebug in
  samples/android (which includes platform/android as a composite build, so the native library is
  compiled from source), then copies the APK to build/samples/ at the repo root:

    build/samples/synesthesiavibe-sample-android-<version>.apk        (--release)
    build/samples/synesthesiavibe-sample-android-<version>-debug.apk  (--debug)

  With --preset-set <dir> the file name gains a set suffix, for example
  synesthesiavibe-sample-android-<version>-debug-favorite-presets-2013-12.apk.

PARAMETERS

  --release  Release build signed with the keystore described by ANDROID_KEYSTORE_FILE,
             ANDROID_KEYSTORE_PASSWORD, ANDROID_KEY_ALIAS and ANDROID_KEY_PASSWORD. All four
             must be set.
  --debug    Debug build signed with the local debug key.
  --preset-set <dir>
             Bundle the presets and textures of a preset set instead of resources/mvp-presets
             (opt-in; passed to Gradle as -PsvPresetSet). <dir> is absolute or relative to the
             repo root and holds presets/ (every .milk, subfolders flattened to
             "<subfolder> - <name>.milk") and optionally textures/, laid over
             resources/beatdrop/textures. Without it the bundled assets are unchanged.
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
    --preset-set)
      [[ $# -ge 2 ]] || { echo "--preset-set needs a directory" >&2; exit 2; }
      PRESET_SET="$2"
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

GRADLE_ARGS=("$TASK")
if [[ -n "$PRESET_SET" ]]; then
  [[ "$PRESET_SET" == /* ]] || PRESET_SET="$REPO_ROOT/$PRESET_SET"
  [[ -d "$PRESET_SET/presets" ]] || fail "--preset-set $PRESET_SET has no presets directory"
  PRESET_SET="$(cd "$PRESET_SET" && pwd)"
  SET_SUFFIX="$(basename "$PRESET_SET" | tr '[:upper:]_' '[:lower:]-' | tr -c 'a-z0-9\n-' '-' | sed 's/--*/-/g; s/^-//; s/-$//')"
  GRADLE_ARGS+=("-PsvPresetSet=$PRESET_SET")
  OUT="${OUT%.apk}-$SET_SUFFIX.apk"
  log "preset set $PRESET_SET"
fi

log "version $VERSION"
log "gradle ${GRADLE_ARGS[*]}"
rm -f "$APK"
(cd "$SAMPLE_DIR" && ./gradlew "${GRADLE_ARGS[@]}") || fail "gradle $TASK failed"

[[ -f "$APK" ]] || fail "missing $APK"

mkdir -p "$OUT_ROOT"
cp "$APK" "$OUT"
log "apk -> $OUT"
