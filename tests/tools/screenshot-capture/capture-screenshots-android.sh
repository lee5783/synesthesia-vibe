#!/usr/bin/env bash
#
#  capture-screenshots-android.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-11.
#

set -euo pipefail

print_help() {
  cat <<'EOF'
capture-screenshots-android.sh -- screenshots/capture-standard.md capture runner for Android

USAGE

  Drives the samples/android app's capture intent extras on an Android emulator or device
  through adb: installs the APK, starts MainActivity once per preset with
  "-e capture <preset-id> -e frames <list> -e out capture/<preset-id>", polls for the app's
  "done" marker, pulls the output directory from the app's external files dir and writes one
  <frame>.sidecar.json per saved PNG, per screenshots/capture-standard.md's schema plus the
  Android fields listed below. The app's own frame_NNNN.json render-state dumps are pulled
  too, so the output directory can be fed to state-compare directly.

    ./capture-screenshots-android.sh --out <dir> [--apk <.apk>] [--serial <adb serial>] \
        [--package <id>] [--activity <name>] [--capture-set <capture-set.json>] \
        [--presets id1,id2,...] [--sample-frames N,N,...] [--build <id>] [--platform <id>] \
        [--renderer <id>] [--capture-timeout-seconds N] [--skip-install] [--non-deterministic]

PARAMETERS

  --out                    Directory this run writes <preset-id>/frame_NNNN.png,
                           <preset-id>/frame_NNNN.json, <preset-id>/gl-strings.txt and
                           <preset-id>/frame_NNNN.sidecar.json into. Required.
  --apk                    The APK to install with adb install -r. Default:
                           samples/android/app/build/outputs/apk/debug/app-debug.apk
                           (a Debug native library is unoptimised; a debug-signed
                           assembleRelease APK captures several times faster).
  --serial                 adb serial. Default: $ANDROID_SERIAL, else the only attached
                           device in "device" state.
  --package                Default: com.synesthesiavibe.sample.
  --activity               Default: .MainActivity.
  --capture-set            Path to the checked-in capture-set definition. Default:
                           resources/beatdrop/capture-set.json (repo-root relative). Source of
                           truth for the preset ids and the WAV recorded in the sidecar; the
                           app bundles the same WAV and resolves the same ids itself.
  --presets                Comma-separated capture ids to run. Default: every preset in
                           --capture-set.
  --sample-frames          Comma-separated frame numbers to save and record. Default:
                           0,30,60,150,300,450,599. Pass 0,1,...,29 for a numeric-smoke run.
  --build                  Sidecar "build" field. Default: android-<variant>, where
                           <variant> is the --apk path's outputs/apk/<variant>/ segment, or
                           "unknown".
  --platform               Sidecar "platform" field. Default: android-<ro.product.cpu.abi>.
  --renderer               Sidecar "renderer" field. Default: android-emu-gles.
  --capture-timeout-seconds
                           Per-preset wall-clock budget for the done marker, in seconds.
                           Default: 600.
  --skip-install           Do not run adb install (the app is already installed).
  --non-deterministic      Mark every sidecar "deterministic": false instead of true.

SIDECAR FIELDS BEYOND THE STANDARD SCHEMA

  "renderer"               --renderer.
  "device"                 ro.product.model and the adb serial.
  "abi"                    adb shell getprop ro.product.cpu.abi.
  "sdk_int"                adb shell getprop ro.build.version.sdk.
  "gl_renderer"            GL_RENDERER from the gl-strings.txt the library writes at init
                           (C-020).
  "gl_version"             GL_VERSION from the same file.
  "audio_texture_format"   AUDIO_TEXTURE_FORMAT (R32F or R16F) from the same file.

OUTPUTS PER PRESET

  _process.logcat.txt      logcat from the launch onwards, tags SynesthesiaVibeSample and
                           SynesthesiaVibe (the library's stderr), plus AndroidRuntime.
  done                     the app's marker: "CAPTURE OK files=N out=..." or
                           "CAPTURE FAILED <msg>".

EXIT CODE

  0  every selected preset wrote "CAPTURE OK" to its done marker and every sampled frame and
     sidecar exists.
  1  a capture timed out, its done marker said CAPTURE FAILED, a sampled frame PNG came back
     missing/empty, or the PNG's IHDR dimensions were not 838x441.
  2  usage error, or adb, the capture set, the WAV, the APK or the device was absent.

am start -W returns as soon as the activity is displayed, long before the capture ends, so the
verdict is the done marker plus the pulled files, never am's exit code.
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

CAPTURE_SET="$REPO_ROOT/resources/beatdrop/capture-set.json"
OUT=""
APK="$REPO_ROOT/samples/android/app/build/outputs/apk/debug/app-debug.apk"
SERIAL="${ANDROID_SERIAL:-}"
PACKAGE="com.synesthesiavibe.sample"
ACTIVITY=".MainActivity"
PRESETS=""
SAMPLE_FRAMES="0,30,60,150,300,450,599"
BUILD_ID=""
PLATFORM_ID=""
RENDERER_ID="android-emu-gles"
HOST_NAME="SynesthesiaVibeSample"
CAPTURE_TIMEOUT_SECONDS=600
SKIP_INSTALL=false
DETERMINISTIC=true
EXPECTED_WIDTH=838
EXPECTED_HEIGHT=441

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --apk) APK="$2"; shift 2 ;;
    --serial) SERIAL="$2"; shift 2 ;;
    --package) PACKAGE="$2"; shift 2 ;;
    --activity) ACTIVITY="$2"; shift 2 ;;
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
if ! command -v adb >/dev/null 2>&1; then
  echo "adb not found. Install the Android SDK platform-tools and put them on PATH."
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
APK="$(resolve_repo_path "$APK")"

