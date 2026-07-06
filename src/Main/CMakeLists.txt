# SPDX-License-Identifier: LGPL-2.1-or-later

configure_file(freecad.rc.cmake ${CMAKE_CURRENT_BINARY_DIR}/freecad.rc)
configure_file(freecadCmd.rc.cmake ${CMAKE_CURRENT_BINARY_DIR}/freecadCmd.rc)
file(COPY icon.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


if(EMSCRIPTEN)
    # Python extension modules are linked statically into the executables and
    # must be registered on the interpreter's inittab before Py_Initialize().
    set(FREECAD_WASM_PY_MODULES "")
    set(FREECAD_WASM_PY_GUI_MODULES "")
    if(BUILD_PART)
        list(APPEND FREECAD_WASM_PY_MODULES Part)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES PartGui)
    endif()
    if(BUILD_SKETCHER)
        list(APPEND FREECAD_WASM_PY_MODULES Sketcher)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES SketcherGui)
    endif()
    if(BUILD_PART_DESIGN)
        list(APPEND FREECAD_WASM_PY_MODULES PartDesign)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES PartDesignGui)
    endif()
    if(BUILD_SPREADSHEET)
        list(APPEND FREECAD_WASM_PY_MODULES Spreadsheet)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES SpreadsheetGui)
    endif()
    if(BUILD_MATERIAL)
        list(APPEND FREECAD_WASM_PY_MODULES Materials)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES MatGui)
    endif()
    if(BUILD_MEASURE)
        list(APPEND FREECAD_WASM_PY_MODULES Measure)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES MeasureGui)
    endif()
    if(BUILD_SURFACE)
        list(APPEND FREECAD_WASM_PY_MODULES Surface)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES SurfaceGui)
    endif()
    if(BUILD_IMPORT)
        list(APPEND FREECAD_WASM_PY_MODULES Import)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES ImportGui)
    endif()
    # ---- wasm parity push: additional workbenches (enabled per-tier via BUILD_* flags) ----
    if(BUILD_MESH)
        list(APPEND FREECAD_WASM_PY_MODULES Mesh)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES MeshGui)
    endif()
    if(BUILD_POINTS)
        list(APPEND FREECAD_WASM_PY_MODULES Points)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES PointsGui)
    endif()
    if(BUILD_MESH_PART)
        list(APPEND FREECAD_WASM_PY_MODULES MeshPart)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES MeshPartGui)
    endif()
    if(BUILD_INSPECTION)
        list(APPEND FREECAD_WASM_PY_MODULES Inspection)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES InspectionGui)
    endif()
    if(BUILD_ROBOT)
        list(APPEND FREECAD_WASM_PY_MODULES Robot)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES RobotGui)
    endif()
    if(BUILD_TEST)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES QtUnitGui)
    endif()
    if(BUILD_DRAFT)
        list(APPEND FREECAD_WASM_PY_MODULES DraftUtils)
    endif()
    if(BUILD_TECHDRAW)
        list(APPEND FREECAD_WASM_PY_MODULES TechDraw)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES TechDrawGui)
    endif()
    if(BUILD_ASSEMBLY)
        list(APPEND FREECAD_WASM_PY_MODULES Assembly)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES AssemblyGui)
    endif()
    if(BUILD_CAM)
        # CAM/Path: target Path -> PyInit_PathApp (mapped below); area + tsp_solver
        # are pybind11 modules (PyInit_area / PyInit_tsp_solver); PathSimulator is the
        # CPU volumetric sim (its GL AppGL/CAMSimulator is excluded on wasm).
        list(APPEND FREECAD_WASM_PY_MODULES Path area tsp_solver PathSimulator)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES PathGui)
    endif()
    if(BUILD_START)
        list(APPEND FREECAD_WASM_PY_GUI_MODULES StartGui)
    endif()
    if(NOT BUILD_GUI)
        set(FREECAD_WASM_PY_GUI_MODULES "")
    endif()

    function(freecad_wasm_generate_inittab outfile)
        set(_decls "")
        set(_regs "")
        foreach(_mod IN LISTS ARGN)
            # An entry may be "RegName=CInitSuffix" to decouple the Python import
            # name (possibly dotted, e.g. PySide6.QtCore) from the C symbol
            # PyInit_<CInitSuffix> (e.g. PyInit_QtCore). Used for the statically
            # linked PySide6/shiboken6 bindings.
            if(_mod MATCHES "=")
                string(REPLACE "=" ";" _kv "${_mod}")
                list(GET _kv 0 _regname)
                list(GET _kv 1 _pymodx)
                string(APPEND _decls "extern \"C\" PyObject* PyInit_${_pymodx}();\n")
                string(APPEND _regs "    PyImport_AppendInittab(\"${_regname}\", PyInit_${_pymodx});\n")
                continue()
            endif()
            # A Python C-extension's module name equals its library OUTPUT_NAME,
            # which usually matches the CMake target name we pass here. A few
            # modules ship a Python *package* of that name wrapping an
            # underscore-prefixed C extension (e.g. target "PartDesign" builds
            # "_PartDesign.a", and PartDesign/__init__.py does `import _PartDesign`).
            # Keep the plain name for linking but register the real module name.
            set(_pymod "${_mod}")
            if(_mod STREQUAL "PartDesign")
                set(_pymod "_PartDesign")
            endif()
            if(_mod STREQUAL "Assembly")
                set(_pymod "AssemblyApp")   # target Assembly builds PyInit_AssemblyApp; __init__.py does `import AssemblyApp`
            endif()
            if(_mod STREQUAL "Path")
                set(_pymod "PathApp")        # (CAM, deferred) target Path builds PyInit_PathApp
            endif()
            string(APPEND _decls "extern \"C\" PyObject* PyInit_${_pymod}();\n")
            string(APPEND _regs "    PyImport_AppendInittab(\"${_pymod}\", PyInit_${_pymod});\n")
        endforeach()
        file(WRITE ${outfile}
            "// Generated by src/Main/CMakeLists.txt (wasm port)\n"
            "#include <Python.h>\n"
            "${_decls}"
            "void freecadWasmRegisterInittab()\n"
            "{\n"
            "${_regs}"
            "}\n")
    endfunction()
    # Extra statically-linked Python C-extension modules (e.g. the PySide6 /
    # shiboken6 bindings). Each entry is "RegName=CInitSuffix" (or a plain name),
    # e.g. "PySide6.QtCore=QtCore", "shiboken6.Shiboken=Shiboken", "minimal".
    set(FREECAD_WASM_EXTRA_PY_MODULES "" CACHE STRING
        "Extra 'RegName=CInitSuffix' python modules to add to the wasm inittab")
    freecad_wasm_generate_inittab(${CMAKE_CURRENT_BINARY_DIR}/WasmInittab.cpp
        ${FREECAD_WASM_PY_MODULES} ${FREECAD_WASM_EXTRA_PY_MODULES})
    freecad_wasm_generate_inittab(${CMAKE_CURRENT_BINARY_DIR}/WasmInittabGui.cpp
        ${FREECAD_WASM_PY_MODULES} ${FREECAD_WASM_PY_GUI_MODULES} ${FREECAD_WASM_EXTRA_PY_MODULES})

    set(FREECAD_WASM_EXTRA_LINK_LIBS "" CACHE STRING
        "Extra static archives appended to the FreeCAD wasm links")
    set(FREECAD_WASM_COMMON_LINK_OPTIONS
        "-sALLOW_MEMORY_GROWTH=1"
        # The indirect function table must grow at runtime: numpy's 12 C-extensions
        # plus the added workbenches register far more function pointers (asyncify
        # instruments every function) than fit the static table, so a fixed table
        # throws "WebAssembly.Table.grow() failed". Allow growth.
        "-sALLOW_TABLE_GROWTH=1"
        "-sMAXIMUM_MEMORY=4GB"
        "-sINITIAL_MEMORY=128MB"
        "-sSTACK_SIZE=5MB"
        "-sFORCE_FILESYSTEM=1"
        "-sWASM_BIGINT=1"
        # Native wasm setjmp/longjmp — required because wasm-EH forbids the legacy
        # emscripten (JS) longjmp, and OCCT + freetype reference __wasm_longjmp.
        "-sSUPPORT_LONGJMP=wasm"
        "-sEXIT_RUNTIME=0"
        # JSPI + native wasm-EH build (migrated from ASYNCIFY+JS-EH). The prior
        # blockers are now stale: clang-22 fixed the OCCT try_table->br_table
        # miscompile, and a pure wasm-EH module (built -fwasm-exceptions
        # -sWASM_LEGACY_EXCEPTIONS=0 everywhere) emits ZERO invoke_* trampolines,
        # so JSPI can suspend across every frame. Qt 6.11's QWasmSuspendResumeControl
        # drives QEventLoop::exec() suspend via WebAssembly.promising; timer/posted-
        # event callbacks are wrapped promising post-link (jspi_postprocess.py).
        # Native wasm exceptions => no -sDISABLE_EXCEPTION_CATCHING; the whole
        # module is compiled with -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0.
        "-fwasm-exceptions"
        "-sWASM_LEGACY_EXCEPTIONS=0"
        # FS/callMain/ENV/ccall/cwrap for our JS API + CPython glue; the rest
        # (UTF16ToString,stringToUTF16,JSEvents,specialHTMLTargets) are what the
        # Qt wasm platform plugin needs — see Qt6WasmMacros.cmake. A single
        # EXPORTED_RUNTIME_METHODS wins at link time, so it must be the union.
        "-sEXPORTED_RUNTIME_METHODS=FS,callMain,ENV,ccall,cwrap,UTF16ToString,stringToUTF16,JSEvents,specialHTMLTargets,stringToNewUTF8,UTF8ToString"
        "SHELL:-s EXPORTED_FUNCTIONS=_main,_freecad_run_python,__embind_initialize_bindings,__PyRuntime,_PyGILState_GetThisThreadState,__Py_DumpTraceback,__PyEM_EMSCRIPTEN_TRAMPOLINE_OFFSET"
        "-sFETCH=1"
        "-sUSE_ZLIB=1"
        "-sUSE_BZIP2=1"
        "-lnodefs.js"
        "-lidbfs.js"
        "-lembind"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createFreeCAD"
    )
    option(FREECAD_WASM_NODERAWFS
        "Link executables against NODERAWFS (direct host FS) for node-based testing" OFF)
