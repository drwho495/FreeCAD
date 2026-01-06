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
class MappedName;

constexpr const char SECTION_DELIMINATOR = ';';
constexpr const char SECTION_SAVE_FORMAT[] = "OpCode;MapModifier;HistoryModifier;IterationTag;ReferenceIDs;LinkedNames;ElementType;Index;TNoSE;DeletedNames;IsForkedElement";

class AppExport MappedSection
{
public:
    MappedSection() = default;

    // MappedSection(enum OperationCode opCode,
    //               enum MapModifier mapModifier,
    //               enum HistoryModifier historyModifier,
    //               int iterationTag,
    //               std::vector<std::string> referenceIDs,
    //               std::vector<std::unique_ptr<MappedName>> linkedNames,
    //               std::string elementType,
    //               int index,
    //               int totalNumberOfSectionElements,
    //               std::vector<std::unique_ptr<MappedName>> deletedNames,
    //               bool isForkedElement)
    // {
    //     this->opCode = opCode;
    //     this->mapModifier = mapModifier;
    //     this->historyModifier = historyModifier;
    //     this->iterationTag = iterationTag;
    //     this->referenceIDs = referenceIDs;
    //     this->setLinkedNames(linkedNames);
    //     this->elementType = elementType;
    //     this->index = index;
    //     this->totalNumberOfSectionElements = totalNumberOfSectionElements;
    //     this->setDeletedNames(deletedNames);
    //     this->isForkedElement = isForkedElement;
    // }

    void setLinkedNames(std::vector<std::unique_ptr<MappedName>> newLinkedNames) {
        this->linkedNames.swap(newLinkedNames);
    }

    void setDeletedNames(std::vector<std::unique_ptr<MappedName>> newDeletedNames) {
        this->deletedNames.swap(newDeletedNames);
    }

    // std::string toString() const;

    enum OperationCode opCode = OperationCode::Maker;
    enum MapModifier mapModifier = MapModifier::Source;
    enum HistoryModifier historyModifier = HistoryModifier::New;
    int iterationTag = 0;
    std::vector<std::string> referenceIDs {};
    std::vector<std::unique_ptr<MappedName>> linkedNames;
    std::string elementType = "";
    int index = 0;

    // these variables do not change the history of an element, they are just used in searching algorithms
    // to improve the quality of their outputs. they are not to be used in equality checks!
    int totalNumberOfSectionElements = 0;
    std::vector<std::unique_ptr<MappedName>> deletedNames;
    bool isForkedElement = false;
};



} // namespace Data

#endif