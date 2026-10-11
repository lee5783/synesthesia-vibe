#!/usr/bin/env bash
#
#  capture-screenshots-ios-sim.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

print_help() {
  cat <<'EOF'
capture-screenshots-ios-sim.sh -- screenshots/capture-standard.md capture runner for the iOS simulator

USAGE

  Drives the UIKit (or SwiftUI) sample's --capture launch-argument mode on an iOS simulator
  through xcrun simctl: boots the simulator, installs the app, launches it once per preset
  (blocking until the app exits), pulls Documents/<out>/<preset-id> from the app's data
  container and writes one <frame>.sidecar.json per saved PNG, per
  screenshots/capture-standard.md's schema plus the iOS fields listed below. The app's own
  frame_NNNN.json render-state dumps are pulled too, so the output directory can be fed to
  state-compare directly.

    ./capture-screenshots-ios-sim.sh --out <dir> [--app <.app>] [--bundle-id <id>] \
        [--device <udid|name>] [--capture-set <capture-set.json>] [--presets id1,id2,...] \
        [--sample-frames N,N,...] [--build <id>] [--platform <id>] [--renderer <id>] \
        [--capture-timeout-seconds N] [--skip-install] [--non-deterministic]

PARAMETERS

  --out                    Directory this run writes <preset-id>/frame_NNNN.png,
                           <preset-id>/frame_NNNN.json and <preset-id>/frame_NNNN.sidecar.json
                           into. Required.
  --app                    The simulator .app to install. Default:
                           build/verify-003-dd/Build/Products/Debug-iphonesimulator/SynesthesiaVibeUIKit.app
  --bundle-id              Default: CFBundleIdentifier read from --app's Info.plist
                           (com.lee5783.synesthesiavibe.uikit for the default app).
  --device                 Simulator UDID or name. Default: "iPhone 17". Booted with
                           xcrun simctl boot when it is not already booted.
  --capture-set            Path to the checked-in capture-set definition. Default:
                           resources/beatdrop/capture-set.json (repo-root relative). Source of
                           truth for the preset ids and the WAV recorded in the sidecar; the
                           app bundles the same WAV and resolves the same ids itself.
  --presets                Comma-separated capture ids to run. Default: every preset in
                           --capture-set.
  --sample-frames          Comma-separated frame numbers to save and record. Default:
                           0,30,60,150,300,450,599. Pass 0,1,...,29 for a numeric-smoke run.
  --build                  Sidecar "build" field. Default: the --app path's own build-dir
                           segment (build/<this>/...), or "unknown".
  --platform               Sidecar "platform" field. Default: ios-simulator-<host arch>.
  --renderer               Sidecar "renderer" field. Default: ios-sim-gles.
  --capture-timeout-seconds
                           Per-preset wall-clock budget, in seconds. Default: 600.
  --skip-install           Do not run xcrun simctl install (the app is already installed).
  --non-deterministic      Mark every sidecar "deterministic": false instead of true.

SIDECAR FIELDS BEYOND THE STANDARD SCHEMA

  "renderer"               --renderer.
  "device"                 The simulator's name and UDID.
  "runtime"                The simulator runtime (e.g. "iOS 26.5").
  "gl_renderer"            GL_RENDERER from the core's one-time stderr line
                           ("render_gl: audio textures ... GL_RENDERER \"...\""), C-020.
  "audio_texture_format"   R32F or R16F from the same line.

EXIT CODE

  0  every selected preset printed CAPTURE OK and wrote all sampled frames and their sidecars.
  1  a capture timed out, printed no CAPTURE OK line, a sampled frame PNG came back
     missing/empty, or the PNG's IHDR dimensions were not 838x441.
  2  usage error, or a required tool/capture-set/WAV/app path or the simulator was absent.

xcrun simctl launch --console-pty returns 0 even when the app exits 1, so the verdict is the
app's "CAPTURE OK" console line plus the pulled files, never the launch exit code.
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

CAPTURE_SET="$REPO_ROOT/resources/beatdrop/capture-set.json"
OUT=""
APP="$REPO_ROOT/build/verify-003-dd/Build/Products/Debug-iphonesimulator/SynesthesiaVibeUIKit.app"
BUNDLE_ID=""
DEVICE="iPhone 17"
PRESETS=""
SAMPLE_FRAMES="0,30,60,150,300,450,599"
BUILD_ID=""
PLATFORM_ID=""
RENDERER_ID="ios-sim-gles"
CAPTURE_TIMEOUT_SECONDS=600
SKIP_INSTALL=false
DETERMINISTIC=true
EXPECTED_WIDTH=838
EXPECTED_HEIGHT=441

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --app) APP="$2"; shift 2 ;;
    --bundle-id) BUNDLE_ID="$2"; shift 2 ;;
    --device) DEVICE="$2"; shift 2 ;;
    --capture-set) CAPTURE_SET="$2"; shift 2 ;;
    --presets) PRESETS="$2"; shift 2 ;;
    --sample-frames) SAMPLE_FRAMES="$2"; shift 2 ;;
    --build) BUILD_ID="$2"; shift 2 ;;
    --platform) PLATFORM_ID="$2"; shift 2 ;;
    --renderer) RENDERER_ID="$2"; shift 2 ;;
    --capture-timeout-seconds) CAPTURE_TIMEOUT_SECONDS="$2"; shift 2 ;;
    --skip-install) SKIP_INSTALL=true; shift ;;
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
if ! [[ "$CAPTURE_TIMEOUT_SECONDS" =~ ^[1-9][0-9]*$ ]]; then
  echo "--capture-timeout-seconds must be a positive integer. See --help." >&2
  exit 2
