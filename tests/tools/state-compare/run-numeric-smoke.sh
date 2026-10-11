#!/usr/bin/env bash
#
#  run-numeric-smoke.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-10.
#

set -euo pipefail

print_help() {
  cat <<'EOF'
run-numeric-smoke.sh -- numeric-smoke runner (Mac/Linux twin)

USAGE

  Thin wrapper around the CMake-built tests/tools/state-compare tool (C2-020: all
  compare, tolerance and JSON logic lives in the C++ tool, not here). Runs the
  capture set -- one fresh process per preset -- and then asks state-compare's
  'compare' subcommand to diff every capture's frame-state JSON against the
  reference at the 1e-5 tolerance (4.8), writing compare.json and compare.txt.
  Mirrors run-numeric-smoke.ps1 argument for argument.

    ./run-numeric-smoke.sh --out <dir> [--reference <dir>] [--capture-set <capture-set.json>] \
        [--exe <exe>] [--state-compare-exe <exe>] [--subset id1,id2,...] \
        [--frames N] [--capture-timeout-seconds N]

PARAMETERS

  --capture-set            Path to the checked-in capture-set definition. Default:
                           tests/golden/capture-set.json (repo-root relative).
  --reference              Directory of golden frame directories to compare against.
                           Default: tests/golden/corpus.
  --out                    Directory this run writes captures, compare.json and
                           compare.txt into. Required.
  --exe                    The GL host for this platform. Default:
                           build/macos-arm64/samples/legacy-samples/synesthesia-vibe-demo-macos/synesthesia-vibe-demo-macos
  --state-compare-exe      state-compare (this tool). If omitted, this script looks in
                           the usual build output locations under build/.
  --subset                 Comma-separated capture ids to run/compare instead of the
                           whole set.
  --frames                 Frames per capture, and the cap on frames compared.
                           Default: 30.
  --capture-timeout-seconds
                           Per-attempt wall-clock budget for a single capture
                           invocation, in seconds. Default: 120.

KNOWN DARK-SIDE CARVE-OUT

  preset-dark-side-of-the-moon's corpus entry is non-reproducible (it does not
  reproduce from any fresh build):
  it fails rand_preset[0] at frame 0 on Windows-GL too. The expected result for the
  full set is therefore 9/10; re-run this script with
  --subset preset-dark-side-of-the-moon --reference tests/golden/phase2-reference
  to confirm it PASSes against the Phase 2 reference instead.

EXIT CODE

  0  state-compare reported every compared frame as PASS.
  1  state-compare reported a divergence (or a missing/unpaired frame).
  2  usage error, or a required tool, capture set or reference path was absent.

  The capture process's own exit code is never the pass/fail signal (P0-007: a
  capture-mode teardown crash after every frame is written is a known issue);
  state-compare's verdict is.
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

CAPTURE_SET="$REPO_ROOT/tests/golden/capture-set.json"
REFERENCE="$REPO_ROOT/tests/golden/corpus"
OUT=""
EXE="$REPO_ROOT/build/macos-arm64/samples/legacy-samples/synesthesia-vibe-demo-macos/synesthesia-vibe-demo-macos"
STATE_COMPARE_EXE=""
SUBSET=""
FRAMES=30
CAPTURE_TIMEOUT_SECONDS=120

while [[ $# -gt 0 ]]; do
  case "$1" in
    --capture-set)
      CAPTURE_SET="$2"
      shift 2
      ;;
    --reference)
      REFERENCE="$2"
      shift 2
      ;;
    --out)
      OUT="$2"
      shift 2
      ;;
    --exe)
      EXE="$2"
      shift 2
      ;;
    --state-compare-exe)
      STATE_COMPARE_EXE="$2"
      shift 2
      ;;
    --subset)
      SUBSET="$2"
      shift 2
      ;;
    --frames)
      FRAMES="$2"
      shift 2
      ;;
    --capture-timeout-seconds)
      CAPTURE_TIMEOUT_SECONDS="$2"
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

if [[ -z "$OUT" ]]; then
  echo "--out is required. See --help." >&2
  exit 2
