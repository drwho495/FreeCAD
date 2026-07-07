# Coin3D (pivy/coin submodule) — WebAssembly/WebGL2 patch

The `src/3rdParty/coin` submodule needs source changes to render under WebGL2 in the
browser build. Those changes are **not** committed as a coin submodule commit (that would
create a local-only commit that `git clone --recursive` could not fetch); instead the
submodule gitlink is pinned to a normal upstream commit and the wasm delta lives here as a
patch.

- **Base commit (the gitlink this repo records):** `555819b1e3` — *"CMake: Keep Win32
  UNICODE workaround local"*, reachable on `FreeCAD/coin` branch `freecad-master`. A plain
  `git submodule update --init --recursive` checks this out cleanly.
- **Patch:** `coin-wasm-webgl2.patch` — apply on top of that checkout to reproduce the build.

```sh
git submodule update --init --recursive
cd src/3rdParty/coin
git apply ../../../deps/patches/coin/coin-wasm-webgl2.patch
```

## What the patch does (6 files)

- `src/glue/gl.cpp`, `src/glue/gl_egl.cpp` — resolve the GL entry points under Emscripten and
  relax the VBO/client-array capability gate so Coin's fixed-function paths work on WebGL2.
- `src/elements/GL/SoGL{LazyElement,ClipPlaneElement,LightIdElement}.cpp` — hardcode the GL
  enums Coin would otherwise fetch via synchronous `glGet*` round-trips (RGBA mode,
  MAX_CLIP_PLANES=6, MAX_LIGHTS=8), which are slow/unsupported through the wasm→JS shim.
- `src/shapenodes/SoIndexedFaceSet.cpp` — the `FC_WASM_VA` gate for the vertex-array fast
  render path.

See the render-perf and viewport notes in the writeup for the full story. The
matching fixed-function GL shim (`WasmGLFixedFunc`, `WasmGLWidget`) lives in the main
FreeCAD tree under `src/Gui/`, committed normally.
