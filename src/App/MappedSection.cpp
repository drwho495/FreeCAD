// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef APP_MAPPED_SECTION_CPP
#define APP_MAPPED_SECTION_CPP

#include <memory>
#include <utility>
#include <string>
#include <sstream>

#include "MappingDataStructures.h"
#include "MappedSection.h"

namespace Data {

    std::string MappedSection::toString() const {
        SavingUtil sectionSaveUtil;

        sectionSaveUtil.setFormattingString(MAPPED_SECTION_SAVE_FORMAT);
        sectionSaveUtil.addSaveKey("OpCode", std::to_string(static_cast<int>(opCode)));
        sectionSaveUtil.addSaveKey("MapModifier", std::to_string(static_cast<int>(mapModifier)));
        sectionSaveUtil.addSaveKey("HistoryModifier", std::to_string(static_cast<int>(historyModifier)));
        sectionSaveUtil.addSaveKey("IterationTag", std::to_string(iterationTag));
        sectionSaveUtil.addSaveKey("ReferenceIDs", referenceIDs);

        auto saveMappedNames = [](std::string saveKey, SavingUtil saveUtil, std::vector<std::pair<PersistentNameInfo, std::vector<MappedSection>>> names) {
            std::vector<std::string> namesList;
            
            for (auto &name : names) {
                SavingUtil nameSaveUtil;

                nameSaveUtil.setFormattingString(MAPPED_NAME_SAVE_FORMAT);
                nameSaveUtil.addSaveKey("ElementMapVersion", std::to_string(ELEMENT_MAP_VERSION)); // just use the default for right now.
                nameSaveUtil.addSaveKey("DuplicateCount", std::to_string(name.first.duplicateCount));
                
                std::ostringstream sectionStream;

                for (size_t i = 0; i < name.second.size(); i++) {
                    std::string sectionString = name.second[i].toString();
        
                    sectionStream << SavingUtil::escapeChars(sectionString, {'|'});

                    if ((i + 1) != name.second.size()) {
                        sectionStream << '|';
                    }
                }

                nameSaveUtil.addSaveKey("MappedSections", sectionStream.str());
                namesList.push_back(nameSaveUtil.getSaveString());
            }

            saveUtil.addSaveKey(saveKey, namesList);
        };

        saveMappedNames("LinkedNames", sectionSaveUtil, linkedNames);
        sectionSaveUtil.addSaveKey("ElementType", elementType);
        sectionSaveUtil.addSaveKey("Index", std::to_string(index));
        sectionSaveUtil.addSaveKey("TNoSE", std::to_string(totalNumberOfSectionElements));
        saveMappedNames("DeletedNames", sectionSaveUtil, deletedNames);
        sectionSaveUtil.addSaveKey("IsForkedElement", std::to_string(isForkedElement));

        return sectionSaveUtil.getSaveString();
    }

}; // namespace Data

#endif // APP_MAPPED_SECTION_CPP