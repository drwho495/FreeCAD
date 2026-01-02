// SPDX-License-Identifier: LGPL-2.1-or-later

/****************************************************************************
 *   Copyright (c) 2018-2022 Zheng, Lei (realthunder)                       *
 *   <realthunder.dev@gmail.com>                                            *
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

#ifndef DATA_ELEMENTMAP_H
#define DATA_ELEMENTMAP_H

#include "FCGlobal.h"

#include "Application.h"
#include "MappedElement.h"

#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>


namespace Data
{

class ElementMap;
using ElementMapPtr = std::shared_ptr<ElementMap>;

/* This class provides for ComplexGeoData's ability to provide proper naming.
 * Specifically, ComplexGeoData uses this class for it's `_id` property.
 * Most of the operations work with the `indexedNames` and `mappedNames` maps.
 * `indexedNames` maps a string to both a name queue and children.
 *   each of those children store an IndexedName, offset details, postfix, ids, and
 *   possibly a recursive elementmap
 * `mappedNames` maps a MappedName to a specific IndexedName.
 */
class AppExport ElementMap
    : public std::enable_shared_from_this<ElementMap>  // TODO can remove shared_from_this?
{
public:
    /** Default constructor: hooks internal functions to \c signalSaveDocument and
     * \c signalStartRestoreDocument. This is related to the save and restore process
     * of the map.
     */
    ElementMap();

    /** Ensures that naming is properly assigned. It then marks as "used" all the StringID
     * that are used to make up this particular map and are stored in the hasherRef passed
     * as a parameter. Finally do this recursively for all childEelementMaps as well.
     *
     * @param hasherRef where all the StringID needed to build the map are stored.
     */
    // FIXME this should be made part of \c save, to achieve symmetry with the restore method
    // void beforeSave(const ::App::StringHasherRef& hasherRef) const;

    /** Serialize this map. Calls \c collectChildMaps to get \c childMapSet and
     * \c postfixMap, then calls the other (private) save function with those parameters.
     * @param stream: serialized stream
     */
    void save(std::ostream& stream) const;

    /** Deserialize and restore this map. This function restores \c childMaps and
     * \c postfixes from the stream, then calls the other (private) restore function with those
     * parameters.
     * @param hasherRef: where all the StringIDs are stored
     * @param stream: stream to deserialize
     */
    ElementMapPtr restore(std::istream& stream);


    /** Add a sub-element name mapping.
     *
     * @param element: the original \c Type + \c Index element name
     * @param name: the mapped sub-element name. May or may not start with
     * elementMapPrefix().
     * @param sid: in case you use a hasher to hash the element name, pass in
     * the string id reference using this parameter. You can have more than one
     * string id associated with the same name.
     * @param overwrite: if true, it will overwrite existing names
     *
     * @return Returns the stored mapped element name.
     *
     * An element can have multiple mapped names. However, a name can only be
     * mapped to one element
     *
     * Note: the original proc was in the context of ComplexGeoData, which provided `Tag` access,
     *   now you must pass in `long masterTag` explicitly.
     */
    MappedName setElementName(const IndexedName& element,
                              const MappedName& name,
                              long masterTag,
                              bool overwrite = false);

    /// Remove \c name from the map
    void erase(const MappedName& name);

    /// Remove \c idx and all the MappedNames associated with it
    void erase(const IndexedName& idx);

    unsigned long size() const;

    bool empty() const;

    IndexedName find(const MappedName& name) const;

    MappedName find(const IndexedName& idx) const;

    std::vector<MappedName> findAll(const IndexedName& idx) const;

    bool hasChildElementMap() const;

    /* Ensures that for each IndexedName mapped to IndexedElements, that
     *  each child is properly hashed (cached).
     *
     * Note: the original proc was in the context of ComplexGeoData, which provided `Tag` access,
     *   now you must pass in `long masterTag` explicitly.
     */
    void hashChildMaps(long masterTag);

    /* Note: the original addChildElements passed `ComplexGeoData& master` for getting the `Tag`,
     *   now it just passes `long masterTag`.*/
    void addChildElements(long masterTag, std::unordered_map<IndexedName, ElementMapPtr, IndexedNameHasher>& children);

    void addChildElement(long masterTag, IndexedName indexedName, ElementMapPtr childMap);

    std::unordered_map<IndexedName, ElementMapPtr, IndexedNameHasher> getChildElements() const;

    std::vector<MappedElement> getAll() const;

    long getElementHistory(const MappedName& name,
                           long masterTag,
                           MappedName* original = nullptr,
                           std::vector<MappedName>* history = nullptr) const;

    /** Iterate through the history of the give element name with a given callback
     *
     * @param name: the input element name
     * @param cb: trace callback with call signature.
     * @sa TraceCallback
     */
    // void traceElement(const MappedName& name, long masterTag, TraceCallback cb) const;

private:
    /** Serialize this map
     * @param stream: serialized stream
     * @param childMapSet: where all child element maps are stored
     * @param postfixMap. where all postfixes are stored
     */
    void save(std::ostream& stream,
              int index,
              const std::map<const ElementMap*, int>& childMapSet,
              const std::map<QByteArray, int>& postfixMap) const;

    /** Deserialize and restore this map.
     * @param hasherRef: where all the StringIDs are stored
     * @param stream: stream to deserialize
     * @param childMaps: where all child element maps are stored
     * @param postfixes. where all postfixes are stored
     */
    ElementMapPtr restore(std::istream& stream,
                          std::vector<ElementMapPtr>& childMaps,
                          const std::vector<std::string>& postfixes);

    /** Associate the MappedName \c name with the IndexedName \c idx.
     * @param name: the name to add
     * @param idx: the indexed name that \c name will be bound to
     * @param sids: where StringIDs that make up the name are stored
     * @param overwrite: if true, all the names associated with \c idx will be discarded
     * @param existing: out variable: if not overwriting, and \c name is already
     * associated with another indexedName, set \c existing to that indexedname
     * @return the name just added, or an empty name if it wasn't added.
     */
    MappedName addName(MappedName& name,
                       const IndexedName& idx,
                       bool overwrite,
                       IndexedName* existing);

    /* Note: the original proc passed `ComplexGeoData& master` for getting the `Tag`,
     *   now it just passes `long masterTag`.*/
    MappedName renameDuplicateElement(const IndexedName& element,
                                      const IndexedName& element2,
                                      const MappedName& name) const;

    // FIXME duplicate code? as in copy/paste
    // const MappedNameRef* findMappedRef(const IndexedName& idx) const;
    // MappedNameRef* findMappedRef(const IndexedName& idx);

    // MappedNameRef& mappedRef(const IndexedName& idx);

    void collectChildMaps(std::map<const ElementMap*, int>& childMapSet,
                          std::vector<const ElementMap*>& childMaps,
                          std::map<QByteArray, int>& postfixMap,
                          std::vector<QByteArray>& postfixes) const;

    struct CStringComp
    {
    public:
        bool operator()(const char* str1, const char* str2) const
        {
            return std::strcmp(str1, str2) < 0;
        }
    };

    struct ChildMapInfo
    {
        int index = 0;
        MappedChildElements* childMap = nullptr;
        std::map<ElementMap*, int> mapIndices;
    };

    std::unordered_map<MappedName, IndexedName, MappedNameHasher> mappedNames;
    std::unordered_map<IndexedName, ElementMapPtr, IndexedNameHasher> childElements;

    void init();
};


}  // namespace Data

#endif  // DATA_ELEMENTMAP_H
