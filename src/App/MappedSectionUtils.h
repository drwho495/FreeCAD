// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef MAPPED_SECTION_UTILS_H
#define MAPPED_SECTION_UTILS_H

#include <string>
#include <utility>
#include "FCGlobal.h"
#include "MappedName.h"
#include "MappedSection.h"
#include "MappingDataStructures.h"

namespace App
{

static MappedSection buildElementSection(enum OperationCode opCode,
                                         enum MapModifier mapModifier,
                                         enum HistoryModifier historyModifier,
                                         int iterationTag,
                                         std::vector<std::string> referenceIDs,
                                         std::vector<MappedName> linkedNames,
                                         std::string elementType,
                                         int index,
                                         int totalNumberOfSectionElements,
                                         std::vector<MappedName> deletedNames,
                                         bool isForkedElement)
{
    MappedSection mappedSection = MappedSection();

    mappedSection.opCode = opCode;
    mappedSection.mapModifier = mapModifier;
    mappedSection.historyModifier = historyModifier;
    mappedSection.iterationTag = iterationTag;
    mappedSection.referenceIDs = referenceIDs;

    std::vector<std::unique_ptr<MappedName>> linkedNamePtrs;
    for (auto &linkedName : linkedNames) {
        linkedNamePtrs.push_back(std::make_unique<MappedName>(linkedName));
    }

    mappedSection.setLinkedNames(linkedNamePtrs);
    mappedSection.elementType = elementType;
    mappedSection.index = index;
    mappedSection.totalNumberOfSectionElements = totalNumberOfSectionElements;

    std::vector<std::unique_ptr<MappedName>> deletedNamePtrs;
    for (auto &deletedName : deletedNames) {
        linkedNamePtrs.push_back(std::make_unique<MappedName>(deletedName));
    }

    mappedSection.setDeletedNames(deletedNamePtrs);
    mappedSection.isForkedElement = isForkedElement;

    return mappedSection;
}

}  // namespace Data
// clang-format on

#endif  // ELEMENT_NAMING_UTILS_H
