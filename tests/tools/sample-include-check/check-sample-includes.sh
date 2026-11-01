#!/usr/bin/env bash
#
#  check-sample-includes.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

SAMPLES_DIR="samples"
OUT_DIR=""

print_help() {
  cat <<'EOF'
check-sample-includes.sh -- samples-use-only-the-public-API checker (Mac/Linux twin of
check-sample-includes.ps1)

USAGE

  Static source-text scan (no build required) of samples/** that verifies every sample reaches
  the library through its public surface only. samples/legacy-samples/ is skipped (the legacy
  GL hosts predate the public API and are exempt).

    ./check-sample-includes.sh --out <dir> [--dir <path>]

PARAMETERS

  --out   Directory this run writes check-report.md into. Required.
  --dir   Samples root scanned, repo-root relative unless absolute. Default: samples.
          <root>/legacy-samples is always skipped.

WHAT COUNTS AS A VIOLATION

  C, C++, Objective-C (*.h, *.hpp, *.c, *.cc, *.cpp, *.m, *.mm):
    - #include/#import <X> where X contains "..", or X is reached through a repo-internal
      include root (src/, src/<module>/, include/synesthesia-vibe/ without its prefix,
      platform/<p>/{include,src}/, vendor/<v>/{,include/}, samples/legacy-samples/shared/),
      unless X starts with synesthesia-vibe/ or SynesthesiaVibeCore/;
    - #include/#import "X" that does not resolve inside the same sample (next to the including
      file, or anywhere under samples/<platform>/<sample>/), unless X starts with
      synesthesia-vibe/ or SynesthesiaVibeCore/;
    - @import M; where M is a project module other than SynesthesiaVibe / SynesthesiaVibeCore.
  Swift (*.swift):
    - import M where M is a project module (Synesthesia*, BeatDrop*, beatdrop*) other than
      SynesthesiaVibe / SynesthesiaVibeCore. Any other module is treated as a system module.
  Kotlin / Java (*.kt, *.kts, *.java):
    - import com.synesthesiavibe.internal.*;
    - System.loadLibrary (JNI loading belongs to platform/android only).
  JavaScript under <root>/web (*.js, *.mjs, and inline <script> blocks in *.html / *.htm other
  than type=importmap / JSON):
    - an import specifier (import ... from, export ... from, import 'X', import('X')) other
      than the bare specifier synesthesia-vibe or a relative path (./, ../) that resolves
      inside <root>/web;
    - a specifier containing synesthesia-vibe-wasm, platform/web or /js/ is always a violation;
    - any use of createSynesthesiaVibeModule (the raw Emscripten factory is not public API);
    - <script src="X"> in HTML where X is not a relative path resolving inside <root>/web.

  Text after // on a line is ignored (for JavaScript only when // starts the line or follows
  whitespace or ;, so URLs inside strings are kept). <!-- --> and /* */ are not stripped.

EXIT CODE

  0   Zero violations (including the "no sample outside legacy-samples yet" case).
  1   One or more violations.
  2   Usage error.
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
      SAMPLES_DIR="$2"
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

if [[ "$SAMPLES_DIR" != /* ]]; then
  SAMPLES_DIR="$REPO_ROOT/$SAMPLES_DIR"
fi
SAMPLES_DIR="${SAMPLES_DIR%/}"
if [[ "$OUT_DIR" != /* ]]; then
  OUT_DIR="$REPO_ROOT/$OUT_DIR"
fi
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

LEGACY_DIR="$SAMPLES_DIR/legacy-samples"

display_path() {
  local P="$1"
  if [[ "$P" == "$REPO_ROOT"/* ]]; then
    echo "${P#"$REPO_ROOT"/}"
  else
    echo "$P"
  fi
}

INTERNAL_ROOTS=()
add_root() {
  if [[ -d "$1" ]]; then
    INTERNAL_ROOTS+=("$1")
  fi
}
add_root "$REPO_ROOT/src"
for D in "$REPO_ROOT"/src/*/ "$REPO_ROOT"/vendor/*/ "$REPO_ROOT"/vendor/*/include/ \
    "$REPO_ROOT"/platform/*/include/ "$REPO_ROOT"/platform/*/src/; do
  add_root "${D%/}"
done
add_root "$REPO_ROOT/include/synesthesia-vibe"
add_root "$REPO_ROOT/vendor"
add_root "$REPO_ROOT/platform"
add_root "$REPO_ROOT/samples/legacy-samples/shared"

C_INCLUDE_RE='^[[:space:]]*#[[:space:]]*(include|import)[[:space:]]*([<"])([^">]+)[>"]'
OBJC_MODULE_RE='^[[:space:]]*@import[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)'
SWIFT_IMPORT_RE='^[[:space:]]*(@[A-Za-z_]+[[:space:]]+)*import[[:space:]]+((typealias|struct|class|enum|protocol|let|var|func)[[:space:]]+)?([A-Za-z_][A-Za-z0-9_]*)'
JVM_INTERNAL_RE='^[[:space:]]*import[[:space:]]+(static[[:space:]]+)?com\.synesthesiavibe\.internal([.;[:space:]]|$)'
LOAD_LIBRARY_RE='System[[:space:]]*\.[[:space:]]*loadLibrary'
JS_QUOTE="[\"'\`]"
JS_NOT_QUOTE="[^\"'\`]"
JS_SPECIFIER_RE="(^|[^A-Za-z0-9_\$.])(from|import)[[:space:]]*\\(?[[:space:]]*${JS_QUOTE}(${JS_NOT_QUOTE}+)${JS_QUOTE}"
JS_FACTORY_RE='createSynesthesiaVibeModule'
HTML_SCRIPT_OPEN_RE='<script([^>]*)>'
HTML_SCRIPT_SRC_RE="[[:space:]]src[[:space:]]*=[[:space:]]*[\"']?([^\"'[:space:]>]+)"
HTML_SCRIPT_DATA_TYPE_RE="type[[:space:]]*=[[:space:]]*[\"']?[^\"'[:space:]>]*(importmap|json)"
WEB_DIR="$SAMPLES_DIR/web"

HITS_FILE="$OUT_DIR/sample-include-hits.txt"
: > "$HITS_FILE"

add_hit() {
  printf '%s:%d: %s: %s\n' "$1" "$2" "$3" "$4" >> "$HITS_FILE"
}

is_public_prefix() {
  case "$1" in
    synesthesia-vibe/*|SynesthesiaVibeCore/*) return 0 ;;
  esac
  return 1
}

is_project_module() {
  case "$1" in
    SynesthesiaVibe|SynesthesiaVibeCore) return 1 ;;
    Synesthesia*|synesthesia*|BeatDrop*|Beatdrop*|beatdrop*) return 0 ;;
  esac
  return 1
}

internal_root_for() {
  local NAME="$1" R
  for R in "${INTERNAL_ROOTS[@]}"; do
    if [[ -e "$R/$NAME" ]]; then
      display_path "$R"
      return 0
    fi
  done
  return 1
}

sample_root_for() {
  local REL="${1#"$SAMPLES_DIR"/}"
  local A="${REL%%/*}"
  local REST="${REL#*/}"
  local B="${REST%%/*}"
  if [[ "$REST" == "$REL" || "$B" == "$REST" ]]; then
    echo "$SAMPLES_DIR/$A"
  else
    echo "$SAMPLES_DIR/$A/$B"
  fi
}

resolves_in_sample() {
  local FILE="$1" NAME="$2" DIR ROOT CANDIDATE
  DIR="$(dirname "$FILE")"
  ROOT="$(sample_root_for "$FILE")"
  if [[ -e "$DIR/$NAME" ]]; then
    CANDIDATE="$(cd "$(dirname "$DIR/$NAME")" && pwd)"
    [[ "$CANDIDATE" == "$ROOT" || "$CANDIDATE" == "$ROOT"/* ]] && return 0
    return 1
  fi
  [[ "$NAME" == *..* ]] && return 1
  if [[ -n "$(find "$ROOT" -path "*/$NAME" -type f -print -quit 2>/dev/null)" ]]; then
    return 0
  fi
  return 1
}

web_relative_resolves() {
  local FILE="$1" NAME="$2" TARGET TARGET_DIR
  TARGET="$(dirname "$FILE")/$NAME"
  [[ -f "$TARGET" ]] || return 1
  TARGET_DIR="$(cd "$(dirname "$TARGET")" && pwd)"
  [[ "$TARGET_DIR" == "$WEB_DIR" || "$TARGET_DIR" == "$WEB_DIR"/* ]]
}

check_js_specifier() {
  local REL="$1" LINE_NUMBER="$2" FILE="$3" SPEC="$4" LINE="$5"
  case "$SPEC" in
    synesthesia-vibe)
      return 0
      ;;
    *synesthesia-vibe-wasm*)
      add_hit "$REL" "$LINE_NUMBER" "raw wasm module import" "$LINE"
      ;;
    *platform/web*)
      add_hit "$REL" "$LINE_NUMBER" "platform/web internal import" "$LINE"
      ;;
    */js/*)
      add_hit "$REL" "$LINE_NUMBER" "library internals import (/js/)" "$LINE"
      ;;
    ./*|../*)
      if ! web_relative_resolves "$FILE" "$SPEC"; then
        add_hit "$REL" "$LINE_NUMBER" "relative import outside this sample" "$LINE"
      fi
      ;;
    *)
      add_hit "$REL" "$LINE_NUMBER" "import other than synesthesia-vibe or a file inside this sample" "$LINE"
      ;;
  esac
}

check_js_code() {
  local REL="$1" LINE_NUMBER="$2" FILE="$3" CODE="$4" LINE="$5" REST
  REST="$CODE"
  while [[ "$REST" =~ $JS_SPECIFIER_RE ]]; do
    check_js_specifier "$REL" "$LINE_NUMBER" "$FILE" "${BASH_REMATCH[3]}" "$LINE"
    REST="${REST#*"${BASH_REMATCH[0]}"}"
  done
  if [[ "$CODE" =~ $JS_FACTORY_RE ]]; then
    add_hit "$REL" "$LINE_NUMBER" "createSynesthesiaVibeModule outside the public API" "$LINE"
  fi
}

strip_js_comment() {
  if [[ "$1" =~ ^[[:space:]]*// ]]; then
    echo ""
  else
    echo "${1%%[[:space:];]//*}"
  fi
}

check_script_src() {
  local REL="$1" LINE_NUMBER="$2" FILE="$3" SRC="$4" LINE="$5"
  case "$SRC" in
    *synesthesia-vibe-wasm*|*platform/web*|*/js/*)
      add_hit "$REL" "$LINE_NUMBER" "script src reaches library internals" "$LINE"
      return 0
      ;;
    /*|*:*)
      add_hit "$REL" "$LINE_NUMBER" "script src outside this sample" "$LINE"
      return 0
      ;;
  esac
  if ! web_relative_resolves "$FILE" "$SRC"; then
    add_hit "$REL" "$LINE_NUMBER" "script src outside this sample" "$LINE"
  fi
}

check_js_file() {
  local F="$1" REL="$2" LINE LINE_NUMBER=0
  while IFS= read -r LINE || [[ -n "$LINE" ]]; do
    LINE_NUMBER=$((LINE_NUMBER + 1))
    check_js_code "$REL" "$LINE_NUMBER" "$F" "$(strip_js_comment "$LINE")" "$LINE"
  done < "$F"
}

check_html_file() {
  local F="$1" REL="$2" LINE LINE_NUMBER=0 IN_SCRIPT=0 ATTRS CODE
  while IFS= read -r LINE || [[ -n "$LINE" ]]; do
    LINE_NUMBER=$((LINE_NUMBER + 1))
    CODE=""
    if [[ "$IN_SCRIPT" -eq 1 ]]; then
      CODE="$LINE"
    elif [[ "$LINE" =~ $HTML_SCRIPT_OPEN_RE ]]; then
      ATTRS="${BASH_REMATCH[1]}"
      if [[ " $ATTRS" =~ $HTML_SCRIPT_SRC_RE ]]; then
        check_script_src "$REL" "$LINE_NUMBER" "$F" "${BASH_REMATCH[1]}" "$LINE"
      elif ! [[ "$ATTRS" =~ $HTML_SCRIPT_DATA_TYPE_RE ]]; then
        IN_SCRIPT=1
        CODE="${LINE#*<script"$ATTRS">}"
      fi
    fi
    if [[ "$IN_SCRIPT" -eq 1 ]]; then
      if [[ "$CODE" == *"</script"* ]]; then
        CODE="${CODE%%</script*}"
        IN_SCRIPT=0
      fi
      check_js_code "$REL" "$LINE_NUMBER" "$F" "$(strip_js_comment "$CODE")" "$LINE"
    fi
  done < "$F"
}

FILES=()
WEB_FILES=()
LEGACY_SKIPPED=0
if [[ -d "$LEGACY_DIR" ]]; then
  LEGACY_SKIPPED="$(find "$LEGACY_DIR" -type f | wc -l | tr -d ' ')"
fi
if [[ -d "$SAMPLES_DIR" ]]; then
  while IFS= read -r -d '' F; do
    FILES+=("$F")
  done < <(find "$SAMPLES_DIR" -path "$LEGACY_DIR" -prune -o -type f \( \
      -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cc' -o -name '*.cpp' \
      -o -name '*.m' -o -name '*.mm' -o -name '*.swift' -o -name '*.kt' -o -name '*.kts' \
      -o -name '*.java' \) -print0 | sort -z)
fi
if [[ -d "$WEB_DIR" ]]; then
  while IFS= read -r -d '' F; do
    WEB_FILES+=("$F")
  done < <(find "$WEB_DIR" -type f \( -name '*.js' -o -name '*.mjs' -o -name '*.html' \
      -o -name '*.htm' \) -print0 | sort -z)
fi

FILE_COUNT=$((${#FILES[@]} + ${#WEB_FILES[@]}))

for F in ${FILES[@]+"${FILES[@]}"}; do
  REL="$(display_path "$F")"
  case "$F" in
    *.swift) KIND="swift" ;;
    *.kt|*.kts|*.java) KIND="jvm" ;;
    *) KIND="c" ;;
  esac
  LINE_NUMBER=0
  while IFS= read -r LINE || [[ -n "$LINE" ]]; do
    LINE_NUMBER=$((LINE_NUMBER + 1))
    CODE="${LINE%%//*}"
    if [[ "$KIND" == "c" ]]; then
      if [[ "$CODE" =~ $C_INCLUDE_RE ]]; then
        DELIM="${BASH_REMATCH[2]}"
        NAME="${BASH_REMATCH[3]}"
        if is_public_prefix "$NAME"; then
          continue
        fi
        if [[ "$DELIM" == "<" ]]; then
          if [[ "$NAME" == *..* ]]; then
            add_hit "$REL" "$LINE_NUMBER" "relative path in system include" "$LINE"
          elif ROOT="$(internal_root_for "$NAME")"; then
            add_hit "$REL" "$LINE_NUMBER" "internal header (resolves under $ROOT)" "$LINE"
          fi
        elif ! resolves_in_sample "$F" "$NAME"; then
          add_hit "$REL" "$LINE_NUMBER" "quoted include outside this sample" "$LINE"
        fi
      elif [[ "$CODE" =~ $OBJC_MODULE_RE ]]; then
        if is_project_module "${BASH_REMATCH[1]}"; then
          add_hit "$REL" "$LINE_NUMBER" "internal module" "$LINE"
        fi
      fi
    elif [[ "$KIND" == "swift" ]]; then
      if [[ "$CODE" =~ $SWIFT_IMPORT_RE ]]; then
        if is_project_module "${BASH_REMATCH[4]}"; then
          add_hit "$REL" "$LINE_NUMBER" "internal module" "$LINE"
        fi
      fi
    else
      if [[ "$CODE" =~ $JVM_INTERNAL_RE ]]; then
        add_hit "$REL" "$LINE_NUMBER" "internal package import" "$LINE"
      fi
      if [[ "$CODE" =~ $LOAD_LIBRARY_RE ]]; then
        add_hit "$REL" "$LINE_NUMBER" "System.loadLibrary outside platform/android" "$LINE"
      fi
    fi
  done < "$F"
done

for F in ${WEB_FILES[@]+"${WEB_FILES[@]}"}; do
  case "$F" in
    *.html|*.htm) check_html_file "$F" "$(display_path "$F")" ;;
    *) check_js_file "$F" "$(display_path "$F")" ;;
  esac
done

HIT_COUNT="$(grep -c . "$HITS_FILE" || true)"

REPORT_PATH="$OUT_DIR/check-report.md"
DISPLAY_DIR="$(display_path "$SAMPLES_DIR")"
{
  echo "# Sample include check (public API only)"
  echo
  echo "Static source scan (no build). Files checked under \`$DISPLAY_DIR/\`: $FILE_COUNT."
  echo
  echo "legacy-samples skipped: \`$(display_path "$LEGACY_DIR")/\` ($LEGACY_SKIPPED files, exempt)."
  echo
  if [[ "$HIT_COUNT" -eq 0 ]]; then
    echo "Overall result: **PASS**"
  else
    echo "Overall result: **FAIL**"
  fi
  echo
  echo "Violations: $HIT_COUNT"
  if [[ "$HIT_COUNT" -gt 0 ]]; then
    echo
    while IFS= read -r LINE; do
      echo "  - \`$LINE\`"
    done < "$HITS_FILE"
  fi
} > "$REPORT_PATH"

echo "Sample files checked: $FILE_COUNT"
echo "legacy-samples skipped ($LEGACY_SKIPPED files)"
echo "Sample include violations: $HIT_COUNT"
echo "Report written to $REPORT_PATH"

if [[ "$HIT_COUNT" -gt 0 ]]; then
  exit 1
fi
exit 0
