#!/usr/bin/env bash
#
#  capture-screenshots-macos-app.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

print_help() {
  cat <<'EOF'
capture-screenshots-macos-app.sh -- screenshots/capture-standard.md runner for the macOS AppKit sample

USAGE

  Thin wrapper around the AppKit sample's own --capture launch-argument mode
  (samples/macos/appkit/Sources/CaptureCommand.m: offscreen OpenGL 3.2 Core context +
  SVCaptureSession, no window). Same CLI shape as capture-screenshots.sh, with --app in place
  of --exe. This script only invokes the app and writes one <frame>.sidecar.json per saved PNG,
  per screenshots/capture-standard.md's schema, plus a "renderer" field.

    ./capture-screenshots-macos-app.sh --out <dir> [--app <path to .app>] \
        [--capture-set <capture-set.json>] [--presets id1,id2,...] [--sample-frames N,N,...] \
        [--frames N] [--wav <path>] [--build <id>] [--platform <id>] [--renderer <id>] \
        [--capture-timeout-seconds N] [--non-deterministic]

PARAMETERS

  --out                    Directory this run writes <preset-id>/frame_NNNN.png,
                           <preset-id>/frame_NNNN.json (the sample's sv_dump_frame_state dump,
                           consumed by state-compare) and <preset-id>/frame_NNNN.sidecar.json
                           into. Required. Ad hoc runs belong under screenshots/local/.
  --app                    The SynesthesiaVibeAppKit.app bundle. Default:
                           build/apple-006-dd/Build/Products/Debug/SynesthesiaVibeAppKit.app
                           (xcodebuild -workspace samples/apple.xcworkspace -scheme
                           SynesthesiaVibeAppKit -destination 'platform=macOS'
                           -derivedDataPath build/apple-006-dd build).
  --capture-set            Path to the checked-in capture-set definition. Default:
                           resources/beatdrop/capture-set.json (repo-root relative). Source of
                           truth for the preset ids and the default WAV. The app resolves each
                           id from its bundled capture-presets/ copy of the same files.
  --presets                Comma-separated capture ids to run. Default: every preset in
                           --capture-set.
  --sample-frames          Comma-separated frame numbers to save and record. Default:
                           0,30,60,150,300,450,599 (screenshots/capture-standard.md's fixed
                           frame list).
  --frames                 Render count, kept for CLI parity with capture-screenshots.sh. The
                           app renders through the highest sample frame; this is only checked
                           to be greater than it. Default: 600.
  --wav                    WAV path, repo-root relative. Default: --capture-set's "wav" field.
  --build                  Sidecar "build" field. Default: the --app path's own build-dir
                           segment (build/<this>/...), or "unknown".
  --platform               Sidecar "platform" field. Default: auto-detected (uname -s/-m).
  --renderer               Sidecar "renderer" field. Default: macos-appkit-gl.
  --capture-timeout-seconds
                           Per-preset wall-clock budget, in seconds. Default: 700.
  --non-deterministic      Mark every sidecar "deterministic": false instead of true. The
                           sample's --capture mode is fixed-step and WAV-fed, so this is not
                           needed for a normal run.

EXIT CODE

  0  every selected preset wrote all sampled frames and their sidecars.
  1  a capture timed out, a sampled frame PNG came back missing/empty, or the captured PNG's
     IHDR dimensions were not 838x441.
  2  usage error, or a required tool/capture-set/WAV/app path was absent.

The "width"/"height" sidecar fields are read back from the saved PNG's own IHDR header (not
asserted), and the "git" field records `git describe --always --dirty` with "dirty" as its own
boolean -- see screenshots/capture-standard.md's schema table. "audio.rate"/"audio.channels"
are read from the WAV's canonical fmt header (44100/2 when it is not canonical).
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

CAPTURE_SET="$REPO_ROOT/resources/beatdrop/capture-set.json"
OUT=""
APP="$REPO_ROOT/build/apple-006-dd/Build/Products/Debug/SynesthesiaVibeAppKit.app"
PRESETS=""
SAMPLE_FRAMES="0,30,60,150,300,450,599"
FRAMES=600
WAV_OVERRIDE=""
BUILD_ID=""
PLATFORM_ID=""
RENDERER_ID="macos-appkit-gl"
CAPTURE_TIMEOUT_SECONDS=700
DETERMINISTIC=true
EXPECTED_WIDTH=838
EXPECTED_HEIGHT=441

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --app) APP="$2"; shift 2 ;;
    --capture-set) CAPTURE_SET="$2"; shift 2 ;;
    --presets) PRESETS="$2"; shift 2 ;;
    --sample-frames) SAMPLE_FRAMES="$2"; shift 2 ;;
    --frames) FRAMES="$2"; shift 2 ;;
    --wav) WAV_OVERRIDE="$2"; shift 2 ;;
    --build) BUILD_ID="$2"; shift 2 ;;
    --platform) PLATFORM_ID="$2"; shift 2 ;;
    --renderer) RENDERER_ID="$2"; shift 2 ;;
    --capture-timeout-seconds) CAPTURE_TIMEOUT_SECONDS="$2"; shift 2 ;;
    --non-deterministic) DETERMINISTIC=false; shift ;;
    --help|-h) print_help; exit 0 ;;
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
OUT="$(resolve_repo_path "$OUT")"
APP="$(resolve_repo_path "${APP%/}")"
EXE="$APP/Contents/MacOS/$(basename "$APP" .app)"

