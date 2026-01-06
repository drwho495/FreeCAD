// SPDX-License-Identifier: LGPL-2.1-or-later

/****************************************************************************
 *   Copyright (c) 2020 Zheng, Lei (realthunder) <realthunder.dev@gmail.com>*
 *                                                                          *
 *   This file is part of the FreeCAD CAx development system.               *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Library General Public            *
 *   License as published by the Free Software Foundation; either           *
 *   version 2 of the License, or (at your option) any later version.       *
 *                                                                          *
 *   This library  is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Library General Public License for more details.                   *
 *                                                                          *
 *   You should have received a copy of the GNU Library General Public      *
 *   License along with this library; see the file COPYING.LIB. If not,     *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,          *
 *   Suite 330, Boston, MA  02111-1307, USA                                 *
 *                                                                          *
 ****************************************************************************/

#include <unordered_set>

#include "MappedSection.h"
#include "MappingDataStructures.h"

#include "Base/Console.h"

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>


FC_LOG_LEVEL_INIT("MappedSection", true, 2);  // NOLINT

namespace Data
{
    // std::string MappedSection::toString() const {
    //     std::ostringstream ss;
    //     std::ostringstream saveName;
    //     std::unordered_set<char> unsafeChars;

    //     int stringLength = strlen(SECTION_SAVE_FORMAT);

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

    //     return ss.str();
    // }
}  // namespace Data