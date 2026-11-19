#!/usr/bin/env bash
#
#  run-android-preset-sweep.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-11-07.
#

set -euo pipefail

print_help() {
  cat <<'EOF'
run-android-preset-sweep.sh -- runs every bundled preset of the Android sample on a device, with a watchdog

USAGE

  tests/tools/android-preset-sweep/run-android-preset-sweep.sh --serial <adb serial> --out <dir> [options]

  Starts the sample's sweep mode (MainActivity extra "sweep"), which walks the bundled library in
  order on the real prepare/commit path, plays the deterministic WAV and appends one JSONL record per
  preset to <app external files>/<run id>/results.jsonl. This script is the watchdog: a preset with no
  progress past --timeout seconds, or a dead process, gets a synthetic timeout/crash record, the app is
  force-stopped and relaunched from the next preset. Every crash is retried once in a fresh process
  (crash_flaky when the retry plays). At the end it pulls the results, merges them into <dir>/results.jsonl
  and restores the device settings it changed.

OPTIONS

  --serial <s>           adb serial (host:port for Wi-Fi adb; it is reconnected when it drops).
                         Default: $ANDROID_SERIAL, else the only attached device.
  --out <dir>            Host output directory. Required.
  --apk <file>           adb install -r this APK first. Default: the app is assumed installed.
  --package <id>         Default: com.synesthesiavibe.sample.
  --activity <name>      Default: .MainActivity.
  --run-id <id>          Device output directory name under the app's external files dir.
                         Default: sweep-<yyyyMMddHHmmss>.
  --start <n|name>       First library index (or flat preset name). Default: 0.
  --limit <n>            Run at most n presets. Default: all.
  --list <file>          Host text file of flat preset names (one per line, .milk optional); only
                         those run. It is copied into the app's private files dir with run-as.
  --filter <text>        Only presets whose file name contains text (case-insensitive).
  --warmup <s>           Seconds per preset before measuring (first second excluded). Default: 1.
  --dwell <s>            Seconds measured per preset. Default: 6.
  --timeout <s>          No progress for this long on one preset gives timeout. Default: 30.
  --startup-timeout <s>  Same, before the app has reported its first preset of a launch. Default: 120.
  --poll <s>             Seconds between device polls. Default: 2.
  --max-fps <n>          Engine fps cap passed as maxFps. Default: 0 (uncapped, asks for the
                         fastest display mode, and keeps the cost measurement free of pacing waits).
  --hz <n>               Refresh rate the pre-flight tries to force. Default: 120.
  --no-refresh-rate      Skip the refresh-rate pre-flight (settings are not touched).
  --no-retry             Do not retry crashes.
  --kill-after <n>       Test hook: force-stop the app once, when n records exist.
  --resume               Continue an interrupted run in the same --out and --run-id.
  --help                 This text.

PRE-FLIGHT

  Records, then sets, system settings miui_refresh_rate, min_refresh_rate, peak_refresh_rate and
  user_refresh_rate to --hz, and turns stay-awake on. The originals go to <dir>/preflight.txt and are put
  back on exit (also on Ctrl-C). After the first preset starts it logs dumpsys display (the
  PRIORITY_MIUI_REFRESH_RATE vote) and SurfaceFlinger (active mode, renderRate) to preflight.txt and
  takes the achieved display Hz from renderRate; the records carry display_hz as the app saw it.

OUTPUT (in <dir>)

  results.jsonl          merged, one record per preset, ordered by index (schema: tests/tools/android-preset-sweep/README.md).
  events.jsonl           host-written timeout/crash records of the main pass.
  device/<run id>/       the app's own files: results.jsonl, started, done, meta.json.
  retry/<index>/         the crash retry of one preset.
  crash-<index>.log      the crash buffer of logcat at the moment of the crash.
  preflight.txt, driver.log

EXIT CODE

  0  the merged file has one record for each preset of the run.
  1  gaps, the app reported a failure, adb was lost for good or progress stopped repeatedly.
  2  usage error or no device.
EOF
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SERIAL="${ANDROID_SERIAL:-}"
OUT=""
APK=""
PACKAGE="com.synesthesiavibe.sample"
ACTIVITY=".MainActivity"
RUN_ID="sweep-$(date +%Y%m%d%H%M%S)"
START="0"
LIMIT=""
LIST_FILE=""
FILTER=""
WARMUP="1"
DWELL="6"
TIMEOUT=30
STARTUP_TIMEOUT=120
POLL=2
MAX_FPS="0"
HZ="120"
SET_RATE=true
RETRY=true
KILL_AFTER=""
RESUME=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --serial) SERIAL="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --apk) APK="$2"; shift 2 ;;
    --package) PACKAGE="$2"; shift 2 ;;
    --activity) ACTIVITY="$2"; shift 2 ;;
    --run-id) RUN_ID="$2"; shift 2 ;;
    --start) START="$2"; shift 2 ;;
    --limit) LIMIT="$2"; shift 2 ;;
    --list) LIST_FILE="$2"; shift 2 ;;
    --filter) FILTER="$2"; shift 2 ;;
    --warmup) WARMUP="$2"; shift 2 ;;
    --dwell) DWELL="$2"; shift 2 ;;
    --timeout) TIMEOUT="$2"; shift 2 ;;
    --startup-timeout) STARTUP_TIMEOUT="$2"; shift 2 ;;
    --poll) POLL="$2"; shift 2 ;;
    --max-fps) MAX_FPS="$2"; shift 2 ;;
    --hz) HZ="$2"; shift 2 ;;
    --no-refresh-rate) SET_RATE=false; shift ;;
    --no-retry) RETRY=false; shift ;;
    --kill-after) KILL_AFTER="$2"; shift 2 ;;
    --resume) RESUME=true; shift ;;
    --help|-h) print_help; exit 0 ;;
    *)
      echo "Unknown argument: $1" >&2
      print_help
      exit 2
      ;;
  esac
