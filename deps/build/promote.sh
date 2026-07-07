#!/usr/bin/env bash
# Part of the FreeCAD -> WebAssembly port (github.com/magik6k/freecad-web).
# ENVIRONMENT-SPECIFIC: hard-codes absolute paths for this build machine
#   /opt/toolchains/...   (the wasm toolchain prefixes: emsdk, qt-jspi, occt, python, vtk, ...)
#   /home/magik6k/lcad-wasm/...  (the FreeCAD checkout, deploy/ dir, pyside-port build)
# Adjust these for your environment before running. See deps/README.md.
# Promote the tested deploy-parity build to the production deploy/ folder.
# Copies the matched FreeCAD.wasm/js pair + the deploy-side scripts (boot.py,
# index.html), then regenerates the precompressed .gz that the host serves.
# NOTE: the .data / .data.js module packages are written straight into deploy/ by
# pack-mods-fixed.sh / pack-pydeps.sh, so they are NOT copied here — only re-gzip
# them if you rebuilt one without its pack script (pack scripts already gzip).
set -euo pipefail
ROOT=/home/magik6k/lcad-wasm/freecad-port
SRC="${1:-$ROOT/deploy-parity}"      # source dir (default: deploy-parity)
DST="$ROOT/deploy"
for f in FreeCAD.wasm FreeCAD.js boot.py index.html; do
  cp "$SRC/$f" "$DST/$f"
  echo "  promoted $f"
done
# regenerate precompressed copies (host prefers .gz; a stale .gz serves a mismatch)
for f in FreeCAD.wasm FreeCAD.js index.html; do
  [ -f "$DST/$f.gz" ] && gzip -9 -c "$DST/$f" > "$DST/$f.gz" && echo "  regen $f.gz"
done
# sanity: the served .gz must decompress to the exact file
zcat "$DST/FreeCAD.wasm.gz" | cmp - "$DST/FreeCAD.wasm" && echo "  wasm.gz consistent"
echo "promote done. Next: cd $ROOT/.. && ./check-deploy.sh --full   # then upload the differing files"
