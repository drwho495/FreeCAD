# Qt 6.11.1 wasm-robustness patch: `qwidgetwindow.cpp`

File: `/opt/toolchains/qtsrc/qtbase/src/widgets/kernel/qwidgetwindow.cpp`
(wasm archive: `/opt/toolchains/qt-jspi/6.11.1/wasm_singlethread/lib/libQt6Widgets.a`)

Hardens `QWidgetWindow::focusObject()` so it cannot dereference a freed widget.

## Added helper (just before `QObject *QWidgetWindow::focusObject() const`)

```cpp
#ifdef __EMSCRIPTEN__
// wasm port robustness: a QWidget can be reclaimed during a FreeCAD workbench
// switch without ~QWidget running — the PySide/shiboken dealloc path can free the
// block via tp_free with a mismatched/absent C++ destructor. That leaves dangling
// but non-null references in QPointer *and* the QWidgetPrivate::allWidgets registry,
// so neither can be trusted to detect the freed widget. Validate the widget's
// private self-reference (a live QObjectData has q_ptr == its owner) before
// dereferencing it, bounds-checking the pointer first so the check cannot itself
// fault on garbage. Reading w->d_ptr is safe: w's own chunk still lies within
// linear memory (wasm never unmaps it), only its contents are stale.
static inline bool qt_wasm_widget_is_live(QWidget *w)
{
    if (!w)
        return false;
    const QWidgetPrivate *wd = QWidgetPrivate::get(w);
    const uintptr_t p = reinterpret_cast<uintptr_t>(wd);
    const uintptr_t heapEnd = static_cast<uintptr_t>(__builtin_wasm_memory_size(0)) << 16;
    if (p < 0x10000u || p + 32u > heapEnd)
        return false;
    return wd->q_ptr == w;
}
#endif
```

## Guards inside `focusObject()`

```cpp
QObject *QWidgetWindow::focusObject() const
{
    QWidget *windowWidget = m_widget;
    if (!windowWidget)
        return nullptr;

#ifdef __EMSCRIPTEN__
    if (!qt_wasm_widget_is_live(windowWidget))
        return nullptr;
#endif
    // A window can't have a focus object if it's being destroyed.
    if (QWidgetPrivate::get(windowWidget)->data.in_destructor)
        return nullptr;

    QWidget *widget = windowWidget->focusWidget();

    if (!widget)
        widget = windowWidget;

#ifdef __EMSCRIPTEN__
    if (!qt_wasm_widget_is_live(widget))
        return nullptr;
#endif
    QObject *focusObj = QWidgetPrivate::get(widget)->focusObject();
    if (focusObj)
        return focusObj;

    return widget;
}
```

Rationale: both `m_widget` and the resolved focus widget are validated
(`q_ptr == w`, bounds-checked) before their `QWidgetPrivate` is dereferenced, so a
widget freed by the shiboken dealloc path during a workbench switch returns
`nullptr` instead of trapping.

## Build / swap mechanism

```sh
# rebuild qwidgetwindow.cpp.o in /opt/toolchains/qtsrc/qtbase-build ...
llvm-ar r /opt/toolchains/qt-jspi/6.11.1/wasm_singlethread/lib/libQt6Widgets.a <path-to>/qwidgetwindow.cpp.o
```

Then relink FreeCAD (`deps/build/femrelink-s2.sh`).

## Exception-handling caveat

This widgets TU compiles **`-fno-exceptions`** → **EH-neutral** (zero `try_table`,
zero legacy `try`), so the recompiled `.o` drops straight into FreeCAD's new-EH
(exnref) link. Sanity-check before splicing:

```sh
llvm-objdump -d qwidgetwindow.cpp.o | grep -c try_table    # must be 0
```

Corelib/gui TUs are *not* EH-neutral — they compile **`-fexceptions`** (legacy
wasm-EH, emitting `__resumeException`). To patch one of those you must recompile it
**new-EH**: strip the CMake PCH block (`-Winvalid-pch` … last `cmake_pch` token) and
swap `-fexceptions` → `-fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0`, else `wasm-ld`
fails on undefined `__resumeException`.