fi
if ! command -v xcrun >/dev/null 2>&1; then
  echo "xcrun not found. Install Xcode and its command-line tools."
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
APP="$(resolve_repo_path "$APP")"

if [[ ! -f "$CAPTURE_SET" ]]; then
  echo "Capture set not found: $CAPTURE_SET"
  exit 2
fi
if [[ ! -f "$APP/Info.plist" ]]; then
  echo "Simulator app not found: $APP"
  echo "Pass --app, or build it first."
  exit 2
fi
if [[ -z "$BUNDLE_ID" ]]; then
  BUNDLE_ID="$(plutil -extract CFBundleIdentifier raw -o - "$APP/Info.plist" 2>/dev/null || true)"
  if [[ -z "$BUNDLE_ID" ]]; then
    echo "Could not read CFBundleIdentifier from $APP/Info.plist. Pass --bundle-id." >&2
    exit 2
  fi
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

CAPTURE_SET_FLAT="$(tr -d '\n' < "$CAPTURE_SET")"
WAV="$(resolve_repo_path "$(json_field_text "$CAPTURE_SET_FLAT" wav)")"

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
  SAMPLE_FRAME_LIST+=("$trimmed")
done
if [[ ${#SAMPLE_FRAME_LIST[@]} -eq 0 ]]; then
  echo "--sample-frames parsed to an empty frame list. See --help." >&2
  exit 2
fi
SAMPLE_FRAMES_ARG="$(IFS=','; echo "${SAMPLE_FRAME_LIST[*]}")"

DEVICE_LINE="$(xcrun simctl list devices available | awk -v want="$DEVICE" '
  /\([0-9A-F]+-[0-9A-F-]+\)/ {
    line = $0
    name = line
    sub(/^[[:space:]]+/, "", name)
    sub(/ \([0-9A-F]+-[0-9A-F-]+\).*/, "", name)
    if (name == want || index(line, "(" want ")")) { print line; exit }
  }' || true)"
if [[ -z "$DEVICE_LINE" ]]; then
  echo "Simulator not found: $DEVICE"
  exit 2
fi
DEVICE_UDID="$(printf '%s\n' "$DEVICE_LINE" | grep -oE '[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}' | head -1)"
DEVICE_NAME="$(printf '%s\n' "$DEVICE_LINE" | sed -E 's/^[[:space:]]*(.*) \([0-9A-F-]{36}\).*/\1/')"
RUNTIME_NAME="$(xcrun simctl list devices available | awk -v udid="$DEVICE_UDID" '/^-- /{runtime=$0} index($0, udid){gsub(/^-- | --$/, "", runtime); print runtime; exit}')"

if ! printf '%s\n' "$DEVICE_LINE" | grep -q '(Booted)'; then
  echo "Booting $DEVICE_NAME ($DEVICE_UDID)"
  xcrun simctl boot "$DEVICE_UDID"
fi
xcrun simctl bootstatus "$DEVICE_UDID" -b >/dev/null

if [[ "$SKIP_INSTALL" != true ]]; then
  echo "Installing $APP"
  xcrun simctl install "$DEVICE_UDID" "$APP"
fi

DATA_CONTAINER="$(xcrun simctl get_app_container "$DEVICE_UDID" "$BUNDLE_ID" data 2>/dev/null || true)"
if [[ -z "$DATA_CONTAINER" || ! -d "$DATA_CONTAINER" ]]; then
  echo "$BUNDLE_ID is not installed on $DEVICE_UDID. Drop --skip-install, or install it first."
  exit 2
fi

if [[ -z "$BUILD_ID" ]]; then
  BUILD_ID="$(printf '%s\n' "$APP" | sed -n "s#.*/build/\([^/]*\)/.*#\1#p")"
  [[ -z "$BUILD_ID" ]] && BUILD_ID="unknown"
fi
if [[ -z "$PLATFORM_ID" ]]; then
  PLATFORM_ID="ios-simulator-$(uname -m)"
fi

GIT_DESCRIBE="$(cd "$REPO_ROOT" && git describe --always --dirty 2>/dev/null || echo unknown)"
case "$GIT_DESCRIBE" in
  *-dirty) GIT_DIRTY=true ;;
  *) GIT_DIRTY=false ;;
esac
GIT_REF="$(cd "$REPO_ROOT" && git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
HOST_NAME="$(basename "$APP")"
HOST_NAME="${HOST_NAME%.*}"
WAV_SHA256="$(shasum -a 256 "$WAV" | awk '{print $1}')"
WAV_REPO_RELATIVE="${WAV#$REPO_ROOT/}"
WAV_RATE=44100
WAV_CHANNELS=2

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
  local gl_renderer="$5"
  local audio_texture_format="$6"
  local frame_name sample_offset time_seconds

  frame_name="$(printf 'frame_%04d' "$frame")"
  sample_offset=$((frame * 735))
  time_seconds="$(awk -v f="$frame" 'BEGIN { printf "%.9g", f / 60.0 }')"

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
  "platform": "$(json_escape "$PLATFORM_ID")",
  "renderer": "$(json_escape "$RENDERER_ID")",
  "device": "$(json_escape "$DEVICE_NAME ($DEVICE_UDID)")",
  "runtime": "$(json_escape "$RUNTIME_NAME")",
  "gl_renderer": "$(json_escape "$gl_renderer")",
  "audio_texture_format": "$(json_escape "$audio_texture_format")",
  "git": "$(json_escape "$GIT_DESCRIBE")",
  "ref": "$(json_escape "$GIT_REF")",
  "dirty": $GIT_DIRTY,
  "seed": 20260927,
  "deterministic": $DETERMINISTIC
}
EOF
}

