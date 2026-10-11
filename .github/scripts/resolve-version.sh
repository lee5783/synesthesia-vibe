#!/usr/bin/env bash
#
#  resolve-version.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

REF="${GITHUB_REF:-}"
FROM_VERSION_FILE=0
VERSION_FILE=""

print_help() {
  cat <<'EOF'
resolve-version.sh -- resolves the release version from a git tag and checks it against VERSION

USAGE

    .github/scripts/resolve-version.sh [--ref <ref-or-tag>] [--from-version-file] [--version-file <path>]

  Runs from any working directory. Takes a tag ref (refs/tags/v1.2.3) or a tag name (v1.2.3),
  strips refs/tags/ and the leading v, validates the rest as SemVer 2.0
  (MAJOR.MINOR.PATCH[-prerelease][+build]) and requires it to equal the trimmed content of the
  repo-root VERSION file exactly, prerelease and build parts included.

PARAMETERS

  --ref <ref>           Tag ref or tag name. Default: $GITHUB_REF.
  --from-version-file   Dry run: take the version from VERSION instead of a tag, still validate it
                        as SemVer, and report the tag as v<version>. --ref is ignored.
  --version-file <path> Read this file instead of the repo-root VERSION.
  --help                This text.

OUTPUT

  Prints version, tag, short-version (MAJOR.MINOR.PATCH) and is-prerelease (true/false) as
  key=value lines on stdout, and appends the same lines to $GITHUB_OUTPUT when it is set.

EXIT CODE

  0   Version resolved.
  1   Not a tag ref, not SemVer, VERSION missing, or tag and VERSION disagree.
  2   Usage error.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ref)
      [[ $# -ge 2 ]] || { echo "Missing value for --ref" >&2; print_help; exit 2; }
      REF="$2"
      shift 2
      ;;
    --from-version-file)
      FROM_VERSION_FILE=1
      shift
      ;;
    --version-file)
      [[ $# -ge 2 ]] || { echo "Missing value for --version-file" >&2; print_help; exit 2; }
      VERSION_FILE="$2"
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
if [[ -z "$VERSION_FILE" ]]; then
  VERSION_FILE="$REPO_ROOT/VERSION"
fi

NUM='(0|[1-9][0-9]*)'
PRE_ID='(0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)'
BUILD_ID='[0-9A-Za-z-]+'
SEMVER_RE="^${NUM}\.${NUM}\.${NUM}(-${PRE_ID}(\.${PRE_ID})*)?(\+${BUILD_ID}(\.${BUILD_ID})*)?$"

log() {
  printf '[resolve-version] %s\n' "$*" >&2
}

fail() {
  printf '[resolve-version] ERROR: %s\n' "$*" >&2
  exit 1
}

[[ -f "$VERSION_FILE" ]] || fail "missing version file $VERSION_FILE"
FILE_VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"
[[ -n "$FILE_VERSION" ]] || fail "version file $VERSION_FILE is empty"

if [[ "$FROM_VERSION_FILE" -eq 1 ]]; then
  log "dry run: version from $VERSION_FILE"
  VERSION="$FILE_VERSION"
  [[ "$VERSION" =~ $SEMVER_RE ]] || fail "VERSION '$VERSION' is not SemVer 2.0 (MAJOR.MINOR.PATCH[-prerelease][+build])"
  TAG="v$VERSION"
else
  [[ -n "$REF" ]] || fail "no ref given (--ref or GITHUB_REF)"
  case "$REF" in
    refs/tags/*)
      TAG="${REF#refs/tags/}"
      ;;
    refs/*)
      fail "ref '$REF' is not a tag ref (expected refs/tags/v<semver>)"
      ;;
    *)
      TAG="$REF"
      ;;
  esac
  [[ "$TAG" == v* ]] || fail "tag '$TAG' does not start with 'v' (expected v<semver>)"
  VERSION="${TAG#v}"
  [[ "$VERSION" =~ $SEMVER_RE ]] || fail "tag '$TAG' is not v + SemVer 2.0 (vMAJOR.MINOR.PATCH[-prerelease][+build])"
  [[ "$VERSION" == "$FILE_VERSION" ]] || fail "tag version '$VERSION' does not match VERSION file '$FILE_VERSION' ($VERSION_FILE)"
fi

SHORT_VERSION="${VERSION%%+*}"
SHORT_VERSION="${SHORT_VERSION%%-*}"
IS_PRERELEASE=false
if [[ "${VERSION%%+*}" == *-* ]]; then
  IS_PRERELEASE=true
fi

log "version $VERSION (tag $TAG, prerelease $IS_PRERELEASE)"

OUTPUTS="version=$VERSION
tag=$TAG
short-version=$SHORT_VERSION
is-prerelease=$IS_PRERELEASE"

printf '%s\n' "$OUTPUTS"
if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
  printf '%s\n' "$OUTPUTS" >> "$GITHUB_OUTPUT"
fi
