// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef MAPPED_SECTION_UTILS_H
#define MAPPED_SECTION_UTILS_H

#include <string>
#include <utility>
#include "FCGlobal.h"
#include "MappedName.h"
#include "MappedSection.h"
#include "MappingDataStructures.h"
#include "ElementNamingUtils.h"
#include <sstream>

namespace Data
{

AppExport MappedSection buildMappedSection(enum OperationCode opCode,
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

    std::vector<std::pair<PersistentNameInfo, std::vector<MappedSection>>> reformattedLinkedNames;
    for (auto &linkedName : linkedNames) {
        reformattedLinkedNames.push_back(std::make_pair(linkedName.nameInfo, linkedName.sections));
    }

    mappedSection.linkedNames = reformattedLinkedNames;
    mappedSection.elementType = elementType;
    mappedSection.index = index;
    mappedSection.totalNumberOfSectionElements = totalNumberOfSectionElements;

    std::vector<std::pair<PersistentNameInfo, std::vector<MappedSection>>> reformattedDeletedNames;
    for (auto &deletedName : deletedNames) {
        reformattedDeletedNames.push_back(std::make_pair(deletedName.nameInfo, deletedName.sections));
    }

    mappedSection.deletedNames = reformattedDeletedNames;
    mappedSection.isForkedElement = isForkedElement;

    return mappedSection;
};

AppExport std::vector<MappedName> getLinkedNames(MappedSection &section) {
    std::vector<MappedName> names;

    for (auto &name : section.linkedNames) {
        MappedName newName = MappedName(AlgorithmType::New);

        newName.nameInfo = name.first;
        newName.sections = name.second;

        names.push_back(newName);
    }

    return names;
};

AppExport std::vector<MappedName> getDeletedNames(MappedSection &section) {
    std::vector<MappedName> names;

    for (auto &name : section.deletedNames) {
        MappedName newName = MappedName(AlgorithmType::New);

        newName.nameInfo = name.first;
        newName.sections = name.second;

        names.push_back(newName);
    }

    return names;
};


}  // namespace Data
// clang-format on

#endif  // ELEMENT_NAMING_UTILS_H
