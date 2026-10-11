#!/usr/bin/env bash
#
#  run-visual-check.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-10.
#

set -euo pipefail

print_help() {
  cat <<'EOF'
run-visual-check.sh -- visual check runner (Mac/Linux twin)

USAGE

  Thin wrapper around the CMake-built tests/tools/state-compare tool (C2-020: all
  SSIM, composite and Markdown logic lives in the C++ tool, not here). Renders the
  selected presets through the GL host with a sampled, PNG-only capture
  (--save-frames, --no-json), then asks state-compare's 'visual' subcommand to pair
  each sampled frame against the reference tree, write a golden | GL |
  amplified-abs-diff composite per pair and write report.md.
  Mirrors run-visual-check.ps1 argument for argument.

    ./run-visual-check.sh --out <dir> [--reference <dir>] [--capture-set <capture-set.json>] \
        [--exe <exe>] [--state-compare-exe <exe>] [--frame-compare-exe <exe>] \
        [--presets id1,id2,...] [--sample-frames N,N,...] [--frames N] \
        [--capture-timeout-seconds N] [--skip-capture]

PARAMETERS

  --capture-set            Path to the checked-in capture-set definition. Default:
                           tests/golden/capture-set.json (repo-root relative). Used
                           for the preset id -> preset file mapping only.
  --reference              Directory of golden frame directories to compare against.
                           Default: tests/golden/corpus.
  --out                    Directory this run writes captures, composites and
                           report.md into. Required.
  --exe                    The GL host for this platform. Default:
                           build/macos-arm64/samples/legacy-samples/synesthesia-vibe-demo-macos/synesthesia-vibe-demo-macos
  --state-compare-exe      state-compare (this tool). If omitted, this script looks in
                           the usual build output locations under build/.
  --frame-compare-exe      frame-compare, the SSIM engine and composite writer
                           state-compare spawns once per frame pair. If omitted, this
                           script looks in the usual build output locations under build/.
  --presets                Comma-separated capture ids to check. Default: the 7
                           presetver-100 presets of the Phase 2 matrix, as in
                           run-visual-check.ps1. Pass all 10 MVP capture ids for the
                           full 10-preset MVP visual check.
  --sample-frames          Comma-separated frame numbers to save and compare. Default:
                           0,30,60,150,300,450,599. Captures are always sampled, never
                           600 PNGs (Phase 2 constraint).
  --frames                 How many frames each capture renders. Default: 600, matching
                           the golden corpus length so frame numbering lines up.
  --capture-timeout-seconds
                           Per-attempt wall-clock budget for a single capture
                           invocation, in seconds. Default: 700.
  --skip-capture           Skip invoking the GL host entirely and compare whatever
                           already exists under <out>/<id>/. Used to re-run the report
                           without paying capture time.

EXIT CODE

  0  state-compare's 'visual' subcommand compared every sampled frame.
  1  a capture timed out / came back incomplete, or state-compare reported a frame
     error (e.g. a sampled PNG missing from a capture).
  2  usage error, or a required tool, capture set, WAV or reference path was absent.

  The capture process's own exit code is never the pass/fail signal (P0-007: a
  capture-mode teardown crash after every frame is written is a known issue);
  sampled-frame completeness and state-compare's verdict are.

  SSIM is REPORTED, never gated (CW-005 recommendation (a)): a low score is not a
  failure of this script. Scores below the C2-011 0.98/0.95 gate are expected for
  audio-reactive feedback presets, where small numeric differences grow with frame
  number. The Mac parity bar is per-preset Mac SSIM >= Windows-GL SSIM - 0.02
  (C-002 a).
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

CAPTURE_SET="$REPO_ROOT/tests/golden/capture-set.json"
REFERENCE="$REPO_ROOT/tests/golden/corpus"
OUT=""
EXE="$REPO_ROOT/build/macos-arm64/samples/legacy-samples/synesthesia-vibe-demo-macos/synesthesia-vibe-demo-macos"
STATE_COMPARE_EXE=""
FRAME_COMPARE_EXE=""
PRESETS="preset-cat-scan,preset-dark-side-of-the-moon,preset-now-and-later,preset-fractopia-upspoken-mix,preset-jesters-calling-2,preset-fuzzy-sciences,preset-shifter-swarm"
SAMPLE_FRAMES="0,30,60,150,300,450,599"
FRAMES=600
CAPTURE_TIMEOUT_SECONDS=700
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
    --presets)
      PRESETS="$2"
      shift 2
      ;;
    --sample-frames)
      SAMPLE_FRAMES="$2"
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
if ! [[ "$FRAMES" =~ ^[1-9][0-9]*$ ]]; then
  echo "--frames must be a positive integer. See --help." >&2
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
if [[ -z "$FRAME_COMPARE_EXE" ]]; then
  FRAME_COMPARE_EXE="$(find_built_tool frame-compare)"
fi
if [[ -n "$STATE_COMPARE_EXE" ]]; then
  STATE_COMPARE_EXE="$(resolve_repo_path "$STATE_COMPARE_EXE")"
fi
if [[ -n "$FRAME_COMPARE_EXE" ]]; then
  FRAME_COMPARE_EXE="$(resolve_repo_path "$FRAME_COMPARE_EXE")"
fi

SAMPLE_FRAME_LIST=()
IFS=',' read -r -a SAMPLE_FRAME_PARTS <<< "$SAMPLE_FRAMES"
for part in "${SAMPLE_FRAME_PARTS[@]}"; do
  trimmed="${part// /}"
  [[ -z "$trimmed" ]] && continue
  if ! [[ "$trimmed" =~ ^[0-9]+$ ]]; then
    echo "--sample-frames must be a comma-separated list of non-negative frame numbers. See --help." >&2
    exit 2
  fi
  SAMPLE_FRAME_LIST+=("$trimmed")
