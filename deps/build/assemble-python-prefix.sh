#!/usr/bin/env bash
# From the freecad-web WebAssembly build (github.com/magik6k/freecad-web). Working build script — contains env-specific absolute paths (/opt/toolchains, /home/magik6k, /tmp); adjust before running elsewhere. See deps/README.md.
# Assemble a clean wasm Python prefix at /opt/toolchains/python-wasm from the
# CPython cross-build tree (Stage 1). Run after `Platforms/emscripten build`.
set -euo pipefail

PYSRC=/opt/toolchains/src/Python-3.14.4
XB=$PYSRC/cross-build/wasm32-emscripten/build/python
OUT=/opt/toolchains/python-wasm

rm -rf "$OUT"
mkdir -p "$OUT/include/python3.14" "$OUT/lib"

# Headers: public includes + generated pyconfig.h
cp -r "$PYSRC/Include/." "$OUT/include/python3.14/"
cp "$XB/pyconfig.h" "$OUT/include/python3.14/"

# Static libs: libpython + the dependency libs the build produced
cp "$XB"/libpython3.14.a "$OUT/lib/"
for lib in "$PYSRC"/cross-build/wasm32-emscripten/prefix/lib/*.a; do
  cp "$lib" "$OUT/lib/"
done
# Modules/expat etc. end up inside libpython for the static build; nothing to add.

# Stdlib: prefer the zip the build produced; also keep a plain tree for MEMFS
if [ -f "$XB/python3.14.zip" ]; then
  cp "$XB/python3.14.zip" "$OUT/lib/"
fi
mkdir -p "$OUT/lib/python3.14"
cp -r "$PYSRC/Lib/." "$OUT/lib/python3.14/"
# build-time sysconfigdata (needed by sysconfig at runtime)
find "$XB" -name '_sysconfigdata*.py' -exec cp {} "$OUT/lib/python3.14/" \;

# Link-flags reference: extract the python.mjs link command for later use
grep -oE '\-l[a-zA-Z0-9_]+|\-s[A-Z_0-9]+(=[^ ]+)?' "$XB/Makefile" | sort -u \
  > "$OUT/link-flags-reference.txt" || true

du -sh "$OUT"
echo "python-wasm prefix assembled at $OUT"
