// SPDX-License-Identifier: LGPL-2.1-or-later

/****************************************************************************
 *   Copyright (c) 2022 Zheng, Lei (realthunder) <realthunder.dev@gmail.com>*
 *   Copyright (c) 2023 FreeCAD Project Association                         *
 *                                                                          *
 *   This file is part of FreeCAD.                                          *
 *                                                                          *
 *   FreeCAD is free software: you can redistribute it and/or modify it     *
 *   under the terms of the GNU Lesser General Public License as            *
 *   published by the Free Software Foundation, either version 2.1 of the   *
 *   License, or (at your option) any later version.                        *
 *                                                                          *
 *   FreeCAD is distributed in the hope that it will be useful, but         *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of             *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU Lesser General Public       *
 *   License along with FreeCAD. If not, see                                *
 *   <https://www.gnu.org/licenses/>.                                       *
 *                                                                          *
 ***************************************************************************/

#ifndef APP_MAPPED_NAME_H
#define APP_MAPPED_NAME_H

#include <memory>
#include <string>

#include <boost/algorithm/string/predicate.hpp>

#include <QByteArray>
#include <QHash>
#include <QVector>
#include <utility>

#include "IndexedName.h"
#include "MappedSection.h"


namespace Data
{

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

/// The MappedName class maintains a two-part name: the first part ("data") is considered immutable
/// once created, while the second part ("postfix") can be modified/appended to by later operations.
/// It uses shared data when possible (see the fromRawData() members). Despite storing data and
/// postfix separately, they can be accessed via calls to size(), operator[], etc. as though they
/// were a single array.
class AppExport MappedName
{
public:
    MappedName(const MappedName& other) = default;
    ~MappedName() = default;

    MappedName(std::vector<MappedSection> sections) {
        for (const MappedSection &section : sections) {
            this->sections.push_back(std::make_unique<MappedSection>(section));
        }
    };

    /// Share data with another MappedName
    MappedName& operator=(const MappedName& other) = default;

    /// Two MappedNames are equal if the concatenation of their data and postfix is equal. The
    /// individual data and postfix may NOT be equal in this case.
    // TODO: implement
    bool operator==(const MappedName& other) const
    {
        if (this->sections.size() != other.sections.size()) {
            return false;
        }

        return this->hash() == other.hash();
    }

    bool operator!=(const MappedName& other) const
    {
        return !(this->operator==(other));
    }

    /// Create a std::string from this instance, starting at startPosition, and extending len bytes.
    ///
    /// \param startPosition The offset into the data
    /// \param len The number of bytes to output
    /// \return A new std::string containing the bytes copied from this instance's data and postfix
    /// (depending on startPosition and len).
    /// \note No effort is made to ensure that these are valid ASCII characters, and it is possible
    /// the data includes embedded null characters, non-ASCII data, etc.
    std::string toString(int startPosition = 0, int len = -1) const
    {
        return std::string();
    }

    // if offset is inside data return data, if offset is > data.size
    //(ends up in postfix) return postfix
    const char* toConstString(int offset, int& size) const
    {
        return toString().c_str();
    }

    /// Treat this MappedName as a single continuous array of bytes, returning the combined size
    /// of the data and postfix.
    int sectionSize() const
    {
        return sections.size();
    }

    /// Treat this MappedName as a single continuous array of bytes, returning true only if both
    /// data and prefix are empty.
    bool empty() const
    {
        return (this->sectionSize() == 0);
    }

    /// If this is shared data, a new unshared copy is made and returned. If it is already unshared
    /// no new copy is made, a new instance is returned that shares is data with the current
    /// instance.
    MappedName copy() const
    {
        return (*this);
    }

    /// Ensure that this data is unshared, making a copy if necessary.
    void compact() const;

    /// Boolean conversion is the inverse of empty(), returning true if there is data in either the
    /// data or postfix, and false if there is nothing in either.
    explicit operator bool() const
    {
        return !empty();
    }

    /// Reset this instance, clearing anything in data and postfix.
    void clear()
    {
    }

    /// Get a hash for this MappedName
    std::size_t hash() const
    {
        return qHash(toString());
    }

private:
    std::vector<std::unique_ptr<MappedSection>> sections;
};


// using ElementIDRefs = QVector<::App::StringIDRef>;

// struct MappedNameRef
// {
//     MappedName name;
//     ElementIDRefs sids;
//     std::unique_ptr<MappedNameRef> next;

//     MappedNameRef() = default;

//     ~MappedNameRef() = default;

//     MappedNameRef(MappedName name, ElementIDRefs sids = ElementIDRefs())
//         : name(std::move(name))
//         , sids(std::move(sids))
//     {
//         compact();
//     }

//     MappedNameRef(const MappedNameRef& other)
//         : name(other.name)
//         , sids(other.sids)
//     {}

//     MappedNameRef(MappedNameRef&& other) noexcept
//         : name(std::move(other.name))
//         , sids(std::move(other.sids))
//         , next(std::move(other.next))
//     {}

//     MappedNameRef& operator=(const MappedNameRef& other) noexcept
//     {
//         name = other.name;
//         sids = other.sids;
//         return *this;
//     }

//     MappedNameRef& operator=(MappedNameRef&& other) noexcept
//     {
//         name = std::move(other.name);
//         sids = std::move(other.sids);
//         next = std::move(other.next);
//         return *this;
//     }

//     explicit operator bool() const
//     {
//         return !name.empty();
//     }

//     void append(const MappedName& _name, const ElementIDRefs _sids = ElementIDRefs())
//     {
//         if (!_name) {
//             return;
//         }
//         if (!this->name) {
//             this->name = _name;
//             this->sids = _sids;
//             compact();
//             return;
//         }
//         std::unique_ptr<MappedNameRef> mappedName(new MappedNameRef(_name, _sids));
//         if (!this->next) {
//             this->next = std::move(mappedName);
//         }
//         else {
//             this->next.swap(mappedName);
//             this->next->next = std::move(mappedName);
//         }
//     }

//     void compact()
//     {
//         if (sids.size() > 1) {
//             std::sort(sids.begin(), sids.end());
//             sids.erase(std::unique(sids.begin(), sids.end()), sids.end());
//         }
//     }

//     bool erase(const MappedName& _name)
//     {
//         if (this->name == _name) {
//             this->name.clear();
//             this->sids.clear();
//             if (this->next) {
//                 this->name = std::move(this->next->name);
//                 this->sids = std::move(this->next->sids);
//                 std::unique_ptr<MappedNameRef> tmp;
//                 tmp.swap(this->next);
//                 this->next = std::move(tmp->next);
//             }
//             return true;
//         }

//         for (std::unique_ptr<MappedNameRef>* ptr = &this->next; *ptr; ptr = &(*ptr)->next) {
//             if ((*ptr)->name == _name) {
//                 std::unique_ptr<MappedNameRef> tmp;
//                 tmp.swap(*ptr);
//                 *ptr = std::move(tmp->next);
//                 return true;
//             }
//         }
//         return false;
//     }

//     void clear()
//     {
//         this->name.clear();
//         this->sids.clear();
//         this->next.reset();
//     }
// };


// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)


}  // namespace Data


#endif  // APP_MAPPED_NAME_H