if [[ ! -f "$CAPTURE_SET" ]]; then
  echo "Capture set not found: $CAPTURE_SET"
  exit 2
fi
if [[ "$SKIP_INSTALL" != true && ! -f "$APK" ]]; then
  echo "APK not found: $APK"
  echo "Pass --apk, or build it first (cd samples/android && ./gradlew :app:assembleDebug)."
  exit 2
fi

if [[ -z "$SERIAL" ]]; then
  ATTACHED=()
  while IFS= read -r line; do
    [[ -n "$line" ]] && ATTACHED+=("$line")
  done < <(adb devices | awk 'NR > 1 && $2 == "device" { print $1 }')
  if [[ ${#ATTACHED[@]} -ne 1 ]]; then
    echo "Expected exactly one attached device, found ${#ATTACHED[@]}. Pass --serial." >&2
    exit 2
  fi
  SERIAL="${ATTACHED[0]}"
fi

ADB=(adb -s "$SERIAL")

if [[ "$("${ADB[@]}" get-state 2>/dev/null || true)" != "device" ]]; then
  echo "Device not ready: $SERIAL"
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

device_prop() {
  "${ADB[@]}" shell getprop "$1" 2>/dev/null | tr -d '\r'
}

gl_string() {
  sed -n "s/^$2=//p" "$1" 2>/dev/null | head -1 | tr -d '\r'
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

DEVICE_ABI="$(device_prop ro.product.cpu.abi)"
DEVICE_SDK="$(device_prop ro.build.version.sdk)"
DEVICE_MODEL="$(device_prop ro.product.model)"
[[ -z "$DEVICE_ABI" ]] && DEVICE_ABI="unknown"
[[ -z "$DEVICE_SDK" ]] && DEVICE_SDK=0

if [[ "$SKIP_INSTALL" != true ]]; then
  echo "Installing $APK"
  "${ADB[@]}" install -r "$APK"
fi

if ! "${ADB[@]}" shell pm path "$PACKAGE" 2>/dev/null | grep -q '^package:'; then
  echo "$PACKAGE is not installed on $SERIAL. Drop --skip-install, or install it first."
  exit 2
fi

if [[ -z "$BUILD_ID" ]]; then
  variant="$(printf '%s\n' "$APK" | sed -n "s#.*/outputs/apk/\([^/]*\)/.*#\1#p")"
  if [[ -n "$variant" ]]; then
    BUILD_ID="android-$variant"
  else
    BUILD_ID="unknown"
  fi
fi
if [[ -z "$PLATFORM_ID" ]]; then
  PLATFORM_ID="android-$DEVICE_ABI"
fi

GIT_DESCRIBE="$(cd "$REPO_ROOT" && git describe --always --dirty 2>/dev/null || echo unknown)"
case "$GIT_DESCRIBE" in
  *-dirty) GIT_DIRTY=true ;;
  *) GIT_DIRTY=false ;;
esac
GIT_REF="$(cd "$REPO_ROOT" && git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
WAV_SHA256="$(shasum -a 256 "$WAV" | awk '{print $1}')"
WAV_REPO_RELATIVE="${WAV#$REPO_ROOT/}"
WAV_RATE=44100
WAV_CHANNELS=2
REMOTE_BASE="/sdcard/Android/data/$PACKAGE/files"

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
  local gl_version="$6"
  local audio_texture_format="$7"
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
  "device": "$(json_escape "$DEVICE_MODEL ($SERIAL)")",
  "abi": "$(json_escape "$DEVICE_ABI")",
  "sdk_int": $DEVICE_SDK,
  "gl_renderer": "$(json_escape "$gl_renderer")",
  "gl_version": "$(json_escape "$gl_version")",
  "audio_texture_format": "$(json_escape "$audio_texture_format")",
  "git": "$(json_escape "$GIT_DESCRIBE")",
  "ref": "$(json_escape "$GIT_REF")",
  "dirty": $GIT_DIRTY,
  "seed": 20260927,
  "deterministic": $DETERMINISTIC
}
EOF
}

