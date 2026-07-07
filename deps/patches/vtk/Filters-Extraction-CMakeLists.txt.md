# VTK 9.3.1 patch: `Filters/Extraction/CMakeLists.txt`

File: `/opt/toolchains/src/VTK-9.3.1/Filters/Extraction/CMakeLists.txt`

## Change

Remove `vtkExpandMarkedElements` from the `set(classes ...)` list (line 4 of the
stock file, between `vtkConvertSelection` and `vtkExtractBlock`).

Stock VTK 9.3.1:

```cmake
set(classes
  vtkBlockSelector
  vtkConvertSelection
  vtkExpandMarkedElements
  vtkExtractBlock
  vtkExtractBlockUsingDataAssembly
  ...
```

Patched (this build) — the line is deleted:

```cmake
set(classes
  vtkBlockSelector
  vtkConvertSelection
  vtkExtractBlock
  vtkExtractBlockUsingDataAssembly
  ...
```

## Rationale

`vtkExpandMarkedElements` is the only class in `VTK::FiltersExtraction` that pulls
in `VTK::ParallelDIY` (DIY2 / diy). That parallel/MPI dependency chain is disabled
for this wasm subset build (see the companion `vtk.module` patch that drops
`VTK::ParallelDIY` from `PRIVATE_DEPENDS`). Dropping the class here removes the
only compilation unit that needs the diy headers, so `VTK::FiltersExtraction`
builds without the ParallelDIY module. FreeCAD/SMESH does not use
`vtkExpandMarkedElements`.

## Reapply

`patch -p1 < vtk-9.3.1-wasm.patch` from the VTK-9.3.1 root, or delete the single
`  vtkExpandMarkedElements` line by hand. Must be applied together with the
`vtk.module` patch below. Re-run cmake afterwards.