endif()

######################## FreeCADMain ########################
if(BUILD_GUI)

    SET(FreeCAD_SRCS
        ${CMAKE_CURRENT_BINARY_DIR}/freecad.rc
        icon.ico
        MainGui.cpp
    )

    SET(FreeCAD_LIBS
        FreeCADGui
    )

    if(NOT BUILD_DYNAMIC_LINK_PYTHON)
        # executables have to be linked against python libraries,
        # because extension modules are not.
        list(APPEND FreeCAD_LIBS
            ${Python3_LIBRARIES}
        )
    endif(NOT BUILD_DYNAMIC_LINK_PYTHON)

    if(EMSCRIPTEN)
        list(APPEND FreeCAD_SRCS ${CMAKE_CURRENT_BINARY_DIR}/WasmInittabGui.cpp)
        list(APPEND FreeCAD_LIBS ${FREECAD_WASM_PY_MODULES} ${FREECAD_WASM_PY_GUI_MODULES}
             ${FREECAD_WASM_EXTRA_LINK_LIBS})
    endif()

    add_executable(FreeCADMain WIN32 ${FreeCAD_SRCS})
    target_link_libraries(FreeCADMain ${FreeCAD_LIBS})
    if(EMSCRIPTEN)
        target_link_options(FreeCADMain PRIVATE
            ${FREECAD_WASM_COMMON_LINK_OPTIONS}
            # NOTE: Emscripten's LEGACY_GL_EMULATION requires WebGL1, but
            # Qt-for-wasm forces MAX_WEBGL_VERSION=2 ("cannot emulate both ES2
            # and legacy GL"), so the two are mutually exclusive. The planned
            # GL1-emulation stopgap is therefore not viable alongside Qt; the
            # GUI boots with a non-rendering 3D viewport (Coin's fixed-function
            # calls no-op against WebGL2) until the real GLES2/WebGL2 Coin
            # backend lands (Stage 4). See research/R3-graphics-coin-webgl.md.
            "-sMAX_WEBGL_VERSION=2"
            "-sFULL_ES3=1"
            # Coin resolves GL entry points at run time via
            # emscripten_GetProcAddress (see the coin glue patch); this
            # populates its name->function table.
            "-sGL_ENABLE_GET_PROC_ADDRESS=1"
            "-lEGL"   # Coin's context glue references EGL entry points
            # Stack switching for synchronous QEventLoop::exec() (nested event
            # loops, modal dialogs) — JSPI (Qt built QT_EMSCRIPTEN_ASYNCIFY=2).
            # JSPI switches native stacks (no whole-module instrumentation), so
            # there is no asyncify stack to size and OCCT's deep recursion is fine.
            # `main` is promising by default; the Qt timer/posted-event glue is
            # wrapped promising post-link so the /fc-cmd.py command pump (a QTimer
            # callback) can suspend on dialogs/nested loops.
            "-sJSPI=1"
            # Vendored Coin bundles its own copy of expat, whose internal
            # symbols collide with the expat inside static libpython. Both are
            # the same upstream library, so tolerate the duplicates here.
            "-Wl,--allow-multiple-definition"
        )
        if(FREECAD_WASM_NODERAWFS)
            target_link_options(FreeCADMain PRIVATE "-sNODERAWFS=1")
        endif()
        # Emscripten's internal wasm-opt optimizes exception handling through
        # Binaryen's legacy-EH representation and emits legacy try/catch for the
        # functions it touches, leaving the linked module a MIX of legacy and new
        # EH (V8: "module uses a mix of legacy and new exception handling
        # instructions"; wasm-opt has no --emit-exnref in emscripten's own call).
        # Normalize the whole module back to a single new-EH (exnref) encoding,
        # which is what JSPI + the new-EH archives require. wasm-opt lives next to
        # the emscripten dir (upstream/bin) — derive it from the compiler path.
        get_filename_component(_em_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        add_custom_command(TARGET FreeCADMain POST_BUILD
            COMMAND "${_em_dir}/../bin/wasm-opt" "$<TARGET_FILE_DIR:FreeCADMain>/FreeCAD.wasm"
                    --translate-to-exnref --emit-exnref --all-features
                    -o "$<TARGET_FILE_DIR:FreeCADMain>/FreeCAD.wasm"
            COMMENT "wasm: normalize mixed legacy/new EH -> exnref (JSPI)"
            VERBATIM)
        # JSPI promising-frame wiring: under JSPI a suspend only works if the call
        # stack was entered through a WebAssembly.promising frame. Emscripten marks
        # only `main` promising; the timer/posted-event callbacks scheduled via
        # emscripten_set_timeout/emscripten_async_call (Qt's QEventDispatcherWasm
        # timer pump, which runs the /fc-cmd.py Python command pump) are plain wasm
        # function pointers whose return value is ignored, so wrapping them in
        # WebAssembly.promising is safe and makes them promising too. This MUST be
        # re-applied on every relink (a bare relink regenerates the unwrapped glue).
        # Note: we intentionally do NOT force CPython's JS trampoline here — under
        # JSPI the native wasm-gc trampoline keeps Python->C calls entirely in wasm
        # so a dialog opened from Python code can suspend across them.
        add_custom_command(TARGET FreeCADMain POST_BUILD
            COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/jspi_postprocess.py"
                    "$<TARGET_FILE_DIR:FreeCADMain>/FreeCAD.js"
            COMMENT "wasm: wrap timer/async_call callbacks in WebAssembly.promising (JSPI)"
            VERBATIM)
    endif()
    if(BUILD_TEST)
        add_dependencies(FreeCADMain TestSources)
        add_dependencies(FreeCADMain TestGuiSources)
    endif()
    if (FREECAD_WARN_ERROR)
        target_compile_warn_error(FreeCADMain)
    endif()

    SET_BIN_DIR(FreeCADMain FreeCAD)

    if(WIN32)
        INSTALL(TARGETS FreeCADMain
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    elseif(APPLE AND NOT BUILD_WITH_CONDA)
        INSTALL(TARGETS FreeCADMain
        RUNTIME DESTINATION MacOS
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    else()
        INSTALL(TARGETS FreeCADMain
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
endif(BUILD_GUI)
######################## FreeCADMainCmd ########################

SET(FreeCADMainCmd_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/freecadCmd.rc
    icon.ico
    MainCmd.cpp
)

if(EMSCRIPTEN)
    list(APPEND FreeCADMainCmd_SRCS ${CMAKE_CURRENT_BINARY_DIR}/WasmInittab.cpp)
endif()

add_executable(FreeCADMainCmd ${FreeCADMainCmd_SRCS})
if(BUILD_TEST)
    add_dependencies(FreeCADMainCmd TestSources)
endif()

SET(FreeCADMainCmd_LIBS
    FreeCADApp
    ${QtCore_LIBRARIES}
    ${QtXml_LIBRARIES}
)

if(EMSCRIPTEN)
    # Pull the statically-built extension modules into the executable; the
    # generated inittab references their PyInit_* symbols.
    list(APPEND FreeCADMainCmd_LIBS ${FREECAD_WASM_PY_MODULES}
         ${FREECAD_WASM_EXTRA_LINK_LIBS})
endif()

if(NOT BUILD_DYNAMIC_LINK_PYTHON)
    # executables have to be linked against python libraries,
    # because extension modules are not.
    list(APPEND FreeCADMainCmd_LIBS
        ${Python3_LIBRARIES}
    )
endif(NOT BUILD_DYNAMIC_LINK_PYTHON)

target_link_libraries(FreeCADMainCmd
    ${FreeCADMainCmd_LIBS}
)

if(EMSCRIPTEN)
    target_link_options(FreeCADMainCmd PRIVATE ${FREECAD_WASM_COMMON_LINK_OPTIONS}
        # QtCore/QtGui/QtWidgets shiboken bindings each emit the same container
        # conversion helpers (createQIntList/QPointFList/... ) — tolerate the
        # duplicates exactly as the GUI target does.
        "-Wl,--allow-multiple-definition")
    if(FREECAD_WASM_NODERAWFS)
        target_link_options(FreeCADMainCmd PRIVATE "-sNODERAWFS=1")
    endif()
endif()
if (FREECAD_WARN_ERROR)
    target_compile_warn_error(FreeCADMainCmd)
endif()

SET_BIN_DIR(FreeCADMainCmd FreeCADCmd)

if(WIN32)
    INSTALL(TARGETS FreeCADMainCmd
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
elseif(APPLE AND NOT BUILD_WITH_CONDA)
    INSTALL(TARGETS FreeCADMainCmd
        RUNTIME DESTINATION MacOS
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
else()
    INSTALL(TARGETS FreeCADMainCmd
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

######################## FreeCADMainPy ########################

SET(FreeCADMainPy_SRCS
    MainPy.cpp
)

add_library(FreeCADMainPy SHARED ${FreeCADMainPy_SRCS})

target_link_libraries(FreeCADMainPy FreeCADApp)
if (FREECAD_WARN_ERROR)
    target_compile_warn_error(FreeCADMainPy)
endif()

SET_BIN_DIR(FreeCADMainPy FreeCAD)
SET_PYTHON_PREFIX_SUFFIX(FreeCADMainPy)

if(WIN32)
    INSTALL(TARGETS FreeCADMainPy
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
else(WIN32)
    INSTALL(TARGETS FreeCADMainPy
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif(WIN32)

######################## FreeCADGuiPy ########################
if(BUILD_GUI)

    SET(FreeCADGuiPy_SRCS
        FreeCADGuiPy.cpp
    )

    add_library(FreeCADGuiPy SHARED ${FreeCADGuiPy_SRCS})

    target_link_libraries(FreeCADGuiPy FreeCADGui)
    if (FREECAD_WARN_ERROR)
        target_compile_warn_error(FreeCADGuiPy)
    endif()

    SET_BIN_DIR(FreeCADGuiPy FreeCADGui)
    SET_PYTHON_PREFIX_SUFFIX(FreeCADGuiPy)
    if(WIN32)
        # Name clash with target "FreeCADGui"
        # Must be called "FreeCADGuiPy_d" and "FreeCADGuiPy" to work so override default
        set_target_properties(FreeCADGuiPy PROPERTIES PDB_NAME_DEBUG "FreeCADGuiPy_d")
        set_target_properties(FreeCADGuiPy PROPERTIES PDB_NAME_RELEASE "FreeCADGuiPy")
    endif(WIN32)

    if(WIN32)
        INSTALL(TARGETS FreeCADGuiPy
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    else(WIN32)
        INSTALL(TARGETS FreeCADGuiPy
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif(WIN32)
endif(BUILD_GUI)
