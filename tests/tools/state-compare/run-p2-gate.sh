#!/usr/bin/env bash
#
#  run-p2-gate.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-07.
#

set -euo pipefail

print_help() {
  cat <<'EOF'
run-p2-gate.sh -- P2-028 Phase 2 gate runner (Mac/Linux twin)

USAGE

  Thin wrapper around the CMake-built tests/tools/state-compare tool (C2-020: all
  compare, tolerance, SSIM and Markdown logic lives in the C++ tool, not here).
  Runs the GL host capture set -- one fresh process per capture -- and then asks
  state-compare to compare it against the reference, write the per-preset matrix
  and write report.md. Mirrors run-p2-gate.ps1 argument for argument.

    ./run-p2-gate.sh --out <dir> [--reference <dir>] [--capture-set <capture-set.json>] \
        [--exe <exe>] [--state-compare-exe <exe>] [--frame-compare-exe <exe>] \
        [--matrix <path>] [--subset id1,id2,...] [--frames N] \
        [--timeout-seconds N] [--skip-capture]

PARAMETERS

  --capture-set        Path to the checked-in capture-set definition. Default:
                       tests/golden/capture-set.json (repo-root relative).
  --reference          Directory of golden frame directories to compare against.
                       Default: tests/golden/corpus.
  --out                Directory this run writes captures, report.md and logs into.
                       Required.
  --exe                The GL host for this platform. No default -- pass the host
                       this platform builds (P2-031 pins the Windows path; its
                       handoff is where the real default lands). Until then use
                       --skip-capture to drive the gate end-to-end against the
                       checked-in golden corpus.
  --state-compare-exe  state-compare (this tool). If omitted, this script looks in
                       the usual build output locations under build/.
  --frame-compare-exe  frame-compare (P0-008), the SSIM engine state-compare spawns
                       once per frame pair. If omitted, this script looks in the
                       usual build output locations under build/.
  --matrix             Path the per-preset matrix is written to. Default:
                       tests/golden/phase2-matrix.md.
  --subset             Comma-separated capture ids to run/compare instead of the
                       whole set.
  --frames             Caps how many frames per capture are captured and compared
                       (0 = every frame in the capture set). Quick subset
                       verification only -- never use this to shrink an actual
                       golden-comparison run.
  --timeout-seconds    Per-attempt wall-clock budget for a single capture
                       invocation, in seconds. Default: 700. Mirrors
                       run-equivalence-gate.ps1; see its TIMEOUT / HANG HANDLING.
  --skip-capture       Skip invoking the GL host entirely and compare whatever
                       already exists under <out>/<id>/. Used to drive the gate
                       against a checked-in capture set without paying capture time.

EXIT CODE

  0  state-compare reported every compared frame as PASS.
  1  state-compare reported a divergence (or a missing/unpaired frame), or a
     capture timed out / came back incomplete.
  2  usage error, or a required tool, capture set or reference path was absent.

  The capture process's own exit code is never the pass/fail signal (P0-007: a
  capture-mode teardown crash after every frame is written is a known issue);
  frame completeness and the compare verdict are.
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

CAPTURE_SET="$REPO_ROOT/tests/golden/capture-set.json"
REFERENCE="$REPO_ROOT/tests/golden/corpus"
OUT=""
EXE=""
STATE_COMPARE_EXE=""
FRAME_COMPARE_EXE=""
MATRIX="$REPO_ROOT/tests/golden/phase2-matrix.md"
SUBSET=""
FRAMES=0
TIMEOUT_SECONDS=700
SKIP_CAPTURE=0

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
    --frame-compare-exe)
      FRAME_COMPARE_EXE="$2"
      shift 2
      ;;
    --matrix)
      MATRIX="$2"
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
    --timeout-seconds)
      TIMEOUT_SECONDS="$2"
      shift 2
      ;;
    --skip-capture)
      SKIP_CAPTURE=1
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

if [[ -z "$OUT" ]]; then
  echo "--out is required. See --help." >&2
  exit 2
fi
if ! [[ "$FRAMES" =~ ^[0-9]+$ ]]; then
  echo "--frames must be 0 (all) or a non-negative count. See --help." >&2
  exit 2
