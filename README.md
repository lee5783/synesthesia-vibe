# SynesthesiaVibe

[![Support on Ko-fi](https://img.shields.io/badge/Ko--fi-Buy%20me%20a%20coffee-FF5E5B?logo=ko-fi&logoColor=white)](https://ko-fi.com/lee5783)

SynesthesiaVibe is a cross-platform C++ music visualizer library that plays MilkDrop 2 presets. It is a
port of [BeatDrop](https://github.com/OfficialIncubo/BeatDrop-Music-Visualizer), a MilkDrop 2 based
visualizer for Windows and DirectX 9, to portable code: a preset parser, an OpenGL 3.3 Core renderer
that also builds as OpenGL ES 3.0, and audio analysis. Preset shaders are written in HLSL and are
translated to GLSL at runtime with the vendored [hlslparser](https://github.com/unknownworlds/hlslparser).

It ships as an Apple XCFramework (Objective-C and Swift API, iOS and macOS), an Android AAR
(Java API), a WebAssembly build with an ES-module JavaScript API (WebGL2) and a Windows DLL with a C API
for x64 and arm64 (OpenGL ES through ANGLE on Direct3D 11), with sample apps for each platform.

## Modules

| Path | Contents |
|---|---|
| [`include/synesthesia-vibe/`](include/synesthesia-vibe) | Public C API (`synesthesia-vibe.h`, `sv_` prefix) |
| [`src/`](src) | Core library: preset parser, audio analysis, renderer (`src/render/gl`), configuration, platform utilities |
| [`vendor/`](vendor) | Vendored third-party code: `hlslparser`, `glad` (GL 3.3 Core and GLES 3.0 loaders), `stb` |
| [`platform/default/`](platform/default) | Portable helpers, such as the WAV reader |
| [`platform/darwin/`](platform/darwin) | Apple wrapper: Objective-C and Swift API, and the Swift package used by the samples |
| [`platform/ios/`](platform/ios), [`platform/macos/`](platform/macos) | Apple view classes, and the XCFramework build script |
| [`platform/android/`](platform/android) | Android library (Gradle project, JNI bridge and Java API), and the AAR build script |
| [`platform/web/`](platform/web) | Web library: the Emscripten wasm target, the ES-module JavaScript API (`js/`), and the dist build script |
| [`platform/windows/`](platform/windows) | Windows host: `synesthesia-vibe.dll` with a C ABI (`svw_` prefix) over ANGLE, and the dist build script |
| [`samples/ios/`](samples/ios), [`samples/macos/`](samples/macos) | UIKit, SwiftUI and AppKit sample apps, opened through [`samples/apple.xcworkspace`](samples/apple.xcworkspace) |
| [`samples/android/`](samples/android) | Jetpack Compose sample app |
| [`samples/web/`](samples/web) | Plain-JavaScript sample page (no bundler) |
| [`samples/windows/`](samples/windows) | C console sample: one window, deterministic capture, or a sync group across monitors |
| [`resources/mvp-presets/`](resources/mvp-presets) | The 10 presets the library is validated against |
| [`resources/beatdrop/`](resources/beatdrop) | Presets, textures and the preset list (`capture-set.json`) that the sample apps bundle |
| [`tests/unit/`](tests/unit) | Unit tests (GoogleTest, run through CTest) |
| [`tests/tools/`](tests/tools) | Desktop check, capture and comparison tools |
| [`triplets/`](triplets) | vcpkg overlay triplets for the Apple and Windows targets |
| [`.github/`](.github) | Release workflow and its build scripts, and the funding link (`FUNDING.yml`) |

## Platforms

| Target | Minimum | Graphics |
|---|---|---|
| iOS | 13 (SwiftUI sample: 15) | OpenGL ES 3.0 |
| macOS | 12 (SwiftUI sample: 15) | OpenGL 3.3 Core |
| Android | API 26 | OpenGL ES 3.0 |
| Web | Chrome/Edge 95, Firefox 100, Safari 15.2 | WebGL2 |
| Windows (x64, arm64) | 10 version 2004 (build 19041) | OpenGL ES 3.0 (`gles` backend) through ANGLE on Direct3D 11 |

Linux is not a release target.

## Playback, validation and comfort settings

These settings are the same on every platform. The C API names come first, and the wrapper names follow
in the tables below.

- **Frame-rate cap:** `sv_config.max_fps`. 0 means uncapped. The view loops pace rendering to it, and it
  is fixed when the context is created. The cap does not change animation speed.
- **Playback rate:** `sv_set_playback_rate(ctx, rate)` sets the simulation speed at runtime. The rate is
  clamped to [0.25, 4] and defaults to 1. Interactive contexts advance in steps of one display period (30 to 120 Hz), so motion
  looks the same at 30, 60 or 120 fps. Below 1× the simulation steps are the 1× steps and the frames between them are
  motion-compensated in-betweens, which show the picture one step late; live audio keeps driving the visuals. A device
  that cannot keep up keeps its display rate and slows the simulation instead. Deterministic (capture) contexts ignore it.
- **Luminance and flash limit:** `sv_config.max_luminance` (0 = off, otherwise a soft ceiling in (0, 1])
  and `sv_config.flash_limit` (0 = off, otherwise the largest rise in mean luminance per second). Both are
  off by default, fixed at creation and only ever darken the image. The flash limit is measured in
  simulation time, so a playback rate above 1 allows a proportionally larger rise per displayed second.
- **Preset validation:** `sv_validate_preset(path, error, len)` and
  `sv_validate_preset_memory(text, len, error, len)` check a preset without a context or GL. They return
  `SV_OK`, `SV_ERR_IO`, `SV_ERR_PARSE` (not a preset, or a code error) or `SV_ERR_SHADER` (a shader that
  does not translate). They may run on any thread, once a context has been created in the process.
- **Loading off the GL thread:** `sv_prepare_preset(ctx, path, &prepared, error, len)` and
  `sv_prepare_preset_memory(ctx, text, len, &prepared, error, len)` read, parse, compile and translate a
  preset on any thread while the context keeps rendering. `sv_commit_preset(ctx, prepared, blend_seconds)`
  then finishes the load on the GL thread in a few milliseconds: init code, random values, the blend and
  the GL programs. Errors and codes match `sv_load_preset*`; a prepare failure gives no handle and its text
  goes to the error buffer, not `sv_last_error`. A handle commits once, to its own context; free it with
  `sv_prepared_preset_free` on any thread, committed or not, even after `sv_destroy`. Do not destroy a
  context while one of its prepares runs. Run prepares on a thread with at least 1 MB of stack.
- **Stricter loading:** `sv_load_preset` and `sv_load_preset_memory` return `SV_ERR_PARSE` for an empty
  file or text that has no `[preset00]` section and no preset key.
- **Preset info:** `sv_get_preset_info(ctx, &info)` reports the loaded preset's `preset_version`, the warp and
  composite pixel-shader versions (0 none, 2 `ps_2_0`, 3 `ps_2_x`, 4 `ps_3_0`), whether each stage runs a shader,
  and a `fallback_reason` text when a stage that asked for a shader runs fixed-function instead. It may be called
  from any thread and returns the values of the last successful load or commit. The reason may contain newlines,
  and reads `no GL pipeline` or `GL pipeline not ready` when there is no usable GL pipeline. A file name ending in
  `.milk2` (MilkDrop 3 double preset) is rejected with `SV_ERR_PARSE` by load, prepare and validate.
- **Debug info:** `sv_set_debug_callback(ctx, fn, user)` and `sv_set_debug_interval(ctx, seconds)` turn on
  a periodic `sv_debug_info` report. It is off by default and costs nothing when off. The interval defaults
  to 1 s, and `<= 0` disables it. The report covers time, frame, measured FPS, simulation steps per frame,
  playback rate, preset version and name, shader stages and their pixel-shader versions, the fallback reason, audio levels, sizes, GL errors since the last
  report, the limiter and the last error. It is delivered from `sv_render_frame` on wall-clock time, so a
  paused view whose loop stops also stops reporting. The struct and its strings are valid only during the
  callback. Deterministic captures are unaffected.
- **Preset blending:** `sv_load_preset*` and `sv_commit_preset` with a positive `blend_seconds` cross-fade from the
  old preset to the new one on both render paths (shader and fixed-function), with per-vertex timing so the new
  preset appears in a shape. `sv_set_blend_pattern(ctx, pattern)` picks that shape: `SV_BLEND_PATTERN_RANDOM`
  (default, a new random pattern per blend) or one of the 19 patterns `0` to `SV_BLEND_PATTERN_COUNT - 1`
  (Fade, Directional Wipe, Plasma, Radial, Clock, Spiral/Snail, Rhombus/Diamond, Nuclear Clock, Square/Diamond,
  Checkerboard Wipe, Curtain, Bubble, Kaleidoscope, Moebius Strip, Star, Disco Floor, Fire/Flame, Whirlpool Drain,
  Julia Fractal), the same set as BeatDrop. A blend of 0 is a hard cut and is unchanged from before.
- **Automatic preset changes:** `sv_set_transition_config(ctx, config)` sets the blend a timer request suggests
  (2.7 s), the seconds between changes (16 s, plus up to 10 s drawn per preset; `<= 0` turns the timer off) and
  the hard cuts (off by default, with a loudness threshold of 2.5 and a half-life of 60 s); `NULL` restores the
  defaults. `sv_set_preset_request_callback(ctx, fn, user)` receives a `sv_preset_request` (reason timer or hard
  cut, and the suggested blend: 0 for a hard cut) from `sv_render_frame`, once per call and after the frame is
  presented. The engine never picks a preset: the callback loads one with `sv_load_preset*` or
  `sv_commit_preset`. The request stays latched, and no further one fires, until a load succeeds; a failed load
  restarts the timer at the next `sv_render_frame`. Timers and blends run on simulation time, so a host that renders
  below 15 frames per second sees them slow down. A context with no callback behaves as before.

| Setting | Apple (Objective-C / Swift) | Android (Java) | Web (JavaScript) |
|---|---|---|---|
| Frame-rate cap | `SVContextConfiguration.maxFps`, runtime `SVView.preferredFramesPerSecond` (Swift `SynesthesiaPlayer.preferredFramesPerSecond`) | `Builder.setMaxFps` | `config.maxFps` |
| Playback rate | `SVContext.playbackRate`, `SVView.playbackRate` (Swift `SynesthesiaPlayer.playbackRate`) | `SynesthesiaContext.setPlaybackRate`, `SynesthesiaRenderer.setPlaybackRate` | `SynesthesiaContext.setPlaybackRate`, `SynesthesiaCanvas.setPlaybackRate` |
| Limiter | `SVContextConfiguration.maxLuminance`, `.flashLimit` | `Builder.setMaxLuminance`, `.setFlashLimit` | `config.maxLuminance`, `config.flashLimit` |
| Debug info | `SVContext` / `SVView` `debugInfoHandler`, `debugInterval`, `debugEnabled` (Swift `SynesthesiaPlayer.debugInfo`) | `SynesthesiaContext.setDebugListener`, `setDebugInterval`; `SynesthesiaRenderer.setDebugListener`, `setDebugInterval` | `SynesthesiaContext.onDebugInfo`, `setDebugInterval`; `SynesthesiaCanvas.onDebugInfo`, `setDebugInterval` |
| Transitions | `SVContext` / `SVView` `transitionConfiguration` (`SVTransitionConfiguration`), `blendPattern` (`SVBlendPattern`, `SVBlendPatternName`), `presetRequestHandler` (`SVPresetRequest`, called on the render thread after the frame); Swift `SynesthesiaPlayer.transitionConfiguration`, `blendPattern`, `presetRequestHandler` | `SynesthesiaContext` / `SynesthesiaRenderer` `setTransitionConfiguration(TransitionConfiguration)` (`null` = defaults), `setBlendPattern(int)` (`BlendPattern`), `setPresetRequestListener(PresetRequestListener)` (called on the GL thread) | `SynesthesiaContext` / `SynesthesiaCanvas` `setTransitionConfig({...})`, `setBlendPattern(n)` (`BLEND_PATTERN_NAMES`), `onPresetRequest(handler)`; re-applied after context restore |
| Validation | `+[SVContext validatePresetAtURL:error:]`, `+validatePresetText:error:` (Swift `validatePreset(at:)`, `validatePreset(text:)`) | `SynesthesiaContext.validatePreset(File)`, `validatePresetText(String[, File])` | `SynesthesiaContext.validatePreset(module, path)`, `validatePresetText(module, text)` |
| Prepare / commit | `-[SVContext preparePresetAtURL:error:]`, `-preparePresetText:error:` → `SVPreparedPreset`, then `-commitPreparedPreset:blendSeconds:error:` (Swift `preparePreset(at:)`, `preparePreset(text:)`, `commitPreset(_:blendSeconds:)`; `SynesthesiaPlayer.loadPreset(at:)` prepares off the main thread) | `SynesthesiaContext.preparePreset(File)`, `preparePresetText(String)` → `PreparedPreset` (close it), then `commitPreset(PreparedPreset, float)` on the GL thread | `SynesthesiaContext.preparePreset(path)`, `preparePresetText(text)` → `PreparedPreset` (call `free()`), then `commitPreset(prepared, blendSeconds)`; `SynesthesiaCanvas.loadPreset` / `loadPresetText` are async |
| Sync group (mirror one render in several views) | `SVSyncGroup`, `SVView.syncGroup`, `SVView.syncScaling` (Swift `SynesthesiaPlayer.syncGroup`, `syncScaling`, `SynesthesiaView.syncGroup(_:scaling:)`) | `SynesthesiaSyncGroup`, `SynesthesiaRenderer.setSyncGroup`, `setSyncScaling(SyncScaling)` | `SynesthesiaSyncGroup.create(module, options)`, `SynesthesiaCanvas.syncGroup`, `syncScaling` |

- **Sync group:** off by default. One group context renders once and every member view presents the frame
  with Fill (default), Fit or Stretch; the group owns playback rate, transitions, debug info, render scale and the
  frame-rate cap while views are grouped, and a `deterministic` configuration is refused. Android needs
  `EGL_KHR_image_base`, `EGL_KHR_gl_texture_2D_image`, `EGL_KHR_fence_sync` and `GL_OES_EGL_image`, and a grouped
  view must call `release()` on its GL thread before it detaches (or `setSyncGroup(null)` / `pause()` first). The web
  group needs a second, already loaded module.
- **Android:** `SynesthesiaRenderer.setPlaybackRate` can be called from any thread; the renderer applies
  it on the GL thread before the next frame. `validatePresetText` writes the text to a temporary file in
  the directory you pass (for example `Context.getCacheDir()`), or in `java.io.tmpdir`, and throws
  `SV_ERR_IO` if it cannot.
- **Debug info delivery:** each wrapper hands over a copy keyed by the C field names, after the frame
  renders. Apple passes an `NSDictionary` on the main thread. Android passes a `Map<String, Object>` on
  the GL thread, outside the context lock, so post to the UI thread yourself. The web passes a plain
  object. Each wrapper re-applies the settings when its context is recreated.

### Apple view and audio inputs

- `SVView.preferredFramesPerSecond` changes the frame-rate cap at runtime. It starts at
  `configuration.maxFps`.
- `SVView.renderScale` renders at a fraction of the native resolution, from 0.25 to 1.
- `-[SVView snapshotWithCompletion:]` returns the next rendered frame as a `CGImage`.
- `-[SVView performWithContextCurrent:]` runs a block with the view's GL context current.
- `-[SVViewDelegate view:didRenderFrameWithDeltaTime:]` is called after each frame and before it is
  presented.
- The macOS view follows its window to another display and picks up that display's refresh rate.
- Swift `SynesthesiaPlayer.startSystemAudio()` (check `isSystemAudioSupported`) drives the system-audio input; one player can use either input.
- `SVAudioInput` and `SVSystemAudioInput` gain the following:
  - `inputGain`;
  - a `pcmHandler` block that receives the 44.1 kHz stereo PCM on the audio thread;
  - `+authorizationStatus` and `+requestAccessWithCompletion:`;
  - `SVErrorPermissionDenied` when microphone access is denied;
  - `SVSystemAudioInput.excludesCurrentProcessAudio`, which is YES by default.
- Inputs rebuild themselves when the audio device or its configuration changes.
- **One input drives every view in the process.** An input keeps feeding the visualizer when the
  context it was created with goes away, as long as another context is alive.

### Windows host

[`platform/windows/`](platform/windows) builds `synesthesia-vibe.dll` with a C ABI for P/Invoke. The header is
`include/synesthesia-vibe/synesthesia-vibe-windows.h` (`svw_` prefix); it includes the core header, and the DLL also
exports every `sv_*` function. ANGLE's `libEGL.dll` and `libGLESv2.dll` ship next to it. The host has three handles:

- `svw_view`, one per `HWND` you create and parent. `svw_view_create` makes the ANGLE (Direct3D 11) display, an
  OpenGL ES 3.0 context and the window surface, starts the view's render thread, paces frames and calls
  `sv_render_frame`. `engine` (a `sv_config*`, may be NULL) supplies the resource path, `max_fps`, `max_luminance`,
  `flash_limit` and the deterministic fields; its `width` and `height` are ignored, because the backing size follows
  the client area in physical pixels times the render scale.
- `svw_group`, a sync group (see below).
- `svw_audio_input`, a WASAPI loopback or microphone input (see below).

**Host capabilities: C API or host layer.**

| Capability | C API (`sv_*`) | Windows host (`svw_*`) |
|---|---|---|
| Frame-rate cap | `sv_config.max_fps`, fixed at creation | Host: `svw_view_set_preferred_fps`, `svw_group_set_preferred_fps` at runtime. Starts at `max_fps`; `<= 0` or above the refresh means the monitor's refresh rate |
| Playback rate | `sv_set_playback_rate` | Both: `svw_view_set_playback_rate`, `svw_group_set_playback_rate` call it on the render thread |
| Max luminance / flash limit | `sv_config.max_luminance`, `.flash_limit`, fixed at creation | C API, passed through the `engine` config of `svw_view_create` / `svw_group_create`. Fixed for the handle's life: recreate the handle to change it (no runtime setter in v1) |
| Preset validation without GL | `sv_validate_preset`, `sv_validate_preset_memory` | C API only, exported from the DLL; no handle needed |
| Render scale | none | Host only: `svw_view_set_render_scale`, `svw_group_set_render_scale`, clamped to [0.25, 1] |
| Input gain | none | Host only: `svw_audio_input_set_gain` |
| Sync group | none by design | Host only: `svw_group_*`, `svw_view_set_sync_group`, `svw_view_set_sync_scaling` |
| Snapshot | none | Host only: `svw_view_snapshot` (top-down RGBA8 callback), `svw_view_snapshot_png` (UTF-8 path) |
| Audio inputs, device change | PCM push only (`sv_push_pcm_float`, `sv_push_pcm_s16`) | Host only: `svw_audio_input_*` |
| Debug info, transitions, blend pattern, preset requests | `sv_set_debug_callback`, `sv_set_transition_config`, `sv_set_blend_pattern`, `sv_set_preset_request_callback` | C API, called inside `svw_view_invoke` / `svw_group_invoke`, which hand you the handle's `sv_context*` on its render thread |

**`sv_render_frame` and `target_fbo` on an ANGLE surface.** The host calls `sv_render_frame(ctx, dt, target_fbo)` on
the render thread with its EGL context current and the window surface bound as the draw and read surface. At render
scale 1, `target_fbo` is 0: the default framebuffer of the ANGLE window surface, which is the back buffer of its
Direct3D 11 swap chain. The engine binds `target_fbo` itself and draws the whole frame, then the host calls
`eglSwapBuffers`. Below scale 1 the host passes its own framebuffer at the backing size and blits it, linearly
filtered, to framebuffer 0. In a sync group the target is the group's offscreen framebuffer. Inside an
`svw_invoke_fn`, draw to the `target_fbo` you are given. Do not swap, make another context current or unbind the
surface. Framebuffer 0 has the usual GL bottom-up row order; snapshots come back top-down.

**`get_proc` under ANGLE.** The host calls `sv_init_gl` once per process with a function that returns
`eglGetProcAddress(name)`. ANGLE's `eglGetProcAddress` resolves the core OpenGL ES 3.0 entry points as well as
extensions. A caller that drives the C API on its own ANGLE context passes the same thing.

**Threads and callbacks.**

- Every `svw_*` call may run on any thread.
- Calls that return a result wait for the handle's render thread. These are create and destroy, preset loads,
  `svw_view_invoke`, `svw_group_invoke`, `svw_view_snapshot_png`, `svw_view_set_sync_group` and
  `svw_group_get_render_size`. While they wait they service sent window messages, so a UI thread cannot deadlock
  against the swap chain.
- Setters return at once and apply before the next frame. These are pause, resume, playback rate, preferred fps,
  render scale, fixed render size, sync scaling, gain and the PCM callback.
- `svw_invoke_fn` and `svw_snapshot_fn` run on the handle's render thread with its GL context current, never on the
  caller's thread. Inside them, `svw_*` calls on the same handle run at once.
- `svw_view_snapshot_png` from inside an `svw_invoke_fn` writes the frame last drawn to `target_fbo`. The
  `svw_snapshot_fn` pixel buffer is borrowed for the duration of the call; its stride is `width * 4`.
- A snapshot of a minimised window or one with no client area returns `SV_ERR_GL` at once. A request still pending
  when the window is minimised fails the same way, and its `svw_snapshot_fn` is not called.
- GL work of every view and group in the process is serialised: ANGLE's Direct3D 11 renderer is shared per
  adapter, so each render thread takes a process-wide turn and makes its context current for it. A slow callback
  delays every handle.
- `svw_pcm_fn` runs on the input's capture thread. Inside it, `svw_audio_input_stop`, `svw_audio_input_destroy` and
  setting the PCM callback return with `SV_ERR_ARG` set.
- Do not call `svw_view_create`, `svw_group_create`, `svw_view_destroy`, `svw_group_destroy` or
  `svw_view_set_sync_group` from a render-thread callback (they return with `SV_ERR_ARG` set). Do not make a waiting
  call on a different handle from a callback.
- Errors come back as `sv_status`. The message is in `svw_last_error(view)`, `svw_group_last_error(group)` or
  `svw_audio_input_last_error(input)`. With a NULL handle, these return the last error of a call on this thread that
  had no handle, such as a failed create. The returned string stays valid until the next `*_last_error` call on the
  same thread.
- A destroyed `HWND` gives `SV_ERR_ARG`. A failed EGL setup gives `SV_ERR_GL`. On a lost Direct3D device the view
  rebuilds its context and reloads the last preset, or reports `SV_ERR_GL`.
- Pause stops the render thread entirely (about 0 % CPU). Resume continues with no time jump.

**Sync group.** `svw_group_create(&config, engine, &group)` makes one headless context with its own render thread.
`svw_view_set_sync_group(view, group)` joins and `svw_view_set_sync_group(view, NULL)` leaves, live, from any
thread. `svw_view_set_sync_scaling` picks `SVW_SYNC_SCALING_FILL` (default), `_FIT` or `_STRETCH` per view.

- The group renders once at the per-axis maximum of its members' surface sizes times its render scale. That size
  only grows, an explicit render-scale change resizes both ways, and `svw_group_set_fixed_render_size` overrides it.
- It presents into every member's window surface with swap interval 0, paced at the lower of its cap and the
  highest refresh rate among the members' monitors.
- A member's own thread parks while it is grouped. While grouped, the group owns the playback rate, render scale,
  frame-rate cap and preset loads: `svw_view_load_preset*` and `svw_view_invoke` on a member go to the group. A
  member's own setters store their values, which apply again when it leaves.
- A paused member keeps its last frame. The group pauses when every member is paused or minimised.
- A member whose window is destroyed, or whose surface fails, leaves the group and the others keep rendering. If
  the group's own context is lost, or another handle's device-loss recovery re-creates the shared display, it releases
  every member back to its own context (each member then rebuilds) and reports `SV_ERR_GL`.
- Destroying a member or the group is safe in any order: a destroyed view leaves first, and a destroyed group
  releases its members.
- A grouped member's snapshot is the frame as presented, at the member's surface size.
- Members must use the group's adapter. A `deterministic` engine config is refused.

**Audio inputs.** `svw_audio_input_create(SVW_AUDIO_LOOPBACK or SVW_AUDIO_MICROPHONE, &input)`, then
`svw_audio_input_start` and `svw_audio_input_stop`.

- Loopback captures the default render endpoint in shared mode. The microphone uses the default capture endpoint.
- The capture thread converts the endpoint's mix format to **44.1 kHz, stereo, float32, interleaved**: a 5.1 or 7.1
  downmix, then linear resampling. It applies the gain and calls `sv_push_pcm_float` once per packet. This is the
  same format the Apple inputs deliver, and the `svw_pcm_fn` tap receives it too.
- The PCM goes into the engine's process-wide ring, so one input feeds every view and group, and a handle created
  later does not restart it. When the endpoint delivers nothing (loopback with nothing playing), the input pushes
  silence, so the levels fall to 0.
- `svw_audio_input_get_levels` returns `sv_get_audio_levels` of whichever handle rendered last, or 0 when none is
  running: the levels drop to 0 as soon as that handle pauses, joins a group, is minimised or is destroyed. `svw_audio_input_set_gain` takes any finite value of 0 or more.
- A change or removal of the default device rebuilds capture on the new default, retrying every 0.5 s while there is
  none. `svw_audio_input_device_name` and `svw_audio_input_last_error` report the new device.
- A microphone blocked by Windows privacy settings makes start return `SV_ERR_IO`, with a message naming
  *Settings > Privacy > Microphone*. A missing device also gives `SV_ERR_IO`.

**Deployment.** The DLL targets Windows 10 version 2004 (build 19041) or later, x64 or arm64. It links the C runtime
dynamically, so the machine needs the Microsoft Visual C++ 2015-2022 Redistributable for the same architecture
(`MSVCP140.dll`, `VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`), or those DLLs app-local. Keep `libEGL.dll` and
`libGLESv2.dll` in the same folder as `synesthesia-vibe.dll`. For P/Invoke, use `CallingConvention.Cdecl`, and
marshal the `bool` fields of `svw_view_config` and `svw_group_config` as one byte (`UnmanagedType.U1`). Strings are
UTF-8: pass path and text arguments with `[MarshalAs(UnmanagedType.LPUTF8Str)]`, and declare functions that return
`const char*` (`*_last_error`, `svw_audio_input_device_name`) as returning `IntPtr` and read them with
`Marshal.PtrToStringUTF8`; a `string` return type makes the marshaller free memory the library owns. A returned string
stays valid only until the next `svw_*` call on the same thread, so copy it right away. Parenting the
window, for example behind the desktop icons, is the app's job.

## Releases

Releases are built by [`.github/workflows/release.yml`](.github/workflows/release.yml) and published on
this repository's Releases page. Every file name carries the version `<v>`:

| Asset | Notes |
|---|---|
| `SynesthesiaVibeCore-<v>.xcframework.zip` + `.zip.checksum` | iOS device, iOS Simulator (arm64 + x86_64) and macOS (arm64 + x86_64) slices. The checksum is from `swift package compute-checksum` |
| `synesthesiavibe-<v>.aar` + `synesthesiavibe-<v>.pom` | Android library, `com.synesthesiavibe:synesthesiavibe:<v>` |
| `synesthesiavibe-sample-ios-uikit-<v>.zip`, `synesthesiavibe-sample-ios-swiftui-<v>.zip` | iOS Simulator `.app` (arm64 + x86_64), unsigned |
| `synesthesiavibe-sample-macos-appkit-<v>.zip`, `synesthesiavibe-sample-macos-swiftui-<v>.zip` | Universal macOS `.app`, ad-hoc signed, not notarized |
| `synesthesiavibe-sample-android-<v>.apk` | Android sample, release-signed |
| `synesthesia-vibe-web-<v>.zip` | Web library dist: `synesthesia-vibe-wasm.{js,wasm}`, the ES-module JavaScript API and a `VERSION` stamp |
| `synesthesia-vibe-windows-x64-<v>.zip`, `synesthesia-vibe-windows-arm64-<v>.zip` | Windows library dist: `bin/` (`synesthesia-vibe.dll`, ANGLE `libEGL.dll` and `libGLESv2.dll`), `lib/synesthesia-vibe.lib`, both headers under `include/synesthesia-vibe/`, `NOTICE` and `VERSION`. Needs the Visual C++ 2015-2022 Redistributable |
| `SHA256SUMS.txt` | sha256 of every other asset |

- **Sample controls.** The samples share one design: a preset playlist, a source picker (WAV, microphone, system sound where supported), a frame-rate set of 30 / 60 / 120 / Uncapped (default 60), an Auto-change toggle with an interval (the engine's timer asks for the next preset), a blend duration (0 to 10 s, default 2 s), a Transition picker (Random and the 19 patterns), a Hard cuts toggle and a debug overlay (shift-cmd-D on macOS). They have no speed, render-scale, snapshot or WAV-export controls.
- **The iOS samples run in the Simulator only.** They are not signed for a device. Install one with
  `xcrun simctl install booted <App>.app`.
- **Gatekeeper blocks the first launch of a macOS sample**, because a downloaded app is quarantined and the
  samples are not notarized. Right-click the app and choose **Open**, or run
  `xattr -dr com.apple.quarantine <App>.app`.
- A prerelease version (for example `0.0.1-rc.1`) is published as a GitHub Pre-release.
- Verify a download with `shasum -a 256 -c SHA256SUMS.txt --ignore-missing`.

## Building from source

### Prerequisites

- CMake 3.25 or newer, and Ninja.
- [vcpkg](https://github.com/microsoft/vcpkg), with `VCPKG_ROOT` exported in every shell that runs a build
  (for example `export VCPKG_ROOT=$HOME/vcpkg`). The build scripts default to `~/vcpkg` when it is unset.
  Dependencies are declared in [`vcpkg.json`](vcpkg.json) and pinned by
  [`vcpkg-configuration.json`](vcpkg-configuration.json).
- Apple targets: Xcode. [XcodeGen](https://github.com/yonaskolb/XcodeGen) only if you change a sample's
  `project.yml`.
- Android targets: the Android SDK with NDK `28.2.13676358` and CMake `4.4.3`. Create a `local.properties`
  in `platform/android/` with `sdk.dir` and `cmake.dir`, and one in `samples/android/` with `sdk.dir`.
  These files are ignored by git.
- Windows targets: Visual Studio 2022 or newer with the C++ desktop workload (MSVC 14.4x and, for arm64, the
  ARM64 build tools), Git Bash, CMake and Ninja on `PATH`.
- Web target: [emsdk](https://github.com/emscripten-core/emsdk) `6.0.12`, activated in the shell that runs
  the build (`source <emsdk>/emsdk_env.sh` exports `EMSDK`), and the `zip` CLI for `--zip`.

The library version is the root [`VERSION`](VERSION) file. Every build script accepts `--help`.

> On macOS, `cmake` can hang when its working directory and `PWD` are the repository root. The build
> scripts already work around this. When you run `cmake` yourself, run it from another directory with
> `PWD` unset, as in the desktop example below.

### Apple XCFramework

```sh
platform/ios/build-xcframework.sh
platform/ios/build-xcframework.sh --slices ios-sim-arm64,macos-arm64
```

The first form builds all five slices (`ios-arm64`, `ios-sim-arm64`, `ios-sim-x64`, `macos-arm64`,
`macos-x64`). The second is a quicker build with only two slices. Output:
`build/xcframework/SynesthesiaVibeCore.xcframework`, a zip of it, and its SPM checksum.

### Apple samples

Build the XCFramework first. The samples depend on the local Swift package in `platform/darwin`, which
points at `build/xcframework/`.

- Open [`samples/apple.xcworkspace`](samples/apple.xcworkspace) in Xcode and run a scheme:
  `SynesthesiaVibeUIKit`, `SynesthesiaVibeSwiftUI`, `SynesthesiaVibeAppKit` or `SynesthesiaVibeMacSwiftUI`.
- From the command line:

  ```sh
  xcodebuild -workspace samples/apple.xcworkspace -scheme SynesthesiaVibeAppKit \
      -destination 'platform=macOS' build
  ```

- To build the release zips the way CI does:

  ```sh
  .github/scripts/build-apple-samples.sh
  .github/scripts/build-apple-samples.sh --samples macos-appkit,ios-uikit
  ```

  Output: `build/samples/synesthesiavibe-sample-<sample>-<v>.zip`.

Each sample's Xcode project is generated from its `project.yml`. After changing one, run
`xcodegen generate` in that sample's folder.

### Android AAR

```sh
platform/android/build-aar.sh
platform/android/build-aar.sh --skip-publish
```

The first form builds the release AAR, publishes `com.synesthesiavibe:synesthesiavibe:<v>` to
`mavenLocal()`, and copies the AAR and POM to `build/android/`. With `--skip-publish` it only builds and
copies the AAR.

### Android sample

The sample builds the library from source through a Gradle composite build.

```sh
cd samples/android && ./gradlew :app:assembleDebug
```

To build the release APK the way CI does:

```sh
.github/scripts/build-android-sample.sh --debug
.github/scripts/build-android-sample.sh --release
```

Output: `build/samples/synesthesiavibe-sample-android-<v>[-debug].apk`. `--release` signs with the
keystore named by `ANDROID_KEYSTORE_FILE`, `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS` and
`ANDROID_KEY_PASSWORD`; all four must be set.

### Web library

```sh
EMSDK=$HOME/emsdk platform/web/build-web.sh
EMSDK=$HOME/emsdk platform/web/build-web.sh --zip
```

Output: `build/web/dist/`, a flat folder with `synesthesia-vibe-wasm.js`, `synesthesia-vibe-wasm.wasm`,
the JavaScript modules and a `VERSION` file. `--zip` also writes `build/web/synesthesia-vibe-web-<v>.zip`,
the release asset. There is no npm package and no bundler: copy the dist folder next to your page and
import `synesthesia-vibe.js`, or map the bare specifier `synesthesia-vibe` to it with an import map.

A minimal integration: `const module = await loadSynesthesiaModule()`, then
`await new ResourceLoader(module).load("manifest.json")` for your presets and textures, then
`const view = await SynesthesiaCanvas.create(canvas, module, { onError })` and
`await view.loadPreset("/synesthesiavibe/presets/<name>.milk")` (it prepares, waits one animation frame, then
commits; the latest call wins). Audio comes from a user gesture through
`AudioInput.fromMicrophone(view)`, `AudioInput.fromMediaElement(view, audioElement)` or a `WavFeeder`.
The page needs a secure context (`localhost` or HTTPS) for the microphone; no COOP/COEP headers are
required.

### Web sample

```sh
EMSDK=$HOME/emsdk .github/scripts/build-web-sample.sh
EMSDK=$HOME/emsdk .github/scripts/build-web-sample.sh --skip-library --serve 8000
```

The first form builds the library, then assembles `build/samples/web/` from [`samples/web/`](samples/web),
the dist folder and the presets, textures and test WAV under `resources/`. `--serve` hosts it on
`127.0.0.1`. The sample page has a Start button, a source selector (WAV, microphone, audio file, and tab
audio on Chromium), preset switching with blending (a Transition picker, a blend duration of 0 to 10 s, Auto-change and Hard cuts toggles) and bass/mid/treble meters.

### Windows library

From Git Bash (the script loads the MSVC environment itself through `vswhere` and `vcvarsall.bat`):

```sh
bash platform/windows/build-windows.sh --zip
bash platform/windows/build-windows.sh --arch arm64 --zip
```

Output: `build/windows/dist-<arch>/`, holding:

- `bin/`: `synesthesia-vibe.dll`, `libEGL.dll` and `libGLESv2.dll`;
- `lib/`: `synesthesia-vibe.lib`;
- `include/synesthesia-vibe/`: `synesthesia-vibe.h` and `synesthesia-vibe-windows.h`;
- `NOTICE` and `VERSION`.

`--zip` also writes `build/windows/synesthesia-vibe-windows-<arch>-<v>.zip`, the release asset. The script uses the
`windows-x64` or `windows-arm64` CMake preset and checks every name in
[`platform/windows/synesthesia-vibe.def`](platform/windows/synesthesia-vibe.def) against `dumpbin /exports`. arm64 is
cross-compiled with the x64-hosted tools. `--vcvars-ver 14.44` picks an MSVC toolset, and `--skip-configure`
rebuilds only. `VCPKG_ROOT` defaults to the vcpkg bundled with Visual Studio. The first build compiles ANGLE from
vcpkg, which takes about 20 minutes and 5 GB of disk.

### Windows sample

```sh
bash .github/scripts/build-windows-sample.sh --arch x64
```

This builds [`samples/windows/`](samples/windows), a C console program that uses only the dist, into
`build/samples/windows-<arch>/`, with the three DLLs next to the exe. To build it yourself from a Visual Studio
developer prompt, run `cmake -S samples/windows -B <dir> -G Ninja -DSYNESTHESIA_VIBE_DIST=<dist folder>`. Presets
and resources are found by walking up from the exe to `resources/`; `--help` lists every option.

- **Single window** (default): cycles `resources/mvp-presets`. Keys: Space pause, N/P preset, 1-4 render scale, C cap,
  S snapshot, Esc quit.
- **`--capture DIR`**: renders every preset deterministically and writes `DIR\<preset>.png`. `--warp` uses the
  software Direct3D 11 adapter.
- **`--sync`**: one window per monitor, or two side by side on a single monitor, in a sync group, with loopback audio
  and a level meter in the title. Windows join and leave live when a monitor is added or removed. Keys: F
  Fill/Fit/Stretch, G leave or join the group, Up/Down gain, M microphone or loopback, A add a window, W close one,
  S snapshot every member, Space pause.

### Desktop library and unit tests (macOS)

The `macos-arm64` preset builds the library with OpenGL 3.3, the desktop tools and the unit tests.

```sh
cd /tmp
export VCPKG_ROOT=$HOME/vcpkg
env -u PWD cmake -S <repo> --preset macos-arm64
env -u PWD cmake --build <repo>/build/macos-arm64
<repo>/tests/unit/run-unit-tests.sh --build-dir <repo>/build/macos-arm64
```

On macOS arm64 the suite reports `94% tests passed, 2 tests failed out of 36`. This is the expected
result:

- Two tests fail: `RngCompat.UniformIntSizeTZeroWidthReimplMatchesReferenceHash` and
  `RngCompat.UniformIntSizeTReimplMatchesReferenceHash`.
- Seven tests are skipped because they need reference data that is not included in this repository:
  `AudioFftLoudnessTest.MatchesPhase2DumpAudioReferenceAcrossAllCapturesAndFrames`,
  `NoiseLevel0Test.MatchesPhase2DumpNoiseReference` and the five `ProjectMEvalQConformance.*` tests.

`run-unit-tests.sh` is a thin wrapper around `ctest`; `--filter <regex>` runs a subset. The other configure
presets in [`CMakePresets.json`](CMakePresets.json) (`macos-x64`, `ios-*`, `android-*`, `web-wasm32*`) are
used by the packaging scripts above.

### Publishing a release

The release version lives in the [`VERSION`](VERSION) file. To publish, push a tag `v<VERSION>` on the
commit to release. The tag must match the file exactly. The workflow builds
every asset above, writes `SHA256SUMS.txt`, and creates the GitHub Release. It never overwrites an existing
release. Signing the Android sample needs four repository secrets: `ANDROID_KEYSTORE_BASE64`,
`ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS` and `ANDROID_KEY_PASSWORD`. A manual run of the workflow
(`workflow_dispatch`) builds everything without publishing.

## Contributing

Issues and pull requests are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening one.

## Support

If SynesthesiaVibe is useful to you, you can leave a tip or buy me a coffee on
[Ko-fi](https://ko-fi.com/lee5783).

## License

SynesthesiaVibe is released under the [MIT License](LICENSE).

It is derived from, and redistributes, third-party code and content under their own licenses. The full
notices are in [NOTICE](NOTICE):

- [BeatDrop](https://github.com/OfficialIncubo/BeatDrop-Music-Visualizer) (originally
  [mvsoft74/BeatDrop](https://github.com/mvsoft74/BeatDrop)) and MilkDrop 2 by Nullsoft: BSD 3-Clause.
  See also [projectM](https://github.com/projectM-visualizer/projectm), the open-source MilkDrop
  reimplementation.
- The presets and textures under [`resources/`](resources), from the BeatDrop distribution. BeatDrop's
  preset notice, reproduced in [NOTICE](NOTICE), states that most MilkDrop presets converted from ShaderToy
  are licensed CC BY-NC-SA 3.0 and may not be used commercially.
- [hlslparser](https://github.com/unknownworlds/hlslparser), via
  [projectM](https://github.com/projectM-visualizer/projectm): MIT.
- [glad](https://github.com/Dav1dde/glad) generated loaders: (WTFPL or CC0-1.0) and Apache-2.0. The Khronos
  `khrplatform.h` header: MIT-style.
- [stb](http://nothings.org/stb) (`stb_image`, `stb_image_write`): MIT or public domain.
- [projectm-eval](https://github.com/projectM-visualizer/projectm-eval), the projectM Expression Evaluation
  Library, and its ns-eel2 compatibility shim in
  [`src/parser/ns-eel2-shim/`](src/parser/ns-eel2-shim): MIT.
- [Emscripten](https://emscripten.org), whose runtime support code is embedded in the generated
  `synesthesia-vibe-wasm.js` and `.wasm` of the web release asset: MIT or University of Illinois/NCSA.
