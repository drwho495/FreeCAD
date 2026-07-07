#!/usr/bin/env bash
# ============================================================================
# NOTE: base/historical config. The FINAL build uses qt-jspi (not qt-asyncify)
# and adds -DFREECAD_WASM_SMESH=ON -DBUILD_FEM=ON on top (see ../README.md, which
# is authoritative for the shipped JSPI + FEM build).
# ============================================================================
# From the freecad-web WebAssembly build (github.com/magik6k/freecad-web). Working build script — contains env-specific absolute paths (/opt/toolchains, /home/magik6k, /tmp); adjust before running elsewhere. See deps/README.md.
# Stage 1: configure FreeCAD headless (BUILD_GUI=OFF, Part+Sketcher) for wasm.
# Usage: configure-stage1.sh [build-dir]
set -euo pipefail

TC=/opt/toolchains
SRC="$(cd "$(dirname "$0")/../FreeCAD" && pwd)"
BUILD="${1:-$TC/src/freecad-headless-build}"
PYWASM=$TC/python-wasm     # assembled prefix (include/, lib/libpython3.14.a, lib/python3.14)

export PATH="$TC/emsdk:$TC/emsdk/upstream/emscripten:$PATH"

emcmake cmake -S "$SRC" -B "$BUILD" -G Ninja \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$TC/freecad-dist \
  -DFREECAD_WASM_NODERAWFS=ON \
  -DCMAKE_FIND_ROOT_PATH="$TC/qt/6.11.1/wasm_singlethread;$TC/occt-wasm;$TC/xerces-wasm;$TC/fmt-wasm;$TC/yaml-wasm;$TC/python-wasm;$TC/boost-wasm;$TC/icu-wasm;$TC" \
  -DCMAKE_CXX_FLAGS="-fexceptions -DEIGEN_DONT_VECTORIZE -DBOOST_HAS_PTHREADS=1 -DBOOST_STACKTRACE_USE_NOOP" \
  -DCMAKE_C_FLAGS="-fexceptions" \
  -DFREECAD_QT_MAJOR_VERSION=6 \
  -DQt6_DIR=$TC/qt/6.11.1/wasm_singlethread/lib/cmake/Qt6 \
  -DQT_HOST_PATH=$TC/qt/6.11.1/gcc_64 \
  -DPython3_EXECUTABLE=/usr/bin/python3 \
  -DPython3_INCLUDE_DIR=$PYWASM/include/python3.14 \
  -DPython3_LIBRARY=$PYWASM/lib/libpython3.14.a \
  "-DFREECAD_WASM_EXTRA_LINK_LIBS=/opt/toolchains/python-wasm/lib/libmpdec.a;/opt/toolchains/python-wasm/lib/libffi.a;/opt/toolchains/python-wasm/lib/libexpat.a;/opt/toolchains/python-wasm/lib/libHacl_Hash_MD5.a;/opt/toolchains/python-wasm/lib/libHacl_Hash_SHA1.a;/opt/toolchains/python-wasm/lib/libHacl_Hash_SHA2.a;/opt/toolchains/python-wasm/lib/libHacl_Hash_SHA3.a;/opt/toolchains/python-wasm/lib/libHacl_Hash_BLAKE2.a;/opt/toolchains/emsdk/upstream/emscripten/cache/sysroot/lib/wasm32-emscripten/libsqlite3.a;/opt/toolchains/icu-wasm/lib/libicudata.a" \
  -DBoost_DIR=$TC/boost-wasm/lib/cmake/Boost-1.86.0 \
  -DEIGEN3_INCLUDE_DIR=/usr/include/eigen3 \
  -DXercesC_INCLUDE_DIR=$TC/xerces-wasm/include \
  -DXercesC_LIBRARY=$TC/xerces-wasm/lib/libxerces-c.a \
  -Dfmt_DIR=$TC/fmt-wasm/lib/cmake/fmt \
  -DOpenCASCADE_DIR=$TC/occt-wasm/lib/cmake/opencascade \
  -Dyaml-cpp_DIR=$TC/yaml-wasm/lib/cmake/yaml-cpp \
  -DZLIB_INCLUDE_DIR=$TC/emsdk/upstream/emscripten/cache/sysroot/include \
  -DZLIB_LIBRARY=$TC/emsdk/upstream/emscripten/cache/sysroot/lib/wasm32-emscripten/libz.a \
  -DBUILD_GUI=OFF \
  -DBUILD_PART=ON -DBUILD_SKETCHER=ON -DBUILD_MATERIAL=ON \
  -DBUILD_PART_DESIGN=OFF -DBUILD_SPREADSHEET=OFF -DBUILD_START=OFF \
  -DBUILD_MEASURE=OFF -DBUILD_MESH=OFF -DBUILD_MESH_PART=OFF \
  -DBUILD_IMPORT=OFF -DBUILD_DRAFT=OFF -DBUILD_BIM=OFF -DBUILD_CAM=OFF \
  -DBUILD_FEM=OFF -DBUILD_TECHDRAW=OFF -DBUILD_ASSEMBLY=OFF -DBUILD_ROBOT=OFF \
  -DBUILD_SURFACE=OFF -DBUILD_POINTS=OFF -DBUILD_INSPECTION=OFF \
  -DBUILD_REVERSEENGINEERING=OFF -DBUILD_OPENSCAD=OFF -DBUILD_PLOT=OFF \
  -DBUILD_SHOW=OFF -DBUILD_TUX=OFF -DBUILD_WEB=OFF -DBUILD_HELP=OFF \
  -DBUILD_ADDONMGR=OFF -DBUILD_JTREADER=OFF -DBUILD_TEST=OFF -DBUILD_FLAT_MESH=OFF \
  -DICU_ROOT=$TC/icu-wasm \
  -DBUILD_START=OFF -DBUILD_SPREADSHEET=OFF \
  -DBUILD_DYNAMIC_LINK_PYTHON=OFF -DENABLE_DEVELOPER_TESTS=OFF \
  -DFREECAD_USE_PYSIDE=OFF -DFREECAD_USE_SHIBOKEN=OFF \
  -DFREECAD_USE_PCL=OFF -DFREECAD_USE_FREETYPE=ON \
  -DFREETYPE_INCLUDE_DIR_ft2build=$TC/emsdk/upstream/emscripten/cache/sysroot/include/freetype2 \
  -DFREETYPE_INCLUDE_DIR_freetype2=$TC/emsdk/upstream/emscripten/cache/sysroot/include/freetype2 \
  -DFREETYPE_LIBRARY=$TC/emsdk/upstream/emscripten/cache/sysroot/lib/wasm32-emscripten/libfreetype.a \
  -DHARFBUZZ_INCLUDE_DIR=$TC/emsdk/upstream/emscripten/cache/sysroot/include/harfbuzz \
  -DHARFBUZZ_LIBRARY=$TC/emsdk/upstream/emscripten/cache/sysroot/lib/wasm32-emscripten/libharfbuzz.a \
  -DFREECAD_USE_PYBIND11=OFF \
  "$@"
