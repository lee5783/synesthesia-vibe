cmake_minimum_required(VERSION 3.25)
project(SynesthesiaVibe LANGUAGES C CXX)

# P2-010: SYNESTHESIA_VIBE_GL_DEV marks the macos-arm64 and windows-gl-dev presets, which configure the
# portable/GL side of the tree instead of the frozen DX9 exe. The DX9-toolchain FATAL_ERROR checks
# below, the DXSDK_DIR requirement and the beatdrop-core/beatdrop-demo-windows branch further down
# all apply only when this is OFF, i.e. only to the DX9 presets (x86-release, portable-only).
option(SYNESTHESIA_VIBE_GL_DEV "Configure the portable/GL dev tree (macos-arm64, windows-gl-dev) instead of the DX9 exe" OFF)

if(NOT SYNESTHESIA_VIBE_GL_DEV)
  if(NOT EXISTS ${CMAKE_SOURCE_DIR}/apps/beatdrop-demo-windows)
    message(FATAL_ERROR "This source tree has no DX9 build. Configure with a CMake preset (cmake --list-presets), e.g. cmake --preset macos-arm64, or pass -DSYNESTHESIA_VIBE_GL_DEV=ON.")
  endif()
  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    message(FATAL_ERROR "The DX9 presets require the Visual Studio generator (v143, toolset 14.44) - use the x86-release CMake preset.")
  endif()
  if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
    message(FATAL_ERROR "The DX9 presets build x86 (Win32) only - use the x86-release CMake preset (-A Win32).")
  endif()
endif()
if(MSVC AND NOT CMAKE_CXX_COMPILER MATCHES "14\\.44\\.35207")
  message(WARNING "Expected MSVC toolset 14.44.35207, got compiler path ${CMAKE_CXX_COMPILER}. Build flags must not drift silently - stop and report if this differs.")
endif()

set(SYNESTHESIA_VIBE_BACKEND "gl" CACHE STRING "Render backend of the portable/GL dev tree: gl (desktop OpenGL 3.3 Core) or gles (OpenGL ES 3.0)")
set_property(CACHE SYNESTHESIA_VIBE_BACKEND PROPERTY STRINGS gl gles)
if(NOT SYNESTHESIA_VIBE_BACKEND MATCHES "^(gl|gles)$")
  message(FATAL_ERROR "SYNESTHESIA_VIBE_BACKEND must be gl or gles, got '${SYNESTHESIA_VIBE_BACKEND}'.")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^(Darwin|Windows|Linux)$" AND NOT CMAKE_CROSSCOMPILING)
  set(SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS_DEFAULT ON)
else()
  set(SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS_DEFAULT OFF)
endif()
option(SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS "Build the desktop-only GLFW hosts, GoogleTest suite and tests/tools executables" ${SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS_DEFAULT})
if(CMAKE_SYSTEM_NAME MATCHES "^(iOS|Android)$")
  set(SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS OFF CACHE BOOL "Build the desktop-only GLFW hosts, GoogleTest suite and tests/tools executables" FORCE)
endif()

# P1-007: when ON, configure only the parser and audio analysis module targets (C-013's
# Windows-header scoping rule: audio/ analysis and parser/ must never include a Windows/D3D
# header). No platform/config/audio-capture/lyrics/render-dx9/apps target is configured, and no
# Windows/DX SDK include dir is added to any target - see the portable-only CMake preset and
# tests/tools/portable-only-check/check-portable-only.ps1, which parses /showIncludes for these
# two targets and fails if either reaches the Windows SDK (um/shared/winrt), the DX SDK, or a
# d3d* header. Audio is expected to pass; parser is expected to fail until P1-011.
option(SYNESTHESIA_VIBE_PORTABLE_ONLY "Configure only parser + audio, with no Windows/DX SDK include dirs (P1-007 Windows-header checker)" OFF)

cmake_policy(SET CMP0091 OLD)
cmake_policy(SET CMP0092 NEW)
set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)

set(CMAKE_CXX_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "" CACHE STRING "" FORCE)

if(ANDROID)
  add_compile_options(-fsigned-char)
endif()

if(NOT SYNESTHESIA_VIBE_PORTABLE_ONLY AND NOT SYNESTHESIA_VIBE_GL_DEV)
  if(NOT DEFINED ENV{DXSDK_DIR})
    message(FATAL_ERROR "DXSDK_DIR is not set - install the DirectX SDK (June 2010) and set the DXSDK_DIR environment variable.")
  endif()
  file(TO_CMAKE_PATH "$ENV{DXSDK_DIR}" BEATDROP_DXSDK_DIR)
endif()

find_package(projectM-Eval REQUIRED COMPONENTS Milkdrop)

# P2-010: SYNESTHESIA_VIBE_ENABLE_VIDEO_TEXTURE gates the FFmpeg-based video-texture support
# (MediaTexture.cpp, portable_dll_loader.cpp). It defaults ON only for the DX9 presets, so the DX9
# exe's FFmpeg dependency is unchanged; it is OFF off-Windows and for the GL dev presets, where
# FFmpeg is not (yet) a build dependency at all (vcpkg.json marks ffmpeg "platform: windows").
if(WIN32 AND NOT SYNESTHESIA_VIBE_GL_DEV)
  set(SYNESTHESIA_VIBE_ENABLE_VIDEO_TEXTURE_DEFAULT ON)
else()
  set(SYNESTHESIA_VIBE_ENABLE_VIDEO_TEXTURE_DEFAULT OFF)
endif()
option(SYNESTHESIA_VIBE_ENABLE_VIDEO_TEXTURE "Enable FFmpeg-based video texture support (MediaTexture); requires FFmpeg" ${SYNESTHESIA_VIBE_ENABLE_VIDEO_TEXTURE_DEFAULT})