if [[ ! -f "$CAPTURE_SET" ]]; then
  echo "Capture set not found: $CAPTURE_SET"
  exit 2
fi
if [[ ! -x "$EXE" ]]; then
  echo "AppKit sample not found: $EXE"
  echo "Pass --app, or build it first (see --help)."
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

json_escape() {
  local escaped="$1"
  escaped="${escaped//\\/\\\\}"
  escaped="${escaped//\"/\\\"}"
  printf '%s' "$escaped"
}

png_ihdr_dimensions() {
  local b0 b1 b2 b3 b4 b5 b6 b7
  { read -r b0 b1 b2 b3 b4 b5 b6 b7; } < <(od -An -v -tu1 -j16 -N8 "$1")
  printf '%d %d\n' \
    $((b0 * 16777216 + b1 * 65536 + b2 * 256 + b3)) \
    $((b4 * 16777216 + b5 * 65536 + b6 * 256 + b7))
}

wav_rate_and_channels() {
  local fmt_tag c0 c1 r0 r1 r2 r3
  fmt_tag="$(od -An -v -c -j12 -N4 "$1" | tr -d ' ')"
  if [[ "$fmt_tag" != "fmt" ]]; then
    printf '44100 2\n'
    return
  fi
  { read -r c0 c1 r0 r1 r2 r3; } < <(od -An -v -tu1 -j22 -N6 "$1")
  printf '%d %d\n' $((r0 + r1 * 256 + r2 * 65536 + r3 * 16777216)) $((c0 + c1 * 256))
}

CAPTURE_SET_FLAT="$(tr -d '\n' < "$CAPTURE_SET")"
WAV="${WAV_OVERRIDE:-$(json_field_text "$CAPTURE_SET_FLAT" wav)}"
WAV="$(resolve_repo_path "$WAV")"

if [[ ! -f "$WAV" ]]; then
  echo "WAV not found: $WAV"
  exit 2
fi

CAPTURE_IDS=()
while IFS= read -r obj; do
  [[ -z "$obj" ]] && continue
  id="$(json_field_text "$obj" id)"
  [[ -z "$id" ]] && continue
  CAPTURE_IDS+=("$id")
done <<< "$(json_object_list "$(json_array_body "$CAPTURE_SET_FLAT" presets)")"

SELECTED_IDS=()
if [[ -z "$PRESETS" ]]; then
  SELECTED_IDS=("${CAPTURE_IDS[@]}")
else
  IFS=',' read -r -a REQUESTED <<< "$PRESETS"
  for id in "${CAPTURE_IDS[@]}"; do
    for want in "${REQUESTED[@]}"; do
      if [[ "$id" == "$want" ]]; then
        SELECTED_IDS+=("$id")
        break
      fi
    done
  done
fi

