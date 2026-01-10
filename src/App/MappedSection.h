// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef APP_MAPPED_SECTION_H
#define APP_MAPPED_SECTION_H

#include <memory>
#include <string>

#include <boost/algorithm/string/predicate.hpp>

#include <utility>

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