wait_for_done_marker() {
  local remote_done="$1"
  local started_at
  started_at="$(date +%s)"
  local elapsed=0
  while [[ "$elapsed" -lt "$CAPTURE_TIMEOUT_SECONDS" ]]; do
    if "${ADB[@]}" shell test -f "$remote_done" 2>/dev/null; then
      echo "done	$(( $(date +%s) - started_at ))"
      return 0
    fi
    sleep 2
    elapsed=$(( $(date +%s) - started_at ))
  done
  echo "timeout	$elapsed"
  return 0
}

echo "device=$DEVICE_MODEL ($SERIAL) abi=$DEVICE_ABI sdk=$DEVICE_SDK package=$PACKAGE build=$BUILD_ID"

CAPTURE_FAILED=0

for id in "${SELECTED_IDS[@]}"; do
  capture_out="$OUT/$id"
  device_out="capture/$id"
  remote_out="$REMOTE_BASE/$device_out"

  echo "[$id] save-frames=$SAMPLE_FRAMES_ARG"

  rm -rf "$capture_out"
  mkdir -p "$capture_out"
  "${ADB[@]}" shell rm -rf "$remote_out" >/dev/null 2>&1 || true

  log_since="$("${ADB[@]}" shell date +%s 2>/dev/null | tr -d '\r')"
  "${ADB[@]}" shell am start -S -W -n "$PACKAGE/$ACTIVITY" \
    -e capture "$id" -e frames "$SAMPLE_FRAMES_ARG" -e out "$device_out" >"$capture_out/_process.am-start.txt" 2>&1 || true

  result="$(wait_for_done_marker "$remote_out/done")"
  IFS=$'\t' read -r outcome wall_seconds <<< "$result"

  "${ADB[@]}" logcat -d -v threadtime -T "${log_since}.000" \
    SynesthesiaVibeSample:V SynesthesiaVibe:V AndroidRuntime:E '*:S' >"$capture_out/_process.logcat.txt" 2>&1 || true

  if [[ "$outcome" == "timeout" ]]; then
    CAPTURE_FAILED=1
    "${ADB[@]}" shell am force-stop "$PACKAGE" >/dev/null 2>&1 || true
    echo "  TIMEOUT -- no done marker after ${CAPTURE_TIMEOUT_SECONDS}s"
    continue
  fi

  pull_dir="$OUT/.pull-$id"
  rm -rf "$pull_dir"
  mkdir -p "$pull_dir"
  "${ADB[@]}" pull "$remote_out" "$pull_dir" >/dev/null
  cp -R "$pull_dir/$(basename "$remote_out")/." "$capture_out/"
  rm -rf "$pull_dir"

  done_line="$(head -1 "$capture_out/done" 2>/dev/null | tr -d '\r')"
  echo "  done after ${wall_seconds}s -- $done_line"
  if [[ "$done_line" != "CAPTURE OK"* ]]; then
    CAPTURE_FAILED=1
    echo "  NO CAPTURE OK -- see $capture_out/_process.logcat.txt"
    continue
  fi

  incomplete="$(sampled_frames_incomplete "$capture_out" || true)"
  if [[ -n "$incomplete" ]]; then
    CAPTURE_FAILED=1
    echo "  INCOMPLETE -- missing/empty sampled frames: $incomplete"
    continue
  fi

  gl_renderer="$(gl_string "$capture_out/gl-strings.txt" GL_RENDERER)"
  gl_version="$(gl_string "$capture_out/gl-strings.txt" GL_VERSION)"
  audio_texture_format="$(gl_string "$capture_out/gl-strings.txt" AUDIO_TEXTURE_FORMAT)"
  [[ -z "$gl_renderer" ]] && gl_renderer="unknown"
  [[ -z "$gl_version" ]] && gl_version="unknown"
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
    write_sidecar "$capture_out" "$frame" "$measured_width" "$measured_height" "$gl_renderer" "$gl_version" "$audio_texture_format"
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