done

[[ -n "$OUT" ]] || { echo "--out is required. See --help." >&2; exit 2; }
command -v adb >/dev/null 2>&1 || { echo "adb not found on PATH." >&2; exit 2; }
command -v python3 >/dev/null 2>&1 || { echo "python3 not found on PATH." >&2; exit 2; }
for number in "$TIMEOUT" "$STARTUP_TIMEOUT" "$POLL" "$HZ"; do
  [[ "$number" =~ ^[1-9][0-9]*$ ]] || { echo "--timeout, --startup-timeout, --poll and --hz must be positive integers." >&2; exit 2; }
done
[[ -z "$LIMIT" || "$LIMIT" =~ ^[1-9][0-9]*$ ]] || { echo "--limit must be a positive integer." >&2; exit 2; }
[[ -z "$LIST_FILE" || -f "$LIST_FILE" ]] || { echo "--list file not found: $LIST_FILE" >&2; exit 2; }
[[ -z "$APK" || -f "$APK" ]] || { echo "APK not found: $APK" >&2; exit 2; }

if [[ "$(awk -v t="$TIMEOUT" -v w="$WARMUP" -v d="$DWELL" 'BEGIN { print (t < w + d + 10) ? "short" : "ok" }')" == "short" ]]; then
  echo "warning: --timeout $TIMEOUT is under warmup + dwell + 10 s; healthy presets may be reported as timeout." >&2
fi

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
LOG="$OUT/driver.log"
PREFLIGHT="$OUT/preflight.txt"
EVENTS="$OUT/events.jsonl"
EXPECTED_FILE="$OUT/expected-total"
MERGE=(python3 -I "$SCRIPT_DIR/merge-sweep-results.py")

log() {
  printf '%s  %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$LOG"
}

fail() {
  log "ERROR: $*"
  exit 1
}

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

reconnect() {
  local attempt
  for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24; do
    if [[ "$SERIAL" == *:* ]]; then
      adb connect "$SERIAL" >/dev/null 2>&1 || true
    fi
    if [[ "$("${ADB[@]}" get-state 2>/dev/null || true)" == "device" ]]; then
      return 0
    fi
    sleep 5
  done
  return 1
}

if [[ "$SERIAL" == *:* ]]; then
  adb connect "$SERIAL" >/dev/null 2>&1 || true
fi
[[ "$("${ADB[@]}" get-state 2>/dev/null || true)" == "device" ]] || { echo "Device not ready: $SERIAL" >&2; exit 2; }

shell_line() {
  "${ADB[@]}" shell "$1" 2>/dev/null | tr -d '\r'
}

quote() {
  printf "'%s'" "${1//\'/\'\\\'\'}"
}

