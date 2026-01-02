// SPDX-License-Identifier: LGPL-2.1-or-later

#include <unordered_map>
#ifndef FC_DEBUG
#include <random>
#endif

#include "ElementMap.h"
#include "ElementNamingUtils.h"
#include <IndexedName.h>

#include "App/Application.h"
#include "Base/Console.h"
#include "Document.h"
#include "DocumentObject.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/io/ios_state.hpp>


FC_LOG_LEVEL_INIT("ElementMap", true, 2);  // NOLINT

namespace Data
{


// Because the existence of hierarchical element maps, for the same document
// we may store an element map more than once in multiple objects. And because
// we may want to support partial loading, we choose to tolerate such redundancy
// for now.
//
// In order to not waste memory space when the file is loaded, we use the
// following two maps to assign a one-time id for each unique element map.  The
// id will be saved together with the element map.
//
// When restoring, we'll read back the id and lookup for an existing element map
// with the same id, and skip loading the current map if one is found.
//
// TODO: Note that the same redundancy can be found when saving OCC shapes,
// because we currently save shapes for each object separately. After restoring,
// any shape sharing is lost. But again, we do want to keep separate shape files
// because of partial loading. The same technique used here can be applied to
// restore shape sharing.
static std::unordered_map<const ElementMap*, unsigned> _elementMapToId;
static std::unordered_map<unsigned, ElementMapPtr> _idToElementMap;


void ElementMap::init()
{
    static bool inited;
    if (!inited) {
        inited = true;
        ::App::GetApplication().signalStartSaveDocument.connect(
            [](const ::App::Document&, const std::string&) {
                _elementMapToId.clear();
            });
        ::App::GetApplication().signalFinishSaveDocument.connect(
            [](const ::App::Document&, const std::string&) {
                _elementMapToId.clear();
            });
        ::App::GetApplication().signalStartRestoreDocument.connect([](const ::App::Document&) {
            _idToElementMap.clear();
        });
        ::App::GetApplication().signalFinishRestoreDocument.connect([](const ::App::Document&) {
            _idToElementMap.clear();
        });
    }
}

ElementMap::ElementMap()
{
    init();
}


// todo: reimplement
// void ElementMap::beforeSave() const
// {
    // unsigned& id = _elementMapToId[this];
    // if (id == 0U) {
    //     id = _elementMapToId.size();
    // }
    // this->_id = id;