if [[ ${#SELECTED_IDS[@]} -eq 0 ]]; then
  echo "--presets matched no capture ids. See --help." >&2
  exit 2
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
  if (( 10#$trimmed >= FRAMES )); then
    echo "--sample-frames entry $trimmed is not below --frames $FRAMES. See --help." >&2
    exit 2
  fi
  SAMPLE_FRAME_LIST+=("$((10#$trimmed))")
done
if [[ ${#SAMPLE_FRAME_LIST[@]} -eq 0 ]]; then
  echo "--sample-frames parsed to an empty frame list. See --help." >&2
  exit 2
fi
SAMPLE_FRAMES_ARG="$(IFS=','; echo "${SAMPLE_FRAME_LIST[*]}")"

if [[ -z "$BUILD_ID" ]]; then
  BUILD_ID="$(printf '%s\n' "$APP" | sed -n "s#.*/build/\([^/]*\)/.*#\1#p")"
  [[ -z "$BUILD_ID" ]] && BUILD_ID="unknown"
fi

if [[ -z "$PLATFORM_ID" ]]; then
  OS_NAME="$(uname -s)"
  ARCH_NAME="$(uname -m)"
  case "$OS_NAME" in
    Darwin) PLATFORM_ID="macos-$ARCH_NAME" ;;
    *) PLATFORM_ID="$(printf '%s' "$OS_NAME" | tr '[:upper:]' '[:lower:]')-$ARCH_NAME" ;;
  esac
fi

GIT_DESCRIBE="$(cd "$REPO_ROOT" && git describe --always --dirty 2>/dev/null || echo unknown)"
case "$GIT_DESCRIBE" in
  *-dirty) GIT_DIRTY=true ;;
  *) GIT_DIRTY=false ;;
esac
GIT_REF="$(cd "$REPO_ROOT" && git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
HOST_NAME="$(basename "$EXE")"
WAV_SHA256="$(shasum -a 256 "$WAV" | awk '{print $1}')"
WAV_REPO_RELATIVE="${WAV#$REPO_ROOT/}"
read -r WAV_RATE WAV_CHANNELS < <(wav_rate_and_channels "$WAV")

mkdir -p "$OUT"

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

write_sidecar() {
  local capture_out_dir="$1"
  local frame="$2"
  local width="$3"
  local height="$4"
  local frame_name sample_offset time_seconds deterministic_json

  frame_name="$(printf 'frame_%04d' "$frame")"
  sample_offset=$((frame * 735))
  time_seconds="$(awk -v f="$frame" 'BEGIN { printf "%.9g", f / 60.0 }')"
  deterministic_json="$DETERMINISTIC"

  cat > "$capture_out_dir/$frame_name.sidecar.json" <<EOF
{
  "frame": $frame,
  "width": $width,
  "height": $height,
  "time_seconds": $time_seconds,
  "audio": {
    "path": "$(json_escape "$WAV_REPO_RELATIVE")",
    "sha256": "$WAV_SHA256",
    "rate": $WAV_RATE,
    "channels": $WAV_CHANNELS,
    "sample_offset": $sample_offset
  },
  "build": "$(json_escape "$BUILD_ID")",
  "host": "$(json_escape "$HOST_NAME")",
  "renderer": "$(json_escape "$RENDERER_ID")",
  "platform": "$(json_escape "$PLATFORM_ID")",
  "git": "$(json_escape "$GIT_DESCRIBE")",
  "ref": "$(json_escape "$GIT_REF")",
  "dirty": $GIT_DIRTY,
  "seed": 20260927,
  "deterministic": $deterministic_json
}
EOF
}

CAPTURE_FAILED=0

for id in "${SELECTED_IDS[@]}"; do
  capture_out="$OUT/$id"

  echo "[$id] save-frames=$SAMPLE_FRAMES_ARG"

  rm -rf "$capture_out"
  mkdir -p "$capture_out"

  stdout_log="$capture_out/_process.log.stdout.txt"
  stderr_log="$capture_out/_process.log.stderr.txt"

  set +e
  timeout_bin="timeout"
  command -v "$timeout_bin" >/dev/null 2>&1 || timeout_bin="gtimeout"
  if command -v "$timeout_bin" >/dev/null 2>&1; then
    ( cd "$REPO_ROOT" && exec "$timeout_bin" "${CAPTURE_TIMEOUT_SECONDS}s" "$EXE" \
      --capture "$id" --wav "$WAV" --frames "$SAMPLE_FRAMES_ARG" --out "$capture_out" \
    ) >"$stdout_log" 2>"$stderr_log"
  else
    ( cd "$REPO_ROOT" && exec "$EXE" \
      --capture "$id" --wav "$WAV" --frames "$SAMPLE_FRAMES_ARG" --out "$capture_out" \
    ) >"$stdout_log" 2>"$stderr_log"
  fi
  exit_code=$?
  set -e

  echo "  exitcode=$exit_code (informational; sampled-frame completeness below is the verdict)"

  incomplete="$(sampled_frames_incomplete "$capture_out" || true)"
  if [[ -n "$incomplete" ]]; then
    CAPTURE_FAILED=1
    echo "  INCOMPLETE -- missing/empty sampled frames: $incomplete"
    continue
  fi

  first_frame="${SAMPLE_FRAME_LIST[0]}"
  first_png="$capture_out/$(printf 'frame_%04d' "$first_frame").png"
  read -r measured_width measured_height < <(png_ihdr_dimensions "$first_png")

  if [[ "$measured_width" != "$EXPECTED_WIDTH" || "$measured_height" != "$EXPECTED_HEIGHT" ]]; then
    CAPTURE_FAILED=1
    echo "  DIMENSION MISMATCH -- $first_png is ${measured_width}x${measured_height}, expected ${EXPECTED_WIDTH}x${EXPECTED_HEIGHT}" >&2
    continue
  fi

  for frame in "${SAMPLE_FRAME_LIST[@]}"; do
    write_sidecar "$capture_out" "$frame" "$measured_width" "$measured_height"
  done

  echo "  OK -- wrote $capture_out/{${SAMPLE_FRAMES_ARG//,/,}}.{png,json,sidecar.json}"
done

if [[ "$CAPTURE_FAILED" -ne 0 ]]; then
  echo "One or more presets failed to produce every sampled frame." >&2
  exit 1
fi

echo
echo "Done: $OUT"
exit 0
