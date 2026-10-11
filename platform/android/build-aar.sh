#!/usr/bin/env bash
#
#  build-aar.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

SKIP_PUBLISH=0

print_help() {
  cat <<'EOF'
build-aar.sh -- builds the SynesthesiaVibe Android AAR and publishes it to mavenLocal

USAGE

    platform/android/build-aar.sh [--skip-publish]

  Runs from any working directory. Calls ./gradlew assembleRelease publishToMavenLocal in
  platform/android (the library is the Gradle root project), which installs
  com.synesthesiavibe:synesthesiavibe:<version> into ~/.m2/repository, then copies the release
  AAR and the generated POM to build/android/ at the repo root.

PARAMETERS

  --skip-publish  Only assembleRelease and copy the AAR; no POM, nothing written to ~/.m2.
  --help          This text.

NOTES

  The version is the root VERSION file when it exists, otherwise 0.0.1 (the same rule as
  build.gradle.kts and platform/ios/build-xcframework.sh). VCPKG_ROOT defaults to ~/vcpkg and
  ANDROID_HOME to ~/Library/Android/sdk when unset and present; platform/android/local.properties
  needs sdk.dir and cmake.dir (a CMake 4.4.3 install).

EXIT CODE

  0   AAR (and POM) written to build/android/.
  1   Gradle failed or an expected output is missing.
  2   Usage error.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-publish)
      SKIP_PUBLISH=1
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
OUT_ROOT="$REPO_ROOT/build/android"

GROUP_ID="com.synesthesiavibe"
ARTIFACT_ID="synesthesiavibe"

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
  printf '[build-aar] %s\n' "$*"
}

fail() {
  printf '[build-aar] ERROR: %s\n' "$*" >&2
  exit 1
}

TASKS=(assembleRelease)
if [[ "$SKIP_PUBLISH" -eq 0 ]]; then
  TASKS+=(publishToMavenLocal)
fi

log "version $VERSION"
log "gradle ${TASKS[*]}"
(cd "$SCRIPT_DIR" && ./gradlew "${TASKS[@]}") || fail "gradle ${TASKS[*]} failed"

AAR="$SCRIPT_DIR/build/outputs/aar/$ARTIFACT_ID-release.aar"
[[ -f "$AAR" ]] || fail "missing $AAR"

mkdir -p "$OUT_ROOT"
cp "$AAR" "$OUT_ROOT/$ARTIFACT_ID-$VERSION.aar"
log "aar -> $OUT_ROOT/$ARTIFACT_ID-$VERSION.aar"

if [[ "$SKIP_PUBLISH" -eq 0 ]]; then
  POM="$SCRIPT_DIR/build/publications/release/pom-default.xml"
  [[ -f "$POM" ]] || fail "missing $POM"
  cp "$POM" "$OUT_ROOT/$ARTIFACT_ID-$VERSION.pom"
  log "pom -> $OUT_ROOT/$ARTIFACT_ID-$VERSION.pom"
  M2_DIR="$HOME/.m2/repository/${GROUP_ID//.//}/$ARTIFACT_ID/$VERSION"
  [[ -d "$M2_DIR" ]] || fail "missing $M2_DIR"
  log "mavenLocal -> $M2_DIR"
fi
