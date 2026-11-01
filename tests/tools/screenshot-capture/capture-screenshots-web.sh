#!/usr/bin/env bash
#
#  capture-screenshots-web.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-12.
#

set -euo pipefail

print_help() {
  cat <<'EOF'
capture-screenshots-web.sh -- screenshots/capture-standard.md capture runner for the web sample

USAGE

  Drives the samples/web page's capture mode in Google Chrome: starts web-capture-server.py
  (serves --site and accepts the page's uploads), then launches one Chrome per preset on
  "?capture=<preset-id>&frames=<list>&out=<preset-id>&upload=http://127.0.0.1:<port>/upload",
  polls for the page's "done" marker, kills that Chrome by PID and writes one
  <frame>.sidecar.json per saved PNG, per screenshots/capture-standard.md's schema plus the web
  fields listed below. The page's own frame_NNNN.json render-state dumps are uploaded too, so
  the output directory can be fed to state-compare directly.

    ./capture-screenshots-web.sh --out <dir> [--site <dir>] [--chrome <path>] \
        [--capture-set <capture-set.json>] [--presets id1,id2,...] [--sample-frames N,N,...] \
        [--frames N] [--port N] [--build <id>] [--platform <id>] [--renderer <id>] \
        [--capture-timeout-seconds N] [--headed] [--non-deterministic]

PARAMETERS

  --out                    Directory this run writes <preset-id>/frame_NNNN.png,
                           <preset-id>/frame_NNNN.json, <preset-id>/gl-strings.txt and
                           <preset-id>/frame_NNNN.sidecar.json into. Required.
  --site                   The assembled web sample. Default: build/samples/web (build it with
                           .github/scripts/build-web-sample.sh).
  --chrome                 Chrome executable. Default: $CHROME, else
                           /Applications/Google Chrome.app/Contents/MacOS/Google Chrome, else
                           google-chrome / chromium on PATH.
  --capture-set            Path to the checked-in capture-set definition. Default:
                           resources/beatdrop/capture-set.json (repo-root relative). Source of
                           truth for the preset ids and the WAV recorded in the sidecar; the
                           site bundles the same WAV and resolves the same ids itself. Its
                           "blends" entries are not run (the page captures a single preset).
  --presets                Comma-separated capture ids to run. Default: every preset in
                           --capture-set.
  --sample-frames          Comma-separated frame numbers to save and record. Default:
                           0,30,60,150,300,450,599. Pass 0,1,...,29 for a numeric-smoke run.
  --frames                 Render count, kept for CLI parity with capture-screenshots.sh. The
                           page renders through the highest sample frame; this is only checked
                           to be greater than it. Default: 600.
  --port                   Port for web-capture-server.py on 127.0.0.1. Default: 8090.
  --build                  Sidecar "build" field. Default: web-wasm32.
  --platform               Sidecar "platform" field. Default: web-wasm32.
  --renderer               Sidecar "renderer" field. Default: web-chrome-swiftshader, or
                           web-chrome-<gl_renderer, kebab-cased> with --headed, or
                           web-chrome-gpu when only the masked "WebKit WebGL" is known.
  --capture-timeout-seconds
                           Per-preset wall-clock budget for the done marker, in seconds.
                           Default: 600.
  --headed                 Run Chrome with a window on the host GPU instead of
                           --headless=new on SwiftShader.
  --non-deterministic      Mark every sidecar "deterministic": false instead of true.

SIDECAR FIELDS BEYOND THE STANDARD SCHEMA

  "renderer"               --renderer.
  "browser"                The --chrome executable's --version line.
  "gl_renderer"            UNMASKED_RENDERER from the gl-strings.txt the page uploads when
                           present, else GL_RENDERER (WebGL masks it as "WebKit WebGL").
  "gl_version"             GL_VERSION from the same file.
  "audio_texture_format"   AUDIO_TEXTURE_FORMAT (R32F or R16F) from the same file.

OUTPUTS

  _server.log              web-capture-server.py's request log for the whole run.
  <preset-id>/_process.chrome.txt
                           Chrome's stderr (--enable-logging=stderr, so the page's console
                           lines are in it).
  <preset-id>/done         the page's marker: "CAPTURE OK files=N out=..." or
                           "CAPTURE FAILED <msg>".

EXIT CODE

  0  every selected preset wrote "CAPTURE OK" to its done marker and every sampled frame and
     sidecar exists.
  1  a capture timed out, its done marker said CAPTURE FAILED, a sampled frame PNG came back
     missing/empty, or the PNG's IHDR dimensions were not 838x441.
  2  usage error, or python3, Chrome, the site, the capture set or the WAV was absent, the
     site's WAV differs from the capture set's, or the server could not start.

Chrome never exits on its own in this mode, so the verdict is the done marker plus the
uploaded files, never Chrome's exit code. Every Chrome is started with its own temporary
--user-data-dir and is stopped by PID, so concurrent Chrome sessions are left alone.
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

CAPTURE_SET="$REPO_ROOT/resources/beatdrop/capture-set.json"
OUT=""
SITE="build/samples/web"
CHROME="${CHROME:-}"
PRESETS=""
SAMPLE_FRAMES="0,30,60,150,300,450,599"
FRAMES=600
PORT=8090
BUILD_ID="web-wasm32"
PLATFORM_ID="web-wasm32"
RENDERER_ID=""
HOST_NAME="SynesthesiaVibeWeb"
MASKED_GL_RENDERER="WebKit WebGL"
CAPTURE_TIMEOUT_SECONDS=600
HEADED=false
DETERMINISTIC=true
EXPECTED_WIDTH=838
EXPECTED_HEIGHT=441

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --site) SITE="$2"; shift 2 ;;
    --chrome) CHROME="$2"; shift 2 ;;
    --capture-set) CAPTURE_SET="$2"; shift 2 ;;
    --presets) PRESETS="$2"; shift 2 ;;
    --sample-frames) SAMPLE_FRAMES="$2"; shift 2 ;;
    --frames) FRAMES="$2"; shift 2 ;;
    --port) PORT="$2"; shift 2 ;;
    --build) BUILD_ID="$2"; shift 2 ;;
    --platform) PLATFORM_ID="$2"; shift 2 ;;
    --renderer) RENDERER_ID="$2"; shift 2 ;;
    --capture-timeout-seconds) CAPTURE_TIMEOUT_SECONDS="$2"; shift 2 ;;
    --headed) HEADED=true; shift ;;
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
if ! [[ "$FRAMES" =~ ^[1-9][0-9]*$ ]]; then
  echo "--frames must be a positive integer. See --help." >&2
  exit 2
