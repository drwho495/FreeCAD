#!/usr/bin/env bash
# ============================================================================
# NOTE: base/historical config. The FINAL build uses qt-jspi (not qt-asyncify)
# and adds -DFREECAD_WASM_SMESH=ON -DBUILD_FEM=ON on top (see ../README.md, which
# is authoritative for the shipped JSPI + FEM build).
# ============================================================================
# From the freecad-web WebAssembly build (github.com/magik6k/freecad-web). Working build script — contains env-specific absolute paths (/opt/toolchains, /home/magik6k, /tmp); adjust before running elsewhere. See deps/README.md.
# Stage 2/3: configure the full FreeCAD GUI (BUILD_GUI=ON) for wasm.
# NOTE: this committed copy points Qt at qt-asyncify (the older ASYNCIFY path). The
# production JSPI build points Qt6_DIR / CMAKE_FIND_ROOT_PATH at qt-jspi instead
# (/opt/toolchains/qt-jspi/6.11.1/wasm_singlethread) — the rest of the flags are the same.
set -euo pipefail

TC=/opt/toolchains
SRC="$(cd "$(dirname "$0")/../FreeCAD" && pwd)"
BUILD="${1:-$TC/src/freecad-gui-build}"
PYWASM=$TC/python-wasm
QTDIR=$TC/qt-asyncify/6.11.1/wasm_singlethread   # asyncify-enabled Qt (source-built)
QTHOST=$TC/qt/6.11.1/gcc_64                       # host tools (moc/rcc/uic/lrelease)
SY=$TC/emsdk/upstream/emscripten/cache/sysroot

export PATH="$TC/emsdk:$TC/emsdk/upstream/emscripten:$PATH"

emcmake cmake -S "$SRC" -B "$BUILD" -G Ninja \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$TC/freecad-dist \
  -DFREECAD_WASM_NODERAWFS=OFF \
  -DCMAKE_FIND_ROOT_PATH="$QTDIR;$TC/occt-wasm;$TC/xerces-wasm;$TC/fmt-wasm;$TC/yaml-wasm;$TC/python-wasm;$TC/boost-wasm;$TC/icu-wasm;$TC" \
  -DCMAKE_CXX_FLAGS="-fexceptions -DEIGEN_DONT_VECTORIZE -DBOOST_HAS_PTHREADS=1 -DBOOST_STACKTRACE_USE_NOOP" \
  -DCMAKE_C_FLAGS="-fexceptions -DXML_DEV_URANDOM" \
  -DFREECAD_QT_MAJOR_VERSION=6 \
  -DQt6_DIR=$QTDIR/lib/cmake/Qt6 \
  -DQT_HOST_PATH=$QTHOST \
  -DPython3_EXECUTABLE=/usr/bin/python3 \
  -DPython3_INCLUDE_DIR=$PYWASM/include/python3.14 \
  -DPython3_LIBRARY=$PYWASM/lib/libpython3.14.a \
  "-DFREECAD_WASM_EXTRA_LINK_LIBS=$PYWASM/lib/libmpdec.a;$PYWASM/lib/libffi.a;$PYWASM/lib/libexpat.a;$PYWASM/lib/libHacl_Hash_MD5.a;$PYWASM/lib/libHacl_Hash_SHA1.a;$PYWASM/lib/libHacl_Hash_SHA2.a;$PYWASM/lib/libHacl_Hash_SHA3.a;$PYWASM/lib/libHacl_Hash_BLAKE2.a;$SY/lib/wasm32-emscripten/libsqlite3.a;$TC/icu-wasm/lib/libicudata.a" \
  -DBoost_DIR=$TC/boost-wasm/lib/cmake/Boost-1.86.0 \
  -DEIGEN3_INCLUDE_DIR=/usr/include/eigen3 \
  -DXercesC_INCLUDE_DIR=$TC/xerces-wasm/include \
  -DXercesC_LIBRARY=$TC/xerces-wasm/lib/libxerces-c.a \
  -Dfmt_DIR=$TC/fmt-wasm/lib/cmake/fmt \
  -DOpenCASCADE_DIR=$TC/occt-wasm/lib/cmake/opencascade \
  -Dyaml-cpp_DIR=$TC/yaml-wasm/lib/cmake/yaml-cpp \
  -DZLIB_INCLUDE_DIR=$SY/include \
  -DZLIB_LIBRARY=$SY/lib/wasm32-emscripten/libz.a \
  -DBUILD_GUI=ON \
  -DCOIN_BUILD_GLX=OFF -DCOIN_BUILD_EGL=OFF -DCOIN_BUILD_TESTS=OFF -DCOIN_BUILD_DOCUMENTATION=OFF \
  -DOPENGL_INCLUDE_DIR=$SY/include \
  -DOPENGL_gl_LIBRARY=$SY/lib/wasm32-emscripten/libGL-emu-full_es3.a \
  -DOPENGL_glu_LIBRARY= \
  -DBUILD_PART=ON -DBUILD_SKETCHER=ON -DBUILD_MATERIAL=ON -DBUILD_START=ON \
  -DBUILD_PART_DESIGN=OFF -DBUILD_SPREADSHEET=OFF \
  -DBUILD_MEASURE=OFF -DBUILD_MESH=OFF -DBUILD_MESH_PART=OFF \
  -DBUILD_IMPORT=OFF -DBUILD_DRAFT=OFF -DBUILD_BIM=OFF -DBUILD_CAM=OFF \
  -DBUILD_FEM=OFF -DBUILD_TECHDRAW=OFF -DBUILD_ASSEMBLY=OFF -DBUILD_ROBOT=OFF \
  -DBUILD_SURFACE=OFF -DBUILD_POINTS=OFF -DBUILD_INSPECTION=OFF \
  -DBUILD_REVERSEENGINEERING=OFF -DBUILD_OPENSCAD=OFF -DBUILD_PLOT=OFF \
  -DBUILD_SHOW=OFF -DBUILD_TUX=OFF -DBUILD_WEB=OFF -DBUILD_HELP=OFF \
  -DBUILD_ADDONMGR=OFF -DBUILD_JTREADER=OFF -DBUILD_TEST=OFF -DBUILD_FLAT_MESH=OFF \
  -DICU_ROOT=$TC/icu-wasm \
  -DBUILD_DYNAMIC_LINK_PYTHON=OFF -DENABLE_DEVELOPER_TESTS=OFF \
  -DFREECAD_USE_PYSIDE=OFF -DFREECAD_USE_SHIBOKEN=OFF \
  -DFREECAD_USE_PCL=OFF -DFREECAD_USE_FREETYPE=ON \
  -DFREETYPE_INCLUDE_DIR_ft2build=$SY/include/freetype2 \
  -DFREETYPE_INCLUDE_DIR_freetype2=$SY/include/freetype2 \
  -DFREETYPE_LIBRARY=$SY/lib/wasm32-emscripten/libfreetype.a \
  -DHARFBUZZ_INCLUDE_DIR=$SY/include/harfbuzz \
  -DHARFBUZZ_LIBRARY=$SY/lib/wasm32-emscripten/libharfbuzz.a \
  -DFREECAD_USE_PYBIND11=OFF \
  "$@"