    // for (auto& indexedName : this->indexedNames) {
        // for (const MappedNameRef& mappedName : indexedName.second.names) {
        //     for (const MappedNameRef* ref = &mappedName; ref; ref = ref->next.get()) {
        //         for (const ::App::StringIDRef& sid : ref->sids) {
        //             if (sid.isFromSameHasher(hasherRef)) {
        //                 sid.mark();
        //             }
        //         }
        //     }
        // }
        // for (auto& childPair : indexedName.second.children) {
        //     if (childPair.second.elementMap) {
        //         childPair.second.elementMap->beforeSave(hasherRef);
        //     }
        //     for (auto& sid : childPair.second.sids) {
        //         if (sid.isFromSameHasher(hasherRef)) {
        //             sid.mark();
        //         }
        //     }
        // }
    // }
// }

void ElementMap::save(std::ostream& stream,
                      int index,
                      const std::map<const ElementMap*, int>& childMapSet,
                      const std::map<QByteArray, int>& postfixMap) const
{
    stream << "\nElementMap " << index << ' ' << this->_id << ' ' << this->indexedNames.size()
           << '\n';

    for (auto& indexedName : this->indexedNames) {
        stream << '\n' << indexedName.first << '\n';

        stream << "\nChildCount " << indexedName.second.children.size() << '\n';
        for (auto& vv : indexedName.second.children) {
            auto& child = vv.second;
            int mapIndex = 0;
            if (child.elementMap) {
                auto it = childMapSet.find(child.elementMap.get());
                if (it == childMapSet.end() || it->second == 0) {
                    FC_ERR("Invalid child element map");  // NOLINT
                }
                else {
                    mapIndex = it->second;
                }
            }
            stream << child.indexedName.getIndex() << ' ' << child.offset << ' ' << child.count
                   << ' ' << child.tag << ' ' << mapIndex << ' ';
            stream.write(child.postfix.constData(), child.postfix.size());
            stream << ' ' << '0';
            for (auto& sid : child.sids) {
                if (sid.isMarked()) {
                    stream << '.' << sid.value();
                }
            }
            stream << '\n';
        }

        stream << "\nNameCount " << indexedName.second.names.size() << '\n';
        if (indexedName.second.names.empty()) {
            continue;
        }

        boost::io::ios_flags_saver ifs(stream);
        stream << std::hex;

        for (auto& dequeueOfMappedNameRef : indexedName.second.names) {
            for (auto ref = &dequeueOfMappedNameRef; ref; ref = ref->next.get()) {
                if (!ref->name) {
                    break;
                }

                // ::App::StringID::IndexID prefixID {};
                // prefixID.id = 0;
                IndexedName idx(ref->name.dataBytes());
                bool printName = true;
                if (idx) {
                    auto key = QByteArray::fromRawData(idx.getType(),
                                                       static_cast<int>(qstrlen(idx.getType())));
                    auto it = postfixMap.find(key);
                    if (it != postfixMap.end()) {
                        stream << ':' << it->second << '.' << idx.getIndex();
                        printName = false;
                    }
                }
                else {
                    prefixID = ::App::StringID::fromString(ref->name.dataBytes());
                    if (prefixID.id != 0) {
                        for (auto& sid : ref->sids) {
                            if (sid.isMarked() && sid.value() == prefixID.id) {
                                stream << '$';
                                stream.write(ref->name.dataBytes().constData(),
                                             ref->name.dataBytes().size());
                                printName = false;
                                break;
                            }
                        }
                        if (printName) {
                            prefixID.id = 0;
                        }
                    }
                }
                if (printName) {
                    stream << ';';
                    stream.write(ref->name.dataBytes().constData(), ref->name.dataBytes().size());
                }

                const QByteArray& postfix = ref->name.postfixBytes();
                if (postfix.isEmpty()) {
                    stream << ".0";
                }
                else {
                    auto it = postfixMap.find(postfix);
                    assert(it != postfixMap.end());
                    stream << '.' << it->second;
                }
                for (auto& sid : ref->sids) {
                    if (sid.isMarked() && sid.value() != prefixID.id) {
                        stream << '.' << sid.value();
                    }
                }

                stream << ' ';
            }
            stream << "0\n";
        }
    }
    stream << "\nEndMap\n";
}

void ElementMap::save(std::ostream& stream) const
{
    std::map<const ElementMap*, int> childMapSet;
    std::vector<const ElementMap*> childMaps;
    std::map<QByteArray, int> postfixMap;
    std::vector<QByteArray> postfixes;

    collectChildMaps(childMapSet, childMaps, postfixMap, postfixes);

    stream << this->_id << " PostfixCount " << postfixes.size() << '\n';
    for (auto& postfix : postfixes) {
        stream.write(postfix.constData(), postfix.size());
        stream << '\n';
    }
    int index = 0;
    stream << "\nMapCount " << childMaps.size() << '\n';
    for (auto& elementMap : childMaps) {
        elementMap->save(stream, ++index, childMapSet, postfixMap);
    }
}

ElementMapPtr ElementMap::restore(std::istream& stream)
{
    const char* msg = "Invalid element map";

    unsigned id = 0;
    int count = 0;
    std::string tmp;
    if (!(stream >> id >> tmp >> count) || tmp != "PostfixCount") {
        FC_THROWM(Base::RuntimeError, msg);  // NOLINT
    }

    auto& map = _idToElementMap[id];
    if (map) {
        return map;
    }

    std::vector<std::string> postfixes;
    postfixes.reserve(count);
    for (int i = 0; i < count; ++i) {
        postfixes.emplace_back();
        stream >> postfixes.back();
    }

    std::vector<ElementMapPtr> childMaps;
    count = 0;
    constexpr int practicalMaximum {(1 << 30) / sizeof(ElementMapPtr)};  // a 1GB child map vector: almost certainly a bug
    if (!(stream >> tmp >> count) || tmp != "MapCount" || count == 0 || count > practicalMaximum) {
        FC_THROWM(Base::RuntimeError, msg);  // NOLINT
    }
    childMaps.reserve(count - 1);
    for (int i = 0; i < count - 1; ++i) {
        childMaps.push_back(
            std::make_shared<ElementMap>()->restore(hasherRef, stream, childMaps, postfixes));
    }

    return restore(hasherRef, stream, childMaps, postfixes);
}

ElementMapPtr ElementMap::restore(std::istream& stream,
                                  std::vector<ElementMapPtr>& childMaps,
                                  const std::vector<std::string>& postfixes)
{
    const char* msg = "Invalid element map";
    const int hexBase {16};
    const int decBase {10};
    std::string tmp;
    int index = 0;
    int typeCount = 0;
    unsigned id = 0;
    if (!(stream >> tmp >> index >> id >> typeCount) || tmp != "ElementMap") {
        FC_THROWM(Base::RuntimeError, msg);  // NOLINT
    }
    constexpr int maxTypeCount(1000);
    if (typeCount < 0 || typeCount > maxTypeCount) {
        FC_THROWM(Base::RuntimeError, "Bad type count in element map, ignoring map");  // NOLINT
    }

    auto& map = _idToElementMap[id];
    if (map) {
        while (tmp != "EndMap") {
            if (!std::getline(stream, tmp)) {
                FC_THROWM(Base::RuntimeError, "unexpected end of child element map");  // NOLINT
            }
        }
        return map;
    }

    const char* hasherWarn = nullptr;
    const char* hasherIDWarn = nullptr;
    const char* postfixWarn = nullptr;
    const char* childSIDWarn = nullptr;
    std::vector<std::string> tokens;

    for (int i = 0; i < typeCount; ++i) {
        int outerCount = 0;
        if (!(stream >> tmp)) {
            FC_THROWM(Base::RuntimeError, "missing element type");  // NOLINT
        }
        IndexedName idx(tmp.c_str(), 1);

        if (!(stream >> tmp >> outerCount) || tmp != "ChildCount") {
            FC_THROWM(Base::RuntimeError, "missing element child count");  // NOLINT
        }

        auto& indices = this->indexedNames[idx.getType()];
        for (int j = 0; j < outerCount; ++j) {
            int cIndex = 0;
            int offset = 0;
            int count = 0;
            long tag = 0;
            int mapIndex = 0;
            if (!(stream >> cIndex >> offset >> count >> tag >> mapIndex >> tmp)) {
                FC_THROWM(Base::RuntimeError, "Invalid element child");  // NOLINT
            }
            if (cIndex < 0) {
                FC_THROWM(Base::RuntimeError, "Invalid element child index");  // NOLINT
            }
            if (offset < 0) {
                FC_THROWM(Base::RuntimeError, "Invalid element child offset");  // NOLINT
            }
            if (mapIndex >= index || mapIndex < 0 || mapIndex > (int)childMaps.size()) {
                FC_THROWM(Base::RuntimeError, "Invalid element child map index");  // NOLINT
            }
            auto& child = indices.children[cIndex + offset + count];
            child.indexedName = IndexedName::fromConst(idx.getType(), cIndex);
            child.offset = offset;
            child.count = count;
            child.tag = tag;
            if (mapIndex > 0) {
                child.elementMap = childMaps[mapIndex - 1];
            }
            else {
                child.elementMap = nullptr;
            }
            child.postfix = tmp.c_str();
            this->childElements[child.postfix].childMap = &child;
            this->childElementSize += child.count;

            if (!(stream >> tmp)) {
                FC_THROWM(Base::RuntimeError, "Invalid element child string id");  // NOLINT
            }

            tokens.clear();
            boost::split(tokens, tmp, boost::is_any_of("."));
            if (tokens.size() > 1) {
                child.sids.reserve(static_cast<int>(tokens.size()) - 1);
                for (unsigned k = 1; k < tokens.size(); ++k) {
                    // The element child string ID is saved as decimal
                    // instead of hex by accident. To simplify maintenance
                    // of backward compatibility, it is not corrected, and
                    // just restored as decimal here.
                    long childID = strtol(tokens[k].c_str(), nullptr, decBase);
                    auto sid = hasherRef->getID(childID);
                    if (!sid) {
                        childSIDWarn = "Missing element child string id";
                    }
                    else {
                        child.sids.push_back(sid);
                    }
                }
            }
        }

        if (!(stream >> tmp >> outerCount) || tmp != "NameCount") {
            FC_THROWM(Base::RuntimeError, "missing element name outerCount");  // NOLINT
        }

        boost::io::ios_flags_saver ifs(stream);
        stream >> std::hex;

        indices.names.resize(outerCount);
        for (int j = 0; j < outerCount; ++j) {
            idx.setIndex(j);
            auto* ref = &indices.names[j];
            int innerCount = 0;
            while (true) {
                if (!(stream >> tmp)) {
                    FC_THROWM(Base::RuntimeError, "Failed to read element name");  // NOLINT
                }
                if (tmp == "0") {
                    break;
                }
                if (innerCount++ != 0) {
                    ref->next = std::make_unique<MappedNameRef>();
                    ref = ref->next.get();
                }
                tokens.clear();
                boost::split(tokens, tmp, boost::is_any_of("."));
                if (tokens.size() < 2) {
                    FC_THROWM(Base::RuntimeError, "Invalid element entry");  // NOLINT
                }

                int offset = 1;
                ::App::StringID::IndexID prefixID {};
                prefixID.id = 0;

                switch (tokens[0][0]) {
                    case ':': {
                        if (tokens.size() < 3) {
                            FC_THROWM(Base::RuntimeError, "Invalid element entry");  // NOLINT
                        }
                        ++offset;
                        long elementNameIndex = strtol(tokens[0].c_str() + 1, nullptr, hexBase);
                        if (elementNameIndex <= 0 || elementNameIndex > (int)postfixes.size()) {
                            FC_THROWM(Base::RuntimeError, "Invalid element name index");  // NOLINT
                        }
                        long elementIndex = strtol(tokens[1].c_str(), nullptr, hexBase);
                        ref->name = MappedName(
                            IndexedName::fromConst(postfixes[elementNameIndex - 1].c_str(),
                                                   static_cast<int>(elementIndex)));
                        break;
                    }
                    case '$':
                        ref->name = MappedName(tokens[0].c_str() + 1);
                        prefixID = ::App::StringID::fromString(ref->name.dataBytes());
                        break;
                    case ';':
                        ref->name = MappedName(tokens[0].c_str() + 1);
                        break;
                    default:
                        FC_THROWM(Base::RuntimeError, "Invalid element name marker");  // NOLINT
                }

                if (tokens[offset] != "0") {
                    long postfixIndex = strtol(tokens[offset].c_str(), nullptr, hexBase);
                    if (postfixIndex <= 0 || postfixIndex > (int)postfixes.size()) {
                        postfixWarn = "Invalid element postfix index";
                    }
                    else {
                        ref->name += postfixes[postfixIndex - 1];
                    }
                }

                this->mappedNames.emplace(ref->name, idx);

                if (!hasherRef) {
                    if (offset + 1 < (int)tokens.size()) {
                        hasherWarn = "No hasherRef";
                    }
                    continue;
                }

                ref->sids.reserve((tokens.size() - offset - 1 + prefixID.id) != 0U ? 1 : 0);
                if (prefixID.id != 0) {
                    auto sid = hasherRef->getID(prefixID.id);
                    if (!sid) {
                        hasherIDWarn = "Missing element name prefix id";
                    }
                    else {
                        ref->sids.push_back(sid);
                    }
                }
                for (int l = offset + 1; l < (int)tokens.size(); ++l) {
                    long readID = strtol(tokens[l].c_str(), nullptr, hexBase);
                    auto sid = hasherRef->getID(readID);
                    if (!sid) {
                        hasherIDWarn = "Invalid element name string id";
                    }
                    else {
                        ref->sids.push_back(sid);
                    }
                }
            }
        }
    }
    if (hasherWarn) {
        FC_WARN(hasherWarn);  // NOLINT
    }
    if (hasherIDWarn) {
        FC_WARN(hasherIDWarn);  // NOLINT
    }
    if (postfixWarn) {
        FC_WARN(postfixWarn);  // NOLINT
    }
    if (childSIDWarn) {
        FC_WARN(childSIDWarn);  // NOLINT
    }

    if (!(stream >> tmp) || tmp != "EndMap") {
        FC_THROWM(Base::RuntimeError, "unexpected end of child element map");  // NOLINT
    }

    return shared_from_this();
}

MappedName ElementMap::addName(MappedName& name,
                               const IndexedName& idx,
                               bool overwrite,
                               IndexedName* existing)
{
    auto it = mappedNames.find(name);

    if (it != mappedNames.end()) {
        *existing = it->second;
        return;
    }

    mappedNames[name] = idx;
    return name;
}

MappedName ElementMap::setElementName(const IndexedName& element,
                                      const MappedName& name,
                                      long masterTag,
                                      bool overwrite)
{
    if (!element) {
        throw Base::ValueError("Invalid input");
    }
    if (!name) {
        erase(element);
        return {};
    }

    for (const char* readChar = element.getType(); *readChar != 0; ++readChar) {
        char check = *readChar;
        if (check == '.' || (std::isspace((int)check) != 0)) {
            FC_THROWM(Base::RuntimeError,  // NOLINT
                      "Illegal character in element name: " << element);
        }
    }

    std::ostringstream ss;
    Data::MappedName mappedName(name);

    IndexedName existing;
    MappedName res = this->addName(mappedName, element, false, &existing);
    
    if (res) {
        return res;
    }

    mappedName = renameDuplicateElement(element, existing, name);
    if (mappedName) {
        res = this->addName(mappedName, element, false, &existing);

        if (res) {
            return res;
        }
    }

    return name;
}

MappedName ElementMap::renameDuplicateElement(const IndexedName& element,
                                              const IndexedName& element2,
                                              const MappedName& name) const
{
    MappedName renamed(name);
    int index = 0;

    for (const auto &nameCombo : mappedNames) {
        if (nameCombo.first.compareSections(name)) {
            ++index;
        }
    }

    renamed.setDuplicateCount(index);

    if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG)) {
        FC_WARN("duplicate element mapping '"  // NOLINT
                << name << " -> " << renamed << ' ' << element << '/' << element2);
    }

    return renamed;
}