fi
if ! [[ "$PORT" =~ ^[1-9][0-9]*$ ]] || [[ "$PORT" -gt 65535 ]]; then
  echo "--port must be a TCP port number. See --help." >&2
  exit 2
fi
if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 not found. web-capture-server.py needs Python 3."
  exit 2
fi
if ! command -v curl >/dev/null 2>&1; then
  echo "curl not found. It is used to wait for web-capture-server.py."
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
SITE="$(resolve_repo_path "$SITE")"

if [[ -z "$CHROME" ]]; then
  for candidate in "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
      "$(command -v google-chrome 2>/dev/null || true)" \
      "$(command -v chromium 2>/dev/null || true)"; do
    if [[ -n "$candidate" && -x "$candidate" ]]; then
      CHROME="$candidate"
      break
    fi
  done
fi
if [[ -z "$CHROME" || ! -x "$CHROME" ]]; then
  echo "Chrome not found. Pass --chrome or set CHROME."
  exit 2
fi
if [[ ! -f "$CAPTURE_SET" ]]; then
  echo "Capture set not found: $CAPTURE_SET"
  exit 2
fi
if [[ ! -f "$SITE/index.html" || ! -f "$SITE/resources/capture-manifest.json" ]]; then
  echo "Web sample not found: $SITE"
  echo "Pass --site, or build it first (.github/scripts/build-web-sample.sh)."
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

gl_string() {
  sed -n "s/^$2=//p" "$1" 2>/dev/null | head -1 | tr -d '\r'
}

kebab_case() {
  printf '%s' "$1" | tr '[:upper:]' '[:lower:]' | sed -e 's/[^a-z0-9]\{1,\}/-/g' -e 's/^-//' -e 's/-$//' | cut -c1-48
}

CAPTURE_SET_FLAT="$(tr -d '\n' < "$CAPTURE_SET")"
WAV="$(resolve_repo_path "$(json_field_text "$CAPTURE_SET_FLAT" wav)")"

