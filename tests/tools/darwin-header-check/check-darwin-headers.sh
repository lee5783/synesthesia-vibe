#!/usr/bin/env bash
#
#  check-darwin-headers.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

OUT_DIR=""
SCAN_DIRS=()
SKIP_COMPILE=0
IOS_MIN="13.0"
MACOS_MIN="12.0"

print_help() {
  cat <<'EOF'
check-darwin-headers.sh -- pure-Objective-C public header checker for the Apple layer
(Darwin only; no .ps1 twin)

USAGE

  Verifies that the Objective-C public headers of the Apple layer never expose C++ to a
  consumer: a Swift or plain Objective-C app importing SynesthesiaVibeCore must compile them as
  pure Objective-C (not Objective-C++). Two passes per header:

    1. Static text scan for C++ markers.
    2. Compile: xcrun clang -x objective-c -fsyntax-only -fobjc-arc, once for the iOS simulator
       (-mios-simulator-version-min=13.0) and once for macOS (-mmacosx-version-min=12.0).
       The compile is the real test; the scan names the likely cause.

    ./check-darwin-headers.sh --out <dir> [--dir <path>]... [--skip-compile]

PARAMETERS

  --out            Directory this run writes check-report.md and compile logs into. Required.
  --dir            Directory scanned (non-recursive, *.h), repo-root relative unless absolute.
                   Repeatable. Default: platform/darwin/include, platform/ios/include,
                   platform/macos/include. With the default set, platform/ios/include compiles
                   only for iOS and platform/macos/include only for macOS; every --dir given
                   explicitly compiles for both. Pass a framework's Headers/ directory to check
                   an assembled framework.
  --skip-compile   Run the static scan only.

WHAT COUNTS AS A VIOLATION

  Static scan (text after // ignored):
    - #include/#import of a header without a .h extension (<string>, <vector>, ...) or of a
      .hpp/.hh/.hxx header;
    - namespace, template <, std::, ::, extern "C++";
    - class followed by whitespace that is not @class.
  Allowed: #import <TargetConditionals.h>, <UIKit/UIKit.h>, <AppKit/AppKit.h>, any other
  system .h, @import, @class, @interface, @protocol.

  Compile: a non-zero clang exit for either target.

  Each scanned directory (and a SynesthesiaVibeCore/ alias of it, so framework-style
  <SynesthesiaVibeCore/X.h> imports resolve) plus include/synesthesia-vibe is on the include
  path.

EXIT CODE

  0   Zero violations (including the trivial "no headers / directories missing" case).
  1   One or more violations.
  2   Usage error, or headers exist but xcrun clang is unavailable.
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
    --dir)
      SCAN_DIRS+=("$2")
      shift 2
      ;;
    --skip-compile)
      SKIP_COMPILE=1
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

if [[ -z "$OUT_DIR" ]]; then
  echo "--out is required. See --help." >&2
  exit 2
fi