now() {
  date +%s
}

RATE_KEYS="miui_refresh_rate min_refresh_rate peak_refresh_rate user_refresh_rate"
ORIGINALS_FILE="$OUT/original-settings.txt"
SETTINGS_CHANGED=false
ACHIEVED_HZ=""
RATE_VERIFIED=false
TOTAL_EXPECTED=""

save_originals() {
  if [[ -f "$ORIGINALS_FILE" ]]; then
    return
  fi
  local key
  : > "$ORIGINALS_FILE"
  for key in $RATE_KEYS; do
    printf 'system %s %s\n' "$key" "$(shell_line "settings get system $key")" >> "$ORIGINALS_FILE"
  done
  printf 'global stay_on_while_plugged_in %s\n' "$(shell_line "settings get global stay_on_while_plugged_in")" >> "$ORIGINALS_FILE"
}

on_signal() {
  restore_device
  exit 130
}

restore_device() {
  trap - EXIT INT TERM
  [[ "$SETTINGS_CHANGED" == true ]] || return 0
  log "restoring device settings"
  reconnect || { log "could not reconnect to restore settings; originals are in $ORIGINALS_FILE"; return 0; }
  "${ADB[@]}" shell am force-stop "$PACKAGE" >/dev/null 2>&1 || true
  local namespace key value
  while read -r -u 3 namespace key value; do
    [[ -n "$key" ]] || continue
    if [[ "$key" == "stay_on_while_plugged_in" ]]; then
      if [[ "$value" == "0" || "$value" == "null" ]]; then
        "${ADB[@]}" shell svc power stayon false >/dev/null 2>&1 || true
      else
        "${ADB[@]}" shell settings put global stay_on_while_plugged_in "$value" >/dev/null 2>&1 || true
      fi
    elif [[ "$value" == "null" ]]; then
      "${ADB[@]}" shell settings delete "$namespace" "$key" >/dev/null 2>&1 || true
    else
      "${ADB[@]}" shell settings put "$namespace" "$key" "$value" >/dev/null 2>&1 || true
    fi
  done 3< "$ORIGINALS_FILE"
  "${ADB[@]}" shell input keyevent KEYCODE_HOME >/dev/null 2>&1 || true
  {
    echo "== restored $(date -u +%FT%TZ)"
    for key in $RATE_KEYS; do
      printf '%s=%s\n' "$key" "$(shell_line "settings get system $key")"
    done
    printf 'stay_on_while_plugged_in=%s\n' "$(shell_line "settings get global stay_on_while_plugged_in")"
  } >> "$PREFLIGHT"
  log "settings restored (see $PREFLIGHT)"
}

preflight() {
  {
    echo "== pre-flight $(date -u +%FT%TZ) serial=$SERIAL"
    echo "model=$(shell_line "getprop ro.product.model") sdk=$(shell_line "getprop ro.build.version.sdk") build=$(shell_line "getprop ro.build.fingerprint")"
  } >> "$PREFLIGHT"
  if [[ "$SET_RATE" == true ]]; then
    save_originals
    SETTINGS_CHANGED=true
    trap restore_device EXIT
    trap on_signal INT TERM
    {
      echo "-- originals"
      cat "$ORIGINALS_FILE"
    } >> "$PREFLIGHT"
    local key
    for key in $RATE_KEYS; do
      "${ADB[@]}" shell settings put system "$key" "$HZ" >/dev/null
    done
    "${ADB[@]}" shell svc power stayon true >/dev/null
    {
      echo "-- applied (system $RATE_KEYS = $HZ, stay-awake on)"
      for key in $RATE_KEYS; do
        printf '%s=%s\n' "$key" "$(shell_line "settings get system $key")"
      done
    } >> "$PREFLIGHT"
  else
    log "refresh-rate pre-flight skipped"
  fi
  "${ADB[@]}" shell input keyevent KEYCODE_WAKEUP >/dev/null 2>&1 || true
  "${ADB[@]}" shell wm dismiss-keyguard >/dev/null 2>&1 || true
}