if [[ ! -f "$WAV" ]]; then
  echo "WAV not found: $WAV"
  exit 2
fi

SITE_WAV="$SITE/resources/audio/$(basename "$WAV")"
if [[ ! -f "$SITE_WAV" ]]; then
  echo "The site does not bundle the capture WAV: $SITE_WAV"
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
  if [[ "$((10#$trimmed))" -ge "$FRAMES" ]]; then
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

GIT_DESCRIBE="$(cd "$REPO_ROOT" && git describe --always --dirty 2>/dev/null || echo unknown)"
case "$GIT_DESCRIBE" in
  *-dirty) GIT_DIRTY=true ;;
  *) GIT_DIRTY=false ;;
esac
GIT_REF="$(cd "$REPO_ROOT" && git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
WAV_SHA256="$(shasum -a 256 "$WAV" | awk '{print $1}')"
SITE_WAV_SHA256="$(shasum -a 256 "$SITE_WAV" | awk '{print $1}')"
WAV_REPO_RELATIVE="${WAV#$REPO_ROOT/}"
WAV_RATE=44100
WAV_CHANNELS=2

if [[ "$WAV_SHA256" != "$SITE_WAV_SHA256" ]]; then
  echo "The site's WAV differs from the capture set's: $SITE_WAV"
  echo "Rebuild the site (.github/scripts/build-web-sample.sh --skip-library)."
  exit 2
fi

BROWSER_VERSION="$("$CHROME" --version 2>/dev/null | head -1 | sed 's/[[:space:]]*$//')"
[[ -z "$BROWSER_VERSION" ]] && BROWSER_VERSION="unknown"

mkdir -p "$OUT"

SERVER_PID=""
CHROME_PID=""
PROFILE_DIR=""

stop_pid() {
  local pid="$1"
  [[ -z "$pid" ]] && return 0
  if kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    local waited=0
    while kill -0 "$pid" 2>/dev/null && [[ "$waited" -lt 50 ]]; do
      sleep 0.1
      waited=$((waited + 1))
    done
    if kill -0 "$pid" 2>/dev/null; then
      kill -9 "$pid" 2>/dev/null || true
    fi
  fi
  wait "$pid" 2>/dev/null || true
}

stop_chrome() {
  stop_pid "$CHROME_PID"
  CHROME_PID=""
  if [[ -n "$PROFILE_DIR" ]]; then
    rm -rf "$PROFILE_DIR"
    PROFILE_DIR=""
  fi
}

cleanup() {
  stop_chrome
  stop_pid "$SERVER_PID"
  SERVER_PID=""
}
trap cleanup EXIT
trap 'exit 130' INT TERM

BASE_URL="http://127.0.0.1:$PORT"

python3 "$SCRIPT_DIR/web-capture-server.py" --site "$SITE" --out "$OUT" --port "$PORT" \
  >"$OUT/_server.stdout.txt" 2>"$OUT/_server.log" &
SERVER_PID=$!

server_ready=false
for _ in $(seq 1 50); do
  if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    break
  fi
  if grep -q '^SERVING ' "$OUT/_server.stdout.txt" 2>/dev/null && curl -fs -o /dev/null "$BASE_URL/index.html"; then
    server_ready=true
    break
  fi
  sleep 0.2
done
if [[ "$server_ready" != true ]]; then
  echo "web-capture-server.py did not start on $BASE_URL -- see $OUT/_server.log"
  exit 2
fi

UPLOAD_URL_ENCODED="$(python3 -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$BASE_URL/upload")"

CHROME_FLAGS=(
  --no-first-run
  --no-default-browser-check
  --disable-extensions
  --disable-sync
  --disable-background-networking
  --disable-background-timer-throttling
  --disable-renderer-backgrounding
  --disable-backgrounding-occluded-windows
  --autoplay-policy=no-user-gesture-required
  --enable-logging=stderr
  --v=0
)
if [[ "$HEADED" == true ]]; then
  CHROME_FLAGS+=(--window-size=1280,800)
else
  CHROME_FLAGS+=(--headless=new --use-angle=swiftshader --enable-unsafe-swiftshader)
