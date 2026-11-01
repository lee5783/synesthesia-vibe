#!/usr/bin/env bash
#
#  build-web-sample.sh
#  SynesthesiaVibe
#
#  Created by lee5783 on 2026-10-12.
#

set -euo pipefail

SKIP_LIBRARY=0
SERVE=0
PORT=8000

print_help() {
  cat <<'EOF'
build-web-sample.sh -- assembles the SynesthesiaVibe web sample into build/samples/web/

USAGE

    .github/scripts/build-web-sample.sh [--skip-library] [--serve [port]]

  Runs from any working directory. Calls platform/web/build-web.sh (the library packaging,
  build/web/dist/), then recreates build/samples/web/:

    index.html main.js capture.js styles.css   copied from samples/web/
    lib/                                       build/web/dist/ (the import map points here)
    resources/presets/                         resources/mvp-presets/*.milk
    resources/capture-presets/                 resources/beatdrop/capture-presets/*.milk
    resources/textures/                        resources/beatdrop/textures/
    resources/audio/deterministic-test.wav     tests/tools/wav-generator/deterministic-test.wav
    resources/capture-set.json                 resources/beatdrop/capture-set.json
    resources/manifest.json                    generated: MVP presets + textures + WAV
    resources/capture-manifest.json            generated: capture presets + textures + WAV

  The manifests are the ResourceLoader format {"presets": [], "textures": [], "audio": []}
  with paths relative to the manifest. Nothing is written under samples/web/.

PARAMETERS

  --skip-library  Reuse the existing build/web/dist/ instead of running build-web.sh.
  --serve [port]  After assembling, serve build/samples/web/ on http://localhost:<port>/
                  (python3 -m http.server, bound to localhost; default port 8000).
  --help          This text.

NOTES

  build-web.sh needs EMSDK (see its --help). Local serving only: the bundled presets and
  textures are approved for sample artifacts, not for public hosting.

EXIT CODE

  0   build/samples/web/ assembled (or the server stopped cleanly).
  1   Library build failed or an input is missing.
  2   Usage error.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-library)
      SKIP_LIBRARY=1
      shift
      ;;
    --serve)
      SERVE=1
      shift
      if [[ $# -gt 0 && "$1" =~ ^[0-9]+$ ]]; then
        PORT="$1"
        shift
      fi
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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SAMPLE_DIR="$REPO_ROOT/samples/web"
DIST="$REPO_ROOT/build/web/dist"
OUT="$REPO_ROOT/build/samples/web"
RESOURCES="$OUT/resources"

MVP_PRESETS="$REPO_ROOT/resources/mvp-presets"
CAPTURE_PRESETS="$REPO_ROOT/resources/beatdrop/capture-presets"
TEXTURES="$REPO_ROOT/resources/beatdrop/textures"
CAPTURE_SET="$REPO_ROOT/resources/beatdrop/capture-set.json"
WAV="$REPO_ROOT/tests/tools/wav-generator/deterministic-test.wav"

log() {
  printf '[build-web-sample] %s\n' "$*"
}

fail() {
  printf '[build-web-sample] ERROR: %s\n' "$*" >&2
  exit 1
}

command -v python3 >/dev/null 2>&1 || fail "python3 is required to generate the manifests"

for input in "$SAMPLE_DIR/index.html" "$SAMPLE_DIR/main.js" "$SAMPLE_DIR/capture.js" "$SAMPLE_DIR/styles.css" "$CAPTURE_SET" "$WAV"; do
  [[ -f "$input" ]] || fail "missing $input"
done
for input in "$MVP_PRESETS" "$CAPTURE_PRESETS" "$TEXTURES"; do
  [[ -d "$input" ]] || fail "missing $input"
done

if [[ $SKIP_LIBRARY -eq 0 ]]; then
  log "library: platform/web/build-web.sh"
  "$REPO_ROOT/platform/web/build-web.sh" || fail "platform/web/build-web.sh failed"
fi

[[ -f "$DIST/synesthesia-vibe.js" && -f "$DIST/synesthesia-vibe-wasm.wasm" ]] || fail "missing library in $DIST (run without --skip-library)"

log "assembling $OUT"
rm -rf "$OUT"
mkdir -p "$OUT/lib" "$RESOURCES/presets" "$RESOURCES/capture-presets" "$RESOURCES/textures" "$RESOURCES/audio"

cp "$SAMPLE_DIR/index.html" "$SAMPLE_DIR/main.js" "$SAMPLE_DIR/capture.js" "$SAMPLE_DIR/styles.css" "$OUT/"
cp "$DIST"/* "$OUT/lib/"
find "$MVP_PRESETS" -maxdepth 1 -type f -name '*.milk' -exec cp {} "$RESOURCES/presets/" \;
find "$CAPTURE_PRESETS" -maxdepth 1 -type f -name '*.milk' -exec cp {} "$RESOURCES/capture-presets/" \;
find "$TEXTURES" -maxdepth 1 -type f ! -name '.*' -exec cp {} "$RESOURCES/textures/" \;
cp "$CAPTURE_SET" "$RESOURCES/capture-set.json"
cp "$WAV" "$RESOURCES/audio/"

python3 - "$RESOURCES" <<'PY'
import json
import os
import sys

root = sys.argv[1]

def listing(folder):
    return sorted(folder + "/" + name for name in os.listdir(os.path.join(root, folder)) if not name.startswith("."))

def write(name, presets_folder):
    manifest = {"presets": listing(presets_folder), "textures": listing("textures"), "audio": listing("audio")}
    with open(os.path.join(root, name), "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2, ensure_ascii=False)
        handle.write("\n")
    print("[build-web-sample] %s: %d presets, %d textures, %d audio" % (name, len(manifest["presets"]), len(manifest["textures"]), len(manifest["audio"])))

write("manifest.json", "presets")
write("capture-manifest.json", "capture-presets")
PY

log "sample -> $OUT"

if [[ $SERVE -eq 1 ]]; then
  log "serving http://localhost:$PORT/ (Ctrl+C to stop)"
  cd "$OUT"
  exec python3 -m http.server "$PORT" --bind 127.0.0.1
fi
