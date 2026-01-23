// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef APP_MAPPED_SECTION_H
#define APP_MAPPED_SECTION_H

#include <memory>
#include <utility>
#include <string>
#include <sstream>

#include "MappingDataStructures.h"
#include "IndexedName.h"

namespace Data
{

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

constexpr const char SECTION_DELIMINATOR = ';';
constexpr const char SECTION_SAVE_FORMAT[] = "OpCode;MapModifier;HistoryModifier;IterationTag;ReferenceIDs;LinkedNames;ElementType;Index;TNoSE;DeletedNames;IsForkedElement";

class AppExport MappedSection
{
public:
    MappedSection() = default;

    bool operator==(const MappedSection &other) const {
        return (opCode == other.opCode and
                mapModifier == other.mapModifier and
                historyModifier == other.historyModifier and
                iterationTag == other.iterationTag and
                referenceIDs == other.referenceIDs and
                linkedNames == other.linkedNames and
                elementType == other.elementType and
                index == other.index);
    }

    int compare(MappedSection &other) const {
        if (other == *this) {
            return 0;
        }

        if (opCode < other.opCode) {
            return -1;
        }
        if (opCode > other.opCode) {
            return 1;
        }

        if (mapModifier < other.mapModifier) {
            return -1;
        }
        if (mapModifier > other.mapModifier) {
            return 1;
        }

        if (historyModifier < other.historyModifier) {
            return -1;
        }
        if (historyModifier > other.historyModifier) {
            return 1;
        }

        if (iterationTag < other.iterationTag) {
            return -1;
        }
        if (iterationTag > other.iterationTag) {
            return 1;
        }

        if (referenceIDs < other.referenceIDs) {
            return -1;
        }
        if (referenceIDs > other.referenceIDs) {
            return 1;
        }

        // if (linkedNames < other.linkedNames) {
            // return -1;
        // }
        // if (linkedNames > other.linkedNames) {
            // return 1;
        // }

        if (index > other.index) {
            return 1;
        }

        return -1;
    }

    bool operator<(MappedSection &other) const {
        return compare(other) == -1;
    }

    bool operator>(MappedSection &other) const {
        return compare(other) == 1;
    }

    std::string toString() const;

    enum OperationCode opCode = OperationCode::Maker;
    enum MapModifier mapModifier = MapModifier::Source;
    enum HistoryModifier historyModifier = HistoryModifier::New;
    int iterationTag = 0;
    std::vector<std::string> referenceIDs {};
    std::vector<std::pair<PersistentNameInfo, std::vector<MappedSection>>> linkedNames;
    std::string elementType = "";
    int index = 0;

    // these variables do not change the history of an element, they are just used in searching algorithms
    // to improve the quality of their outputs. they are not to be used in equality checks!
    int totalNumberOfSectionElements = 0;
    std::vector<std::pair<PersistentNameInfo, std::vector<MappedSection>>> deletedNames;
    bool isForkedElement = false;
};



} // namespace Data

#endif