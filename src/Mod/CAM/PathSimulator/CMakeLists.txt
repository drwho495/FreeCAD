# SPDX-License-Identifier: LGPL-2.1-or-later

add_subdirectory(App)
# wasm: AppGL builds the CAMSimulator module, which is a self-contained desktop
# OpenGL (fixed-function + GLSL) renderer (OpenGlWrapper/Shader/Texture/SimDisplay).
# That backend is not viable under Emscripten's WebGL2/GLES-only context and its
# desktop-GL entry points fail to link, so it is excluded from the wasm build.
# The Python consumer (Path/Main/Gui/SimulatorGL.py) is import-guarded in GuiInit.
if(NOT EMSCRIPTEN)
    add_subdirectory(AppGL)
endif()
#if(BUILD_GUI)
#     add_subdirectory(Gui)
#endif(BUILD_GUI)

# install(
    # FILES
        # Gui/PathSimulator.py
    # DESTINATION
        # Mod/CAM
# )
