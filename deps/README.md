# FreeCAD → WebAssembly: build dependencies & out-of-tree patches

This `deps/` directory is the reproduction kit for the FreeCAD → WebAssembly
build published as **github.com/magik6k/freecad-web**. The FreeCAD *source*
changes live in this repository, guarded by `__EMSCRIPTEN__` / `FC_OS_WASM`.
What is collected here is everything that is **not** a FreeCAD source edit:

- `build/` — the build/link/deploy scripts (configure flags, the manual relink
  command, the wasm-EH + JSPI post-processing, the data-pack and deploy scripts).
- `patches/` — patches to **non-FreeCAD** dependencies that are extracted tarballs
  or separately-built source trees (VTK, Qt, Boost) and therefore cannot carry the
  edit in-tree.

The `/opt/toolchains` layout and the non-obvious rules the dependency builds (OCCT,
Qt-from-source with JSPI, CPython, ICU, Boost, VTK) had to get right are documented
in **§1 Toolchain layout** below.

> **Paths are environment-specific.** Every script hard-codes absolute paths for
> the build machine: `/opt/toolchains/...` (the wasm toolchain prefixes) and
> `/home/magik6k/lcad-wasm/...` (the FreeCAD checkout, the `deploy/` dir, and the
> PySide port). They are recorded verbatim so the exact build is reproducible;
> adjust the paths for your environment before running. This is a working recipe,
> not a turnkey CI script.

## Browser requirement

The produced binary needs a **JSPI-capable browser: Chromium/Chrome 137+** (JSPI
= JavaScript Promise Integration, `-sJSPI=1`). It must be served with
cross-origin isolation headers so `SharedArrayBuffer` / memory growth work:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Firefox/Safari do not (yet) ship JSPI and will not run this build.

---

## 1. Toolchain layout (`/opt/toolchains`)

All wasm dependencies are prebuilt static (`.a`) prefixes. Everything is compiled
with the **same** exception model: `-fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0`
(see the EH note in the pipeline below — mixing legacy and exnref EH breaks V8).

| Prefix | What it is |
|---|---|
| `emsdk/` | Emscripten SDK **4.0.12** (clang/llvm, `em++`, `wasm-opt`, `file_packager.py`). |
| `qt-jspi/6.11.1/wasm_singlethread/` | **Qt 6.11.1** built from source (`/opt/toolchains/qtsrc`) with **JSPI + wasm-EH**, single-thread. Widgets archive is patched — see `patches/qt/`. |
| `occt-wasm/` | OpenCASCADE (the CAD kernel: TK* libs). |
| `boost-wasm/` | Reduced Boost **1.86.0** prefix. Missing `assign` was added — see `patches/boost/`. |
| `python-wasm/` | **CPython 3.14** static (`libpython3.14.a`, stdlib, mpdec, HACL hashes, expat, ffi). |
| `icu-wasm/` | ICU (`icuuc`, `icui18n`, `icudata`). |
| `xerces-wasm/`, `fmt-wasm/`, `yaml-wasm/` | Xerces-C, {fmt}, yaml-cpp. |
| `src/vtk-wasm-build/` | The **VTK 9.3.1** subset build (SMESH/FEM needs it). Configured by `build/vtk-configure.sh`; source at `src/VTK-9.3.1` is patched — see `patches/vtk/`. `VTK_DIR` points at `src/vtk-wasm-build/lib/cmake/vtk-9.3`. |

Plus, **outside** `/opt/toolchains`:

| Path | What it is |
|---|---|
| `/home/magik6k/lcad-wasm/pyside-port/build/` | The **PySide6 / shiboken6** wasm build (static, no-dlopen, asyncify, CPython 3.14): `libpyside_wasm.a`, `libShiboken_wasm.a`, the `QtCore/QtGui/QtWidgets/QtSvg` wrapper libs, plus `numpy-wasm`, `pivy-coin`, and `glstubs`. Linked into FreeCAD via `FREECAD_WASM_EXTRA_LINK_LIBS`. |

> **Host prereq — libclang soname shim.** The host `shiboken6` generator (native,
> `pyside-host/`) was linked against `libclang-21.so.21`, but the build host (Arch)
> ships llvm/clang **22**. A soname shim symlink
> (`pyside-port/libshim/libclang-21.so.21 → /usr/lib/libclang.so.22.1.5`) on
> `LD_LIBRARY_PATH` bridges it — libclang's C API is ABI-stable across majors. Other
> host packages needed: `clang llvm swig` (Sketcher/pivy bindings hard-error without
> `swig`) + cmake/ninja/node/python3.14.

---

## 2. Build directory & configure

Build dir: **`/opt/toolchains/src/freecad-gui-build`** — Ninja generator, the
Emscripten CMake toolchain (`emcmake`).