if(SYNESTHESIA_VIBE_ENABLE_VIDEO_TEXTURE)
  find_path(FFMPEG_INCLUDE_DIR libavformat/avformat.h REQUIRED)
endif()

set(BEATDROP_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(BEATDROP_APPS_HOST_DIR ${CMAKE_SOURCE_DIR}/apps/beatdrop-demo-windows)

# plugin.vcxproj Release_x86|Win32 ClCompile preprocessor defines (shared by every src/ module
# target and the beatdrop-demo-windows host, since the host TU sits in the same
# ItemDefinitionGroup)
set(BEATDROP_REF_COMPILE_DEFINITIONS
  _WIN10_BUILD
  WIN32
  NDEBUG
  _WINDOWS
  USE_VIS_HDR_HWND
  STRSAFE_NO_DEPRECATE
  NSEEL_REENTRANT_EXECUTION
  _CRT_SECURE_NO_WARNINGS
  _CRT_NON_CONFORMING_SWPRINTFS
  _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
  _VC80_UPGRADE=0x0710
  _MBCS
)

# plugin.vcxproj Release_x86|Win32 ClCompile flags, verbatim (see CL.command.1.tlog)
set(BEATDROP_REF_COMPILE_OPTIONS
  /Zi /nologo /W3 /WX- /diagnostics:column
  /O1 /Ob2 /Oy
  /GF /Gm- /EHsc /MD /GS- /Gy /fp:precise
  /Zc:wchar_t /Zc:forScope- /Zc:inline
  /std:c++17
  /external:W3 /Gd /TP /wd4996 /analyze- /FC
)

## P2-030: vendor/glad (C2-008) - the GL 3.3 Core loader generated by glad2 2.0.8 (see
## vendor/glad/GLAD-VENDOR.md for the exact generator command line). The include root below is
## what makes both <glad/gl.h> and its nested <KHR/khrplatform.h> reachable.
set(BEATDROP_GLAD_DIR ${CMAKE_SOURCE_DIR}/vendor/glad)

# P1-001 (verbatim relocation): beatdrop-ref's ref-src/-path TU list above is replaced by the
# src/-path module TU lists below. Every file here is byte-identical to its ref-src counterpart
# (see develop/evidence/P1-001/hash-identity.md) - only the compiled path changed.
#
# P1-001's placement map splits what used to be one flat vis_milk2/+audio/+spoutDX9/ directory
# set into src/{platform,config,audio,audio/capture,parser,lyrics,render/dx9} plus
# apps/beatdrop-demo-windows/{,spoutDX9}. That nesting breaks a handful of ref-src's own
# "../sibling-dir/foo.h" relative includes (they assumed the old flat layout). Rather than edit
# those bytes, small forwarding shim headers stand in at the exact relative path the untouched
# #include text still expects (src/ns-eel2-shim/, src/render/ns-eel2-shim/, src/render/spoutDX9/,
# src/render/audio/) - each one #includes the real file at its placement-map location. These are
# new files, not moved ones, so they carry the standard header.
# P2-012: beatdrop-platform split into a portable target (no apps/ include, usable by synesthesia-vibe
# and linkable with no Win32 TU) and the existing Win32 forwarders (unchanged in behavior). The
# portable side gives every parser_win32.h function a POSIX/portable body (English string table,
# wchar_t->UTF-8 fopen, C-locale float parse, portable local time), a portable RemoveExtension, and
# the NSEEL_HOSTSTUB_Enter/LeaveMutex definitions moved out of render/dx9/plugin.cpp.
set(BEATDROP_PLATFORM_WIN32_SOURCES
  ${BEATDROP_SRC_DIR}/platform/portable_dll_loader.cpp
  ${BEATDROP_SRC_DIR}/platform/utility.cpp
  ${BEATDROP_SRC_DIR}/platform/wasabi.cpp
  ${BEATDROP_SRC_DIR}/platform/parser_win32.cpp
)
# P2-012: parser_win32_portable.cpp duplicates every symbol beatdrop-platform-win32's
# parser_win32.cpp already defines (both implement the same parser_win32.h surface) - linking both
# into the same binary (the DX9 presets do, via beatdrop-core) is a silent LNK4006 "second
# definition ignored" duplicate-symbol collision, not a real multi-definition error, so it would
# otherwise go unnoticed while quietly swapping which body the DX9 exe actually calls. It is only
# ever the sole provider of that surface under SYNESTHESIA_VIBE_GL_DEV (windows-gl-dev, macos-arm64), where
# beatdrop-platform-win32 is never configured at all; path_utils.cpp/nseel_hoststub.cpp have no such
# duplicate and are always in the portable target.
set(BEATDROP_PLATFORM_PORTABLE_SOURCES
  ${BEATDROP_SRC_DIR}/platform/path_utils.cpp
  ${BEATDROP_SRC_DIR}/platform/nseel_hoststub.cpp
  ${BEATDROP_SRC_DIR}/platform/rng_compat.cpp
)
if(SYNESTHESIA_VIBE_GL_DEV)
  list(APPEND BEATDROP_PLATFORM_PORTABLE_SOURCES ${BEATDROP_SRC_DIR}/platform/parser_win32_portable.cpp)
endif()
set(BEATDROP_AUDIO_SOURCES
  ${BEATDROP_SRC_DIR}/audio/audiobuf.cpp
  ${BEATDROP_SRC_DIR}/audio/fft.cpp
  ${BEATDROP_SRC_DIR}/audio/sound_analysis.cpp
)
# P2-013: audiobuf.{h,cpp} (the ring core - circular buffer, 8-bit/float conversion, downsample,
# SAMPLE_SIZE_LPB=2304 framing, drain semantics) moved here from audio/capture/ and is now portable
# (no <windows.h>; the drain clock is injected via SetAudioDrainClock). WASAPI-specific capture
# (device enumeration, loopback capture, prefs/log/guid glue) stays below in audio/capture/, which
# wires the injected clock to the real GetTickCount64 (still subject to the DX9 host's own
# deterministic-mode IAT interception) and stays Win32-only.
set(BEATDROP_AUDIO_CAPTURE_SOURCES
  ${BEATDROP_SRC_DIR}/audio/capture/audiodevicehandler.cpp
  ${BEATDROP_SRC_DIR}/audio/capture/guid.cpp
  ${BEATDROP_SRC_DIR}/audio/capture/log.cpp
  ${BEATDROP_SRC_DIR}/audio/capture/loopback-capture.cpp
  ${BEATDROP_SRC_DIR}/audio/capture/prefs.cpp
)
set(BEATDROP_PARSER_SOURCES
  ${BEATDROP_SRC_DIR}/parser/state.cpp
  ${BEATDROP_SRC_DIR}/parser/shader_text_gen.cpp
  ${BEATDROP_SRC_DIR}/parser/equation_drivers.cpp
)
set(BEATDROP_LYRICS_SOURCES
  ${BEATDROP_SRC_DIR}/lyrics/lyrics_editor.cpp
  ${BEATDROP_SRC_DIR}/lyrics/lyrics_manager.cpp
  ${BEATDROP_SRC_DIR}/lyrics/songtitlegetter.cpp
)
# P2-020: src/render/core/ is BeatDrop's backend-neutral render core (C2-001) - code the DX9 and
# (later) GL backends both need, extracted out of CPlugin/CPluginShell so it is written and gated
# once instead of duplicated per backend. This first extraction moves FrameContext (a byte-identical
# relocation, C-017 precedent - see src/render/core/frame_context.h) and ports CPluginShell::DoTime
# into TimeModel::Step, with the OS clock and the g_plugin-touching reset side effect threaded
# through as injection seams (render_core::IRawClock, the resetOccurred/forceReset parameters)
# instead of being called directly, so this directory stays GL/D3D/OS-header-free (enforced by
# tests/tools/dx9-confinement-check, already watching this directory since P2-015).
set(BEATDROP_RENDER_CORE_SOURCES
  ${BEATDROP_SRC_DIR}/render/core/time_model.cpp
  ${BEATDROP_SRC_DIR}/render/core/frame_driver.cpp
  ${BEATDROP_SRC_DIR}/render/core/mesh.cpp
  ${BEATDROP_SRC_DIR}/render/core/noise.cpp
  ${BEATDROP_SRC_DIR}/render/core/shader_constants.cpp
  ${BEATDROP_SRC_DIR}/render/core/sampler_resolver.cpp
  ${BEATDROP_SRC_DIR}/render/core/shader_text.cpp
)

## P2-030: src/render/gl/ is the OpenGL 3.3 Core backend (C2-001), the GL twin of render/dx9. It
## includes nothing from render/dx9 and nothing from include/synesthesia-vibe (checked by
## tests/tools/dx9-confinement-check, which has watched src/render/gl since P2-015).
if(SYNESTHESIA_VIBE_BACKEND STREQUAL "gles")
  set(BEATDROP_GLAD_LOADER_SOURCE ${BEATDROP_GLAD_DIR}/src/gles2.c)
else()
  set(BEATDROP_GLAD_LOADER_SOURCE ${BEATDROP_GLAD_DIR}/src/gl.c)
endif()

set(BEATDROP_RENDER_GL_SOURCES
  ${BEATDROP_GLAD_LOADER_SOURCE}
  ${BEATDROP_SRC_DIR}/render/gl/gl_error.cpp
  ${BEATDROP_SRC_DIR}/render/gl/gl_loader.cpp
  ${BEATDROP_SRC_DIR}/render/gl/framebuffer.cpp
  ${BEATDROP_SRC_DIR}/render/gl/texture.cpp
  ${BEATDROP_SRC_DIR}/render/gl/shader.cpp
  ${BEATDROP_SRC_DIR}/render/gl/vertex_array.cpp
  ${BEATDROP_SRC_DIR}/render/gl/capture.cpp
)

## glad2 2.0.8 emits C. This tree tracks no .c file and forces CMAKE_C_FLAGS to "" (CMakeLists.txt
## line 30), so this is the tree's first source-file property: compile gl.c as C++ so the module
## stays single-language under one flag set. gl.c is verified C++-safe (0 designated initializers,
## 0 compound literals) and glad/gl.h wraps its declarations in extern "C".
set_source_files_properties(${BEATDROP_GLAD_LOADER_SOURCE} PROPERTIES LANGUAGE CXX)

set(BEATDROP_RENDER_DX9_SOURCES
  ${BEATDROP_SRC_DIR}/render/dx9/dxcontext.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/harness_dump_hooks.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/lyrics_renderer.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/MediaTexture.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/menu.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/milkdropfs.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/plugin.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/plugin_config.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/pluginshell.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/pluginshell_config.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/shader_loader.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/shader_params.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/support.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/texmgr.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/textmgr.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/texsize_alloc.cpp
  ${BEATDROP_SRC_DIR}/render/dx9/utility_dx9.cpp
)

# P1-016: real, per-module include directories, replacing P1-001's temporary
# BEATDROP_INCLUDE_BRIDGE_DIRS bridge (which gave every module every other module's directory).
# Each list below was derived by grep-tracing every bare (non-relative) #include actually reached,
# transitively, from that module's own sources - see develop/output-P1-016.md's "Per-module
# include-dependency map" for the full trace. The 4 forwarding-shim directories P1-001 added
# (src/ns-eel2-shim, src/render/ns-eel2-shim, src/render/spoutDX9, src/render/audio) are
# deliberately NOT listed anywhere below: every reference to a shim is a *relative* #include
# ("../ns-eel2-shim/ns-eel.h", "..\spoutDX9\SpoutDX9.h", "../audio/audiobuf.h"), which resolves by
# path concatenation against the including file's own directory - it never needed a -I entry, on
# the bridge or off it. The shim files themselves still exist and are still required (the files
# that reference them have not had their relative #include text fixed up).
set(BEATDROP_PLATFORM_WIN32_INCLUDE_DIRS
  ${BEATDROP_SRC_DIR}/platform
  ${BEATDROP_APPS_HOST_DIR}                 # parser_win32.cpp bare-#includes resource.h (a leaf header, no further transitive deps)
)
# P2-012: the portable platform target gets no apps/ include - it must be usable by synesthesia-vibe,
# which has no apps/ dependency at all.
set(BEATDROP_PLATFORM_PORTABLE_INCLUDE_DIRS
  ${BEATDROP_SRC_DIR}/platform               # parser_win32.h, path_utils.h
)
set(BEATDROP_AUDIO_INCLUDE_DIRS
  ${BEATDROP_SRC_DIR}/audio
  ${BEATDROP_SRC_DIR}/config                 # sound_analysis.h -> analysis_settings.h
)
set(BEATDROP_AUDIO_CAPTURE_INCLUDE_DIRS
  ${BEATDROP_SRC_DIR}/audio/capture           # no cross-module bare #include found (grep-verified)
)
set(BEATDROP_PARSER_INCLUDE_DIRS
  ${BEATDROP_SRC_DIR}/parser
  ${BEATDROP_SRC_DIR}/platform                # math_macros.h, parser_win32.h
  ${BEATDROP_SRC_DIR}/config                  # state.h -> analysis_settings.h
)
set(BEATDROP_LYRICS_INCLUDE_DIRS
  ${BEATDROP_SRC_DIR}/lyrics                  # no cross-module bare #include found (grep-verified)
)
set(BEATDROP_RENDER_CORE_INCLUDE_DIRS
  ${BEATDROP_SRC_DIR}/render/core
  ${BEATDROP_SRC_DIR}/parser                    # mesh.cpp -> state.h, equation_drivers.h, md_defines.h, frame_inputs.h, parser_host.h
  ${BEATDROP_SRC_DIR}/config                    # state.h -> analysis_settings.h
  ${BEATDROP_SRC_DIR}/platform                  # noise.cpp -> rng_compat.h
  ${BEATDROP_SRC_DIR}/audio                     # shader_constants.cpp -> frame_context.h -> sound_analysis.h
)
set(BEATDROP_RENDER_DX9_INCLUDE_DIRS
  ${BEATDROP_SRC_DIR}/render/dx9
  ${BEATDROP_SRC_DIR}/render/core               # frame_context.h (plugin.h), time_model.h (pluginshell.h)
  ${BEATDROP_SRC_DIR}/platform                 # unicode_text.h, utility.h, wasabi.h, AutoCharFn.h, AutoWide.h, portable_dll_loader.h
  ${BEATDROP_SRC_DIR}/config                   # plugin.h -> analysis_settings.h
  ${BEATDROP_SRC_DIR}/parser                   # state.h, md_defines.h, equation_drivers.h, frame_inputs.h, parser_host.h
  ${BEATDROP_SRC_DIR}/audio                    # pluginshell.h -> fft.h, sound_analysis.h
  ${BEATDROP_SRC_DIR}/lyrics                   # plugin.h -> lyrics_editor.h, lyrics_manager.h
  ${BEATDROP_APPS_HOST_DIR}                    # resource.h, Milkdrop2PcmVisualizer.h, AMDDetection.h, GpuShaderPolicy.h
)
# beatdrop-audio and beatdrop-audio-capture deliberately do NOT include render/dx9 or apps/ - this
# is the CMake side of "parser/audio do not link or include render" (the source side is confirmed
# by grep in develop/output-P1-016.md; beatdrop-parser above matches the same rule).

function(beatdrop_add_module_target name)
  cmake_parse_arguments(ARG "NEEDS_DXSDK_INCLUDE" "" "SOURCES;INCLUDES" ${ARGN})
  add_library(${name} OBJECT ${ARG_SOURCES})
  set(_beatdrop_module_include_dirs ${ARG_INCLUDES})
  # P2-015 (L-5): only the targets that actually reach a d3d9.h/d3dx9.h header (grep-verified -
  # render/dx9 and the Win32 platform/audio-capture/lyrics forwarders that sit alongside it) get
  # the DX SDK include dir. beatdrop-audio and beatdrop-parser must stay DX9-confined in the main
  # (x86-release) build too, not just under the portable-only preset, so they no longer opt in.
  if(ARG_NEEDS_DXSDK_INCLUDE AND NOT SYNESTHESIA_VIBE_PORTABLE_ONLY AND NOT SYNESTHESIA_VIBE_GL_DEV)
    list(APPEND _beatdrop_module_include_dirs ${BEATDROP_DXSDK_DIR}/Include)
  endif()
  target_include_directories(${name} PUBLIC ${_beatdrop_module_include_dirs})
  target_compile_definitions(${name} PRIVATE ${BEATDROP_REF_COMPILE_DEFINITIONS})
  # P2-010: per-compiler option sets. MSVC (the DX9 presets and windows-gl-dev, C2-005) keeps the
  # vcxproj-identical DX9 parser/audio flag set verbatim; a non-MSVC compiler (clang/AppleClang on
  # macos-arm64) gets the portable set instead.
  if(MSVC)
    target_compile_options(${name} PRIVATE ${BEATDROP_REF_COMPILE_OPTIONS})
  else()
    target_compile_options(${name} PRIVATE -std=c++17 -ffp-contract=off)
  endif()
endfunction()

if(SYNESTHESIA_VIBE_PORTABLE_ONLY)
  # P1-007 checker: /showIncludes on stdout is how
  # tests/tools/portable-only-check/check-portable-only.ps1 finds every header these two targets
  # pull in. Appended only here so the x86-release configuration's flags stay vcxproj-identical.
  list(APPEND BEATDROP_REF_COMPILE_OPTIONS /showIncludes)
endif()

# FFmpeg headers are only #include'd (grep-verified) from src/platform/portable_dll_loader.cpp and
# src/render/dx9/MediaTexture.cpp - every other module (including beatdrop-demo-windows itself,
# which only links the FFmpeg libs, never includes their headers directly) gets no FFMPEG_INCLUDE_DIR.
beatdrop_add_module_target(beatdrop-audio SOURCES ${BEATDROP_AUDIO_SOURCES} INCLUDES ${BEATDROP_AUDIO_INCLUDE_DIRS})
beatdrop_add_module_target(beatdrop-parser SOURCES ${BEATDROP_PARSER_SOURCES} INCLUDES ${BEATDROP_PARSER_INCLUDE_DIRS})

## P2-012: beatdrop-platform-portable builds alongside parser/audio (DX9 and windows-gl-dev alike,
## and later macos-arm64) - it is the target proved to link with beatdrop-parser and no Win32 TU
## under windows-gl-dev (see tests/tools/platform-link-check below).
if(NOT SYNESTHESIA_VIBE_PORTABLE_ONLY)
  beatdrop_add_module_target(beatdrop-platform-portable SOURCES ${BEATDROP_PLATFORM_PORTABLE_SOURCES} INCLUDES ${BEATDROP_PLATFORM_PORTABLE_INCLUDE_DIRS})
endif()

## P2-020: beatdrop-render-core builds alongside parser/audio/platform-portable (DX9 and
## windows-gl-dev alike, and later macos-arm64) - both backends link it, so it is not scoped to the
## DX9-only block below. It gets no NEEDS_DXSDK_INCLUDE: the whole point of this directory (C2-001)
## is that it stays reachable from a GL/Mac build with no D3D/Windows header in sight.
if(NOT SYNESTHESIA_VIBE_PORTABLE_ONLY)
  beatdrop_add_module_target(beatdrop-render-core SOURCES ${BEATDROP_RENDER_CORE_SOURCES} INCLUDES ${BEATDROP_RENDER_CORE_INCLUDE_DIRS})
endif()

## tests/tools/frame-compare (P0-008): a standalone, portable PNG-comparison CLI. It links against
## neither beatdrop-core, ref-src, DirectX nor the vcpkg-installed FFmpeg/projectM-Eval deps above -
## just stb_image (vendor/stb) and the standard library. P2-010 moved it outside the DX9 branch (it
## now also builds under windows-gl-dev and, later, macos-arm64) and gave it compiler-conditional
## options of its own (see tests/tools/frame-compare/CMakeLists.txt).
if(NOT SYNESTHESIA_VIBE_PORTABLE_ONLY AND SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS)
  add_subdirectory(tests/tools/frame-compare)
endif()

## tests/tools/state-compare (P2-028): hex-aware frame-state JSON compare + the P2 gate matrix
## generator. Portable, no GL and no DX SDK, so it sits beside frame-compare and builds under
## x86-release, windows-gl-dev and (later) macos-arm64 alike.
if(NOT SYNESTHESIA_VIBE_PORTABLE_ONLY AND SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS)
  add_subdirectory(tests/tools/state-compare)
endif()

## tests/tools/platform-link-check (P2-012): links beatdrop-parser + beatdrop-platform-portable +
## projectM::ns-eel2 into a real executable - the "no Win32 TU" link proof the task's acceptance
## criterion asks for. Builds wherever beatdrop-platform-portable does (DX9 and windows-gl-dev
## alike); the windows-gl-dev build is the one that actually proves no Win32 TU is pulled in, since
## the DX9 presets still also build the Win32 platform target alongside it.
if(NOT SYNESTHESIA_VIBE_PORTABLE_ONLY AND SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS)
  add_subdirectory(tests/tools/platform-link-check)
endif()

## tests/tools/gl-context-probe (P2-001): a standalone GL 3.3 Core forward-compatible context
## probe. It links against neither beatdrop-core, ref-src nor DirectX - just vcpkg GLFW 3 and the
## system OpenGL loader lib. Additive only: it does not touch beatdrop-demo-windows or its flags.
if(NOT SYNESTHESIA_VIBE_PORTABLE_ONLY AND SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS)
  add_subdirectory(tests/tools/gl-context-probe)
endif()

if(NOT SYNESTHESIA_VIBE_PORTABLE_ONLY AND NOT SYNESTHESIA_VIBE_GL_DEV)

beatdrop_add_module_target(beatdrop-platform-win32 NEEDS_DXSDK_INCLUDE SOURCES ${BEATDROP_PLATFORM_WIN32_SOURCES} INCLUDES ${BEATDROP_PLATFORM_WIN32_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIR})
beatdrop_add_module_target(beatdrop-audio-capture NEEDS_DXSDK_INCLUDE SOURCES ${BEATDROP_AUDIO_CAPTURE_SOURCES} INCLUDES ${BEATDROP_AUDIO_CAPTURE_INCLUDE_DIRS})
beatdrop_add_module_target(beatdrop-lyrics NEEDS_DXSDK_INCLUDE SOURCES ${BEATDROP_LYRICS_SOURCES} INCLUDES ${BEATDROP_LYRICS_INCLUDE_DIRS})
beatdrop_add_module_target(beatdrop-render-dx9 NEEDS_DXSDK_INCLUDE SOURCES ${BEATDROP_RENDER_DX9_SOURCES} INCLUDES ${BEATDROP_RENDER_DX9_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIR})

# beatdrop-core: the combined static lib the host links, replacing beatdrop-ref. OBJECT libraries
# per module (above) sidestep static-lib link-order/circular-reference issues between the modules
# (render <-> parser <-> audio all still reach each other through CPlugin globals at this stage of
# Phase 1) while still giving each module its own CMake target and TU list.
add_library(beatdrop-core STATIC
  $<TARGET_OBJECTS:beatdrop-platform-win32>
  $<TARGET_OBJECTS:beatdrop-platform-portable>
  $<TARGET_OBJECTS:beatdrop-audio>
  $<TARGET_OBJECTS:beatdrop-audio-capture>
  $<TARGET_OBJECTS:beatdrop-parser>
  $<TARGET_OBJECTS:beatdrop-lyrics>
  $<TARGET_OBJECTS:beatdrop-render-core>
  $<TARGET_OBJECTS:beatdrop-render-dx9>
)

# beatdrop-core's PUBLIC include set is the union of every module's own real include dirs (above),
# so the host exe (which links beatdrop-core, and itself bare-#includes headers from several
# modules - plugin.h, pluginshell.h, audiobuf.h, common.h - directly in its own TUs) inherits them
# through target_link_libraries' usage-requirement propagation instead of re-deriving its own
# bridge. This is real per-module propagation, not a bridge: each dir here is on this list only
# because some module's real #include graph (traced above) actually needs it.
target_include_directories(beatdrop-core PUBLIC
  ${BEATDROP_SRC_DIR}/platform
  ${BEATDROP_SRC_DIR}/config
  ${BEATDROP_SRC_DIR}/audio
  ${BEATDROP_SRC_DIR}/audio/capture
  ${BEATDROP_SRC_DIR}/parser
  ${BEATDROP_SRC_DIR}/lyrics
  ${BEATDROP_SRC_DIR}/render/core
  ${BEATDROP_SRC_DIR}/render/dx9
  ${BEATDROP_APPS_HOST_DIR}
  ${FFMPEG_INCLUDE_DIR}
  ${BEATDROP_DXSDK_DIR}/Include
)

target_link_directories(beatdrop-core PUBLIC
  ${BEATDROP_DXSDK_DIR}/Lib/x86
)

target_link_libraries(beatdrop-core PUBLIC
  projectM::ns-eel2
)

## apps/beatdrop-demo-windows (P0-003, restructured by P1-001): the Option A host exe.
#
# Milkdrop2PcmVisualizer.cpp/.h are a verbatim copy of the current (already host-fixed)
# ref-src/beatdrop/vis_milk2 files - see apps/beatdrop-demo-windows/ and
# develop/evidence/P0-003/host-fixes.diff for the two-fix diff against the clean v1.5 baseline.
# Everything else the host TU needs (g_plugin, api_orig_hinstance, g_use_C_locale, keyMappings,
# plugin.h/pluginshell.h) comes from linking beatdrop-core (src/render/dx9 etc.). resource.h,
# AMDDetection.cpp/.h, plugin.rc, plugin_icon.ico, text1.bin and spoutDX9/ are placement-mapped to
# apps/beatdrop-demo-windows/ (P1-001), so AMDDetection.cpp and the spoutDX9/*.cpp sources compile
# as host sources here rather than through beatdrop-core.
get_filename_component(BEATDROP_VCPKG_INSTALLED_DIR ${FFMPEG_INCLUDE_DIR} DIRECTORY)

add_executable(beatdrop-demo-windows WIN32
  apps/beatdrop-demo-windows/Milkdrop2PcmVisualizer.cpp
  apps/beatdrop-demo-windows/determinism.cpp
  apps/beatdrop-demo-windows/wavaudiosource.cpp
  apps/beatdrop-demo-windows/shaderconstanthook.cpp
  apps/beatdrop-demo-windows/capture.cpp
  apps/beatdrop-demo-windows/harnessdump.cpp
  apps/beatdrop-demo-windows/stb_image_write_impl.cpp
  apps/beatdrop-demo-windows/AMDDetection.cpp
  apps/beatdrop-demo-windows/GpuShaderPolicy.cpp
  apps/beatdrop-demo-windows/spoutDX9/SpoutCopy.cpp
  apps/beatdrop-demo-windows/spoutDX9/SpoutDX9.cpp
  apps/beatdrop-demo-windows/spoutDX9/SpoutFrameCount.cpp
  apps/beatdrop-demo-windows/spoutDX9/SpoutSenderNames.cpp
  apps/beatdrop-demo-windows/spoutDX9/SpoutSharedMemory.cpp
  apps/beatdrop-demo-windows/spoutDX9/SpoutUtils.cpp
  apps/beatdrop-demo-windows/plugin.rc
)

# Every header the host TUs bare-#include from a module (plugin.h, pluginshell.h, audiobuf.h,
# common.h, ...) comes from beatdrop-core's PUBLIC include set below via target_link_libraries'
# usage-requirement propagation (set regardless of link call order); the host adds only what it
# alone needs (stb, not linked through beatdrop-core at all).
target_include_directories(beatdrop-demo-windows PRIVATE
  ${CMAKE_SOURCE_DIR}/vendor/stb
)

target_compile_definitions(beatdrop-demo-windows PRIVATE ${BEATDROP_REF_COMPILE_DEFINITIONS})
target_compile_options(beatdrop-demo-windows PRIVATE ${BEATDROP_REF_COMPILE_OPTIONS})

target_link_directories(beatdrop-demo-windows PRIVATE
  ${BEATDROP_VCPKG_INSTALLED_DIR}/lib
  ${BEATDROP_DXSDK_DIR}/Lib/x86
)

# AdditionalDependencies (Release_x86|Win32, plugin.vcxproj:186) not already pulled in
# transitively through beatdrop-core -> projectM::ns-eel2 -> projectM::Eval.
target_link_libraries(beatdrop-demo-windows PRIVATE
  beatdrop-core
  windowsapp.lib
  winhttp.lib
  bcrypt.lib
  DbgHelp.lib
  Winmm.lib
  d3d9.lib
  d3dx9.lib
  Shlwapi.lib
  Shell32.lib
  windowscodecs.lib
  ole32.lib
  avformat.lib
  avcodec.lib
  avutil.lib
  swscale.lib
  delayimp.lib
)

# Link flags (plugin.vcxproj:185-203, Release_x86|Win32) plus the matching DelayLoadDLLs list.
target_link_options(beatdrop-demo-windows PRIVATE
  /MACHINE:X86
  /LARGEADDRESSAWARE
  /OPT:REF
  /OPT:ICF
  /DYNAMICBASE:NO
  /SAFESEH
  /INCREMENTAL:NO
  /DELAYLOAD:d3dx9_43.dll
  /DELAYLOAD:avformat-63.dll
  /DELAYLOAD:avcodec-63.dll
  /DELAYLOAD:avutil-61.dll
  /DELAYLOAD:swscale-10.dll
)

# Post-build: lay out a run directory mirroring ref-src/beatdrop/BeatDrop/ (resources, presets, d3dx9 and
# FFmpeg DLLs) so the exe finds its runtime dependencies exactly like the original
# ref-src/beatdrop/BeatDrop/BeatDrop_x86.exe layout. ref-src/beatdrop/BeatDrop/BeatDrop Resources/dlls/x86 already
# holds the d3dx9_31/d3dx9_43 and FFmpeg runtime DLLs from a prior local build; this only reads
# from ref-src/beatdrop, never writes to it. ref-src/beatdrop/BeatDrop/ is runtime assets (presets, DLLs, ini,
# licenses), not vis_milk2/audio source, so it is out of scope for P1-001's relocation and stays
# read from ref-src/beatdrop/ for now.
set(BEATDROP_REF_RUNTIME_DIR ${CMAKE_SOURCE_DIR}/ref-src/beatdrop/BeatDrop)
set(BEATDROP_DEMO_RUN_DIR ${CMAKE_SOURCE_DIR}/run/beatdrop-demo-windows)
add_custom_command(TARGET beatdrop-demo-windows POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${BEATDROP_DEMO_RUN_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${BEATDROP_REF_RUNTIME_DIR}/BeatDrop Resources" "${BEATDROP_DEMO_RUN_DIR}/BeatDrop Resources"
  COMMAND ${CMAKE_COMMAND} -E copy "${BEATDROP_REF_RUNTIME_DIR}/beatdrop.ini" ${BEATDROP_DEMO_RUN_DIR}/beatdrop.ini
  COMMAND ${CMAKE_COMMAND} -E copy "${BEATDROP_REF_RUNTIME_DIR}/beatdrop_img.ini" ${BEATDROP_DEMO_RUN_DIR}/beatdrop_img.ini
  COMMAND ${CMAKE_COMMAND} -E copy "${BEATDROP_REF_RUNTIME_DIR}/beatdrop_msg.ini" ${BEATDROP_DEMO_RUN_DIR}/beatdrop_msg.ini
  COMMAND ${CMAKE_COMMAND} -E copy "${BEATDROP_REF_RUNTIME_DIR}/precache.txt" ${BEATDROP_DEMO_RUN_DIR}/precache.txt
  COMMAND ${CMAKE_COMMAND} -E copy "${BEATDROP_REF_RUNTIME_DIR}/LICENSE.txt" ${BEATDROP_DEMO_RUN_DIR}/LICENSE.txt
  COMMAND ${CMAKE_COMMAND} -E copy "${BEATDROP_REF_RUNTIME_DIR}/PRESET LICENSE.md" "${BEATDROP_DEMO_RUN_DIR}/PRESET LICENSE.md"
  COMMAND ${CMAKE_COMMAND} -E copy "${BEATDROP_REF_RUNTIME_DIR}/api-ms-win-shcore-scaling-l1-1-1.dll" ${BEATDROP_DEMO_RUN_DIR}/api-ms-win-shcore-scaling-l1-1-1.dll
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:beatdrop-demo-windows> ${BEATDROP_DEMO_RUN_DIR}/$<TARGET_FILE_NAME:beatdrop-demo-windows>
  VERBATIM
)

endif() # NOT SYNESTHESIA_VIBE_PORTABLE_ONLY AND NOT SYNESTHESIA_VIBE_GL_DEV

## tests/unit (P2-019, C2-004): GoogleTest harness. synesthesia-vibe-unit-tests builds only for the
## portable/GL dev tree (windows-gl-dev, macos-arm64) - the DX9 presets (x86-release,
## portable-only) leave SYNESTHESIA_VIBE_GL_DEV OFF and never configure this target, so the DX9 flag-diff
## stays zero.
if(SYNESTHESIA_VIBE_GL_DEV AND SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS)
  enable_testing()
  add_subdirectory(tests/unit)
endif()

## src/synesthesia-vibe (P2-027, C2-001): synesthesia-vibe, the public sv_* C API over the neutral render core,
## plus tests/tools/synesthesia-vibe-headless - the executable that proves it links without beatdrop-core,
## without a Win32 platform TU and without the DX SDK. GL-dev only, so the DX9 presets' flag-diff
## stays zero (they never configure this target).
##
## src/render/gl (P2-030, C2-001): the OpenGL 3.3 Core backend and its dev-only self-test. GL-dev
## only, mirroring the block above, so the DX9 presets' flag-diff stays zero. It deliberately does
## NOT use beatdrop_add_module_target: that helper force-applies BEATDROP_REF_COMPILE_DEFINITIONS
## (WIN32, _WINDOWS, USE_VIS_HDR_HWND, ...), which must not reach AppleClang TUs at P2-032. Only
## the helper's two-branch option line is copied.
if(SYNESTHESIA_VIBE_GL_DEV)
  add_subdirectory(src/synesthesia-vibe)
  add_subdirectory(platform/default)
  if(APPLE AND NOT SYNESTHESIA_VIBE_PORTABLE_ONLY)
    add_subdirectory(platform/darwin)
  endif()
  if(ANDROID AND NOT SYNESTHESIA_VIBE_PORTABLE_ONLY)
    add_subdirectory(platform/android/src/main/cpp)
  endif()
  if(SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS)
    add_subdirectory(tests/tools/synesthesia-vibe-headless)
  endif()

  set(BEATDROP_HLSLPARSER_DIR ${CMAKE_SOURCE_DIR}/vendor/hlslparser/src)

  add_library(synesthesia-vibe-hlslparser STATIC
    ${BEATDROP_HLSLPARSER_DIR}/CodeWriter.cpp
    ${BEATDROP_HLSLPARSER_DIR}/Engine.cpp
    ${BEATDROP_HLSLPARSER_DIR}/GLSLGenerator.cpp
    ${BEATDROP_HLSLPARSER_DIR}/HLSLParser.cpp
    ${BEATDROP_HLSLPARSER_DIR}/HLSLTokenizer.cpp
    ${BEATDROP_HLSLPARSER_DIR}/HLSLTree.cpp
  )

  set_target_properties(synesthesia-vibe-hlslparser PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
  )

  target_include_directories(synesthesia-vibe-hlslparser PUBLIC ${BEATDROP_HLSLPARSER_DIR})

  if(MSVC)
    target_compile_options(synesthesia-vibe-hlslparser PRIVATE /W3 /EHsc /MD)
  else()
    target_compile_options(synesthesia-vibe-hlslparser PRIVATE -std=c++17 -ffp-contract=off)
  endif()

  add_library(synesthesia-vibe-render-gl OBJECT ${BEATDROP_RENDER_GL_SOURCES})

  target_link_libraries(synesthesia-vibe-render-gl PUBLIC synesthesia-vibe-hlslparser)

  target_sources(synesthesia-vibe-render-gl PRIVATE
    ${BEATDROP_SRC_DIR}/render/gl/gl_object_counts.cpp
    ${BEATDROP_SRC_DIR}/render/gl/blend_state.cpp
    ${BEATDROP_SRC_DIR}/render/gl/draw_program.cpp
    ${BEATDROP_SRC_DIR}/render/gl/mesh_buffer.cpp
    ${BEATDROP_SRC_DIR}/render/gl/frame_targets.cpp
    ${BEATDROP_SRC_DIR}/render/gl/warp_blit.cpp
    ${BEATDROP_SRC_DIR}/render/gl/show_to_user.cpp
    ${BEATDROP_SRC_DIR}/render/gl/warp_blit_shaders.cpp
    ${BEATDROP_SRC_DIR}/render/gl/show_to_user_shaders.cpp
    ${BEATDROP_SRC_DIR}/render/gl/motion_vectors.cpp
    ${BEATDROP_SRC_DIR}/render/gl/wave_draw.cpp
    ${BEATDROP_SRC_DIR}/render/gl/custom_shapes.cpp
    ${BEATDROP_SRC_DIR}/render/gl/custom_waves.cpp
    ${BEATDROP_SRC_DIR}/render/gl/screen_borders.cpp
    ${BEATDROP_SRC_DIR}/render/gl/noise_textures.cpp
    ${BEATDROP_SRC_DIR}/render/gl/blur_chain.cpp
    ${BEATDROP_SRC_DIR}/render/gl/preset_shader_header.cpp
    ${BEATDROP_SRC_DIR}/render/gl/shader_translator.cpp
    ${BEATDROP_SRC_DIR}/render/gl/shader_prologue.cpp
    ${BEATDROP_SRC_DIR}/render/gl/disk_textures.cpp
    ${BEATDROP_SRC_DIR}/render/gl/audio_textures.cpp
    ${BEATDROP_SRC_DIR}/render/gl/preset_shader_inputs.cpp
  )

  target_include_directories(synesthesia-vibe-render-gl PUBLIC
    ${BEATDROP_SRC_DIR}/render/gl
    ${BEATDROP_SRC_DIR}/render/core
    ${BEATDROP_SRC_DIR}/parser
    ${BEATDROP_SRC_DIR}/config
    ${BEATDROP_SRC_DIR}/platform
    ${BEATDROP_SRC_DIR}/audio
    ${BEATDROP_GLAD_DIR}/include
  )

  target_include_directories(synesthesia-vibe-render-gl PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/stb
  )

  target_compile_definitions(synesthesia-vibe-render-gl PRIVATE NDEBUG)

  if(SYNESTHESIA_VIBE_BACKEND STREQUAL "gles")
    target_compile_definitions(synesthesia-vibe-render-gl PUBLIC SYNESTHESIA_VIBE_GLES=1)
  endif()

  if(MSVC)
    target_compile_options(synesthesia-vibe-render-gl PRIVATE ${BEATDROP_REF_COMPILE_OPTIONS})
  else()
    target_compile_options(synesthesia-vibe-render-gl PRIVATE -std=c++17 -ffp-contract=off)
  endif()

  if(SYNESTHESIA_VIBE_BUILD_DESKTOP_TOOLS)
    add_subdirectory(tests/tools/render-gl-selftest)
    add_subdirectory(tests/tools/shader-translate-check)

    find_package(glfw3 CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)

    ## samples/legacy-samples: synesthesia-vibe-demo-shared + the two GL dev hosts (P2-031, C2-019), built from
    ## samples/legacy-samples/CMakeLists.txt. GL-dev only, so the DX9 presets' flag-diff stays zero. The Windows GL
    ## host's target name and output directory are matched by run-p2-gate.ps1's -Exe default (keep them in sync).
    if(EXISTS ${CMAKE_SOURCE_DIR}/samples/legacy-samples/CMakeLists.txt)
      add_subdirectory(samples/legacy-samples)
    endif()
  endif()
endif()
