# boost-wasm patch: add the missing `assign` sublibrary

Target prefix: `/opt/toolchains/boost-wasm` (a size-reduced Boost prefix — only a
subset of Boost headers/libs were built for the wasm toolchain).

## Problem

The reduced `boost-wasm` prefix shipped without the header-only `boost::assign`
sublibrary. FreeCAD's FEM module needs it: `src/Mod/Fem/App/PreCompiled.h` (and
the real `FemMesh.cpp`) use `boost::assign::list_of(...)`. With `BUILD_FEM=ON` the
Fem compile fails with missing `boost/assign.hpp` / `boost/assign/list_of.hpp`.

## Fix

Copy the `boost/assign*` headers from a full Boost 1.86 source tree into the
wasm prefix:

```sh
# from an unpacked boost_1_86_0 source tree:
cp -r boost_1_86_0/boost/assign      /opt/toolchains/boost-wasm/include/boost/
cp    boost_1_86_0/boost/assign.hpp  /opt/toolchains/boost-wasm/include/boost/
```

`assign` is header-only, so no library needs to be built — dropping the headers
in place is sufficient. After copying, `BUILD_FEM=ON` compiles `FemMesh.cpp`.

## Verify

```sh
ls /opt/toolchains/boost-wasm/include/boost/assign.hpp \
   /opt/toolchains/boost-wasm/include/boost/assign/list_of.hpp
```

Both must exist. (Boost version must match the rest of the prefix — this build
uses Boost 1.86.0, see `Boost_DIR=.../boost-wasm/lib/cmake/Boost-1.86.0`.)