Configure the full GUI app. The load-bearing flags for this port are
`-DFREECAD_WASM_SMESH=ON`, `-DVTK_DIR=.../vtk-wasm-build/lib/cmake/vtk-9.3`,
`-DBUILD_FEM=ON`, and the module `BUILD_*` set. The exact values below are the
ones recorded in this build's `CMakeCache.txt`:

```sh
source /opt/toolchains/emsdk/emsdk_env.sh
TC=/opt/toolchains
QTDIR=$TC/qt-jspi/6.11.1/wasm_singlethread          # JSPI + wasm-EH Qt
SY=$TC/emsdk/upstream/emscripten/cache/sysroot

emcmake cmake -S /home/magik6k/lcad-wasm/freecad-port/FreeCAD \
              -B $TC/src/freecad-gui-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_GUI=ON \
  -DFREECAD_WASM_SMESH=ON \
  -DVTK_DIR=$TC/src/vtk-wasm-build/lib/cmake/vtk-9.3 \
  -DBUILD_FEM=ON -DBUILD_FEM_NETGEN=OFF \
  `# ---- module set (all ON) ----` \
  -DBUILD_PART=ON -DBUILD_PART_DESIGN=ON -DBUILD_SKETCHER=ON \
  -DBUILD_SPREADSHEET=ON -DBUILD_MATERIAL=ON -DBUILD_MEASURE=ON \
  -DBUILD_SURFACE=ON -DBUILD_IMPORT=ON -DBUILD_MESH=ON -DBUILD_MESH_PART=ON \
  -DBUILD_POINTS=ON -DBUILD_INSPECTION=ON -DBUILD_ROBOT=ON -DBUILD_DRAFT=ON \
  -DBUILD_TECHDRAW=ON -DBUILD_ASSEMBLY=ON -DBUILD_CAM=ON -DBUILD_START=ON \
  -DBUILD_TEST=ON -DBUILD_ADDONMGR=OFF \
  `# ---- toolchain prefixes ----` \
  -DCMAKE_FIND_ROOT_PATH="$QTDIR;$TC/occt-wasm;$TC/xerces-wasm;$TC/fmt-wasm;$TC/yaml-wasm;$TC/python-wasm;$TC/boost-wasm;$TC/icu-wasm;$TC" \
  -DQt6_DIR=$QTDIR/lib/cmake/Qt6 -DQT_HOST_PATH=$TC/qt/6.11.1/gcc_64 \
  -DPython3_EXECUTABLE=/usr/bin/python3 \
  -DPython3_INCLUDE_DIR=$TC/python-wasm/include/python3.14 \
  -DPython3_LIBRARY=$TC/python-wasm/lib/libpython3.14.a \
  -DBoost_DIR=$TC/boost-wasm/lib/cmake/Boost-1.86.0 \
  -DOpenCASCADE_DIR=$TC/occt-wasm/lib/cmake/opencascade \
  -DXercesC_INCLUDE_DIR=$TC/xerces-wasm/include -DXercesC_LIBRARY=$TC/xerces-wasm/lib/libxerces-c.a \
  -Dfmt_DIR=$TC/fmt-wasm/lib/cmake/fmt -Dyaml-cpp_DIR=$TC/yaml-wasm/lib/cmake/yaml-cpp \
  -DICU_ROOT=$TC/icu-wasm -DEIGEN3_INCLUDE_DIR=/usr/include/eigen3 \
  -DCMAKE_CXX_FLAGS="-fexceptions -DEIGEN_DONT_VECTORIZE -DBOOST_HAS_PTHREADS=1 -DBOOST_STACKTRACE_USE_NOOP" \
  `# ---- static PySide/Python link group + static py-module map (abbreviated) ----` \
  "-DFREECAD_WASM_EXTRA_LINK_LIBS=<see CMakeCache: the -Wl,--start-group ... pyside-port + python-wasm + icu libs ... -Wl,--end-group>" \
  "-DFREECAD_WASM_EXTRA_PY_MODULES=shiboken6.Shiboken=Shiboken;PySide6.QtCore=QtCore;PySide6.QtGui=QtGui;PySide6.QtWidgets=QtWidgets;PySide6.QtSvg=QtSvg;numpy...;pivy._coin=_coin"
```

> The full literal values of `FREECAD_WASM_EXTRA_LINK_LIBS` and
> `FREECAD_WASM_EXTRA_PY_MODULES` are long (the static no-dlopen link group and the
> `module=initfn` map for every statically-embedded Python extension). They are
> reproduced verbatim inside the relink scripts (`build/femrelink-s2.sh`,
> `build/nfrelink.sh`) — the relink command is the authoritative link line.

---

## 3. Full pipeline (in order)

```
configure  →  ninja <targets>  →  recompile WasmInittabGui.cpp.o
           →  manual Stage-2 relink (femrelink-s2.sh)
           →  wasm-opt --translate-to-exnref  (MANDATORY EH normalization)
           →  jspi_postprocess.py  (wrap timer/event callbacks in WebAssembly.promising)
           →  copy bin/FreeCAD.{js,wasm} + gzip  (deploy-s2.sh)
           →  data packs (pack-mods-fixed.sh)  →  deploy/ (index.html + app.html)
