Pin record for `vendor/hlslparser` (C2-008, P2-004)

- Source: `vendor/hlslparser/` inside https://github.com/projectM-visualizer/projectm
- Pinned commit: `1e7ef7803b69024d1e0656705670adda2ffac817` (branch `master`, fetched 2026-09-30)
- Upstream lineage: a fork of Unknown Worlds Entertainment's `hlslparser`
  (https://github.com/unknownworlds/hlslparser, MIT), further extended by projectM.
  `README.md` and `LICENSE` in this directory are projectM's own copies, unmodified.
- License: MIT (see `LICENSE`, copyright Unknown Worlds Entertainment, Inc.)
- Files pulled: `.gitignore`, `CMakeLists.txt`, `LICENSE`, `README.md`, and everything under
  `src/` (`CodeWriter`, `Engine`, `GLSLGenerator`, `HLSLParser`, `HLSLTokenizer`, `HLSLTree`).
  Fetched via `raw.githubusercontent.com` at the pinned commit (git's smart-http/SSH transports
  were not reachable from this session; HTTPS to github.com and raw.githubusercontent.com was).
- `CMakeLists.txt` here is projectM's own (it links `projectM_api` for `projectM_cxx_export.h`,
  used only by a commented-out logging include). It is kept for provenance but not used to build
  this vendored copy yet; `tests/tools/hlsl-survey/CMakeLists.txt` compiles the needed `src/*.cpp`
  files directly. Wiring hlslparser into the main GL build is P2-090.

**One local patch on top of the pinned snapshot** (`src/Engine.cpp`, both hunks tagged
"BeatDrop local patch"): upstream's `Log_ErrorArgList` formats the HLSL parser/generator error
message and then discards it (its `LOG_ERROR(...)` call targets projectM's own `Logging.hpp`,
which is not part of this vendored snapshot, so it is commented out and the hook is inert as
pinned). The patch adds a `namespace M4` string, `g_lastLogError`, and assigns the formatted
message to it. This is the only content change to the pinned upstream source, and it exists
solely so `tests/tools/hlsl-survey` can record each shader's first parse/generate error. No
parsing, tokenizing or code-generation behavior is changed.

**Second local patch** (`src/GLSLGenerator.cpp`, W-2e): under `Flag_AlternateNanPropagation`, float scalar/vector
division is emitted as `mult(a, (1.0 / b))` so the existing alternate multiply (`0 * x = 0`) also covers `0 / 0`,
matching D3D9 SM3 (`a * rcp(b)`). `src/render/gl/shader_translator.cpp` sets the flag. Fixes Transcendance frame 0
(golden white, GL black from 0 * inf = NaN).

**Follow-up to the second patch** (CR-FIX-3): under the same flag, `*=` and `/=` on float scalars/vectors now go through
the alternate multiply as `x = mult(x, y)` / `x = mult(x, (1.0 / y))`, and the plain `*` alternate path is limited to
float scalars/vectors so matrix and integer products keep the upstream IEEE emission. `g_lastLogError` is now
`thread_local`.