launch_capture_once() {
  local console_log="$1"
  shift

  local started_at
  started_at="$(date +%s)"
  xcrun simctl launch --console-pty --terminate-running-process "$DEVICE_UDID" "$BUNDLE_ID" "$@" \
    >"$console_log" 2>&1 </dev/null &
  local pid=$!

  local elapsed=0
  while kill -0 "$pid" 2>/dev/null; do
    if [[ "$elapsed" -ge "$CAPTURE_TIMEOUT_SECONDS" ]]; then
      xcrun simctl terminate "$DEVICE_UDID" "$BUNDLE_ID" >/dev/null 2>&1 || true
      kill -TERM "$pid" 2>/dev/null || true
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

echo "device=$DEVICE_NAME ($DEVICE_UDID) runtime=$RUNTIME_NAME bundle=$BUNDLE_ID"

CAPTURE_FAILED=0

for id in "${SELECTED_IDS[@]}"; do
  capture_out="$OUT/$id"
  container_out="$DATA_CONTAINER/Documents/capture/$id"

  echo "[$id] save-frames=$SAMPLE_FRAMES_ARG"

  rm -rf "$capture_out" "$container_out"
  mkdir -p "$capture_out"

  console_log="$capture_out/_process.log.console.txt"
  result="$(launch_capture_once "$console_log" \
    --capture "$id" --frames "$SAMPLE_FRAMES_ARG" --out "capture/$id")"
  IFS=$'\t' read -r outcome exit_code wall_seconds <<< "$result"

  if [[ "$outcome" == "timeout" ]]; then
    CAPTURE_FAILED=1
    echo "  TIMEOUT -- exceeded ${CAPTURE_TIMEOUT_SECONDS}s"
    continue
  fi
  echo "  launch exitcode=$exit_code wall=${wall_seconds}s (informational; CAPTURE OK and the pulled frames are the verdict)"

  if ! grep -q 'CAPTURE OK' "$console_log"; then
    CAPTURE_FAILED=1
    echo "  NO CAPTURE OK -- $(grep -m1 'CAPTURE FAILED' "$console_log" || echo 'no CAPTURE line; see _process.log.console.txt')"
    continue
  fi

  if [[ -d "$container_out" ]]; then
    cp -R "$container_out/." "$capture_out/"
  fi

  incomplete="$(sampled_frames_incomplete "$capture_out" || true)"
  if [[ -n "$incomplete" ]]; then
    CAPTURE_FAILED=1
    echo "  INCOMPLETE -- missing/empty sampled frames: $incomplete"
    continue
  fi

  renderer_line="$(grep -m1 'render_gl: audio textures' "$console_log" || true)"
  gl_renderer="$(printf '%s' "$renderer_line" | sed -n 's/.*GL_RENDERER "\([^"]*\)".*/\1/p')"
  audio_texture_format="$(printf '%s' "$renderer_line" | sed -n 's/.*audio textures \([A-Z0-9]*\).*/\1/p')"
  [[ -z "$gl_renderer" ]] && gl_renderer="unknown"
  [[ -z "$audio_texture_format" ]] && audio_texture_format="unknown"

  first_frame="${SAMPLE_FRAME_LIST[0]}"
  first_png="$capture_out/$(printf 'frame_%04d' "$first_frame").png"
  read -r measured_width measured_height < <(png_ihdr_dimensions "$first_png")

  if [[ "$measured_width" != "$EXPECTED_WIDTH" || "$measured_height" != "$EXPECTED_HEIGHT" ]]; then
    CAPTURE_FAILED=1
    echo "  DIMENSION MISMATCH -- $first_png is ${measured_width}x${measured_height}, expected ${EXPECTED_WIDTH}x${EXPECTED_HEIGHT}" >&2
    continue
  fi

  for frame in "${SAMPLE_FRAME_LIST[@]}"; do
    write_sidecar "$capture_out" "$frame" "$measured_width" "$measured_height" "$gl_renderer" "$audio_texture_format"
  done

  echo "  OK -- gl_renderer=\"$gl_renderer\" audio_textures=$audio_texture_format wrote ${#SAMPLE_FRAME_LIST[@]} frames"
done

if [[ "$CAPTURE_FAILED" -ne 0 ]]; then
  echo "One or more presets failed to produce every sampled frame." >&2
  exit 1
fi

echo
echo "Done: $OUT"
exit 0
