#!/usr/bin/env bash
# Part of the FreeCAD -> WebAssembly port (github.com/magik6k/freecad-web).
# ENVIRONMENT-SPECIFIC: hard-codes absolute paths for this build machine
#   /opt/toolchains/...   (the wasm toolchain prefixes: emsdk, qt-jspi, occt, python, vtk, ...)
#   /home/magik6k/lcad-wasm/...  (the FreeCAD checkout, deploy/ dir, pyside-port build)
# Adjust these for your environment before running. See deps/README.md.
set -uo pipefail
source /opt/toolchains/emsdk/emsdk_env.sh 2>/dev/null
cd /opt/toolchains/src
rm -rf vtk-wasm-build && mkdir -p vtk-wasm-build && cd vtk-wasm-build
# FMT_USE_CHAR8_T=0: VTK's bundled diy2/fmt uses std::char_traits<fmt::char8_t>,
# which newer libc++ (clang-22) rejects (undefined for non-standard char types).
EHFLAGS="-fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0 -DFMT_USE_CHAR8_T=0"
emcmake cmake ../VTK-9.3.1 -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DVTK_BUILD_TESTING=OFF \
  -DVTK_BUILD_EXAMPLES=OFF \
  -DVTK_ENABLE_WRAPPING=OFF \
  -DVTK_WRAP_PYTHON=OFF \
  -DVTK_SMP_IMPLEMENTATION_TYPE=Sequential \
  -DVTK_GROUP_ENABLE_Rendering=DONT_WANT \
  -DVTK_GROUP_ENABLE_MPI=DONT_WANT \
  -DVTK_GROUP_ENABLE_Web=DONT_WANT \
  -DVTK_GROUP_ENABLE_Qt=DONT_WANT \
  -DVTK_GROUP_ENABLE_Views=DONT_WANT \
  -DVTK_GROUP_ENABLE_Imaging=DONT_WANT \
  -DVTK_GROUP_ENABLE_StandAlone=DONT_WANT \
  -DVTK_BUILD_ALL_MODULES=OFF \
  -DVTK_MODULE_ENABLE_VTK_CommonCore=YES \
  -DVTK_MODULE_ENABLE_VTK_CommonDataModel=YES \
  -DVTK_MODULE_ENABLE_VTK_CommonExecutionModel=YES \
  -DVTK_MODULE_ENABLE_VTK_CommonMath=YES \
  -DVTK_MODULE_ENABLE_VTK_CommonMisc=YES \
  -DVTK_MODULE_ENABLE_VTK_CommonSystem=YES \
  -DVTK_MODULE_ENABLE_VTK_CommonTransforms=YES \
  -DVTK_MODULE_ENABLE_VTK_FiltersCore=YES \
  -DVTK_MODULE_ENABLE_VTK_FiltersGeneral=YES \
  -DVTK_MODULE_ENABLE_VTK_FiltersVerdict=YES \
  -DVTK_MODULE_ENABLE_VTK_FiltersExtraction=YES \
  -DVTK_MODULE_ENABLE_VTK_FiltersSources=YES \
  -DVTK_MODULE_ENABLE_VTK_FiltersGeometry=YES \
  -DVTK_MODULE_ENABLE_VTK_FiltersModeling=YES \
  -DVTK_MODULE_ENABLE_VTK_IOCore=YES \
  -DVTK_MODULE_ENABLE_VTK_IOXML=YES \
  -DVTK_MODULE_ENABLE_VTK_IOXMLParser=YES \
  -DVTK_MODULE_ENABLE_VTK_IOLegacy=YES \
  -DCMAKE_CXX_FLAGS="$EHFLAGS" \
  -DCMAKE_C_FLAGS="$EHFLAGS" \
  2>&1
echo "VTK_CONFIGURE_EXIT=$?"