DEFAULT_SET=0
if [[ ${#SCAN_DIRS[@]} -eq 0 ]]; then
  DEFAULT_SET=1
  SCAN_DIRS=("platform/darwin/include" "platform/ios/include" "platform/macos/include")
fi

if [[ "$OUT_DIR" != /* ]]; then
  OUT_DIR="$REPO_ROOT/$OUT_DIR"
fi
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

ABS_DIRS=()
for D in "${SCAN_DIRS[@]}"; do
  if [[ "$D" != /* ]]; then
    D="$REPO_ROOT/$D"
  fi
  ABS_DIRS+=("${D%/}")
done

display_path() {
  local P="$1"
  if [[ "$P" == "$REPO_ROOT"/* ]]; then
    echo "${P#"$REPO_ROOT"/}"
  else
    echo "$P"
  fi
}

INCLUDE_RE='^[[:space:]]*#[[:space:]]*(include|import)[[:space:]]*[<"]([^">]+)[>"]'
NAMESPACE_RE='(^|[^[:alnum:]_])namespace([^[:alnum:]_]|$)'
TEMPLATE_RE='(^|[^[:alnum:]_])template[[:space:]]*<'
EXTERN_CXX_RE='extern[[:space:]]*"C\+\+"'
CLASS_RE='(^|[^@[:alnum:]_])class[[:space:]]'

HITS_FILE="$OUT_DIR/darwin-header-hits.txt"
COMPILE_FILE="$OUT_DIR/darwin-header-compile.txt"
: > "$HITS_FILE"
: > "$COMPILE_FILE"
mkdir -p "$OUT_DIR/compile-logs"

add_hit() {
  printf '%s:%d: %s: %s\n' "$1" "$2" "$3" "$4" >> "$HITS_FILE"
}

HEADERS=()
HEADER_TARGETS=()
DIR_SUMMARY=()
for I in "${!ABS_DIRS[@]}"; do
  D="${ABS_DIRS[$I]}"
  TARGETS="ios macos"
  if [[ "$DEFAULT_SET" -eq 1 ]]; then
    case "$(display_path "$D")" in
      platform/ios/include) TARGETS="ios" ;;
      platform/macos/include) TARGETS="macos" ;;
    esac
  fi
  if [[ ! -d "$D" ]]; then
    DIR_SUMMARY+=("\`$(display_path "$D")\`: missing (0 headers)")
    continue
  fi
  COUNT=0
  while IFS= read -r -d '' H; do
    HEADERS+=("$H")
    HEADER_TARGETS+=("$TARGETS")
    COUNT=$((COUNT + 1))
  done < <(find "$D" -maxdepth 1 -type f -name '*.h' -print0 | sort -z)
  DIR_SUMMARY+=("\`$(display_path "$D")\`: $COUNT headers (compiled for: $TARGETS)")
done

HEADER_COUNT=${#HEADERS[@]}

for H in ${HEADERS[@]+"${HEADERS[@]}"}; do
  REL="$(display_path "$H")"
  LINE_NUMBER=0
  while IFS= read -r LINE || [[ -n "$LINE" ]]; do
    LINE_NUMBER=$((LINE_NUMBER + 1))
    CODE="${LINE%%//*}"
    if [[ "$CODE" =~ $INCLUDE_RE ]]; then
      NAME="${BASH_REMATCH[2]}"
      case "$NAME" in
        *.hpp|*.hh|*.hxx) add_hit "$REL" "$LINE_NUMBER" "C++ header" "$LINE" ;;
        *.h) ;;
        *) add_hit "$REL" "$LINE_NUMBER" "non-.h header (C++ std header?)" "$LINE" ;;
      esac
      continue
    fi
    if [[ "$CODE" =~ $NAMESPACE_RE ]]; then
      add_hit "$REL" "$LINE_NUMBER" "namespace" "$LINE"
    fi
    if [[ "$CODE" =~ $TEMPLATE_RE ]]; then
      add_hit "$REL" "$LINE_NUMBER" "template" "$LINE"
    fi
    if [[ "$CODE" == *"std::"* ]]; then
      add_hit "$REL" "$LINE_NUMBER" "std::" "$LINE"
    elif [[ "$CODE" == *"::"* ]]; then
      add_hit "$REL" "$LINE_NUMBER" "::" "$LINE"
    fi
    if [[ "$CODE" =~ $EXTERN_CXX_RE ]]; then
      add_hit "$REL" "$LINE_NUMBER" "extern \"C++\"" "$LINE"
    fi
    if [[ "$CODE" =~ $CLASS_RE ]]; then
      add_hit "$REL" "$LINE_NUMBER" "class (not @class)" "$LINE"
    fi
  done < "$H"
done

COMPILE_RUNS=0
COMPILE_FAILURES=0
COMPILE_STATUS="not run (0 headers)"
if [[ "$HEADER_COUNT" -gt 0 && "$SKIP_COMPILE" -eq 1 ]]; then
  COMPILE_STATUS="skipped (--skip-compile)"
elif [[ "$HEADER_COUNT" -gt 0 ]]; then
  if ! command -v xcrun >/dev/null 2>&1 || ! xcrun --find clang >/dev/null 2>&1; then
    echo "xcrun clang is not available; headers exist, so the compile pass cannot run." >&2
    exit 2
  fi
  COMPILE_STATUS="ran"
  ALIAS_ROOT="$OUT_DIR/module-alias"
  rm -rf "$ALIAS_ROOT"
  mkdir -p "$ALIAS_ROOT"
  INCLUDE_FLAGS=("-I" "$REPO_ROOT/include" "-I" "$REPO_ROOT/include/synesthesia-vibe")
  for I in "${!ABS_DIRS[@]}"; do
    D="${ABS_DIRS[$I]}"
    if [[ -d "$D" ]]; then
      mkdir -p "$ALIAS_ROOT/$I"
      ln -s "$D" "$ALIAS_ROOT/$I/SynesthesiaVibeCore"
      INCLUDE_FLAGS+=("-I" "$D" "-I" "$ALIAS_ROOT/$I")
    fi
  done
  for J in "${!HEADERS[@]}"; do
    H="${HEADERS[$J]}"
    REL="$(display_path "$H")"
    for T in ${HEADER_TARGETS[$J]}; do
      if [[ "$T" == "ios" ]]; then
        SDK="iphonesimulator"
        TARGET_FLAGS=("-arch" "arm64" "-mios-simulator-version-min=$IOS_MIN")
      else
        SDK="macosx"
        TARGET_FLAGS=("-arch" "arm64" "-mmacosx-version-min=$MACOS_MIN")
      fi
      LOG="$OUT_DIR/compile-logs/$(basename "$H" .h)-$T.log"
      COMPILE_RUNS=$((COMPILE_RUNS + 1))
      if xcrun --sdk "$SDK" clang -x objective-c -fsyntax-only -fobjc-arc \
          "${TARGET_FLAGS[@]}" "${INCLUDE_FLAGS[@]}" "$H" > "$LOG" 2>&1; then
        printf '%s [%s]: PASS\n' "$REL" "$T" >> "$COMPILE_FILE"
      else
        COMPILE_FAILURES=$((COMPILE_FAILURES + 1))
        printf '%s [%s]: FAIL (%s)\n' "$REL" "$T" "$(display_path "$LOG")" >> "$COMPILE_FILE"
      fi
    done
  done
fi

HIT_COUNT="$(grep -c . "$HITS_FILE" || true)"
VIOLATIONS=$((HIT_COUNT + COMPILE_FAILURES))

REPORT_PATH="$OUT_DIR/check-report.md"
{
  echo "# Darwin public-header (pure Objective-C) check"
  echo
  echo "Headers checked: $HEADER_COUNT"
  echo
  for S in "${DIR_SUMMARY[@]}"; do
    echo "  - $S"
  done
  echo
  if [[ "$VIOLATIONS" -eq 0 ]]; then
    echo "Overall result: **PASS**"
  else
    echo "Overall result: **FAIL**"
  fi
  echo
  echo "C++ marker hits: $HIT_COUNT"
  if [[ "$HIT_COUNT" -gt 0 ]]; then
    echo
    while IFS= read -r LINE; do
      echo "  - \`$LINE\`"
    done < "$HITS_FILE"
  fi
  echo
  echo "Pure Objective-C compile ($COMPILE_STATUS): $COMPILE_RUNS runs, $COMPILE_FAILURES failures (iOS simulator $IOS_MIN, macOS $MACOS_MIN, -x objective-c -fobjc-arc)"
  if [[ -s "$COMPILE_FILE" ]]; then
    echo
    while IFS= read -r LINE; do
      echo "  - $LINE"
    done < "$COMPILE_FILE"
  fi
} > "$REPORT_PATH"

echo "Darwin headers checked: $HEADER_COUNT headers"
echo "C++ marker hits: $HIT_COUNT"
echo "Compile failures: $COMPILE_FAILURES of $COMPILE_RUNS"
echo "Darwin header violations: $VIOLATIONS"
echo "Report written to $REPORT_PATH"

if [[ "$VIOLATIONS" -gt 0 ]]; then
  exit 1
fi
exit 0