fi

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
  local renderer="$5"
  local gl_renderer="$6"
  local gl_version="$7"
  local audio_texture_format="$8"
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
  "renderer": "$(json_escape "$renderer")",
  "browser": "$(json_escape "$BROWSER_VERSION")",
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
  local done_path="$1"
  local started_at
  started_at="$(date +%s)"
  local elapsed=0
  while [[ "$elapsed" -lt "$CAPTURE_TIMEOUT_SECONDS" ]]; do
    if [[ -f "$done_path" ]]; then
      echo "done	$(( $(date +%s) - started_at ))"
      return 0
    fi
    if ! kill -0 "$CHROME_PID" 2>/dev/null; then
      echo "exited	$(( $(date +%s) - started_at ))"
      return 0
    fi
    sleep 1
    elapsed=$(( $(date +%s) - started_at ))
  done
  echo "timeout	$elapsed"
  return 0
}

echo "browser=$BROWSER_VERSION headed=$HEADED site=${SITE#$REPO_ROOT/} server=$BASE_URL build=$BUILD_ID"

CAPTURE_FAILED=0

for id in "${SELECTED_IDS[@]}"; do
  capture_out="$OUT/$id"

  echo "[$id] save-frames=$SAMPLE_FRAMES_ARG"

  rm -rf "$capture_out"
  mkdir -p "$capture_out"

  PROFILE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/synesthesia-web-capture.XXXXXX")"
  url="$BASE_URL/index.html?capture=$id&frames=$SAMPLE_FRAMES_ARG&out=$id&upload=$UPLOAD_URL_ENCODED"
  "$CHROME" "${CHROME_FLAGS[@]}" --user-data-dir="$PROFILE_DIR" "$url" \
    >"$capture_out/_process.chrome.txt" 2>&1 &
  CHROME_PID=$!

  result="$(wait_for_done_marker "$capture_out/done")"
  IFS=$'\t' read -r outcome wall_seconds <<< "$result"
  stop_chrome

  if [[ "$outcome" == "timeout" ]]; then
    CAPTURE_FAILED=1
    echo "  TIMEOUT -- no done marker after ${CAPTURE_TIMEOUT_SECONDS}s"
    continue
  fi
  if [[ "$outcome" == "exited" ]]; then
    CAPTURE_FAILED=1
    echo "  CHROME EXITED -- no done marker after ${wall_seconds}s, see $capture_out/_process.chrome.txt"
    continue
  fi

  done_line="$(head -1 "$capture_out/done" 2>/dev/null | tr -d '\r')"
  echo "  done after ${wall_seconds}s -- $done_line"
  if [[ "$done_line" != "CAPTURE OK"* ]]; then
    CAPTURE_FAILED=1
    echo "  NO CAPTURE OK -- see $capture_out/_process.chrome.txt"
    continue
  fi

  incomplete="$(sampled_frames_incomplete "$capture_out" || true)"
  if [[ -n "$incomplete" ]]; then
    CAPTURE_FAILED=1
    echo "  INCOMPLETE -- missing/empty sampled frames: $incomplete"
    continue
  fi

  gl_renderer="$(gl_string "$capture_out/gl-strings.txt" UNMASKED_RENDERER)"
  [[ -z "$gl_renderer" ]] && gl_renderer="$(gl_string "$capture_out/gl-strings.txt" GL_RENDERER)"
  gl_version="$(gl_string "$capture_out/gl-strings.txt" GL_VERSION)"
  audio_texture_format="$(gl_string "$capture_out/gl-strings.txt" AUDIO_TEXTURE_FORMAT)"
  [[ -z "$gl_renderer" ]] && gl_renderer="unknown"
  [[ -z "$gl_version" ]] && gl_version="unknown"
  [[ -z "$audio_texture_format" ]] && audio_texture_format="unknown"

  renderer="$RENDERER_ID"
  if [[ -z "$renderer" ]]; then
    if [[ "$HEADED" == true && "$gl_renderer" == "$MASKED_GL_RENDERER" ]]; then
      renderer="web-chrome-gpu"
    elif [[ "$HEADED" == true ]]; then
      renderer="web-chrome-$(kebab_case "$gl_renderer")"
    else
      renderer="web-chrome-swiftshader"
    fi
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
    write_sidecar "$capture_out" "$frame" "$measured_width" "$measured_height" "$renderer" "$gl_renderer" "$gl_version" "$audio_texture_format"
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
