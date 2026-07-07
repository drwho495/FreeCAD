#!/usr/bin/env python3
# COPY of FreeCAD/src/Main/jspi_postprocess.py (in-tree). Kept here so deps/ is a
# self-contained recipe; the pipeline invokes the in-tree copy. Keep the two in sync.
"""Post-process the Emscripten-generated FreeCAD.js for the JSPI build.

Qt for WebAssembly suspends the calling wasm stack (QDialog::exec(), combobox
popups, nested event loops, blocking file/settings I/O, ...) via a
WebAssembly.Suspending import. Under JSPI a suspend only works if the current
call stack was entered through a WebAssembly.promising frame.

Emscripten marks only `main` promising. The remaining JS->wasm entry points that
can reach a Qt suspend are the timer and posted-event callbacks scheduled through
emscripten_set_timeout() and emscripten_async_call() — in particular Qt's
QEventDispatcherWasm timer pump, which for this port runs the /fc-cmd.py Python
command pump (a QTimer callback) directly in the timeout callback. Those
callbacks are plain wasm function pointers dispatched via dynCall_*/getWasmTableEntry;
their return value is ignored, so running them through WebAssembly.promising is
safe and makes them promising frames too.

Two Emscripten output shapes are handled (the exact one depends on whether the
callback is dispatched via a compiled dynCall_* export or via getWasmTableEntry):

  dynCall form:            (a1=>dynCall_vi(func,a1))(arg)
                        -> (a1=>WebAssembly.promising(dynCall_vi)(func,a1))(arg)

  getWasmTableEntry form:  getWasmTableEntry(cb)(userData)
                        -> WebAssembly.promising(getWasmTableEntry(cb))(userData)

Idempotent; fails loudly if NO promising wrapping could be applied, so the build
breaks instead of silently shipping a wasm that deadlocks on the first nested
suspend. Run again after every relink (a bare relink regenerates the glue).
"""
import re
import sys


# (name, regex, replacement, required)
#  - regex uses named group `d` for the callback-dispatch expression to wrap.
#  - `required=True` rules must match (or already be wrapped) or we exit non-zero.
RULES = [
    # emscripten_async_call, dynCall form (observed in this port's output)
    (
        "async_call/dynCall",
        re.compile(r"(?P<pre>var wrapper=\(\)=>\(a1=>)dynCall_(?P<sig>\w+)\((?P<cb>\w+),a1\)(?P<post>\)\((?P<arg>\w+)\))"),
        r"\g<pre>WebAssembly.promising(dynCall_\g<sig>)(\g<cb>,a1)\g<post>",
        False,
    ),
    # emscripten_async_call, getWasmTableEntry form
    (
        "async_call/getWasmTableEntry",
        re.compile(r"(var wrapper=\(\)=>)getWasmTableEntry\((?P<cb>\w+)\)\((?P<arg>\w+)\)"),
        r"\1WebAssembly.promising(getWasmTableEntry(\g<cb>))(\g<arg>)",
        False,
    ),
    # emscripten_set_timeout, getWasmTableEntry form (return safeSetTimeout(()=>getWasmTableEntry(cb)(userData),msecs))
    (
        "set_timeout/getWasmTableEntry",
        re.compile(r"(safeSetTimeout\(\(\)=>)getWasmTableEntry\((?P<cb>\w+)\)\((?P<arg>\w+)\)"),
        r"\1WebAssembly.promising(getWasmTableEntry(\g<cb>))(\g<arg>)",
        False,
    ),
    # emscripten_set_timeout, dynCall form
    (
        "set_timeout/dynCall",
        re.compile(r"(safeSetTimeout\(\(\)=>\(a1=>)dynCall_(?P<sig>\w+)\((?P<cb>\w+),a1\)(\)\((?P<arg>\w+)\))"),
        r"\1WebAssembly.promising(dynCall_\g<sig>)(\g<cb>,a1)\3",
        False,
    ),
]


def main(path):
    with open(path, "r") as f:
        src = f.read()

    # Idempotency signal: both callbacks already wrapped near their scheduler.
    already = src.count("WebAssembly.promising(dynCall_") + src.count("WebAssembly.promising(getWasmTableEntry(")

    total_applied = 0
    report = []
    for name, rx, repl, required in RULES:
        n = len(rx.findall(src))
        if n:
            src = rx.sub(repl, src)
            total_applied += n
            report.append(f"  [+] {name}: wrapped {n}")
        else:
            report.append(f"  [ ] {name}: no match")

    # Presence of the two schedulers is our sanity check on the emscripten shape.
    has_async = "_emscripten_async_call=" in src
    has_timeout = "_emscripten_set_timeout=" in src

    sys.stderr.write("jspi_postprocess: rules\n" + "\n".join(report) + "\n")
    sys.stderr.write(f"jspi_postprocess: schedulers present async_call={has_async} set_timeout={has_timeout}; "
                     f"already-wrapped={already}\n")

    if total_applied == 0:
        if already > 0:
            print(f"jspi_postprocess: {path} already wrapped ({already} promising sites) — idempotent no-op")
            return 0
        sys.stderr.write(
            "jspi_postprocess: ERROR — no promising wrapping applied and none present.\n"
            "  The Emscripten timer/async_call glue shape changed; inspect FreeCAD.js\n"
            "  for _emscripten_set_timeout / _emscripten_async_call and update RULES.\n"
        )
        return 2

    with open(path, "w") as f:
        f.write(src)
    print(f"jspi_postprocess: patched {path} ({total_applied} callback dispatch site(s) -> WebAssembly.promising)")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stderr.write("usage: jspi_postprocess.py <path-to-FreeCAD.js>\n")
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
