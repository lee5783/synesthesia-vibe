# SynesthesiaVibe

SynesthesiaVibe is a cross-platform C++ music visualizer library that plays MilkDrop 2 presets. It is a
port of [BeatDrop](https://github.com/OfficialIncubo/BeatDrop-Music-Visualizer), a MilkDrop 2 based
visualizer for Windows and DirectX 9, to portable code: a preset parser, an OpenGL 3.3 Core renderer
that also builds as OpenGL ES 3.0, and audio analysis. Preset shaders are written in HLSL and are
translated to GLSL at runtime with the vendored [hlslparser](https://github.com/unknownworlds/hlslparser).

It ships as an Apple XCFramework (Objective-C and Swift API, iOS and macOS) and an Android AAR
(Java API), with sample apps for each platform.

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
| [`samples/ios/`](samples/ios), [`samples/macos/`](samples/macos) | UIKit, SwiftUI and AppKit sample apps, opened through [`samples/apple.xcworkspace`](samples/apple.xcworkspace) |
| [`samples/android/`](samples/android) | Jetpack Compose sample app |
| [`resources/mvp-presets/`](resources/mvp-presets) | The 10 presets the library is validated against |
| [`resources/beatdrop/`](resources/beatdrop) | Presets, textures and the preset list (`capture-set.json`) that the sample apps bundle |
| [`tests/unit/`](tests/unit) | Unit tests (GoogleTest, run through CTest) |
| [`tests/tools/`](tests/tools) | Desktop check, capture and comparison tools |
| [`triplets/`](triplets) | vcpkg overlay triplets for the Apple targets |
| [`.github/`](.github) | Release workflow and its build scripts |

## Platforms

| Target | Minimum | Graphics |
|---|---|---|
| iOS | 13 (SwiftUI sample: 15) | OpenGL ES 3.0 |
| macOS | 12 (SwiftUI sample: 15) | OpenGL 3.3 Core |
| Android | API 26 | OpenGL ES 3.0 |

Windows and Linux are not release targets.

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
| `SHA256SUMS.txt` | sha256 of every other asset |

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
presets in [`CMakePresets.json`](CMakePresets.json) (`macos-x64`, `ios-*`, `android-*`) are used by the
packaging scripts above.

### Publishing a release

The release version lives in the [`VERSION`](VERSION) file. To publish, push a tag `v<VERSION>` on the
commit to release. The tag must match the file exactly. The workflow builds
every asset above, writes `SHA256SUMS.txt`, and creates the GitHub Release. It never overwrites an existing
release. Signing the Android sample needs four repository secrets: `ANDROID_KEYSTORE_BASE64`,
`ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS` and `ANDROID_KEY_PASSWORD`. A manual run of the workflow
(`workflow_dispatch`) builds everything without publishing.

## Contributing

Issues and pull requests are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening one.

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