void ElementMap::erase(const MappedName& name)
{
    auto it = this->mappedNames.find(name);
    if (it == this->mappedNames.end()) {
        return;
    }
    this->mappedNames.erase(it);
}

void ElementMap::erase(const IndexedName& idx)
{
    std::unordered_map<MappedName, IndexedName, MappedNameHasher> newMappedNames;

    for (const auto &name : mappedNames) {
        if (name.second != idx) {
            newMappedNames[name.first] = name.second;
        }
    }

    mappedNames = newMappedNames;
}

unsigned long ElementMap::size() const
{
    return mappedNames.size() + childElements.size();
}

bool ElementMap::empty() const
{
    return mappedNames.empty() && childElements.empty();
}

std::vector<MappedName> ElementMap::findAll(const IndexedName& idx) const
{
    // std::vector<std::pair<MappedName, ElementIDRefs>> res;
    // if (!idx) {
    //     return res;
    // }

    // auto iter = this->indexedNames.find(idx.getType());
    // if (iter == this->indexedNames.end()) {
    //     return res;
    // }

    // auto& indices = iter->second;
    // if (idx.getIndex() < (int)indices.names.size()) {
    //     const MappedNameRef& ref = indices.names[idx.getIndex()];
    //     int count = 0;
    //     for (auto nameRef = &ref; nameRef; nameRef = nameRef->next.get()) {
    //         if (nameRef->name) {
    //             ++count;
    //         }
    //     }
    //     if (count != 0) {
    //         res.reserve(count);
    //         for (auto nameRef = &ref; nameRef; nameRef = nameRef->next.get()) {
    //             if (nameRef->name) {
    //                 res.emplace_back(nameRef->name, nameRef->sids);
    //             }
    //         }
    //         return res;
    //     }
    // }

    // auto it = indices.children.upper_bound(idx.getIndex());
    // if (it != indices.children.end()
    //     && it->second.indexedName.getIndex() + it->second.offset <= idx.getIndex()) {
    //     auto& child = it->second;
    //     IndexedName childIdx(idx.getType(), idx.getIndex() - child.offset);
    //     if (child.elementMap) {
    //         res = child.elementMap->findAll(childIdx);
    //         for (auto& v : res) {
    //             v.first += child.postfix;
    //         }
    //     }
    //     else {
    //         res.emplace_back(MappedName(childIdx) + child.postfix, ElementIDRefs());
    //     }
    // }

    // return res;
}