done
if [[ ${#SAMPLE_FRAME_LIST[@]} -eq 0 ]]; then
  echo "--sample-frames parsed to an empty frame list. See --help." >&2
  exit 2
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
  echo "  cmake --build build/macos-arm64 --target state-compare"
  exit 2
fi
if [[ -z "$FRAME_COMPARE_EXE" || ! -x "$FRAME_COMPARE_EXE" ]]; then
  echo "frame-compare not found. Pass --frame-compare-exe, e.g.:"
  echo "  build/macos-arm64/tests/tools/frame-compare/frame-compare"
  exit 2
fi
if [[ "$SKIP_CAPTURE" -eq 0 && ! -x "$EXE" ]]; then
  echo "GL host not found: $EXE"
  echo "Pass --exe, or use --skip-capture to compare an existing capture directory."
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

if [[ "$SKIP_CAPTURE" -eq 0 && ! -f "$WAV" ]]; then
  echo "WAV not found: $WAV"
  exit 2
fi

CAPTURE_IDS=()
CAPTURE_FILES=()
while IFS= read -r obj; do
  [[ -z "$obj" ]] && continue
  id="$(json_field_text "$obj" id)"
  file="$(json_field_text "$obj" file)"
  [[ -z "$id" || -z "$file" ]] && continue
  CAPTURE_IDS+=("$id")
  CAPTURE_FILES+=("$file")
done <<< "$(json_object_list "$(json_array_body "$CAPTURE_SET_FLAT" presets)")"

SELECTED_IDS=()
SELECTED_FILES=()
IFS=',' read -r -a REQUESTED <<< "$PRESETS"
for index in "${!CAPTURE_IDS[@]}"; do
  for want in "${REQUESTED[@]}"; do
    if [[ "${CAPTURE_IDS[$index]}" == "$want" ]]; then
      SELECTED_IDS+=("${CAPTURE_IDS[$index]}")
      SELECTED_FILES+=("${CAPTURE_FILES[$index]}")
      break
    fi
  done
done

if [[ ${#SELECTED_IDS[@]} -eq 0 ]]; then
  echo "--presets matched no capture ids. See --help." >&2
  exit 2
fi

mkdir -p "$OUT"

SAMPLE_FRAMES_ARG="$(IFS=','; echo "${SAMPLE_FRAME_LIST[*]}")"

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

sampled_frames_incomplete() {
  local capture_out_dir="$1"
  local missing=()
  local frame frame_name path
  for frame in "${SAMPLE_FRAME_LIST[@]}"; do
    frame_name="$(printf 'frame_%04d' "$frame")"
    path="$capture_out_dir/$frame_name.png"
    if [[ ! -f "$path" ]]; then
      missing+=("$frame_name.png (missing)")
    elif [[ ! -s "$path" ]]; then
      missing+=("$frame_name.png (empty)")
    fi
  done
  if [[ ${#missing[@]} -eq 0 ]]; then
    return 1
  fi
  printf '%s' "$(IFS=', '; echo "${missing[*]}")"
  return 0
}

CAPTURE_FAILED=0

for index in "${!SELECTED_IDS[@]}"; do
  id="${SELECTED_IDS[$index]}"
  file="${SELECTED_FILES[$index]}"
  capture_out="$OUT/$id"

  echo "[$id] frames=$FRAMES save-frames=$SAMPLE_FRAMES_ARG"

  if [[ "$SKIP_CAPTURE" -eq 0 ]]; then
    rm -rf "$capture_out"
    mkdir -p "$capture_out"

    result="$(invoke_capture_process_once "$EXE" "$CAPTURE_TIMEOUT_SECONDS" \
      "$capture_out/_process.log.stdout.txt" "$capture_out/_process.log.stderr.txt" \
      --capture "$PRESET_ROOT/$file" "$WAV" --frames "$FRAMES" --out "$capture_out" \
      --save-frames "$SAMPLE_FRAMES_ARG" --no-json)"
    IFS=$'\t' read -r outcome exit_code wall_seconds <<< "$result"

    printf '(stdout/stderr redirected to %s / %s)\n' \
      "$capture_out/_process.log.stdout.txt" "$capture_out/_process.log.stderr.txt" \
      > "$capture_out/_process.log"

    if [[ "$outcome" == "timeout" ]]; then
      echo "  [TIMEOUT] capture did not finish within ${CAPTURE_TIMEOUT_SECONDS}s"
      CAPTURE_FAILED=1
      continue
    fi

    echo "  exitcode=$exit_code wall=${wall_seconds}s (informational only -- see EXIT CODE)"
  else
    if [[ ! -d "$capture_out" ]]; then
      echo "Error: --skip-capture given but $capture_out does not exist"
      exit 2
    fi
  fi

  incomplete="$(sampled_frames_incomplete "$capture_out" || true)"
  if [[ -n "$incomplete" ]]; then
    CAPTURE_FAILED=1
    echo "  INCOMPLETE -- missing/empty sampled frames: $incomplete"
  fi
done

visual_args=(visual --golden "$REFERENCE" --actual "$OUT" --frame-compare "$FRAME_COMPARE_EXE" --out "$OUT")
for id in "${SELECTED_IDS[@]}"; do
  visual_args+=(--capture "$id")
done

set +e
"$STATE_COMPARE_EXE" "${visual_args[@]}"
VISUAL_EXIT=$?
set -e

echo
echo "Report:  $OUT/report.md"

if [[ "$VISUAL_EXIT" -ne 0 ]]; then
  exit "$VISUAL_EXIT"
fi
if [[ "$CAPTURE_FAILED" -eq 1 ]]; then
  exit 1
fi
exit 0