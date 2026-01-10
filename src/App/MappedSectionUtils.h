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

AppExport MappedSection buildElementSection(enum OperationCode opCode,
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

AppExport MappedSection buildElementSection(enum OperationCode opCode,
                                            enum MapModifier mapModifier,
                                            enum HistoryModifier historyModifier,
                                            int iterationTag,
                                            std::vector<std::string> referenceIDs,
                                            std::vector<std::pair<PersistentNameInfo, std::vector<MappedSection>>> linkedNames,
                                            std::string elementType,
                                            int index,
                                            int totalNumberOfSectionElements,
                                            std::vector<std::pair<PersistentNameInfo, std::vector<MappedSection>>> deletedNames,
                                            bool isForkedElement)
{
    MappedSection mappedSection = MappedSection();

    mappedSection.opCode = opCode;
    mappedSection.mapModifier = mapModifier;
    mappedSection.historyModifier = historyModifier;
    mappedSection.iterationTag = iterationTag;
    mappedSection.referenceIDs = referenceIDs;
    mappedSection.linkedNames = linkedNames;
    mappedSection.elementType = elementType;
    mappedSection.index = index;
    mappedSection.totalNumberOfSectionElements = totalNumberOfSectionElements;
    mappedSection.deletedNames = deletedNames;
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

AppExport std::string toString(MappedSection &section) {
    // std::ostringstream ss;
    // std::ostringstream saveName;
    // std::unordered_set<char> unsafeChars;

    // int stringLength = strlen(SECTION_SAVE_FORMAT);

    //     unsafeChars.insert(SECTION_DELIMINATOR);
    //     unsafeChars.insert(':');
        
    //     for (int i = 0; i < stringLength; i++) {
    //         if (SECTION_SAVE_FORMAT[i] != SECTION_DELIMINATOR) {
    //             saveName << SECTION_SAVE_FORMAT[i];
    //         }

    //         bool endOfString = ((i + 1) == stringLength);
            
    //         if (SECTION_SAVE_FORMAT[i] == SECTION_DELIMINATOR || endOfString) {
    //             std::string saveNameStr = saveName.str();
    //             std::ostringstream newSection;

    //             if (saveNameStr == "OpCode") {
    //                 newSection << static_cast<int>(opCode);
    //             } else if (saveNameStr == "MapModifier") {
    //                 newSection << static_cast<int>(mapModifier);
    //             } else if (saveNameStr == "HistoryModifier") {
    //                 newSection << static_cast<int>(historyModifier);
    //             } else if (saveNameStr == "IterationTag") {
    //                 newSection << iterationTag;
    //             } else if (saveNameStr == "ReferenceIDs") {
    //                 std::string separator = ",";

    //                 newSection << std::accumulate(referenceIDs.begin(), referenceIDs.end(), std::string(""),
    //                         [&separator](const std::string& a, const std::string& b) -> std::string {
    //                             if (a.empty()) {
    //                                 return b;
    //                             } else {
    //                                 return a + separator + b;
    //                             }
    //                         });
    //             }

    //             ss << escapeChars(newSection.str(), unsafeChars);

    //             if (!endOfString) {
    //                 ss << SECTION_DELIMINATOR;
    //             }

    //             saveName.clear();
    //         }
    //     }

    // return ss.str();
}

}  // namespace Data
// clang-format on

#endif  // ELEMENT_NAMING_UTILS_H