verify_rate() {
  RATE_VERIFIED=true
  local surface_dump active
  surface_dump="$("${ADB[@]}" shell dumpsys SurfaceFlinger 2>/dev/null | tr -d '\r')"
  {
    echo "-- verification while the sweep renders $(date -u +%FT%TZ)"
    echo "[dumpsys display]"
    "${ADB[@]}" shell dumpsys display 2>/dev/null | tr -d '\r' | grep -E "mActiveModeId|PRIORITY_MIUI_REFRESH_RATE|PRIORITY_APP_REQUEST" | sort | uniq -c | head -12
    echo "[SurfaceFlinger]"
    printf '%s\n' "$surface_dump" | grep -E "activeMode=|renderRate|vsyncRate" | sort | uniq -c | head -12
  } >> "$PREFLIGHT"
  active="$(printf '%s\n' "$surface_dump" | sed -n 's/^ *renderRate=\([0-9.]*\) Hz.*/\1/p' | head -1)"
  if [[ -n "$active" ]]; then
    ACHIEVED_HZ="$(printf '%.0f' "$active")"
  fi
  log "achieved display Hz (SurfaceFlinger renderRate): ${ACHIEVED_HZ:-unknown}, requested $HZ"
  echo "achieved_display_hz=${ACHIEVED_HZ:-unknown} requested=$HZ" >> "$PREFLIGHT"
}

DEVICE_BASE="/sdcard/Android/data/$PACKAGE/files"
DEV_NAME=""
DEVICE_DIR=""
HOST_EVENTS=""
POLL_RAW=""

poll() {
  POLL_RAW="$("${ADB[@]}" shell "d=$(quote "$DEVICE_DIR"); echo @started; cat \$d/started 2>/dev/null; echo @last; tail -n 1 \$d/results.jsonl 2>/dev/null; echo @lines; wc -l < \$d/results.jsonl 2>/dev/null; echo @done; cat \$d/done 2>/dev/null; echo @meta; cat \$d/meta.json 2>/dev/null; echo @pid; pidof $PACKAGE; echo @end" 2>/dev/null | tr -d '\r' || true)"
  [[ "$POLL_RAW" == *"@end"* ]]
}

section() {
  printf '%s\n' "$POLL_RAW" | awk -v marker="@$1" '$0 == marker { on = 1; next } /^@/ { on = 0 } on'
}

push_list() {
  [[ -n "$LIST_FILE" ]] || return 0
  "${ADB[@]}" shell "run-as $PACKAGE sh -c 'cat > files/sweep-list.txt'" < "$LIST_FILE"
}

launch() {
  local start="$1" limit="$2"
  local command="am start -n $PACKAGE/$ACTIVITY --ez sweep true --es sweepOut $(quote "$DEV_NAME") --es sweepStart $(quote "$start") --ef sweepWarmup $WARMUP --ef sweepDwell $DWELL --ef maxFps $MAX_FPS"
  [[ -z "$limit" ]] || command="$command --ei sweepLimit $limit"
  [[ -z "$LIST_FILE" ]] || command="$command --es sweepList sweep-list.txt"
  [[ -z "$FILTER" ]] || command="$command --es sweepFilter $(quote "$FILTER")"
  "${ADB[@]}" shell am force-stop "$PACKAGE" >/dev/null 2>&1 || true
  "${ADB[@]}" shell "rm -f $(quote "$DEVICE_DIR")/started $(quote "$DEVICE_DIR")/done" >/dev/null 2>&1 || true
  "${ADB[@]}" shell input keyevent KEYCODE_WAKEUP >/dev/null 2>&1 || true
  "${ADB[@]}" shell "$command" >/dev/null
  log "launched $DEV_NAME start=$start limit=${limit:-all}"
}

pull_device() {
  mkdir -p "$OUT/device"
  rm -rf "$OUT/device/$DEV_NAME"
  "${ADB[@]}" pull "$DEVICE_DIR" "$OUT/device/" >/dev/null 2>&1 || log "pull of $DEVICE_DIR failed"
}

crash_text() {
  local index="$1" text
  "${ADB[@]}" logcat -d -b crash -t 60 2>/dev/null | tr -d '\r' > "$OUT/crash-$index.log" || true
  "${ADB[@]}" logcat -b crash -c >/dev/null 2>&1 || true
  text="$(grep -m1 -E "Fatal signal|FATAL EXCEPTION|Abort message|signal [0-9]+" "$OUT/crash-$index.log" | sed 's/^[0-9: .-]* [A-Z] //' || true)"
  printf '%s' "${text:-process died}"
}

