#!/usr/bin/env bash
# Part of the FreeCAD -> WebAssembly port (github.com/magik6k/freecad-web).
# ENVIRONMENT-SPECIFIC: hard-codes absolute paths for this build machine
#   /opt/toolchains/...   (the wasm toolchain prefixes: emsdk, qt-jspi, occt, python, vtk, ...)
#   /home/magik6k/lcad-wasm/...  (the FreeCAD checkout, deploy/ dir, pyside-port build)
# Adjust these for your environment before running. See deps/README.md.
# Stage-2 finalize: wasm-opt EH normalization (exnref) + JSPI js post-process + gzip deploy.
set -uo pipefail
cd /opt/toolchains/src/freecad-gui-build
WASMOPT=/opt/toolchains/emsdk/upstream/bin/wasm-opt
DP=/home/magik6k/lcad-wasm/freecad-port/deploy-parity
echo "=== wasm-opt translate-to-exnref (normalize EH) ==="
$WASMOPT --translate-to-exnref --emit-exnref --all-features -g bin/FreeCAD.wasm -o bin/FreeCAD.wasm.exn 2>&1 | tail -3
mv bin/FreeCAD.wasm.exn bin/FreeCAD.wasm
echo "  exnref done: $(ls -la bin/FreeCAD.wasm | awk '{print $5}')"
echo "=== jspi_postprocess ==="
python3 /home/magik6k/lcad-wasm/freecad-port/FreeCAD/src/Main/jspi_postprocess.py bin/FreeCAD.js 2>&1 | grep -iE "wrapped|patched|error" | head
echo "=== deploy to deploy-parity + gzip ==="
cp bin/FreeCAD.js bin/FreeCAD.wasm "$DP/"
gzip -9 -c bin/FreeCAD.wasm > "$DP/FreeCAD.wasm.gz"
gzip -9 -c bin/FreeCAD.js > "$DP/FreeCAD.js.gz"
echo "DEPLOY_S2_DONE wasm=$(ls -la "$DP/FreeCAD.wasm" | awk '{print $5}')"
