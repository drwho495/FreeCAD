# VTK 9.3.1 patch: `Filters/Extraction/vtk.module`

File: `/opt/toolchains/src/VTK-9.3.1/Filters/Extraction/vtk.module`

## Change

Remove `VTK::ParallelDIY` from the `PRIVATE_DEPENDS` block.

Stock VTK 9.3.1:

```
PRIVATE_DEPENDS
  VTK::CommonDataModel
  VTK::FiltersCore
  VTK::FiltersHyperTree
  VTK::FiltersStatistics
  VTK::ParallelDIY
OPTIONAL_DEPENDS
  VTK::ParallelMPI
```

Patched (this build) — the `VTK::ParallelDIY` line is deleted:

```
PRIVATE_DEPENDS
  VTK::CommonDataModel
  VTK::FiltersCore
  VTK::FiltersHyperTree
  VTK::FiltersStatistics
OPTIONAL_DEPENDS
  VTK::ParallelMPI
```

## Rationale

The wasm subset build enables only the modules SMESH needs (see
`deps/build/vtk-configure.sh`) and deliberately does not build `VTK::ParallelDIY`
(diy2 — a distributed/MPI helper that is meaningless in the single-threaded
browser sandbox). Declaring it in `PRIVATE_DEPENDS` makes the module system
either fail resolution or force-enable ParallelDIY (and its dependency chain).
Removing the dep — together with dropping `vtkExpandMarkedElements`, the only
class that actually used it — lets `VTK::FiltersExtraction` configure and build
against the reduced module set.

## Reapply

`patch -p1 < vtk-9.3.1-wasm.patch` from the VTK-9.3.1 root, or delete the single
`  VTK::ParallelDIY` line by hand. Must be applied together with the
`Filters/Extraction/CMakeLists.txt` patch. Re-run cmake afterwards.
