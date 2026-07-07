# Qt 6.11.1 wasm-robustness patch: `qwidget.cpp`

File: `/opt/toolchains/qtsrc/qtbase/src/widgets/kernel/qwidget.cpp`
(Qt built from source; the wasm archive is
`/opt/toolchains/qt-jspi/6.11.1/wasm_singlethread/lib/libQt6Widgets.a`.)

Two independent `#ifdef __EMSCRIPTEN__` hardenings. Both guard against a
use-after-free that is specific to this port: PySide/shiboken can free a QWidget
block (via `tp_free`) without running `~QWidget`, and the async wasm event pump
defers deletions, so focus-proxy / focusObject resolution can dereference a
widget whose backing memory was freed and reused, trapping the wasm VM
("table index is out of bounds" / OOB load).

## 1. `deepestFocusProxy()` — validate every proxy pointer in the chain

Added helper (just before `QWidget *QWidgetPrivate::deepestFocusProxy() const`):

```cpp
#ifdef __EMSCRIPTEN__
// A focus-proxy pointer in the chain can dangle after a task-panel/dialog widget is
// torn down (the async wasm event pump defers deletion), so dereferencing it while
// resolving the window's focusObject() traps with an OOB. Validate liveness: a live
// QWidget's private back-points to it (wd->q_ptr == w), and the pointer is in-heap.
static inline bool qt_wasm_focusproxy_is_live(const QWidget *w)
{
    const uintptr_t p = reinterpret_cast<uintptr_t>(w);
    const uintptr_t heapEnd = static_cast<uintptr_t>(__builtin_wasm_memory_size(0)) << 16;
    if (p < 1024 || p + sizeof(QWidget) > heapEnd)
        return false;
    const QWidgetPrivate *wd = QWidgetPrivate::get(const_cast<QWidget *>(w));
    if (reinterpret_cast<uintptr_t>(wd) < 1024
        || reinterpret_cast<uintptr_t>(wd) + sizeof(QWidgetPrivate) > heapEnd)
        return false;
    return wd->q_ptr == w;
}
#endif
```

and the body of `deepestFocusProxy()` walks the chain through the check:

```cpp
    QWidget *focusProxy = q->focusProxy();
    if (!focusProxy)
        return nullptr;

#ifdef __EMSCRIPTEN__
    if (!qt_wasm_focusproxy_is_live(focusProxy))
        return nullptr;
    while (QWidget *nextFocusProxy = focusProxy->focusProxy()) {
        if (!qt_wasm_focusproxy_is_live(nextFocusProxy))
            break;
        focusProxy = nextFocusProxy;
    }
#else
    while (QWidget *nextFocusProxy = focusProxy->focusProxy())
        focusProxy = nextFocusProxy;
#endif

    return focusProxy;
```

Rationale: fixes the material-editor `focusObject()` OOB — a dangling focus-proxy
was dereferenced while Qt resolved the active window's focus object.

## 2. `isActiveWindow()` — skip the dead window-container probe on wasm

In `bool QWidget::isActiveWindow() const`, the "active window container" probe is
compiled out under wasm:

```cpp
    // Check for an active window container
#ifndef __EMSCRIPTEN__
    if (QWindow *ww = QGuiApplication::focusWindow()) {
        while (ww) {
            QWidgetWindow *qww = qobject_cast<QWidgetWindow *>(ww);
            QWindowContainer *qwc = qww ? qobject_cast<QWindowContainer *>(qww->widget()) : 0;
            if (qwc && qwc->topLevelWidget() == tlw)
                return true;
            ww = ww->parent();
        }
    }
#else
    // wasm/single-thread port: everything renders into one offscreen canvas and
    // no native child windows are ever embedded via QWindowContainer, so this
    // probe can never match here (verified empirically: it always walks to the
    // end without finding a container). During a workbench-switch teardown a
    // QHideEvent storm reaches isActiveWindow() while QWidgetWindow::widget() —
    // and even the QWidgetPrivate::allWidgets registry — still reference a widget
    // whose backing memory has just been freed and reused, so the qobject_cast's
    // metaObject() call would dereference freed memory and trap ("table index is
    // out of bounds"). Skipping this dead probe eliminates that use-after-free
    // crash with no change in behaviour on this platform.
#endif
```

Rationale: on wasm there is one offscreen canvas and no `QWindowContainer`-embedded
native child windows, so the probe never matches; during a workbench-switch
`QHideEvent` storm the `qobject_cast` would dereference a freed widget and trap.

## Build / swap mechanism

Recompile in the Qt build tree and replace the object inside the static archive
(no full Qt reinstall needed):

```sh
# rebuild qwidget.cpp.o in /opt/toolchains/qtsrc/qtbase-build ...
llvm-ar r /opt/toolchains/qt-jspi/6.11.1/wasm_singlethread/lib/libQt6Widgets.a <path-to>/qwidget.cpp.o
```

Then relink FreeCAD (`deps/build/femrelink-s2.sh`).

## Exception-handling caveat

This widgets TU compiles **`-fno-exceptions`** → **EH-neutral** (zero `try_table`,
zero legacy `try`), so the recompiled `.o` drops straight into FreeCAD's new-EH
(exnref) link. Sanity-check before splicing:

```sh
llvm-objdump -d qwidget.cpp.o | grep -c try_table    # must be 0
```

Corelib/gui TUs are *not* EH-neutral — they compile **`-fexceptions`** (legacy
wasm-EH, emitting `__resumeException`). To patch one of those you must recompile it
**new-EH**: strip the CMake PCH block (`-Winvalid-pch` … last `cmake_pch` token) and
swap `-fexceptions` → `-fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0`, else `wasm-ld`
fails on undefined `__resumeException`.