// const MappedNameRef* ElementMap::findMappedRef(const IndexedName& idx) const
// {
//     auto iter = this->indexedNames.find(idx.getType());
//     if (iter == this->indexedNames.end()) {
//         return nullptr;
//     }
//     auto& indices = iter->second;
//     if (idx.getIndex() >= (int)indices.names.size()) {
//         return nullptr;
//     }
//     return &indices.names[idx.getIndex()];
// }

// MappedNameRef* ElementMap::findMappedRef(const IndexedName& idx)
// {
//     auto iter = this->indexedNames.find(idx.getType());
//     if (iter == this->indexedNames.end()) {
//         return nullptr;
//     }
//     auto& indices = iter->second;
//     if (idx.getIndex() >= (int)indices.names.size()) {
//         return nullptr;
//     }
//     return &indices.names[idx.getIndex()];
// }

// MappedNameRef& ElementMap::mappedRef(const IndexedName& idx)
// {
//     assert(idx);
//     auto& indices = this->indexedNames[idx.getType()];
//     if (idx.getIndex() >= (int)indices.names.size()) {
//         indices.names.resize(idx.getIndex() + 1);
//     }
//     return indices.names[idx.getIndex()];
// }

bool ElementMap::hasChildElementMap() const
{
    return !childElements.empty();
}