```

1. **Configure** — section 2.

2. **`ninja <targets>`** — build the module archives and objects. The per-module
   `.a` files (Part, Sketcher, Fem, FemGui, SMESH/salomesmesh, the Gui libs, …)
   and the `WasmInittabGui.cpp.o` object are produced here.

3. **Recompile `WasmInittabGui.cpp.o`.** `WasmInittabGui.cpp` is **generated** by
   `src/Main/CMakeLists.txt` at configure time (the static Python inittab +
   no-dlopen module-registration table for the wasm build) and lives in the build
   tree at `src/Main/WasmInittabGui.cpp`. Whenever the module set changes it must
   be regenerated/recompiled before relinking:
   ```sh
   ninja src/Main/CMakeFiles/FreeCADMain.dir/WasmInittabGui.cpp.o
   ```

4. **Manual Stage-2 relink** — `build/femrelink-s2.sh`. The default ninja link does
   **not** wrap the VTK / salomesmesh / Fem / FemGui archives in a single
   `-Wl,--start-group` (they have circular deps), nor does it include the
   `pthread_getname/setname` stub that VTK loguru references. This script is the
   extracted `em++` link command with those fixes: the `--start-group` over
   `FemGui.a Fem.a libStdMeshers.a libMEFISTO2.a <pthread_name_stub.o> libSMESH.a …
   <all vtk*-9.3.a>`. Build the stub first:
   ```sh
   emcc -c deps/build/pthread_name_stub.c -o /opt/toolchains/src/freecad-gui-build/pthread_name_stub.o
   bash deps/build/femrelink-s2.sh          # -> bin/FreeCAD.js + bin/FreeCAD.wasm
   ```
   (`build/nfrelink.sh` is the base, non-FEM relink — same command without the
   SMESH/Fem/VTK group; useful when FEM is off.)

5. **`wasm-opt --translate-to-exnref` — MANDATORY.** The whole toolchain is built
   with the new (exnref) wasm-EH, but some prebuilt objects still carry legacy EH.
   **V8 rejects a module that mixes legacy and exnref exception handling**, so the
   linked `.wasm` must be normalized to exnref:
   ```sh
   /opt/toolchains/emsdk/upstream/bin/wasm-opt \
     --translate-to-exnref --emit-exnref --all-features -g \
     bin/FreeCAD.wasm -o bin/FreeCAD.wasm
   ```
   *EH detection pitfalls.* An `invoke_`-count of 0 does **not** prove new-EH — a lib
   built `-fwasm-exceptions` *without* `-sWASM_LEGACY_EXCEPTIONS=0` has legacy
   `try`/`catch` opcodes **and** zero `invoke_`; to detect legacy EH, disassemble and
   grep for `delegate`/`rethrow`/solo-`try` (legacy-only), **not** `catch`/`catch_all`
   (those are valid `try_table` clause keywords too). Separately, clang-22 miscompiles
   a few deeply-nested-EH OCCT TUs' `try_table`→`br_table` at `-O3` into a spec-invalid
   `br_table` that V8 rejects — recompile just those 8 TUs (`ChFi3d_Builder`,
   `ShapeCustom_BSplineRestriction`, `ShapeFix_FaceConnect`,
   `ShapeUpgrade_{ShapeDivide,SplitCurve2dContinuity,SplitCurve3dContinuity,SplitSurface}`,
   `BRepCheck_Analyzer`) at `-O1` and `emar r` them back into their archives.

6. **`jspi_postprocess.py` on the `.js`.** Under JSPI a `WebAssembly.Suspending`
   import (Qt's `QDialog::exec()`, popups, nested event loops, blocking I/O) only
   works if the current JS→wasm entry was reached through a `WebAssembly.promising`
   frame. Emscripten marks only `main` promising; this script wraps the timer /
   posted-event callbacks (`emscripten_set_timeout` / `emscripten_async_call`
   dispatch sites, incl. Qt's `QEventDispatcherWasm` timer pump that runs the
   Python command pump) in `WebAssembly.promising`. It is idempotent and **fails
   loudly** if it can wrap nothing, so a shape change breaks the build instead of
   shipping a wasm that deadlocks on the first nested suspend. Re-run after **every**
   relink (a bare relink regenerates the glue).
   ```sh
   python3 deps/build/jspi_postprocess.py bin/FreeCAD.js
   ```
   (Steps 5+6+copy+gzip are wrapped in `build/deploy-s2.sh`.)

7. **Copy + gzip.** Copy `bin/FreeCAD.js` and `bin/FreeCAD.wasm` into the deploy
   dir and pre-compress (`gzip -9`; the host prefers `.gz`). `build/promote.sh`
   promotes a tested `deploy-parity/` build into the served `deploy/` and
   regenerates the `.gz` copies consistently.

8. **Data packs.** `build/pack-mods-fixed.sh <outname> <Mod…>` uses Emscripten's
   `file_packager.py` to bundle each workbench's Python/UI/SVG/resource files into
   a preload `.data` (+ `.data.js` + `.gz`) mounted at `/freecad/Mod/<Mod>`. The
   FreeCAD Python is delivered as these preload packs, not compiled into the wasm.

9. **Deploy dir.** The served directory contains `index.html` (the project
   write-up / landing page) and `app.html` (the actual loader that instantiates
   the module, wires the canvas, and streams in the data packs), alongside
   `FreeCAD.{js,wasm}(.gz)` and the `*.data(.js)(.gz)` packs.

---

## 4. `build/` contents

| File | Role |
|---|---|
| `femrelink-s2.sh` | Stage-2 manual relink (full `em++` link line + the VTK/salomesmesh/Fem/FemGui `--start-group` + `MEFISTO2.a` + `${STUB_O}` pthread stub). **Authoritative link command.** |
| `nfrelink.sh` | Base (non-FEM) relink — same link line without the SMESH/Fem/VTK group. |
| `deploy-s2.sh` | Steps 5–7: `wasm-opt` exnref normalization + `jspi_postprocess.py` + copy/gzip into `deploy-parity/`. |
| `promote.sh` | Promote a tested `deploy-parity/` build to the served `deploy/` and regenerate `.gz`. |
| `pack-mods-fixed.sh` | Build a workbench preload `.data` pack via `file_packager.py`. |
| `jspi_postprocess.py` | Copy of the in-tree `src/Main/jspi_postprocess.py` (kept here so `deps/` is self-contained; the pipeline runs the in-tree copy — keep the two in sync). |
| `vtk-configure.sh` | `emcmake` configure recipe for the VTK 9.3.1 wasm subset (module subset, `-DFMT_USE_CHAR8_T=0`, Sequential SMP, Rendering/MPI/etc. `DONT_WANT`, static, wasm-EH flags). |
| `pthread_name_stub.c` | `pthread_getname_np` / `pthread_setname_np` no-op stubs for VTK loguru (emscripten lacks them). Compile to `.o` and include in the Stage-2 relink. |

---

## 5. `patches/` contents (out-of-tree dependency patches)

### `patches/vtk/` — VTK 9.3.1 (extracted tarball at `/opt/toolchains/src/VTK-9.3.1`)
- `vtk-9.3.1-wasm.patch` — all three edits as one reapplyable unified diff
  (`patch -p1` from the VTK-9.3.1 root).
- `ThirdParty-expat-CMakeLists.txt.md` — `XML_LARGE_SIZE 1 → 0` (the i32/i64
  `XML_Size` ABI fix so `vtkexpat` matches `vtkIOXMLParser`; else `.vtu` parse traps).
- `Filters-Extraction-CMakeLists.txt.md` — drop `vtkExpandMarkedElements` from the
  `set(classes …)` list (the only class needing ParallelDIY).
- `Filters-Extraction-vtk.module.md` — drop `VTK::ParallelDIY` from `PRIVATE_DEPENDS`.

### `patches/qt/` — Qt 6.11.1 widgets (source tree at `/opt/toolchains/qtsrc/qtbase`)
- `qwidget.cpp.md` — `deepestFocusProxy()` guarded with a heap-bounds +
  `wd->q_ptr==w` liveness check (`qt_wasm_focusproxy_is_live`), fixing the
  material-editor `focusObject()` OOB; plus the `isActiveWindow()` container-probe
  skip on wasm.
- `qwidgetwindow.cpp.md` — `focusObject()` hardened with `qt_wasm_widget_is_live`.
- Swap mechanism: recompile in `/opt/toolchains/qtsrc/qtbase-build`, then
  `llvm-ar r /opt/toolchains/qt-jspi/6.11.1/wasm_singlethread/lib/libQt6Widgets.a <obj>`.

### `patches/boost/` — reduced boost-wasm prefix
- `README.md` — the `assign` sublibrary was missing; copy `boost/assign*` headers
  from Boost 1.86 source into `/opt/toolchains/boost-wasm/include/boost/` (needed
  by Fem's `PreCompiled.h` / `FemMesh.cpp`, which use `boost::assign::list_of`).