write_event() {
  local index="$1" name="$2" source="$3" status="$4" error="$5"
  "${MERGE[@]}" event --index "$index" --name "$name" --source "$source" --status "$status" --error "$error" ${ACHIEVED_HZ:+--display-hz "$ACHIEVED_HZ"} >> "$HOST_EVENTS"
  log "$status index=$index name=$name: $error"
}

PASS_FAILED=false
KILLED=false
LAUNCH_GRACE=20

run_pass() {
  local start="$1" limit="$2" stalls=0 last_total=-1 lines_before="${3:-0}"
  PASS_FAILED=false
  while :; do
    local remaining="$limit" events_now=0
    [[ -f "$HOST_EVENTS" ]] && events_now=$(wc -l < "$HOST_EVENTS" | tr -d ' ')
    if [[ -n "$limit" ]]; then
      remaining=$((limit - events_now - lines_before))
      [[ "$remaining" -gt 0 ]] || return 0
    fi
    launch "$start" "$remaining"
    local last_sig="" last_progress launched reason="" started="" index last_index lines="0" done_text pid name source allowed="$TIMEOUT"
    launched="$(now)"
    last_progress="$launched"
    while :; do
      sleep "$POLL"
      if ! poll; then
        log "adb unreachable, reconnecting"
        reconnect || fail "adb connection to $SERIAL lost for good"
        last_progress="$(now)"
        continue
      fi
      started="$(section started)"
      lines="$(section lines | tr -d ' ')"
      lines="${lines:-0}"
      done_text="$(section done)"
      pid="$(section pid | tr -d ' ')"
      if [[ -z "$TOTAL_EXPECTED" ]]; then
        TOTAL_EXPECTED="$(section meta | sed -n 's/.*"run_count":\([0-9]*\).*/\1/p' | head -1)"
        [[ -z "$TOTAL_EXPECTED" ]] || echo "$TOTAL_EXPECTED" > "$EXPECTED_FILE"
      fi
      if [[ "$started|$lines" != "$last_sig" ]]; then
        last_sig="$started|$lines"
        last_progress="$(now)"
      fi
      if [[ "$lines" -gt "$lines_before" && "$RATE_VERIFIED" == false && "$SET_RATE" == true ]]; then
        verify_rate
      fi
      if [[ -n "$done_text" ]]; then
        log "$done_text"
        lines_before="$lines"
        pull_device
        [[ "$done_text" == "SWEEP OK"* ]] || { PASS_FAILED=true; return 1; }
        return 0
      fi
      if [[ -n "$KILL_AFTER" && "$KILLED" == false && "$lines" -ge "$KILL_AFTER" ]]; then
        KILLED=true
        log "test hook: forcing am force-stop at $lines records"
        "${ADB[@]}" shell am force-stop "$PACKAGE" >/dev/null 2>&1 || true
        continue
      fi
      if [[ -z "$pid" ]]; then
        if [[ -z "$started" && $(( $(now) - launched )) -lt "$LAUNCH_GRACE" ]]; then
          continue
        fi
        reason="crash"
        break
      fi
      allowed="$TIMEOUT"
      [[ -n "$started" ]] || allowed="$STARTUP_TIMEOUT"
      if [[ $(( $(now) - last_progress )) -gt "$allowed" ]]; then
        reason="timeout"
        break
      fi
    done

    index="$(printf '%s' "$started" | cut -f1)"
    name="$(printf '%s' "$started" | cut -f2)"
    source="$(printf '%s' "$started" | cut -f4)"
    last_index="$(section last | sed -n 's/.*"index":\([0-9]*\).*/\1/p' | head -1)"
    last_index="${last_index:--1}"
    local error="no progress for ${allowed}s"
    if [[ "$reason" == "crash" ]]; then
      error="$(crash_text "${index:-none}")"
    fi
    "${ADB[@]}" shell am force-stop "$PACKAGE" >/dev/null 2>&1 || true
    if [[ -n "$index" && "$index" -gt "$last_index" ]]; then
      write_event "$index" "$name" "${source:-$name}" "$reason" "$error"
      start=$((index + 1))
    else
      log "$reason with no preset in flight"
      [[ "$last_index" -lt 0 ]] || start=$((last_index + 1))
    fi
    lines_before="$lines"
    [[ -f "$HOST_EVENTS" ]] && events_now=$(wc -l < "$HOST_EVENTS" | tr -d ' ')
    if [[ $((events_now + lines)) -eq "$last_total" ]]; then
      stalls=$((stalls + 1))
    else
      stalls=0
    fi
    last_total=$((events_now + lines))
    if [[ "$stalls" -ge 3 ]]; then
      PASS_FAILED=true
      log "ERROR: no progress after $stalls relaunches"
      return 1
    fi
    pull_device
  done
}

