# android-preset-sweep

Plays every preset of a bundled set on a connected Android device, one after the other, in the
sample's fullscreen view, and writes one JSON record per preset: load time, fps, frame cost,
status and a few health flags.

## Build the APK with a preset set

```
.github/scripts/build-android-sample.sh --debug --preset-set <dir>
```

`<dir>` holds `presets/**/*.milk` and optionally `textures/`. Presets are flattened to
`<folder> - <name>.milk`; a `preset-set.json` asset keeps the original path.

## Run

```
tests/tools/android-preset-sweep/run-android-preset-sweep.sh --serial <adb serial> --out <dir> \
  [--apk <apk>] [--run-id <name>] [--resume] [--start <n>] [--limit <n>] [--list <names.txt>]
```

`--help` lists every option. The driver forces the display to `--hz` (default 120), keeps the screen
on, restarts the app after a crash or a stall, retries a crashed preset once in a fresh process, and
restores the device settings on exit. `--resume` continues from what is on the device and in
`events.jsonl`.

## Output

`results.jsonl` (merged, one record per preset, ordered by `index`), `events.jsonl`,
`device/<run id>/`, `retry/`, `preflight.txt`, `driver.log`, `expected-total`.

## Record fields

- `index`, `name`, `source`, `status`: `ok`, `degraded`, `load_error`, `timeout`, `crash`, `crash_flaky`
- `error`, `last_error`: load error text, or the engine's fallback note
- `prepare_ms`, `commit_ms`: preset load on the prepare worker and on the GL thread
- `fps_mean`, `fps_p5`: from an uninstrumented window of `fps_window_s` seconds (`fps_frames` frames)
- `cost_p50_ms`, `cost_p95_ms`: render plus `glFinish` per frame, from a separate window of
  `cost_window_s` seconds (`cost_frames` frames); `frames` is the total
- `surface_w`, `surface_h`, `texsize_x`, `texsize_y`: the GL surface and the engine texture size
- `sim_steps_per_frame`, `warp_shader`, `comp_shader`, `gl_errors`, `reports`, `report_display_fps`
- `blank`, `frozen`: image heuristics
- `thermal_status`, `display_hz`, `warmup_s`, `measure_s`, `set`, `timestamp_ms`
- merged only: `first_failure`, `retried`, `retry_error`