fi
if ! [[ "$FRAMES" =~ ^[0-9]+$ ]]; then
  echo "--frames must be 0 (all) or a non-negative count. See --help." >&2
  exit 2
fi
if ! [[ "$CAPTURE_TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]]; then
  echo "--capture-timeout-seconds must be a positive integer. See --help." >&2
  exit 2
fi

resolve_repo_path() {
  if [[ "$1" == /* ]]; then
    printf '%s' "$1"
  else
    printf '%s' "$REPO_ROOT/$1"
  fi
}

CAPTURE_SET="$(resolve_repo_path "$CAPTURE_SET")"
REFERENCE="$(resolve_repo_path "$REFERENCE")"
OUT="$(resolve_repo_path "$OUT")"
EXE="$(resolve_repo_path "$EXE")"

find_built_tool() {
  local name="$1"
  local candidate
  for candidate in \
    "$REPO_ROOT/build/windows-gl-dev/tests/tools/$name/Release/$name.exe" \
    "$REPO_ROOT/build/x86-release/tests/tools/$name/Release/$name.exe" \
    "$REPO_ROOT/build/macos-arm64/tests/tools/$name/Release/$name" \
    "$REPO_ROOT/build/macos-arm64/tests/tools/$name/$name" \
    "$REPO_ROOT/build/windows-gl-dev/tests/tools/$name/$name.exe" \
    "$REPO_ROOT/build/x86-release/tests/tools/$name/$name.exe" \
    "$REPO_ROOT/build/windows-gl-dev/tests/tools/$name/Debug/$name.exe" \
    "$REPO_ROOT/build/x86-release/tests/tools/$name/Debug/$name.exe"
  do
    if [[ -x "$candidate" ]]; then
      printf '%s' "$candidate"
      return 0
    fi
  done
  printf ''
}

if [[ -z "$STATE_COMPARE_EXE" ]]; then
  STATE_COMPARE_EXE="$(find_built_tool state-compare)"
fi
if [[ -n "$STATE_COMPARE_EXE" ]]; then
  STATE_COMPARE_EXE="$(resolve_repo_path "$STATE_COMPARE_EXE")"
fi

if [[ ! -f "$CAPTURE_SET" ]]; then
  echo "Capture set not found: $CAPTURE_SET"
  exit 2
fi
if [[ ! -d "$REFERENCE" ]]; then
  echo "Reference directory not found: $REFERENCE"
  exit 2
fi
if [[ ! -x "$EXE" ]]; then
  echo "GL host not found: $EXE"
  echo "Pass --exe, or build the Mac host (see --help)."
  exit 2
fi
if [[ -z "$STATE_COMPARE_EXE" || ! -x "$STATE_COMPARE_EXE" ]]; then
  echo "state-compare not found. Build it first (see --help), or pass --state-compare-exe:"
  echo "  cmake --build build/macos-arm64 --target state-compare"
  exit 2
fi

json_field_text() {
  printf '%s\n' "$1" | sed -n "s/.*\"$2\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p"
}

json_array_body() {
  printf '%s\n' "$1" | sed -n "s/.*\"$2\"[[:space:]]*:[[:space:]]*\[\([^]]*\)\].*/\1/p"
}

json_object_list() {
  printf '%s\n' "$1" | grep -oE '\{[^{}]*\}' || true
}

CAPTURE_SET_FLAT="$(tr -d '\n' < "$CAPTURE_SET")"
WAV="$(resolve_repo_path "$(json_field_text "$CAPTURE_SET_FLAT" wav)")"
PRESET_ROOT="$REPO_ROOT/ref-src/beatdrop/BeatDrop/$(json_field_text "$CAPTURE_SET_FLAT" presetPathPrefix)"

if [[ ! -f "$WAV" ]]; then
  echo "WAV not found: $WAV"
  exit 2
fi
if [[ ! -d "$PRESET_ROOT" ]]; then
  echo "Preset root not found: $PRESET_ROOT"
  exit 2
fi

IDS=()
FILES=()
while IFS= read -r obj; do
  [[ -z "$obj" ]] && continue
  id="$(json_field_text "$obj" id)"
  file="$(json_field_text "$obj" file)"
  [[ -z "$id" || -z "$file" ]] && continue
  IDS+=("$id")
  FILES+=("$file")
done <<< "$(json_object_list "$(json_array_body "$CAPTURE_SET_FLAT" presets)")"

if [[ ${#IDS[@]} -eq 0 ]]; then
  echo "Capture set yielded no presets: $CAPTURE_SET"
  exit 2
fi

if [[ -n "$SUBSET" ]]; then
  IFS=',' read -r -a REQUESTED <<< "$SUBSET"
  FILTERED_IDS=()
  FILTERED_FILES=()
  for index in "${!IDS[@]}"; do
    for want in "${REQUESTED[@]}"; do
      if [[ "${IDS[$index]}" == "$want" ]]; then
        FILTERED_IDS+=("${IDS[$index]}")
        FILTERED_FILES+=("${FILES[$index]}")
        break
      fi
    done
  done
  if [[ ${#FILTERED_IDS[@]} -eq 0 ]]; then
    echo "--subset matched no capture ids. See --help." >&2
    exit 2
  fi
  IDS=("${FILTERED_IDS[@]}")
  FILES=("${FILTERED_FILES[@]}")
fi

mkdir -p "$OUT"

invoke_capture_process_once() {
  local exe_path="$1"
  local timeout_seconds="$2"
  local stdout_path="$3"
  local stderr_path="$4"
  shift 4

  pkill -x "$(basename "$exe_path")" 2>/dev/null || true
  sleep 0.5

  local started_at
  started_at="$(date +%s)"
  ( cd "$REPO_ROOT" && exec "$exe_path" "$@" ) >"$stdout_path" 2>"$stderr_path" &
  local pid=$!

  local elapsed=0
  while kill -0 "$pid" 2>/dev/null; do
    if [[ "$elapsed" -ge "$timeout_seconds" ]]; then
      kill -TERM "$pid" 2>/dev/null || true
      sleep 2
      kill -KILL "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
      echo "timeout	124	$(( $(date +%s) - started_at ))"
      return 0
    fi
    sleep 1
    elapsed=$((elapsed + 1))
  done

  local status=0
  wait "$pid" || status=$?
  echo "completed	$status	$(( $(date +%s) - started_at ))"
  return 0
}

for index in "${!IDS[@]}"; do
  id="${IDS[$index]}"
  file="${FILES[$index]}"
  capture_out="$OUT/$id"
  rm -rf "$capture_out"
  mkdir -p "$capture_out"

  echo "[$id] frames=$FRAMES"
  result="$(invoke_capture_process_once "$EXE" "$CAPTURE_TIMEOUT_SECONDS" \
    "$capture_out/_process.stdout.txt" "$capture_out/_process.stderr.txt" \
    --capture "$PRESET_ROOT/$file" "$WAV" --frames "$FRAMES" --out "$capture_out")"
  IFS=$'\t' read -r outcome exit_code wall_seconds <<< "$result"

  if [[ "$outcome" == "timeout" ]]; then
    echo "  [TIMEOUT] exceeded ${CAPTURE_TIMEOUT_SECONDS}s -- reporting TIMEOUT, continuing with remaining presets"
  else
    echo "  exitcode=$exit_code wall=${wall_seconds}s (exit code is informational only -- see EXIT CODE)"
  fi
done

compare_args=(compare --golden "$REFERENCE" --actual "$OUT" --frames "$FRAMES" --json-out "$OUT/compare.json")
for id in "${IDS[@]}"; do
  compare_args+=(--capture "$id")
done

set +e
"$STATE_COMPARE_EXE" "${compare_args[@]}" | tee "$OUT/compare.txt"
COMPARE_EXIT=${PIPESTATUS[0]}
set -e

echo
echo "Compare: $OUT/compare.txt"
echo "JSON:    $OUT/compare.json"

exit "$COMPARE_EXIT"