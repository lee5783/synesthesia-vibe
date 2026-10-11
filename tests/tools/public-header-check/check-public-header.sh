#!/usr/bin/env bash
#
#  check-public-header.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-10.
#

set -euo pipefail

PUBLIC_HEADER_DIR="include/synesthesia-vibe"
OUT_DIR=""

print_help() {
  cat <<'EOF'
check-public-header.sh -- P2-015 public-header hygiene checker (Mac/Linux twin)

USAGE

  Static source-text scan (no build required) of synesthesia-vibe's public header
  directory (include/synesthesia-vibe/) that verifies it never leaks a platform/GL
  header into its own text - a consumer building against include/synesthesia-vibe/*.h
  alone (no beatdrop-core, no render/gl, no platform TU) must never be forced
  to also have the Windows SDK, an OpenGL loader or the Apple SDK on its
  include path. Mirrors check-public-header.ps1 argument for argument and hit
  for hit.

    ./check-public-header.sh --out <dir> [--dir <path>]

PARAMETERS

  --out                 Directory this run writes check-report.md into.
                        Required.
  --dir                 Directory scanned, repo-root relative unless absolute.
                        Default: include/synesthesia-vibe. --public-header-dir is
                        accepted as an alias.

WHAT COUNTS AS A HIT

  A case-insensitive #include or #import of any of:
    - <GL/gl.h> (or any GL/*.h - the OpenGL loader/header family)
    - <GLES2/...>, <GLES3/...>, <OpenGLES/...>, <EGL/...> (OpenGL ES / EGL)
    - <Metal/...>, <MetalKit/...>
    - <jni.h>, <android/...>
    - <glad/...>
    - <windows.h>
    - <TargetConditionals.h>

  found anywhere under the scanned directory (recursively, *.h/*.hpp).

  Text after // on a line is ignored; mentions found only there are listed in
  the report as informational. /* */ block comments are not stripped.

EXIT CODE

  0   Zero forbidden-header hits under the scanned directory (including the
      trivial "directory does not exist" case).
  1   One or more forbidden-header hits found.
  2   usage error.
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out)
      OUT_DIR="$2"
      shift 2
      ;;
    --dir|--public-header-dir)
      PUBLIC_HEADER_DIR="$2"
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

if [[ -z "$OUT_DIR" ]]; then
  echo "--out is required. See --help." >&2
  exit 2
fi

if [[ "$PUBLIC_HEADER_DIR" != /* ]]; then
  PUBLIC_HEADER_DIR="$REPO_ROOT/$PUBLIC_HEADER_DIR"
fi
if [[ "$OUT_DIR" != /* ]]; then
  OUT_DIR="$REPO_ROOT/$OUT_DIR"
fi
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

# Case-insensitive matching so <Windows.h>, <GL/GL.h> etc. are caught like
# the .ps1 twin and the sibling check-portable-only.sh (grep -E -i).
shopt -s nocasematch

FORBIDDEN_INCLUDE_RE='#[[:space:]]*(include|import)[[:space:]]*[<"](GL/[^">]+\.h|(GLES2|GLES3|OpenGLES|EGL|Metal|MetalKit|android|glad)/[^">]+|jni\.h|windows\.h|TargetConditionals\.h)[>"]'

HITS_FILE="$OUT_DIR/public-header-hits.txt"
MENTIONS_FILE="$OUT_DIR/public-header-comment-mentions.txt"
: > "$HITS_FILE"
: > "$MENTIONS_FILE"

HEADER_COUNT=0

if [[ -d "$PUBLIC_HEADER_DIR" ]]; then
  while IFS= read -r -d '' HEADER; do
    HEADER_COUNT=$((HEADER_COUNT + 1))
    REL="${HEADER#"$REPO_ROOT"/}"
    LINE_NUMBER=0
    while IFS= read -r LINE || [[ -n "$LINE" ]]; do
      LINE_NUMBER=$((LINE_NUMBER + 1))
      CODE="${LINE%%//*}"
      if [[ "$CODE" =~ $FORBIDDEN_INCLUDE_RE ]]; then
        printf '%s:%d:%s\n' "$REL" "$LINE_NUMBER" "$LINE" >> "$HITS_FILE"
      elif [[ "$LINE" =~ $FORBIDDEN_INCLUDE_RE ]]; then
        printf '%s:%d:%s\n' "$REL" "$LINE_NUMBER" "$LINE" >> "$MENTIONS_FILE"
      fi
    done < "$HEADER"
  done < <(find "$PUBLIC_HEADER_DIR" -type f \( -name '*.h' -o -name '*.hpp' \) -print0 | sort -z)
fi

HIT_COUNT="$(grep -c . "$HITS_FILE" || true)"
MENTION_COUNT="$(grep -c . "$MENTIONS_FILE" || true)"

REPORT_PATH="$OUT_DIR/check-report.md"
DISPLAY_DIR="${PUBLIC_HEADER_DIR#"$REPO_ROOT"/}"
{
  echo "# P2-015 public-header hygiene check"
  echo
  if [[ "$HEADER_COUNT" -eq 0 ]]; then
    echo "\`$DISPLAY_DIR/\` does not exist yet (created by P2-027) - trivial PASS, 0 files checked."
  else
    echo "Static source scan (no build). Files checked under \`$DISPLAY_DIR/\`: $HEADER_COUNT."
  fi
  echo
  if [[ "$HIT_COUNT" -eq 0 ]]; then
    echo "Overall result: **PASS**"
  else
    echo "Overall result: **FAIL**"
  fi
  echo
  echo "Forbidden platform/GL header hits: $HIT_COUNT"
  if [[ "$HIT_COUNT" -gt 0 ]]; then
    echo
    while IFS= read -r LINE; do
      echo "  - \`$LINE\`"
    done < "$HITS_FILE"
  fi
  echo
  echo "Forbidden-header mentions inside // comments only (informational, not a violation): $MENTION_COUNT"
  if [[ "$MENTION_COUNT" -gt 0 ]]; then
    echo
    while IFS= read -r LINE; do
      echo "  - \`$LINE\`"
    done < "$MENTIONS_FILE"
  fi
} > "$REPORT_PATH"

echo "Public-header forbidden hits: $HIT_COUNT"
echo "Report written to $REPORT_PATH"

if [[ "$HIT_COUNT" -gt 0 ]]; then
  exit 1
fi
exit 0