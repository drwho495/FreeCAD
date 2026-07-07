# VTK 9.3.1 patch: `ThirdParty/expat/CMakeLists.txt`

File: `/opt/toolchains/src/VTK-9.3.1/ThirdParty/expat/CMakeLists.txt`

## Change

`XML_LARGE_SIZE` for VTK's bundled (INTERNAL) expat is switched from `1` to `0`.

Stock VTK 9.3.1 (around line 29):

```cmake
# match setting of EXPAT_LARGE_SIZE in our build
if(NOT VTK_MODULE_USE_EXTERNAL_vtkexpat)
  set(XML_LARGE_SIZE 1)
endif()
```

Patched (this build):

```cmake
# match setting of EXPAT_LARGE_SIZE in our build. EXPAT_LARGE_SIZE defaults OFF, so
# expat's own xmlparse.c uses 32-bit XML_Size; force the generated vtk_expat.h that
# consumers include to 0 as well, else XML_GetCurrentLineNumber/Column/ByteIndex
# mismatch (i64 vs i32) and the wasm indirect call traps parsing .vtu (FemPost restore).
if(NOT VTK_MODULE_USE_EXTERNAL_vtkexpat)
  set(XML_LARGE_SIZE 0)
endif()
```

## Rationale

`XML_LARGE_SIZE` controls the width of the expat `XML_Size` / `XML_Index` types
in the generated `vtk_expat.h` that VTK's own `vtkIOXMLParser` includes. With
`XML_LARGE_SIZE 1` those become 64-bit, but the actual compiled `vtkexpat`
(`xmlparse.c`) is built with `EXPAT_LARGE_SIZE` OFF, i.e. 32-bit. The ABI of
`XML_GetCurrentLineNumber` / `XML_GetCurrentColumnNumber` / `XML_GetCurrentByteIndex`
then mismatches between caller (i64) and callee (i32). Under wasm this is not a
silent truncation but a hard trap: the indirect call's function-type signature
does not match, so V8 aborts with "function signature mismatch" the moment a
`.vtu` file is parsed (hit when restoring a FemPost result mesh). Forcing the
consumer-visible size to 0 (32-bit) makes header and library agree.

## Reapply

`patch -p1 < vtk-9.3.1-wasm.patch` from the VTK-9.3.1 root, or edit the single
`set(XML_LARGE_SIZE 1)` -> `set(XML_LARGE_SIZE 0)` by hand. After changing it you
must re-run cmake and rebuild at least `vtkexpat`, `vtkIOXML` and `vtkIOXMLParser`,
then relink FreeCAD.
