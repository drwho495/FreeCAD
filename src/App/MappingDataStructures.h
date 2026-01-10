#ifndef DATA_MAPPING_STRUCTURES_H
#define DATA_MAPPING_STRUCTURES_H

#include "FCGlobal.h"

namespace Data
{

constexpr int ELEMENT_MAP_VERSION = 6;

struct PersistentNameInfo {
    int duplicateCount = 0;
};

enum class AppExport AlgorithmType : int {
    Old = 0,
    New = 1
};

enum class AppExport OperationCode : int {
    Fuse = 0,
    Cut = 1,
    Common = 2,
    Section = 3,
    Xor = 4,
    Compound = 5,
    Compsolid = 6,
    Pipe = 7,
    Shell = 8,
    Wire = 9,

    Tag = 10,
    Copy = 11,
    Transform = 12,
    Gtransform = 13,
    Face = 14,
    FilledFace = 15,
    Extrude = 16,
    GeneralFuse = 17,
    Refine = 18,
    Boolean = 19,
    Slice = 20,
    Maker = 21,
    Fillet = 22,
    Chamfer = 23,
    Thicken = 24,
    Offset = 25,
    Offset2D = 26,
    Revolve = 27,
    Loft = 28,
    Sweep = 29,
    PipeShell = 30,
    ShellFill = 31,
    Solid = 32,
    RuledSurface = 33,
    Mirror = 34,
    Sketch = 35,
    SketchExport = 36,
    Shapebinder = 37,
    ThruSections = 38,
    Sewing = 39,
    Prism = 40,
    Draft = 41,
    HalfSpace = 42,
    BSplineFace = 43,
    Split = 44,
    Evolve = 45
};

enum class AppExport HistoryModifier : int {
    New = 0,
    Iteration = 1,
};

enum class AppExport MapModifier : int {
    Extruded = 0,
    Copy = 1,
    Remap = 2,
    Source = 3,
    Merge = 4
};


};  // namespace Part

#endif  // DATA_MAPPING_STRUCTURES_H