void ElementMap::collectChildMaps(std::map<const ElementMap*, int>& childMapSet,
                                  std::vector<const ElementMap*>& childMaps,
                                  std::map<QByteArray, int>& postfixMap,
                                  std::vector<QByteArray>& postfixes) const
{
    // auto res = childMapSet.insert(std::make_pair(this, 0));
    // if (!res.second) {
    //     return;
    // }

    // for (auto& indexedName : this->indexedNames) {
    //     addPostfix(QByteArray::fromRawData(indexedName.first,
    //                                        static_cast<int>(qstrlen(indexedName.first))),
    //                postfixMap,
    //                postfixes);

    //     for (auto& childPair : indexedName.second.children) {
    //         auto& child = childPair.second;
    //         if (child.elementMap) {
    //             child.elementMap->collectChildMaps(childMapSet, childMaps, postfixMap, postfixes);
    //         }
    //     }
    // }

    // for (auto& mappedName : this->mappedNames) {
    //     addPostfix(mappedName.first.constPostfix(), postfixMap, postfixes);
    // }

    // childMaps.push_back(this);
    // res.first->second = (int)childMaps.size();
}

void ElementMap::addChildElements(long masterTag, std::unordered_map<IndexedName, ElementMapPtr, IndexedNameHasher>& children)
{
}