fi
if ! [[ "$TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]]; then
  echo "--timeout-seconds must be a positive integer. See --help." >&2
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
MATRIX="$(resolve_repo_path "$MATRIX")"
if [[ -n "$EXE" ]]; then
  EXE="$(resolve_repo_path "$EXE")"
fi

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
if [[ -z "$FRAME_COMPARE_EXE" ]]; then
  FRAME_COMPARE_EXE="$(find_built_tool frame-compare)"
fi
if [[ -n "$STATE_COMPARE_EXE" ]]; then
  STATE_COMPARE_EXE="$(resolve_repo_path "$STATE_COMPARE_EXE")"
fi
if [[ -n "$FRAME_COMPARE_EXE" ]]; then
  FRAME_COMPARE_EXE="$(resolve_repo_path "$FRAME_COMPARE_EXE")"
fi

if [[ ! -f "$CAPTURE_SET" ]]; then
  echo "Capture set not found: $CAPTURE_SET"
  exit 2
fi
if [[ ! -d "$REFERENCE" ]]; then
  echo "Reference directory not found: $REFERENCE"
  exit 2
fi
if [[ -z "$STATE_COMPARE_EXE" || ! -x "$STATE_COMPARE_EXE" ]]; then
  echo "state-compare not found. Build it first (see --help), or pass --state-compare-exe:"
  echo "  cmake --build build/windows-gl-dev --config Release --target state-compare"
  exit 2
fi
if [[ -z "$FRAME_COMPARE_EXE" || ! -x "$FRAME_COMPARE_EXE" ]]; then
  echo "frame-compare not found. Pass --frame-compare-exe, e.g.:"
  echo "  build/x86-release/tests/tools/frame-compare/Release/frame-compare.exe"
  exit 2
fi
if [[ "$SKIP_CAPTURE" -eq 0 && ( -z "$EXE" || ! -x "$EXE" ) ]]; then
  echo "GL host not found: $EXE"
  echo "Pass --exe, or use --skip-capture to compare an existing capture directory."
  exit 2
fi

json_field_text() {
  printf '%s\n' "$1" | sed -n "s/.*\"$2\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p"
}

json_field_number() {
  printf '%s\n' "$1" | sed -n "s/.*\"$2\"[[:space:]]*:[[:space:]]*\(-\{0,1\}[0-9][0-9.]*\).*/\1/p"
}

json_array_body() {
  printf '%s\n' "$1" | sed -n "s/.*\"$2\"[[:space:]]*:[[:space:]]*\[\([^]]*\)\].*/\1/p"
}

json_object_list() {
  printf '%s\n' "$1" | grep -o '\{[^{}]*\}' || true
}

CAPTURES=()
WAV=""
PRESET_PREFIX=""
DEFAULT_FRAMES=0
BLEND_PATTERN=0

CAPTURE_SET_FLAT="$(tr -d '\n' < "$CAPTURE_SET")"
WAV="$(resolve_repo_path "$(json_field_text "$CAPTURE_SET_FLAT" wav)")"
PRESET_PREFIX="$(json_field_text "$CAPTURE_SET_FLAT" presetPathPrefix)"
DEFAULT_FRAMES="$(json_field_number "$CAPTURE_SET_FLAT" defaultFrames)"
BLEND_PATTERN="$(json_field_number "$CAPTURE_SET_FLAT" blendPattern)"

if [[ "$SKIP_CAPTURE" -eq 0 && ! -f "$WAV" ]]; then
  echo "WAV not found: $WAV"
  exit 2
fi

while IFS= read -r obj; do
  [[ -z "$obj" ]] && continue
  id="$(json_field_text "$obj" id)"
  file="$(json_field_text "$obj" file)"
  [[ -z "$id" || -z "$file" ]] && continue
  CAPTURES+=("single	$id	$file				$DEFAULT_FRAMES")
done <<< "$(json_object_list "$(json_array_body "$CAPTURE_SET_FLAT" presets)")"

while IFS= read -r obj; do
  [[ -z "$obj" ]] && continue
  id="$(json_field_text "$obj" id)"
  from="$(json_field_text "$obj" from)"
  to="$(json_field_text "$obj" to)"
  at="$(json_field_number "$obj" blendAtFrame)"
  blend_time="$(json_field_number "$obj" blendTimeSeconds)"
  frames="$(json_field_number "$obj" frames)"
  [[ -z "$id" || -z "$from" || -z "$to" ]] && continue
  if [[ -z "$frames" ]]; then
    frames="$DEFAULT_FRAMES"
  fi
  CAPTURES+=("blend	$id	$from	$to	$at	$blend_time	$frames")
done <<< "$(json_object_list "$(json_array_body "$CAPTURE_SET_FLAT" blends)")"

if [[ ${#CAPTURES[@]} -eq 0 ]]; then
  echo "Capture set yielded no captures: $CAPTURE_SET"
  exit 2
fi

if [[ -n "$SUBSET" ]]; then
  IFS=',' read -r -a REQUESTED <<< "$SUBSET"
  FILTERED=()
  for record in "${CAPTURES[@]}"; do
    IFS=$'\t' read -r kind id preset blend_to blend_at blend_time frames <<< "$record"
    for want in "${REQUESTED[@]}"; do
      if [[ "$id" == "$want" ]]; then
        FILTERED+=("$record")
        break
      fi
    done
  done
  if [[ ${#FILTERED[@]} -eq 0 ]]; then
    echo "--subset matched no capture ids. See --help." >&2
    exit 2
  fi
  CAPTURES=("${FILTERED[@]}")
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
  "$exe_path" "$@" >"$stdout_path" 2>"$stderr_path" &
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

capture_frames_incomplete() {
  local capture_out_dir="$1"
  local frame_count="$2"
  local missing=()
  local i frame_name ext path
  for ((i = 0; i < frame_count; i++)); do
    frame_name="$(printf 'frame_%04d' "$i")"
    for ext in png json; do
      path="$capture_out_dir/$frame_name.$ext"
      if [[ ! -f "$path" ]]; then
        missing+=("$frame_name.$ext (missing)")
      elif [[ ! -s "$path" ]]; then
        missing+=("$frame_name.$ext (empty)")
      fi
    done
  done
  if [[ ${#missing[@]} -eq 0 ]]; then
    return 1
  fi
  printf '%s' "$(IFS=', '; echo "${missing[*]}")"
  return 0
}

CAPTURE_FAILED=0
SELECTED_IDS=()

for record in "${CAPTURES[@]}"; do
  IFS=$'\t' read -r kind id preset blend_to blend_at blend_time frames <<< "$record"
  frame_count="$frames"
  if [[ "$FRAMES" -gt 0 ]]; then
    frame_count="$FRAMES"
  fi

  capture_out_dir="$OUT/$id"
  SELECTED_IDS+=("$id")
  echo "[$id] frames=$frame_count kind=$kind"
  timed_out=0

  if [[ "$SKIP_CAPTURE" -eq 0 ]]; then
    rm -rf "$capture_out_dir"
    mkdir -p "$capture_out_dir"

    capture_args=(--capture "$PRESET_PREFIX/$preset" "$WAV" --frames "$frame_count" --out "$capture_out_dir")
    if [[ "$kind" == "blend" ]]; then
      capture_args+=(--blend-to "$PRESET_PREFIX/$blend_to" --blend-at "$blend_at" --blend-time "$blend_time" --blend-pattern "$BLEND_PATTERN")
    fi

    result="$(invoke_capture_process_once "$EXE" "$TIMEOUT_SECONDS" \
      "$capture_out_dir/_process.log.stdout.txt" "$capture_out_dir/_process.log.stderr.txt" \
      "${capture_args[@]}")"
    IFS=$'\t' read -r outcome exit_code wall_seconds <<< "$result"

    printf '(stdout/stderr redirected to %s / %s)\n' \
      "$capture_out_dir/_process.log.stdout.txt" "$capture_out_dir/_process.log.stderr.txt" \
      > "$capture_out_dir/_process.log"

    if [[ "$outcome" == "timeout" ]]; then
      timed_out=1
      echo "  [TIMEOUT] exceeded ${TIMEOUT_SECONDS}s -- reporting TIMEOUT, continuing with remaining captures"
    else
      echo "  exitcode=$exit_code wall=${wall_seconds}s (exit code is informational only -- see KNOWN-ISSUE note)"
    fi
  else
    if [[ ! -d "$capture_out_dir" ]]; then
      echo "--skip-capture given but $capture_out_dir does not exist"
      exit 2
    fi
  fi

  if [[ "$timed_out" -eq 1 ]]; then
    CAPTURE_FAILED=1
    continue
  fi

  incomplete="$(capture_frames_incomplete "$capture_out_dir" "$frame_count" || true)"
  if [[ -n "$incomplete" ]]; then
    CAPTURE_FAILED=1
    echo "  INCOMPLETE -- missing/empty frames: $incomplete"
  fi
done

report_args=(report --golden "$REFERENCE" --actual "$OUT" --frame-compare "$FRAME_COMPARE_EXE" --out "$OUT" --matrix "$MATRIX")
if [[ "$FRAMES" -gt 0 ]]; then
  report_args+=(--frames "$FRAMES")
fi
for id in "${SELECTED_IDS[@]}"; do
  report_args+=(--capture "$id")
done

set +e
"$STATE_COMPARE_EXE" "${report_args[@]}"
REPORT_EXIT=$?
set -e

echo
echo "Report:  $OUT/report.md"
echo "Matrix:  $MATRIX"

if [[ "$REPORT_EXIT" -ne 0 ]]; then
  exit "$REPORT_EXIT"
fi
if [[ "$CAPTURE_FAILED" -eq 1 ]]; then
  exit 1
fi
exit 0