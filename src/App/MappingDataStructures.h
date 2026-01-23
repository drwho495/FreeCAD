#ifndef DATA_MAPPING_STRUCTURES_H
#define DATA_MAPPING_STRUCTURES_H

#include "FCGlobal.h"
#include <string>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace Data
{

constexpr const int ELEMENT_MAP_VERSION = 6;
constexpr const char MAPPED_NAME_SAVE_FORMAT[] = "ElementMapVersion;DuplicateCount;MappedSections";
constexpr const char MAPPED_SECTION_SAVE_FORMAT[] = "OpCode;MapModifier;HistoryModifier;IterationTag;ReferenceIDs;LinkedNames;ElementType;Index;TNoSE;DeletedNames;IsForkedElement";

struct SavingUtil {
    std::string formattingString;
    std::vector<std::string> keys;
    std::unordered_map<std::string, std::vector<std::string>> keyStorage;
    std::unordered_set<char> deliminators {';', ':'};
    std::string cachedSaveString;
    char mainDeliminator = ';';
    char subDeliminator = ':';

    void setFormattingString(std::string newFormattingStr) {
        formattingString = newFormattingStr;

        keyStorage.clear();

        compileKeys();
    };

    void compileKeys();

    static std::string escapeChars(
        const std::string& input,
        const std::unordered_set<char>& chars_to_escape)
    {
        std::string result;
        result.reserve(input.size() * 2);

        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];
            bool is_target = chars_to_escape.count(c) != 0;

            if (is_target) {
                result.push_back('\\');
            }

            result.push_back(c);
        }

        return result;
    }

    void addDeliminator(char addDelim) {
        deliminators.insert(addDelim);
    };

    void addSaveKey(std::string stringKey, std::string saveInfo) {
        keyStorage[stringKey] = { saveInfo };
    };

    void addSaveKey(std::string stringKey, std::vector<std::string> saveInfo) {
        keyStorage[stringKey] = saveInfo;
    };

    std::unordered_map<std::string, std::vector<std::string>> getRestoredKeyStorage(std::string restoreString);
    
    std::string getSaveString();
};

struct PersistentNameInfo {
    bool operator==(const PersistentNameInfo& other) const {
        return duplicateCount == other.duplicateCount;
    }

    bool operator<(const PersistentNameInfo& other) const {
        return duplicateCount < other.duplicateCount;
    }

    bool operator>(const PersistentNameInfo& other) const {
        return duplicateCount > other.duplicateCount;
    }

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