preflight

if [[ -n "$APK" ]]; then
  log "installing $APK"
  "${ADB[@]}" install -r "$APK" >/dev/null
fi

push_list

DEV_NAME="$RUN_ID"
DEVICE_DIR="$DEVICE_BASE/$DEV_NAME"
HOST_EVENTS="$EVENTS"
mkdir -p "$OUT/retry"

RESUME_LINES=0
if [[ "$RESUME" == true ]]; then
  "${ADB[@]}" shell am force-stop "$PACKAGE" >/dev/null 2>&1 || true
  pull_device
  if [[ -f "$EXPECTED_FILE" ]]; then
    TOTAL_EXPECTED="$(tr -d '[:space:]' < "$EXPECTED_FILE")"
  fi
  [[ -f "$EVENTS" ]] || : > "$EVENTS"
  DEVICE_RESULTS="$OUT/device/$RUN_ID/results.jsonl"
  [[ -f "$DEVICE_RESULTS" ]] || : > "$DEVICE_RESULTS"
  RESUME_LINES="$(grep -c . "$DEVICE_RESULTS" || true)"
  LAST_DONE="$(python3 -I -c 'import json,sys
last = -1
for path in sys.argv[1:]:
    for l in open(path):
        if l.strip():
            last = max(last, json.loads(l)["index"])
print(last)' "$DEVICE_RESULTS" "$EVENTS")"
  [[ "$LAST_DONE" -lt 0 ]] || START="$((LAST_DONE + 1))"
  log "resuming from $START ($RESUME_LINES device records, $(grep -c . "$EVENTS" || true) host events, expected ${TOTAL_EXPECTED:-unknown})"
else
  : > "$EVENTS"
  rm -f "$EXPECTED_FILE"
fi

log "sweep $RUN_ID on $SERIAL start=$START limit=${LIMIT:-all} warmup=$WARMUP dwell=$DWELL timeout=$TIMEOUT"
MAIN_FAILED=false
run_pass "$START" "$LIMIT" "$RESUME_LINES" || MAIN_FAILED=true
pull_device

if [[ "$RETRY" == true && "$MAIN_FAILED" == false && -s "$EVENTS" ]]; then
  CRASHED="$(python3 -I -c 'import json,sys
for l in open(sys.argv[1]):
    r = json.loads(l) if l.strip() else None
    if r and r["status"] == "crash":
        print(r["index"])' "$EVENTS")"
  for index in $CRASHED; do
    log "retrying crashed preset $index in a fresh process"
    DEV_NAME="$RUN_ID-retry-$index"
    DEVICE_DIR="$DEVICE_BASE/$DEV_NAME"
    HOST_EVENTS="$OUT/retry/$index/events.jsonl"
    mkdir -p "$OUT/retry/$index"
    : > "$HOST_EVENTS"
    KILLED=true
    run_pass "$index" "1" || true
    cp "$OUT/device/$DEV_NAME/results.jsonl" "$OUT/retry/$index/results.jsonl" 2>/dev/null || true
  done
  DEV_NAME="$RUN_ID"
  DEVICE_DIR="$DEVICE_BASE/$DEV_NAME"
fi

EXPECTED_ARGS=()
[[ -z "$TOTAL_EXPECTED" ]] || EXPECTED_ARGS=(--expected "$TOTAL_EXPECTED")
SUMMARY=""
STATUS=0
SUMMARY="$("${MERGE[@]}" merge --device-results "$OUT/device/$RUN_ID/results.jsonl" --events "$EVENTS" --retry-root "$OUT/retry" --out "$OUT/results.jsonl" ${EXPECTED_ARGS[@]+"${EXPECTED_ARGS[@]}"})" || STATUS=1
log "merged: $SUMMARY (expected ${TOTAL_EXPECTED:-unknown})"
[[ "$MAIN_FAILED" == false ]] || STATUS=1
exit "$STATUS"