void ElementMap::addChildElement(long masterTag, IndexedName indexedName, ElementMapPtr childMap)
{
    std::unordered_map<IndexedName, ElementMapPtr, IndexedNameHasher> addChildMap;

    addChildMap[indexedName] = childMap;

    addChildElements(masterTag, addChildMap);
}

std::unordered_map<IndexedName, ElementMapPtr, IndexedNameHasher> ElementMap::getChildElements() const
{
    return childElements;
}

std::vector<MappedElement> ElementMap::getAll() const
{
    std::vector<MappedElement> ret;
    ret.reserve(size());
    for (auto& mappedName : this->mappedNames) {
        ret.emplace_back(mappedName.first, mappedName.second);
    }
    for (auto& childElement : this->childElements) {
        IndexedName idx(childElement.first);
        MappedName name = (*childElement.second).find(idx);

        if (name.numberOfSections()) {
            ret.emplace_back(name, idx);
        }
    }
    return ret;
}

// long ElementMap::getElementHistory(const MappedName& name,
//                                    long masterTag,
//                                    MappedName* original,
//                                    std::vector<MappedName>* history) const
// {
//     long tag = 0;
//     int len = 0;
//     int pos = name.findTagInElementName(&tag, &len, nullptr, nullptr, true);
//     if (pos < 0) {
//         if (original) {
//             *original = name;
//         }
//         return tag;
//     }
//     if (!original && !history) {
//         return tag;
//     }

//     MappedName tmp;
//     MappedName& ret = original ? *original : tmp;
//     if (name.startsWith(ELEMENT_MAP_PREFIX)) {
//         unsigned offset = ELEMENT_MAP_PREFIX_SIZE;
//         ret = MappedName::fromRawData(name, static_cast<int>(offset));
//     }
//     else {
//         ret = name;
//     }

