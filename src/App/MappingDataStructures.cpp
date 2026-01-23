#ifndef DATA_MAPPING_STRUCTURES_CPP
#define DATA_MAPPING_STRUCTURES_CPP

#include "MappingDataStructures.h"
#include "Base/Console.h"

namespace Data
{
    void SavingUtil::compileKeys() {
        std::ostringstream ss;
        
        keys.clear();
        cachedSaveString.clear();

        if (formattingString.size()) {
            for (size_t i = 0; i < formattingString.size(); i++) {
                if (formattingString[i] != mainDeliminator)
                    ss << formattingString[i];
                
                if (formattingString[i] == mainDeliminator || ((i + 1) == formattingString.size() && ss.str().size())) {
                    keys.push_back(ss.str());
                    
                    ss.str("");
                }
            }
        }
    }

    std::string SavingUtil::getSaveString() {
        if (cachedSaveString.size()) {
            return cachedSaveString;
        } else {
            std::ostringstream ss;

            for (size_t i = 0; i < keys.size(); i++) {
                std::string key = keys[i];
                auto it = keyStorage.find(key);

                if (it != keyStorage.end()) {
                    std::vector<std::string> strList = it->second;

                    for (size_t j = 0; j < strList.size(); j++) {
                        std::string currentStr = strList[j];
                        
                        ss << (escapeChars(currentStr, deliminators));

                        if ((j + 1) != strList.size()) ss << subDeliminator;
                    }
                }

                if ((i + 1) != keys.size()) ss << mainDeliminator;
            }

            if (ss.str().size()) {
                cachedSaveString = ss.str();
                return cachedSaveString;
            }
        }

        return "";
    }

    std::unordered_map<std::string, std::vector<std::string>> SavingUtil::getRestoredKeyStorage(std::string restoreString) 
    {
        keyStorage.clear();

        int listPosition = 0;
        int subListPosition = 0;
        int escapeCount = 0;

        std::ostringstream ss;
        std::vector<std::string> value;

        for (size_t i = 0; i < restoreString.size(); i++) {
            char currentChr = restoreString[i];

            if (currentChr == '\\') {
                ++escapeCount;

                if (escapeCount == 1) {
                    continue; // avoid adding only one \ to `ss`
                }
            } else if (currentChr == mainDeliminator && escapeCount == 0) {
                ++listPosition;
                subListPosition = 0;

                if (ss.str().size()) {
                    value.push_back(ss.str());
                    ss.clear();
                }

                keyStorage[keys[listPosition]] = value;
                value.clear();

                continue;
            } else if (currentChr == subDeliminator && escapeCount == 0) {
                ++subListPosition;

                value.push_back(ss.str());
                ss.clear();

                continue;
            }

            ss << currentChr;

            // this needs to be the last thing done in this loop!
            if (currentChr != '\\') {
                escapeCount = 0;
            }
        }
    }
} // namespace Data

#endif // DATA_MAPPING_STRUCTURES_CPP