#!/usr/bin/env bash
# Part of the FreeCAD -> WebAssembly port (github.com/magik6k/freecad-web).
# ENVIRONMENT-SPECIFIC: hard-codes absolute paths for this build machine
#   /opt/toolchains/...   (the wasm toolchain prefixes: emsdk, qt-jspi, occt, python, vtk, ...)
#   /home/magik6k/lcad-wasm/...  (the FreeCAD checkout, deploy/ dir, pyside-port build)
# Adjust these for your environment before running. See deps/README.md.
# pack-mods-fixed.sh <outname> <Mod1> <Mod2> ...  -> preload mounting src/Mod/<M> to /freecad/Mod/<M>
set -euo pipefail
SRC=/home/magik6k/lcad-wasm/freecad-port/FreeCAD/src/Mod
DEPLOY=/home/magik6k/lcad-wasm/freecad-port/deploy
STAGE="${STAGE:-/tmp/fcweb-modstage}"
FP=/opt/toolchains/emsdk/upstream/emscripten/tools/file_packager.py
OUT="$1"; shift
rm -rf "$STAGE"; mkdir -p "$STAGE/freecad/Mod"
for m in "$@"; do
  [ -d "$SRC/$m" ] || { echo "!! no such module: $m" >&2; exit 1; }
  rsync -a --include='*/' --include='*.py' --include='*.ui' --include='*.svg' --include='*.json'     --include='*.FCMacro' --include='*.txt' --include='*.qml' --include='*.iv' --include='*.wrl' --include='*.vrml' --exclude='*' "$SRC/$m" "$STAGE/freecad/Mod/"
  echo "  staged $m: $(find "$STAGE/freecad/Mod/$m" -name '*.py' | wc -l) .py"
done
find "$STAGE" -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
cd "$DEPLOY"
python3 "$FP" "$OUT.data" --preload "$STAGE/freecad@/freecad" --js-output="$OUT.data.js" --export-name=Module 2>&1 | grep -v "^file_packager: warning: Remember" || true
gzip -9 -c "$OUT.data" > "$OUT.data.gz"
echo "== $OUT.data ($(du -h "$OUT.data" | cut -f1)) =="