//     while (true) {
//         if ((len == 0) || len > pos) {
//             FC_WARN("invalid name length " << name);  // NOLINT
//             return 0;
//         }
//         bool deHashed = false;
//         if (ret.startsWith(MAPPED_CHILD_ELEMENTS_PREFIX, len)) {
//             int offset = (int)POSTFIX_TAG_SIZE;
//             MappedName tmp2 = MappedName::fromRawData(ret, len + offset, pos - len - offset);
//             MappedName postfix = dehashElementName(tmp2);
//             if (postfix != tmp2) {
//                 deHashed = true;
//                 ret = MappedName::fromRawData(ret, 0, len) + postfix;
//             }
//         }
//         if (!deHashed) {
//             ret = dehashElementName(MappedName::fromRawData(ret, 0, len));
//         }

//         long tag2 = 0;
//         pos = ret.findTagInElementName(&tag2, &len, nullptr, nullptr, true);
//         if (pos < 0 || (tag2 != tag && tag2 != -tag && tag != masterTag && -tag != masterTag)) {
//             return tag;
//         }
//         tag = tag2;
//         if (history) {
//             history->push_back(ret.copy());
//         }
//     }
// }

// void ElementMap::traceElement(const MappedName& name, long masterTag, TraceCallback cb) const
// {
//     long encodedTag = 0;
//     int len = 0;

//     auto pos = name.findTagInElementName(&encodedTag, &len, nullptr, nullptr, true);
//     if (cb(name, len, encodedTag, masterTag) || pos < 0) {
//         return;
//     }

//     if (name.startsWith(POSTFIX_EXTERNAL_TAG, len)) {
//         return;
//     }

//     std::set<long> tagSet;

//     std::vector<MappedName> names;
//     if (masterTag) {
//         tagSet.insert(std::abs(masterTag));
//     }
//     if (encodedTag) {
//         tagSet.insert(std::abs(encodedTag));
//     }
//     names.push_back(name);

//     masterTag = encodedTag;
//     MappedName tmp;
//     bool first = true;

//     // TODO: element tracing without object is inherently unsafe, because of
//     // possible external linking object which means the element may be encoded
//     // using external string table. Looking up the wrong table may accidentally
//     // cause circular mapping, and is actually quite easy to reproduce. See
//     //
//     // https://github.com/realthunder/FreeCAD_assembly3/issues/968
//     //
//     // An arbitrary depth limit is set here to not waste time. 'tagSet' above is
//     // also used for early detection of 'recursive' mapping.

//     for (int index = 0; index < 50; ++index) {
//         if (!len || len > pos) {
//             return;
//         }
//         if (first) {
//             first = false;
//             size_t offset = 0;
//             if (name.startsWith(ELEMENT_MAP_PREFIX)) {
//                 offset = ELEMENT_MAP_PREFIX_SIZE;
//             }
//             tmp = MappedName(name, offset, len);
//         }
//         else {
//             tmp = MappedName(tmp, 0, len);
//         }
//         tmp = dehashElementName(tmp);
//         names.push_back(tmp);
//         encodedTag = 0;
//         pos = tmp.findTagInElementName(&encodedTag, &len, nullptr, nullptr, true);
//         if (pos >= 0 && tmp.startsWith(POSTFIX_EXTERNAL_TAG, len)) {
//             break;
//         }

//         if (encodedTag && masterTag != std::abs(encodedTag)
//             && !tagSet.insert(std::abs(encodedTag)).second) {
//             if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG)) {
//                 FC_WARN("circular element mapping");
//                 if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_TRACE)) {
//                     auto doc = App::GetApplication().getActiveDocument();
//                     if (doc) {
//                         auto obj = doc->getObjectByID(masterTag);
//                         if (obj) {
//                             FC_LOG("\t" << obj->getFullName() << obj->getFullName() << "." << name);
//                         }
//                     }
//                     for (auto& errname : names) {
//                         FC_ERR("\t" << errname);
//                     }
//                 }
//             }
//             break;
//         }

//         if (cb(tmp, len, encodedTag, masterTag) || pos < 0) {
//             return;
//         }
//         masterTag = encodedTag;
//     }
// }


}  // Namespace Data
