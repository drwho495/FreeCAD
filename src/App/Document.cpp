/***************************************************************************
 *   Copyright (c) 2002 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


/*!
\defgroup Document Document
\ingroup APP
\brief The Base class of the FreeCAD Document

This (besides the App::Application class) is the most important class in FreeCAD.
It contains all the data of the opened, saved, or newly created FreeCAD Document.
The App::Document manages the Undo and Redo mechanism and the linking of documents.

\namespace App \class App::Document
This is besides the Application class the most important class in FreeCAD
It contains all the data of the opened, saved or newly created FreeCAD Document.
The Document manage the Undo and Redo mechanism and the linking of documents.

Note: the documents are not free objects. They are completely handled by the
App::Application. Only the Application can Open or destroy a document.

\section Exception Exception handling
As the document is the main data structure of FreeCAD we have to take a close
look at how Exceptions affect the integrity of the App::Document.

\section UndoRedo Undo Redo an Transactions
Undo Redo handling is one of the major mechanism of a document in terms of
user friendliness and speed (no one will wait for Undo too long).

\section Dependency Graph and dependency handling
The FreeCAD document handles the dependencies of its DocumentObjects with
an adjacence list. This gives the opportunity to calculate the shortest
recompute path. Also, it enables more complicated dependencies beyond trees.

@see App::Application
@see App::DocumentObject
*/

#include "PreCompiled.h"

#ifndef _PreComp_
# include <bitset>
# include <stack>
# include <boost/filesystem.hpp>
#endif

#include <boost/algorithm/string.hpp>
#include <boost/graph/strong_components.hpp>

#ifdef USE_OLD_DAG
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/visitors.hpp>
#endif //USE_OLD_DAG

#include <boost/regex.hpp>
#include <random>
#include <unordered_map>
#include <unordered_set>

#include <QMap>
#include <QFileInfo>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QCoreApplication>

#include <App/DocumentPy.h>
#include <Base/Console.h>
#include <Base/Exception.h>
#include <Base/ExceptionSafeCall.h>
#include <Base/FileInfo.h>
#include <Base/TimeInfo.h>
#include <Base/Reader.h>
#include <Base/Writer.h>
#include <Base/Tools.h>
#include <Base/Uuid.h>
#include <Base/Sequencer.h>
#include <Base/Stream.h>

#include "Document.h"
#include "private/DocumentP.h"
#include "Application.h"
#include "AutoTransaction.h"
#include "DocumentObserver.h"
#include "DocumentObject.h"
#include "DocumentParams.h"
#include "ExpressionParser.h"
#include "GeoFeature.h"
#include "License.h"
#include "Link.h"
#include "MergeDocuments.h"
#include "Origin.h"
#include "OriginGroupExtension.h"
#include "StringHasher.h"
#include "Transactions.h"

#ifdef _MSC_VER
#include <zipios++/zipios-config.h>
#endif
#include <zipios++/zipfile.h>
#include <zipios++/zipinputstream.h>
#include <zipios++/zipoutputstream.h>
#include <zipios++/meta-iostreams.h>

FC_LOG_LEVEL_INIT("App", true, 2, true)

using Base::Console;
using Base::streq;
using Base::Writer;
using namespace App;
using namespace std;
using namespace boost;
using namespace zipios;

#if FC_DEBUG
#  define FC_LOGFEATUREUPDATE
#endif

namespace fs = boost::filesystem;

namespace App {

static bool globalIsRestoring;

DocumentP::DocumentP()
{
#ifndef FC_DEBUG
    static std::random_device _RD;
    static std::mt19937 _RGEN(_RD());
    static std::uniform_int_distribution<> _RDIST(10,5000);
    // Set some random offset to reduce likelihood of ID collision when
    // copying shape from other document. It is probably better to randomize
    // on each object ID.
    lastObjectId = _RDIST(_RGEN);
#else
    lastObjectId = 10;
#endif
    Hasher.reset(new StringHasher);
    activeObject = nullptr;
    activeUndoTransaction = nullptr;
    iTransactionMode = 0;
    rollback = false;
    undoing = false;
    committing = false;
    opentransaction = false;
    StatusBits.set((size_t)Document::Closable, true);
    StatusBits.set((size_t)Document::KeepTrailingDigits, true);
    StatusBits.set((size_t)Document::Restoring, false);
    iUndoMode = 0;
    UndoMemSize = 0;
    UndoMaxStackSize = 20;
}


} // namespace App

PROPERTY_SOURCE(App::Document, App::PropertyContainer)

bool Document::testStatus(Status pos) const
{
    return d->StatusBits.test((size_t)pos);
}

void Document::setStatus(Status pos, bool on)
{
    d->StatusBits.set((size_t)pos, on);
}

//bool _has_cycle_dfs(const DependencyList & g, vertex_t u, default_color_type * color)
//{
//  color[u] = gray_color;
//  graph_traits < DependencyList >::adjacency_iterator vi, vi_end;
//  for (tie(vi, vi_end) = adjacent_vertices(u, g); vi != vi_end; ++vi)
//    if (color[*vi] == white_color)
//      if (has_cycle_dfs(g, *vi, color))
//        return true;            // cycle detected, return immediately
//      else if (color[*vi] == gray_color)        // *vi is an ancestor!
//        return true;
//  color[u] = black_color;
//  return false;
//}

bool Document::checkOnCycle()
{
#if 0
  std::vector < default_color_type > color(num_vertices(_DepList), white_color);
  graph_traits < DependencyList >::vertex_iterator vi, vi_end;
  for (tie(vi, vi_end) = vertices(_DepList); vi != vi_end; ++vi)
    if (color[*vi] == white_color)
      if (_has_cycle_dfs(_DepList, *vi, &color[0]))
        return true;
#endif
    return false;
}

bool Document::undo(int id)
{
    if (d->iUndoMode) {
        if(id) {
            auto it = mUndoMap.find(id);
            if(it == mUndoMap.end())
                return false;
            if(it->second != d->activeUndoTransaction) {
                TransactionGuard guard(TransactionGuard::Undo);
                while(!mUndoTransactions.empty() && mUndoTransactions.back()!=it->second)
                    undo(0);
            }
        }

        if (d->activeUndoTransaction)
            _commitTransaction(true);
        if (mUndoTransactions.empty())
            return false;

        TransactionGuard guard(TransactionGuard::Undo);

        // redo
        d->activeUndoTransaction = new Transaction(mUndoTransactions.back()->getID());
        d->activeUndoTransaction->Name = mUndoTransactions.back()->Name;

        Base::FlagToggler<bool> flag(d->undoing);
        // applying the undo
        mUndoTransactions.back()->apply(*this,false);

        // save the redo
        mRedoMap[d->activeUndoTransaction->getID()] = d->activeUndoTransaction;
        mRedoTransactions.push_back(d->activeUndoTransaction);
        d->activeUndoTransaction = nullptr;

        mUndoMap.erase(mUndoTransactions.back()->getID());
        delete mUndoTransactions.back();
        mUndoTransactions.pop_back();
        return true;
    }

    return false;
}

bool Document::redo(int id)
{
    if (d->iUndoMode) {
        if(id) {
            auto it = mRedoMap.find(id);
            if(it == mRedoMap.end())
                return false;
            {
                TransactionGuard guard(TransactionGuard::Redo);
                while(mRedoTransactions.size() && mRedoTransactions.back()!=it->second)
                    redo(0);
            }
        }

        if (d->activeUndoTransaction)
            _commitTransaction(true);

        assert(mRedoTransactions.size()!=0);

        TransactionGuard guard(TransactionGuard::Redo);

        // undo
        d->activeUndoTransaction = new Transaction(mRedoTransactions.back()->getID());
        d->activeUndoTransaction->Name = mRedoTransactions.back()->Name;

        // do the redo
        Base::FlagToggler<bool> flag(d->undoing);
        mRedoTransactions.back()->apply(*this,true);

        mUndoMap[d->activeUndoTransaction->getID()] = d->activeUndoTransaction;
        mUndoTransactions.push_back(d->activeUndoTransaction);
        d->activeUndoTransaction = nullptr;

        mRedoMap.erase(mRedoTransactions.back()->getID());
        delete mRedoTransactions.back();
        mRedoTransactions.pop_back();
        return true;
    }

    return false;
}

App::Property* Document::addDynamicProperty(
    const char* type, const char* name, const char* group, const char* doc,
    short attr, bool ro, bool hidden)
{
    auto prop = PropertyContainer::addDynamicProperty(type,name,group,doc,attr,ro,hidden);
    if(prop)
        _addOrRemoveProperty(nullptr, prop, true);
    return prop;
}

bool Document::removeDynamicProperty(const char* name)
{
    Property* prop = getDynamicPropertyByName(name);
    if(!prop || prop->testStatus(App::Property::LockDynamic))
        return false;

    _addOrRemoveProperty(nullptr, prop, false);
    return PropertyContainer::removeDynamicProperty(name);
}

void Document::addOrRemovePropertyOfObject(TransactionalObject* obj, Property *prop, bool add)
{
    if (!prop || !obj || !obj->isAttachedToDocument())
        return;
    _addOrRemoveProperty(obj, prop, add);
}

void Document::_addOrRemoveProperty(TransactionalObject* obj, Property *prop, bool add)
{
    if(d->iUndoMode && !isPerformingTransaction() && !d->activeUndoTransaction) {
        if(!testStatus(Restoring) || testStatus(Importing)) {
            int tid=0;
            const char *name = GetApplication().getActiveTransaction(&tid);
            if(name && tid>0)
                _openTransaction(name,tid);
        }
    }
    if (d->activeUndoTransaction && !d->rollback)
        d->activeUndoTransaction->addOrRemoveProperty(obj, prop, add);
}

bool Document::isPerformingTransaction() const
{
    return d->undoing || d->rollback || Transaction::isApplying();
}

std::vector<std::string> Document::getAvailableUndoNames() const
{
    std::vector<std::string> vList;
    if (d->activeUndoTransaction)
        vList.push_back(d->activeUndoTransaction->Name);
    for (std::list<Transaction*>::const_reverse_iterator It=mUndoTransactions.rbegin();It!=mUndoTransactions.rend();++It)
        vList.push_back((**It).Name);
    return vList;
}

std::vector<std::string> Document::getAvailableRedoNames() const
{
    std::vector<std::string> vList;
    for (std::list<Transaction*>::const_reverse_iterator It=mRedoTransactions.rbegin();It!=mRedoTransactions.rend();++It)
        vList.push_back((**It).Name);
    return vList;
}

void Document::openTransaction(const char* name) {
    if(isPerformingTransaction() || d->committing) {
        if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG))
            FC_WARN("Cannot open transaction while transacting");
        return;
    }

    GetApplication().setActiveTransaction(name?name:"<empty>");
}

int Document::_openTransaction(const char* name, int id)
{
    if(isPerformingTransaction() || d->committing) {
        if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG))
            FC_WARN("Cannot open transaction while transacting");
        return 0;
    }

    if (d->iUndoMode) {
        // Avoid recursive calls that is possible while
        // clearing the redo transactions and will cause
        // a double deletion of some transaction and thus
        // a segmentation fault
        if (d->opentransaction)
            return 0;
        Base::FlagToggler<> flag(d->opentransaction);

        if(id && mUndoMap.find(id)!=mUndoMap.end())
            throw Base::RuntimeError("invalid transaction id");
        if (d->activeUndoTransaction)
            _commitTransaction(true);
        _clearRedos();

        d->activeUndoTransaction = new Transaction(id);
        if (!name)
            name = "<empty>";
        d->activeUndoTransaction->Name = name;
        mUndoMap[d->activeUndoTransaction->getID()] = d->activeUndoTransaction;
        id = d->activeUndoTransaction->getID();

        signalOpenTransaction(*this, name);

        auto &app = GetApplication();
        auto activeDoc = app.getActiveDocument();
        if(activeDoc &&
           activeDoc!=this &&
           !activeDoc->hasPendingTransaction())
        {
            std::string aname("-> ");
            aname += d->activeUndoTransaction->Name;
            FC_LOG("auto transaction " << getName() << " -> " << activeDoc->getName());
            activeDoc->_openTransaction(aname.c_str(),id);
        }
        return id;
    }
    return 0;
}

void Document::renameTransaction(const char *name, int id) {
    if(name && d->activeUndoTransaction && d->activeUndoTransaction->getID()==id) {
        if(boost::starts_with(d->activeUndoTransaction->Name, "-> "))
            d->activeUndoTransaction->Name.resize(3);
        else
            d->activeUndoTransaction->Name.clear();
        d->activeUndoTransaction->Name += name;
    }
}

void Document::_checkTransaction(DocumentObject* pcDelObj, const Property *What, int line)
{
    // if the undo is active but no transaction open, open one!
    if (d->iUndoMode && !isPerformingTransaction()) {
        if (!d->activeUndoTransaction) {
            if(!testStatus(Restoring) || testStatus(Importing)) {
                int tid=0;
                const char *name = GetApplication().getActiveTransaction(&tid);
                if(name && tid>0) {
                    bool ignore = false;
                    if(What) {
                        if(What->testStatus(Property::NoModify))
                            ignore = true;
                        else if(!Base::freecad_dynamic_cast<Document>(What->getContainer())
                                && !DocumentParams::getViewObjectTransaction()
                                && !AutoTransaction::recordViewObjectChange()
                                && !Base::freecad_dynamic_cast<DocumentObject>(What->getContainer()))
                            ignore = true;
                    }
                    if(FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG)) {
                        if(What)
                            FC_LOG((ignore?"ignore":"auto") << " transaction ("
                                    << line << ") '" << What->getFullName());
                        else
                            FC_LOG((ignore?"ignore":"auto") <<" transaction ("
                                    << line << ") '" << name << "' in " << getName());
                    }
                    if(!ignore)
                        _openTransaction(name,tid);
                    return;
                }
            }
            if(!pcDelObj)
                return;
            // When the object is going to be deleted we have to check if it has already been added to
            // the undo transactions
            std::list<Transaction*>::iterator it;
            for (it = mUndoTransactions.begin(); it != mUndoTransactions.end(); ++it) {
                if ((*it)->hasObject(pcDelObj)) {
                    _openTransaction("Delete");
                    break;
                }
            }
        }
    }
}

void Document::_clearRedos()
{
    if(isPerformingTransaction() || d->committing) {
        FC_ERR("Cannot clear redo while transacting");
        return;
    }

    mRedoMap.clear();
    while (!mRedoTransactions.empty()) {
        delete mRedoTransactions.back();
        mRedoTransactions.pop_back();
    }
}

void Document::commitTransaction() {
    if(isPerformingTransaction() || d->committing) {
        if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG))
            FC_WARN("Cannot commit transaction while transacting");
        return;
    }

    if (d->activeUndoTransaction)
        GetApplication().closeActiveTransaction(false,d->activeUndoTransaction->getID());
}

void Document::_commitTransaction(bool notify)
{
    if (d->activeUndoTransaction) {
        if(d->undoing || d->rollback || d->committing) {
            if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG))
                FC_WARN("Cannot commit transaction while transacting");
            return;
        }
        Base::FlagToggler<> flag(d->committing);
        Application::TransactionSignaller signaller(false,true);
        int id = d->activeUndoTransaction->getID();
        mUndoTransactions.push_back(d->activeUndoTransaction);
        d->activeUndoTransaction = nullptr;
        // check the stack for the limits
        if(mUndoTransactions.size() > d->UndoMaxStackSize){
            mUndoMap.erase(mUndoTransactions.front()->getID());
            delete mUndoTransactions.front();
            mUndoTransactions.pop_front();
        }
        signalCommitTransaction(*this);

        if (notify)
            GetApplication().closeActiveTransaction(false,id);
    }
}

void Document::abortTransaction() {
    if(isPerformingTransaction() || d->committing) {
        if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG))
            FC_WARN("Cannot abort transaction while transacting");
        return;
    }
    if (d->activeUndoTransaction)
        GetApplication().closeActiveTransaction(true,d->activeUndoTransaction->getID());
}

void Document::_abortTransaction()
{
    if (d->activeUndoTransaction) {
        if(d->undoing || d->rollback || d->committing) {
            if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG))
                FC_WARN("Cannot abort transaction while transacting");
            return;
        }
        Base::FlagToggler<bool> flag(d->rollback);
        Application::TransactionSignaller signaller(true,true);
        TransactionGuard guard(TransactionGuard::Abort);

        // applying the so far made changes
        d->activeUndoTransaction->apply(*this,false);

        // destroy the undo
        mUndoMap.erase(d->activeUndoTransaction->getID());
        delete d->activeUndoTransaction;
        d->activeUndoTransaction = nullptr;
        signalAbortTransaction(*this);
    }
}

bool Document::hasPendingTransaction() const
{
    if (d->activeUndoTransaction)
        return true;
    else
        return false;
}

int Document::getTransactionID(bool undo, unsigned pos) const {
    if(undo) {
        if(d->activeUndoTransaction) {
            if(pos == 0)
                return d->activeUndoTransaction->getID();
            --pos;
        }
        if(pos>=mUndoTransactions.size())
            return 0;
        auto rit = mUndoTransactions.rbegin();
        for(;pos;++rit,--pos)
            continue;
        return (*rit)->getID();
    }
    if(pos>=mRedoTransactions.size())
        return 0;
    auto rit = mRedoTransactions.rbegin();
    for(;pos;++rit,--pos);
    return (*rit)->getID();
}

bool Document::isTransactionEmpty() const
{
    if (d->activeUndoTransaction) {
        // Transactions are now only created when there are actual changes.
        // Empty transaction is now significant for marking external changes. It
        // is used to match ID with transactions in external documents and
        // trigger undo/redo there.

        // return d->activeUndoTransaction->isEmpty();

        return false;
    }

    return true;
}

void Document::clearDocument()
{
    d->activeObject = nullptr;

    if (!d->objectArray.empty()) {
        GetApplication().signalDeleteDocument(*this);
        d->clearDocument();
        GetApplication().signalNewDocument(*this,false);
    }

    Base::FlagToggler<> flag(globalIsRestoring, false);

    setStatus(Document::PartialDoc,false);

    d->clearRecomputeLog();
    d->objectArray.clear();
    d->objectMap.clear();
    d->objectIdMap.clear();
    d->lastObjectId = 0;
}


void Document::clearUndos()
{
    if(isPerformingTransaction() || d->committing) {
        FC_ERR("Cannot clear undos while transacting");
        return;
    }

    if (d->activeUndoTransaction)
        _commitTransaction(true);

    mUndoMap.clear();

    // When cleaning up the undo stack we must delete the transactions from front
    // to back because a document object can appear in several transactions but
    // once removed from the document the object can never ever appear in any later
    // transaction. Since the document object may be also deleted when the transaction
    // is deleted we must make sure not access an object once it's destroyed. Thus, we
    // go from front to back and not the other way round.
    while (!mUndoTransactions.empty()) {
        delete mUndoTransactions.front();
        mUndoTransactions.pop_front();
    }
    //while (!mUndoTransactions.empty()) {
    //    delete mUndoTransactions.back();
    //    mUndoTransactions.pop_back();
    //}

    _clearRedos();
}

int Document::getAvailableUndos(int id) const
{
    if(id) {
        auto it = mUndoMap.find(id);
        if(it == mUndoMap.end())
            return 0;
        int i = 0;
        if(d->activeUndoTransaction) {
            ++i;
            if(d->activeUndoTransaction->getID()==id)
                return i;
        }
        auto rit = mUndoTransactions.rbegin();
        for(;rit!=mUndoTransactions.rend()&&*rit!=it->second;++rit)
            ++i;
        assert(rit!=mUndoTransactions.rend());
        return i+1;
    }
    if (d->activeUndoTransaction)
        return static_cast<int>(mUndoTransactions.size() + 1);
    else
        return static_cast<int>(mUndoTransactions.size());
}

int Document::getAvailableRedos(int id) const
{
    if(id) {
        auto it = mRedoMap.find(id);
        if(it == mRedoMap.end())
            return 0;
        int i = 0;
        for(auto rit=mRedoTransactions.rbegin();*rit!=it->second;++rit)
            ++i;
        assert(i<(int)mRedoTransactions.size());
        return i+1;
    }
    return static_cast<int>(mRedoTransactions.size());
}

void Document::setUndoMode(int iMode)
{
    if (d->iUndoMode && !iMode)
        clearUndos();

    d->iUndoMode = iMode;
}

int Document::getUndoMode() const
{
    return d->iUndoMode;
}

unsigned int Document::getUndoMemSize () const
{
    return d->UndoMemSize;
}

void Document::setUndoLimit(unsigned int UndoMemSize)
{
    d->UndoMemSize = UndoMemSize;
}

void Document::setMaxUndoStackSize(unsigned int UndoMaxStackSize)
{
     d->UndoMaxStackSize = UndoMaxStackSize;
}

unsigned int Document::getMaxUndoStackSize()const
{
    return d->UndoMaxStackSize;
}

void Document::onBeforeChange(const Property* prop)
{
    if(!d->rollback) {
        _checkTransaction(0, prop, __LINE__);
        if (d->activeUndoTransaction)
            d->activeUndoTransaction->addObjectChange(nullptr, prop);
    }
    if(prop == &FileName)
        ExpressionBlocker::check();
    if(prop == &Label)
        oldLabel = Label.getValue();
    signalBeforeChange(*this, *prop);
}

void Document::onChanged(const Property* prop)
{
    signalChanged(*this, *prop);

    // the Name property is a label for display purposes
    if (prop == &Label) {
        App::GetApplication().signalRelabelDocument(*this);
    } else if(prop == &ShowHidden) {
        App::GetApplication().signalShowHidden(*this);
    } else if (prop == &Uid) {
        std::string new_dir = getTransientDirectoryName(this->Uid.getValueStr(),this->FileName.getStrValue());
        std::string old_dir = this->TransientDir.getStrValue();
        Base::FileInfo TransDirNew(new_dir);
        Base::FileInfo TransDirOld(old_dir);
        // this directory should not exist
        if (!TransDirNew.exists()) {
            if (TransDirOld.exists()) {
                if (!TransDirOld.renameFile(new_dir.c_str()))
                    Base::Console().Warning("Failed to rename '%s' to '%s'\n", old_dir.c_str(), new_dir.c_str());
                else
                    this->TransientDir.setValue(new_dir);
            }
            else {
                if (!TransDirNew.createDirectories())
                    Base::Console().Warning("Failed to create '%s'\n", new_dir.c_str());
                else
                    this->TransientDir.setValue(new_dir);
            }
        }
        // when reloading an existing document the transient directory doesn't change
        // so we must avoid to generate a new uuid
        else if (TransDirNew.filePath() != TransDirOld.filePath()) {
            // make sure that the uuid is unique
            std::string uuid = this->Uid.getValueStr();
            Base::Uuid id;
            Base::Console().Warning("Document with the UUID '%s' already exists, change to '%s'\n",
                                    uuid.c_str(), id.getValue().c_str());
            // recursive call of onChanged()
            this->Uid.setValue(id);
        }
    } else if(prop == &UseHasher) {
        for(auto obj : d->objectArray) {
            auto geofeature = dynamic_cast<GeoFeature*>(obj);
            if(geofeature && geofeature->getPropertyOfGeometry())
                geofeature->enforceRecompute();
        }
    }
}

void Document::onBeforeChangeProperty(const TransactionalObject *Who, const Property *What)
{
    if(Who->isDerivedFrom(App::DocumentObject::getClassTypeId()))
        signalBeforeChangeObject(*static_cast<const App::DocumentObject*>(Who), *What);
    if(!d->rollback) {
        _checkTransaction(nullptr, What, __LINE__);
        if (d->activeUndoTransaction)
            d->activeUndoTransaction->addObjectChange(Who, What);
    }
}

void Document::onChangedProperty(const DocumentObject *Who, const Property *What)
{
    if (What == &Who->TreeRank) {
        if (d->treeRankRevision == d->revision) {
            long r = Who->TreeRank.getValue();
            if (r < d->treeRanks.first)
                d->treeRanks.first = r;
            else if (r > d->treeRanks.second)
                d->treeRanks.second = r;
        }
    }
    signalChangedObject(*Who, *What);
}

void Document::setTransactionMode(int iMode)
{
    d->iTransactionMode = iMode;
}

//--------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------
Document::Document(const char* documentName)
    : myName(documentName)
{
    // Remark: In a constructor we should never increment a Python object as we cannot be sure
    // if the Python interpreter gets a reference of it. E.g. if we increment but Python don't
    // get a reference then the object wouldn't get deleted in the destructor.
    // So, we must increment only if the interpreter gets a reference.
    // Remark: We force the document Python object to own the DocumentPy instance, thus we don't
    // have to care about ref counting any more.
    d = new DocumentP;
    d->DocumentPythonObject = Py::Object(new DocumentPy(this), true);

#ifdef FC_LOGUPDATECHAIN
    Console().Log("+App::Document: %p\n", this);
#endif

    std::string CreationDateString = Base::TimeInfo::currentDateTimeString();
    std::string Author = DocumentParams::getprefAuthor();
    std::string AuthorComp = DocumentParams::getprefCompany();
    ADD_PROPERTY_TYPE(Label, ("Unnamed"), 0, Prop_None, "The name of the document");
    ADD_PROPERTY_TYPE(FileName,
                      (""),
                      0,
                      PropertyType(Prop_Transient | Prop_ReadOnly),
                      "The path to the file where the document is saved to");
    ADD_PROPERTY_TYPE(CreatedBy, (Author.c_str()), 0, Prop_None, "The creator of the document");
    ADD_PROPERTY_TYPE(
        CreationDate, (CreationDateString.c_str()), 0, Prop_ReadOnly, "Date of creation");
    ADD_PROPERTY_TYPE(LastModifiedBy, (""), 0, Prop_None, 0);
    ADD_PROPERTY_TYPE(LastModifiedDate, ("Unknown"), 0, Prop_ReadOnly, "Date of last modification");
    ADD_PROPERTY_TYPE(Company,
                      (AuthorComp.c_str()),
                      0,
                      Prop_None,
                      "Additional tag to save the name of the company");
    ADD_PROPERTY_TYPE(Comment, (""), 0, Prop_None, "Additional tag to save a comment");
    ADD_PROPERTY_TYPE(Meta, (), 0, Prop_None, "Map with additional meta information");
    ADD_PROPERTY_TYPE(Material, (), 0, Prop_None, "Map with material properties");
    // create the uuid for the document
    Base::Uuid id;
    ADD_PROPERTY_TYPE(Id, (""), 0, Prop_None, "ID of the document");
    ADD_PROPERTY_TYPE(Uid, (id), 0, Prop_ReadOnly, "UUID of the document");

    ADD_PROPERTY_TYPE(SaveThumbnail, (DocumentParams::getSaveThumbnail()), 0, Prop_None,
            "Whether to auto update thumbnail on saving the document");
    ADD_PROPERTY_TYPE(ThumbnailFile, (""), 0, Prop_None,
            "User defined thumnail file. The thumnail will be saved into the\n"
            "document file. It will only be updated oncei when the user changes\n"
            "this property. An non-empty value of this property will also disable\n"
            "thumbnail auto update regardless of setting in SaveThumbnail.");
    ThumbnailFile.setFilter("Image files (*.jpg *.jpeg *.png *.bmp *.gif);;All files (*)");

    // license stuff
    auto index = static_cast<int>(DocumentParams::getprefLicenseType());
    const char* name = App::licenseItems.at(index).at(App::posnOfFullName);
    const char* url = App::licenseItems.at(index).at(App::posnOfUrl);
    std::string licenseUrl;
    if(DocumentParams::getprefLicenseUrl().empty()) {
        licenseUrl = DocumentParams::getprefLicenseUrl();
    } else if (url) {
        licenseUrl = url;
    }

    ADD_PROPERTY_TYPE(License, (name), 0, Prop_None, "License string of the Item");
    ADD_PROPERTY_TYPE(
        LicenseURL, (licenseUrl.c_str()), 0, Prop_None, "URL to the license text/contract");
    ADD_PROPERTY_TYPE(ShowHidden,
                      (false),
                      0,
                      PropertyType(Prop_None),
                      "Whether to show hidden object items in the tree view");
    ADD_PROPERTY_TYPE(UseHasher,(true), 0,PropertyType(Prop_Hidden), 
                        "Whether to use hasher on topological naming");
    if(!DocumentParams::getUseHasher())
        UseHasher.setValue(false);

    // this creates and sets 'TransientDir' in onChanged()
    ADD_PROPERTY_TYPE(TransientDir,
                      (""),
                      0,
                      PropertyType(Prop_Transient | Prop_ReadOnly),
                      "Transient directory, where the files live while the document is open");
    ADD_PROPERTY_TYPE(
        Tip, (nullptr), 0, PropertyType(Prop_Transient), "Link of the tip object of the document");
    ADD_PROPERTY_TYPE(TipName,
                      (""),
                      0,
                      PropertyType(Prop_Hidden | Prop_ReadOnly),
                      "Link of the tip object of the document");
    Uid.touch();

    ADD_PROPERTY_TYPE(ForceXML,(3),"Format", Prop_None,
            "Preference of storing data as XML.\n"
            "Higher number means stronger preference.\n"
            "Only effective when saving document in directory.");
    ForceXML.setValue(DocumentParams::getForceXML());
    ADD_PROPERTY_TYPE(SplitXML,(true),"Format",Prop_None,
            "Save object data in separate XML file.\n"
            "Only effective when saving document in directory.");
    SplitXML.setValue(DocumentParams::getSplitXML());
    ADD_PROPERTY_TYPE(PreferBinary,(false),"Format",Prop_None,
            "Prefer binary format when saving object data.\n"
            "This can result in smaller file but bad for version control.");
    PreferBinary.setValue(DocumentParams::getPreferBinary());
}

Document::~Document()
{
#ifdef FC_LOGUPDATECHAIN
    Console().Log("-App::Document: %s %p\n",getName(), this);
#endif

    try {
        clearUndos();
    }
    catch (const boost::exception&) {
    }

#ifdef FC_LOGUPDATECHAIN
    Console().Log("-Delete Features of %s \n",getName());
#endif

    d->clearDocument();

    // Remark: The API of Py::Object has been changed to set whether the wrapper owns the passed
    // Python object or not. In the constructor we forced the wrapper to own the object so we need
    // not to dec'ref the Python object any more.
    // But we must still invalidate the Python object because it doesn't need to be
    // destructed right now because the interpreter can own several references to it.
    Base::PyGILStateLocker lock;
    Base::PyObjectBase* doc = static_cast<Base::PyObjectBase*>(d->DocumentPythonObject.ptr());
    // Call before decrementing the reference counter, otherwise a heap error can occur
    doc->setInvalid();

    // remove Transient directory
    try {
        Base::FileInfo TransDir(TransientDir.getValue());
        TransDir.deleteDirectoryRecursive();
    }
    catch (const Base::Exception& e) {
        std::cerr << "Removing transient directory failed: " << e.what() << std::endl;
    }
    delete d;
}

std::string Document::getTransientDirectoryName(const std::string& uuid, const std::string& filename) const
{
    // Create a directory name of the form: {ExeName}_Doc_{UUID}_{HASH}_{PID}
    std::stringstream s;
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(filename.c_str(), filename.size());
    s << App::Application::getUserCachePath() << App::Application::getExecutableName()
      << "_Doc_" << uuid
      << "_" << hash.result().toHex().left(6).constData()
      << "_" << QCoreApplication::applicationPid();
    return s.str();
}

//--------------------------------------------------------------------------
// Exported functions
//--------------------------------------------------------------------------

#define FC_DOC_SCHEMA_VER 4

void Document::Save (Base::Writer &writer) const
{
    d->hashers.clear();
    addStringHasher(d->Hasher);

    writer.Stream() << "<Document SchemaVersion=\"" << FC_DOC_SCHEMA_VER 
                    << "\" ProgramVersion=\""
                    << App::Application::Config()["BuildVersionMajor"] << "."
                    << App::Application::Config()["BuildVersionMinor"] << "R"
                    << App::Application::Config()["BuildRevision"]
                    << "\" FileVersion=\"" << writer.getFileVersion() 
                    << "\" Uid=\"" << Uid.getValueStr()
                    << "\" StringHasher=\"1\">\n";
    
    writer.incInd();

    // NOTE: DO NOT save the main string hasher as separate file, because it is
    // required by many objects, which assume the string hasher is fully
    // restored.
    d->Hasher->setPersistenceFileName(0);

    for (auto o : d->objectArray)
        o->beforeSave();
    beforeSave();

    d->Hasher->Save(writer);

    writer.decInd();

    PropertyContainer::Save(writer);

    // writing the features types
    writeObjects(d->objectArray, writer);
}

void Document::Restore(Base::XMLReader &reader)
{
    int i,Cnt;
    d->hashers.clear();
    d->touchedObjs.clear();
    addStringHasher(d->Hasher);

    Base::ReaderContext rctx(getName());

    setStatus(Document::PartialDoc,false);

    reader.readElement("Document");
    long scheme = reader.getAttributeAsInteger("SchemaVersion");
    reader.DocumentSchema = scheme;
    if (reader.hasAttribute("ProgramVersion")) {
        reader.ProgramVersion = reader.getAttribute("ProgramVersion");
    } else {
        reader.ProgramVersion = "pre-0.14";
    }
    if (reader.hasAttribute("FileVersion")) {
        reader.FileVersion = reader.getAttributeAsUnsigned("FileVersion");
    } else {
        reader.FileVersion = 0;
    }

    if (reader.hasAttribute("Uid"))
        Uid.setValue(reader.getAttribute("Uid"));

    if (reader.hasAttribute("StringHasher")) {
        Base::ReaderContext rctx("StringHasher");
        d->Hasher->Restore(reader);
    } else
        d->Hasher->clear();

    // When this document was created the FileName and Label properties
    // were set to the absolute path or file name, respectively. To save
    // the document to the file it was loaded from or to show the file name
    // in the tree view we must restore them after loading the file because
    // they will be overridden.
    // Note: This does not affect the internal name of the document in any way
    // that is kept in Application.
    std::string FilePath = FileName.getValue();
    std::string DocLabel = Label.getValue();

    // read the Document Properties, when reading in Uid the transient directory gets renamed automatically
    PropertyContainer::Restore(reader);

    // We must restore the correct 'FileName' property again because the stored
    // value could be invalid.
    FileName.setValue(FilePath.c_str());
    Label.setValue(DocLabel.c_str());

    // SchemeVersion "2"
    if ( scheme == 2 ) {
        // read the feature types
        reader.readElement("Features");
        Cnt = reader.getAttributeAsInteger("Count");
        for (i=0 ;i<Cnt ;i++) {
            reader.readElement("Feature");
            string type = reader.getAttribute("type");
            string name = reader.getAttribute("name");
            try {
                addObject(type.c_str(), name.c_str(), /*isNew=*/ false);
            }
            catch ( Base::Exception& ) {
                Base::Console().Message("Cannot create object '%s'\n", name.c_str());
            }
        }
        reader.readEndElement("Features");

        // read the features itself
        reader.readElement("FeatureData");
        Cnt = reader.getAttributeAsInteger("Count");
        for (i=0 ;i<Cnt ;i++) {
            reader.readElement("Feature");
            string name = reader.getAttribute("name");
            Base::ReaderContext rctx(name);
            DocumentObject* pObj = getObject(name.c_str());
            if (pObj) { // check if this feature has been registered
                pObj->setStatus(ObjectStatus::Restore, true);
                pObj->Restore(reader);
                pObj->setStatus(ObjectStatus::Restore, false);
            }
            reader.readEndElement("Feature");
        }
        reader.readEndElement("FeatureData");
    } // SchemeVersion "3" or higher
    else if ( scheme >= 3 ) {
        // read the feature types
        readObjects(reader);

        // tip object handling. First the whole document has to be read, then we
        // can restore the Tip link out of the TipName Property:
        Tip.setValue(getObject(TipName.getValue()));
    }

    reader.readEndElement("Document");
}

std::pair<bool,int> Document::addStringHasher(const StringHasherRef & hasher) const {
    if (!hasher)
        return std::make_pair(false, 0);
    auto ret = d->hashers.left.insert(HasherMap::left_map::value_type(hasher,(int)d->hashers.size()));
    if (ret.second)
        hasher->clearMarks();
    return std::make_pair(ret.second,ret.first->second);
}

StringHasherRef Document::getHasher() const {
    return d->Hasher;
}

StringHasherRef Document::getStringHasher(int idx) const {
    StringHasherRef hasher;
    if(idx<0) {
        if(UseHasher.getValue())
            return d->Hasher;
        return hasher;
    }

    auto it = d->hashers.right.find(idx);
    if(it == d->hashers.right.end()) {
        hasher = new StringHasher;
        d->hashers.right.insert(HasherMap::right_map::value_type(idx,hasher));
    }else
        hasher = it->second;
    return hasher;
}

struct DocExportStatus {
    Document::ExportStatus status;
    std::set<const App::DocumentObject*> objs;
};

static DocExportStatus _ExportStatus;

// Exception-safe exporting status setter
class DocumentExporting {
public:
    explicit DocumentExporting(const std::vector<App::DocumentObject*> &objs) {
        _ExportStatus.status = Document::Exporting;
        _ExportStatus.objs.insert(objs.begin(),objs.end());
    }

    ~DocumentExporting() {
        _ExportStatus.status = Document::NotExporting;
        _ExportStatus.objs.clear();
    }
};

// The current implementation choose to use a static variable for exporting
// status because we can be exporting multiple objects from multiple documents
// at the same time. I see no benefits in distinguish which documents are
// exporting, so just use a static variable for global status. But the
// implementation can easily be changed here if necessary.
Document::ExportStatus Document::isExporting(const App::DocumentObject *obj) const {
    if(_ExportStatus.status!=Document::NotExporting &&
       (!obj || _ExportStatus.objs.find(obj)!=_ExportStatus.objs.end()))
        return _ExportStatus.status;
    return Document::NotExporting;
}

void Document::exportObjects(const std::vector<App::DocumentObject*>& obj, std::ostream& out) {

    DocumentExporting exporting(obj);
    d->hashers.clear();

    if(FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG)) {
        for(auto o : obj) {
            if(o && o->getNameInDocument()) {
                FC_LOG("exporting " << o->getFullName());
                if (!o->getPropertyByName("_ObjectUUID")) {
                    auto prop = static_cast<PropertyUUID*>(o->addDynamicProperty(
                            "App::PropertyUUID", "_ObjectUUID", nullptr, nullptr,
                            Prop_Output | Prop_Hidden));
                    prop->setValue(Base::Uuid::createUuid());
                }
            }
        }
    }

    Base::ZipWriter writer(out);
    writer.putNextEntry("Document.xml");
    writer.Stream() << "<?xml version='1.0' encoding='utf-8'?>\n";
    writer.Stream() << "<Document SchemaVersion=\"" << FC_DOC_SCHEMA_VER 
                        << "\" ProgramVersion=\""
                        << App::Application::Config()["BuildVersionMajor"] << "."
                        << App::Application::Config()["BuildVersionMinor"] << "R"
                        << App::Application::Config()["BuildRevision"]
                        << "\" FileVersion=\"1\">\n";
    // Add this block to have the same layout as for normal documents
    writer.Stream() << "<Properties Count=\"0\">\n";
    writer.Stream() << "</Properties>\n";

    // writing the object types
    writeObjects(obj, writer);

    // Hook for others to add further data.
    signalExportObjects(obj, writer);

    // write additional files
    writer.writeFiles();
    d->hashers.clear();
}

#define FC_ATTR_DEPENDENCIES "Dependencies"
#define FC_ELEMENT_OBJECT_DEPS "ObjectDeps"
#define FC_ATTR_DEP_COUNT "Count"
#define FC_ATTR_DEP_OBJ_NAME "Name"
#define FC_ATTR_DEP_ALLOW_PARTIAL "AllowPartial"
#define FC_ELEMENT_OBJECT_DEP "Dep"

void Document::writeObjects(const std::vector<App::DocumentObject*>& obj,
                            Base::Writer &writer) const
{
    // writing the features types
    writer.incInd(); // indentation for 'Objects count'
    writer.Stream() << writer.ind() << "<Objects Count=\"" << obj.size();
    if(!isExporting(nullptr))
        writer.Stream() << "\" " FC_ATTR_DEPENDENCIES "=\"1";
    writer.Stream() << "\">\n";

    writer.incInd(); // indentation for 'Object type'

    if(!isExporting(nullptr)) {
        for(auto o : obj) {
            const auto &outList = o->getOutList(DocumentObject::OutListNoHidden
                                                | DocumentObject::OutListNoXLinked);
            std::set<App::DocumentObject*> outSet(outList.begin(),outList.end());
            writer.Stream() << writer.ind() 
                << "<" FC_ELEMENT_OBJECT_DEPS " " FC_ATTR_DEP_OBJ_NAME "=\""
                << o->getNameInDocument() << "\" " FC_ATTR_DEP_COUNT "=\"" << outSet.size();
            if(outSet.empty()) {
                writer.Stream() << "\"/>\n";
                continue;
            }
            int partial = o->canLoadPartial();
            if(partial>0)
                writer.Stream() << "\" " FC_ATTR_DEP_ALLOW_PARTIAL << "=\"" << partial;
            writer.Stream() << "\">\n";
            writer.incInd();
            for(auto dep : outSet) {
                auto name = dep?dep->getNameInDocument():"";
                writer.Stream() << writer.ind() << "<" FC_ELEMENT_OBJECT_DEP " "
                    FC_ATTR_DEP_OBJ_NAME "=\"" << (name?name:"") << "\"/>\n";
            }
            writer.decInd();
            writer.Stream() << writer.ind() << "</" FC_ELEMENT_OBJECT_DEPS ">\n";
        }
    }

    std::vector<DocumentObject*>::const_iterator it;
    for (it = obj.begin(); it != obj.end(); ++it) {
        writer.Stream() << writer.ind() << "<Object "
        << "type=\"" << (*it)->getTypeId().getName()     << "\" "
        << "name=\"" << (*it)->getExportName()       << "\" "
        << "id=\"" << (*it)->getID()       << "\" "
        << "revision=\"" << (*it)->getRevision() << "\" ";

        // Only write out custom view provider types
        std::string viewType = (*it)->getViewProviderNameStored();
        if (viewType != (*it)->getViewProviderName())
            writer.Stream() << "ViewType=\"" << viewType << "\" ";

        // See DocumentObjectPy::getState
        if ((*it)->testStatus(ObjectStatus::Touch))
            writer.Stream() << "Touched=\"1\" ";
        if ((*it)->testStatus(ObjectStatus::Error)) {
            writer.Stream() << "Invalid=\"1\" ";
            auto desc = getErrorDescription(*it);
            if(desc)
                writer.Stream() << "Error=\"" << Property::encodeAttribute(desc) << "\" ";
        }

        if(writer.isSplitXML()) {
            std::string name((*it)->getNameInDocument());
            if(name == "Document" || name == "GuiDocument")
                name += "-Obj";
            writer.Stream() << "file=\"" 
                << writer.addFile(name+".xml",this) << "\" ";
        }

        writer.Stream() << "/>\n";
    }

    writer.decInd();  // indentation for 'Object type'
    writer.Stream() << writer.ind() << "</Objects>\n";

    // writing the features itself
    writer.Stream() << writer.ind() << "<ObjectData Count=\"";
    if(writer.isSplitXML())
        writer.Stream() << "0\">\n";
    else {
        writer.Stream() << obj.size() <<"\">\n";

        writer.incInd(); // indentation for 'Object name'
        for (it = obj.begin(); it != obj.end(); ++it) 
            writeObject(writer,*it);
        writer.decInd(); // indentation for 'Object name'
    }
    writer.Stream() << writer.ind() << "</ObjectData>\n";
    writer.decInd();  // indentation for 'Objects count'
    writer.Stream() << "</Document>\n";
}

void Document::writeObject(Base::Writer &writer, DocumentObject *obj) const 
{
    writer.Stream() << writer.ind() << "<Object name=\"" << obj->getExportName() << "\"";
    if(obj->canSaveExtension())
        writer.Stream() << " Extensions=\"True\"";

    writer.Stream() << ">\n";
    obj->Save(writer);
    writer.Stream() << writer.ind() << "</Object>\n";
}

void Document::SaveDocFile(Base::Writer &writer) const {
    Base::FileInfo fi(writer.getCurrentFileName());
    auto obj = getObject(fi.fileNamePure().c_str());
    if(!obj) 
        FC_ERR("Cannot find object " << fi.fileNamePure());
    else {
        writer.Stream() << "<?xml version='1.0' encoding='utf-8'?>\n"
                        << "<!-- FreeCAD DocumentObject -->\n"
                        << "<Document SchemaVersion=\"" << FC_DOC_SCHEMA_VER
                        << "\" FileVersion=\"" << writer.getFileVersion()
                        << "\">\n";
        writeObject(writer,obj);
        writer.Stream() << "</Document>\n";
    }
}

void Document::RestoreDocFile(Base::Reader &reader) {
    Base::XMLReader xmlReader(reader);
    xmlReader.readElement("Document");
    xmlReader.DocumentSchema = xmlReader.getAttributeAsInteger("SchemaVersion","");
    if(!xmlReader.DocumentSchema)
        xmlReader.DocumentSchema = reader.getDocumentSchema();
    xmlReader.FileVersion = xmlReader.getAttributeAsInteger("FileVersion","");
    if(!xmlReader.FileVersion)
        xmlReader.FileVersion = reader.getFileVersion();
    xmlReader.readElement("Object");
    readObject(xmlReader);
}

void Document::readObject(Base::XMLReader &reader) {
    std::string name = reader.getName(reader.getAttribute("name"));
    Base::ReaderContext rctx(name);
    DocumentObject* pObj = getObject(name.c_str());
    if (pObj && !pObj->testStatus(App::PartialObject)) { // check if this feature has been registered
        pObj->setStatus(ObjectStatus::Restore, true);
        try {
            FC_TRACE("restoring " << pObj->getFullName());
            pObj->Restore(reader);
        }
        // Try to continue only for certain exception types if not handled
        // by the feature type. For all other exception types abort the process.
        catch (const Base::UnicodeError &e) {
            e.ReportException();
        }
        catch (const Base::ValueError &e) {
            e.ReportException();
        }
        catch (const Base::IndexError &e) {
            e.ReportException();
        }
        catch (const Base::RuntimeError &e) {
            e.ReportException();
        }
        catch (const Base::XMLAttributeError &e) {
            e.ReportException();
        }

        pObj->setStatus(ObjectStatus::Restore, false);

        if (reader.testStatus(Base::XMLReader::ReaderStatus::PartialRestoreInDocumentObject)) {
            Base::Console().Error("Object \"%s\" was subject to a partial restore. As a result geometry may have changed or be incomplete.\n",name.c_str());
            reader.clearPartialRestoreDocumentObject();
        }
    }
}

struct DepInfo {
    std::unordered_set<std::string> deps;
    int canLoadPartial = 0;
};

static void _loadDeps(const std::string &name,
        std::unordered_map<std::string,bool> &objs,
        const std::unordered_map<std::string,DepInfo> &deps)
{
    auto it = deps.find(name);
    if(it == deps.end()) {
        objs.emplace(name,true);
        return;
    }
    if(it->second.canLoadPartial) {
        if(it->second.canLoadPartial == 1) {
            // canLoadPartial==1 means all its children will be created but not
            // restored, i.e. exists as if newly created object, and therefore no
            // need to load dependency of the children
            for(auto &dep : it->second.deps)
                objs.emplace(dep,false);
            objs.emplace(name,true);
        }else
            objs.emplace(name,false);
        return;
    }
    objs[name] = true;
    // If cannot load partial, then recurse to load all children dependency
    for(auto &dep : it->second.deps) {
        auto it = objs.find(dep);
        if(it!=objs.end() && it->second)
            continue;
        _loadDeps(dep,objs,deps);
    }
}

std::vector<App::DocumentObject*>
Document::readObjects(Base::XMLReader& reader)
{
    d->touchedObjs.clear();
    bool keepDigits = testStatus(Document::KeepTrailingDigits);
    setStatus(Document::KeepTrailingDigits, !reader.doNameMapping());
    std::vector<App::DocumentObject*> objs;

    // read the object types
    reader.readElement("Objects");
    int Cnt = reader.getAttributeAsInteger("Count");

    if(!reader.hasAttribute(FC_ATTR_DEPENDENCIES))
        d->partialLoadObjects.clear();
    else if(!d->partialLoadObjects.empty()) {
        std::unordered_map<std::string,DepInfo> deps;
        for (int i=0 ;i<Cnt ;i++) {
            reader.readElement(FC_ELEMENT_OBJECT_DEPS);
            int dcount = reader.getAttributeAsInteger(FC_ATTR_DEP_COUNT);
            if(!dcount)
                continue;
            auto &info = deps[reader.getAttribute(FC_ATTR_DEP_OBJ_NAME)];
            if(reader.hasAttribute(FC_ATTR_DEP_ALLOW_PARTIAL))
                info.canLoadPartial = reader.getAttributeAsInteger(FC_ATTR_DEP_ALLOW_PARTIAL);
            for(int j=0;j<dcount;++j) {
                reader.readElement(FC_ELEMENT_OBJECT_DEP);
                const char *name = reader.getAttribute(FC_ATTR_DEP_OBJ_NAME);
                if(name && name[0])
                    info.deps.insert(name);
            }
            reader.readEndElement(FC_ELEMENT_OBJECT_DEPS);
        }
        std::vector<std::string> objs;
        objs.reserve(d->partialLoadObjects.size());
        for(auto &v : d->partialLoadObjects)
            objs.emplace_back(v.first.c_str());
        for(auto &name : objs)
            _loadDeps(name,d->partialLoadObjects,deps);
        if(Cnt > (int)d->partialLoadObjects.size())
            setStatus(Document::PartialDoc,true);
        else {
            for(auto &v : d->partialLoadObjects) {
                if(!v.second) {
                    setStatus(Document::PartialDoc,true);
                    break;
                }
            }
            if(!testStatus(Document::PartialDoc))
                d->partialLoadObjects.clear();
        }
    }

    long lastId = 0;
    for (int i=0 ;i<Cnt ;i++) {
        reader.readElement("Object");
        std::string type = reader.getAttribute("type");
        std::string name = reader.getAttribute("name");
        Base::ReaderContext rctx(name);
        std::string viewType = reader.hasAttribute("ViewType")?reader.getAttribute("ViewType"):"";
        int rev = reader.getAttributeAsInteger("revision", "");

        bool partial = false;
        if(!d->partialLoadObjects.empty()) {
            auto it = d->partialLoadObjects.find(name);
            if(it == d->partialLoadObjects.end())
                continue;
            partial = !it->second;
        }

        if(!testStatus(Status::Importing) && reader.hasAttribute("id")) {
            // if not importing, then temporary reset lastObjectId and make the
            // following addObject() generate the correct id for this object.
            d->lastObjectId = reader.getAttributeAsInteger("id")-1;
        }

        // To prevent duplicate name when export/import of objects from
        // external documents, we append those external object name with
        // @<document name>. Before importing (here means we are called by
        // importObjects), we shall strip the postfix. What the caller
        // (MergeDocument) sees is still the unstripped name mapped to a new
        // internal name, and the rest of the link properties will be able to
        // correctly unmap the names.
        auto pos = name.find('@');
        std::string _obj_name;
        const char *obj_name;
        if(pos!=std::string::npos) {
            _obj_name = name.substr(0,pos);
            obj_name = _obj_name.c_str();
        }else
            obj_name = name.c_str();

        try {
            // Use name from XML as is and do NOT remove trailing digits because
            // otherwise we may cause a dependency to itself
            // Example: Object 'Cut001' references object 'Cut' and removing the
            // digits we make an object 'Cut' referencing itself.
            App::DocumentObject* obj = addObject(type.c_str(), obj_name, /*isNew=*/ false, viewType.c_str(), partial);
            if (obj) {
                if(lastId < obj->_Id)
                    lastId = obj->_Id;
                objs.push_back(obj);
                // use this name for the later access because an object with
                // the given name may already exist
                reader.addName(name.c_str(), obj->getNameInDocument());

                // restore touch/error status flags
                if (reader.hasAttribute("Touched")) {
                    if(reader.getAttributeAsInteger("Touched") != 0)
                        d->touchedObjs.insert(obj);
                }
                if (reader.hasAttribute("Invalid")) {
                    obj->setStatus(ObjectStatus::Error, reader.getAttributeAsInteger("Invalid") != 0);
                    if(obj->isError() && reader.hasAttribute("Error"))
                        d->addRecomputeLog(reader.getAttribute("Error"),obj);
                }

                obj->_revision = rev;
            }

            const char *file = reader.getAttribute("file","");
            if(file && file[0])
                reader.addFile(file, this);
        }
        catch (const Base::Exception& e) {
            Base::Console().Error("Cannot create object '%s': (%s)\n", name.c_str(), e.what());
        }
    }
    if(!testStatus(Status::Importing))
        d->lastObjectId = lastId;

    reader.readEndElement("Objects");
    setStatus(Document::KeepTrailingDigits, keepDigits);

    // read the features itself
    reader.clearPartialRestoreDocumentObject();

    reader.readElement("ObjectData");
    Cnt = reader.getAttributeAsInteger("Count");
    std::string objName;
    try {
        for (int i=0 ;i<Cnt ;i++) {
            int guard;
            reader.readElement("Object", &guard);
            objName = reader.getAttribute("name");
            readObject(reader);
            reader.readEndElement("Object",&guard);
        }
    } catch (Base::XMLParseException &e) {
        e.ReportException();
        FC_ERR("Exception while restoring " << getName() << '.' << objName);
        throw;
    }
    reader.readEndElement("ObjectData");

    return objs;
}

void Document::addRecomputeObject(DocumentObject *obj) {
    if(testStatus(Status::Restoring) && obj) {
        setStatus(Status::RecomputeOnRestore, true);
        d->touchedObjs.insert(obj);
        obj->enforceRecompute();
    }
}

std::vector<App::DocumentObject*>
Document::importObjects(Base::XMLReader& reader)
{
    d->hashers.clear();
    Base::FlagToggler<> flag(globalIsRestoring, false);
    Base::ObjectStatusLocker<Status, Document> restoreBit(Status::Restoring, this);
    Base::ObjectStatusLocker<Status, Document> restoreBit2(Status::Importing, this);
    ExpressionParser::ExpressionImporter expImporter(reader);
    reader.readElement("Document");
    long scheme = reader.getAttributeAsInteger("SchemaVersion");
    reader.DocumentSchema = scheme;
    if (reader.hasAttribute("ProgramVersion")) {
        reader.ProgramVersion = reader.getAttribute("ProgramVersion");
    } else {
        reader.ProgramVersion = "pre-0.14";
    }
    if (reader.hasAttribute("FileVersion")) {
        reader.FileVersion = reader.getAttributeAsUnsigned("FileVersion");
    } else {
        reader.FileVersion = 0;
    }

    Base::ReaderContext rctx(getName());
    std::vector<App::DocumentObject*> objs = readObjects(reader);
    for(auto o : objs) {
        if(o && o->getNameInDocument()) {
            o->setStatus(App::ObjImporting,true);
            FC_LOG("importing " << o->getFullName());
            if (auto propUUID = Base::freecad_dynamic_cast<PropertyUUID>(
                        o->getPropertyByName("_ObjectUUID")))
            {
                auto propSource = Base::freecad_dynamic_cast<PropertyUUID>(
                        o->getPropertyByName("_SourceUUID"));
                if (!propSource)
                    propSource = static_cast<PropertyUUID*>(o->addDynamicProperty(
                                "App::PropertyUUID", "_SourceUUID", nullptr, nullptr,
                                Prop_Output | Prop_Hidden));
                if (propSource)
                    propSource->setValue(propUUID->getValue());
                propUUID->setValue(Base::Uuid::createUuid());
            }
        }
    }

    reader.readEndElement("Document");

    signalImportObjects(objs, reader);
    afterRestore(objs,true);

    signalFinishImportObjects(objs);

    for(auto o : objs) {
        if(o && o->getNameInDocument())
            o->setStatus(App::ObjImporting,false);
    }
    d->hashers.clear();
    return objs;
}

unsigned int Document::getMemSize () const
{
    unsigned int size = 0;

    // size of the DocObjects in the document
    std::vector<DocumentObject*>::const_iterator it;
    for (it = d->objectArray.begin(); it != d->objectArray.end(); ++it)
        size += (*it)->getMemSize();

    size += d->Hasher->getMemSize();

    // size of the document properties...
    size += PropertyContainer::getMemSize();

    // Undo Redo size
    size += getUndoMemSize();

    return size;
}

static std::string checkFileName(const char *file) {
    Base::FileInfo fi(file);
    if(fi.isDir())
        return file;

    std::string fn(file);

    // Append extension if missing. This option is added for security reason, so
    // that the user won't accidentally overwrite other file that may be critical.
    if(DocumentParams::getCheckExtension())
    {
        const char *ext = strrchr(file,'.');
        if(!ext || !boost::iequals(ext+1,"fcstd")) {
            if(ext && ext[1] == 0)
                fn += "FCStd";
            else
                fn += ".FCStd";
        }
    }
    return fn;
}

bool Document::saveAs(const char* _file)
{
    std::string file = checkFileName(_file);
    Base::FileInfo fi(file.c_str());
    if (this->FileName.getStrValue() != file) {
        this->FileName.setValue(file);
        this->Label.setValue(fi.fileNamePure());
        this->Uid.touch(); // this forces a rename of the transient directory
    }

    return save();
}

bool Document::saveCopy(const char* _file) const
{
    std::string file = checkFileName(_file);
    if (this->FileName.getStrValue() != file) {
        bool result = saveToFile(file.c_str());
        return result;
    }
    return false;
}

// Save the document under the name it has been opened
bool Document::save ()
{
    if(testStatus(Document::PartialDoc)) {
        FC_ERR("Partial loaded document '" << Label.getValue() << "' cannot be saved");
        // TODO We don't make this a fatal error and return 'true' to make it possible to
        // save other documents that depends on this partial opened document. We need better
        // handling to avoid touching partial documents.
        return true;
    }

    if (*(FileName.getValue()) != '\0') {
        // Save the name of the tip object in order to handle in Restore()
        if (Tip.getValue()) {
            TipName.setValue(Tip.getValue()->getNameInDocument());
        }

        std::string LastModifiedDateString = Base::TimeInfo::currentDateTimeString();
        LastModifiedDate.setValue(LastModifiedDateString.c_str());
        // set author if needed
        bool saveAuthor = DocumentParams::getprefSetAuthorOnSave();
        if (saveAuthor) {
            LastModifiedBy.setValue(DocumentParams::getprefAuthor().c_str());
        }

        return saveToFile(FileName.getValue());
    }

    return false;
}

namespace App {
// Helper class to handle different backup policies
class BackupPolicy {
public:
    enum Policy {
        Standard,
        TimeStamp
    };
    BackupPolicy() {
        policy = Standard;
        numberOfFiles = 1;
        useFCBakExtension = false;
        saveBackupDateFormat = "%Y%m%d-%H%M%S";
    }
    ~BackupPolicy() = default;
    void setPolicy(Policy p) {
        policy = p;
    }
    void setNumberOfFiles(int count) {
        numberOfFiles = count;
    }
    void useBackupExtension(bool on) {
        useFCBakExtension = on;
    }
    void setDateFormat(const std::string& fmt) {
        saveBackupDateFormat = fmt;
    }
    void apply(const std::string& sourcename, const std::string& targetname) {
        switch (policy) {
        case Standard:
            applyStandard(sourcename, targetname);
            break;
        case TimeStamp:
            applyTimeStamp(sourcename, targetname);
            break;
        }
    }

private:
    void applyStandard(const std::string& sourcename, const std::string& targetname) {
        // if saving the project data succeeded rename to the actual file name
        Base::FileInfo fi(targetname);
        if (fi.exists()) {
            if (numberOfFiles > 0) {
                int nSuff = 0;
                std::string fn = fi.fileName();
                Base::FileInfo di(fi.dirPath());
                std::vector<Base::FileInfo> backup;
                std::vector<Base::FileInfo> files = di.getDirectoryContent();
                for (std::vector<Base::FileInfo>::iterator it = files.begin(); it != files.end(); ++it) {
                    std::string file = it->fileName();
                    if (file.substr(0,fn.length()) == fn) {
                        // starts with the same file name
                        std::string suf(file.substr(fn.length()));
                        if (!suf.empty()) {
                            std::string::size_type nPos = suf.find_first_not_of("0123456789");
                            if (nPos==std::string::npos) {
                                // store all backup files
                                backup.push_back(*it);
                                nSuff = std::max<int>(nSuff, std::atol(suf.c_str()));
                            }
                        }
                    }
                }

                if (!backup.empty() && (int)backup.size() >= numberOfFiles) {
                    // delete the oldest backup file we found
                    Base::FileInfo del = backup.front();
                    for (std::vector<Base::FileInfo>::iterator it = backup.begin(); it != backup.end(); ++it) {
                        if (it->lastModified() < del.lastModified())
                            del = *it;
                    }

                    del.deleteFile();
                    fn = del.filePath();
                }
                else {
                    // create a new backup file
                    std::stringstream str;
                    str << fi.filePath() << (nSuff + 1);
                    fn = str.str();
                }

                if (!fi.renameFile(fn.c_str()))
                    Base::Console().Warning("Cannot rename project file to backup file\n");
            }
            else if (fi.isDir()) {
                fi.deleteDirectoryRecursive();
            }
            else {
                fi.deleteFile();
            }
        }

        Base::FileInfo tmp(sourcename);
        if (!tmp.renameFile(targetname.c_str())) {
            throw Base::FileException(
                "Cannot rename tmp save file to project file", targetname);
        }
    }
    void applyTimeStamp(const std::string& sourcename, const std::string& targetname) {
        Base::FileInfo fi(targetname);

        std::string fn = sourcename;
        std::string ext = fi.extension();
        std::string bn; // full path with no extension but with "."
        std::string pbn; // base name of the project + "."
        if (!ext.empty()) {
            bn = fi.filePath().substr(0, fi.filePath().length() - ext.length());
            pbn = fi.fileName().substr(0, fi.fileName().length() - ext.length());
        }
        else {
            bn = fi.filePath() + ".";
            pbn = fi.fileName() + ".";
        }

        bool backupManagementError = false; // Note error and report at the end
        if (fi.exists()) {
            if (numberOfFiles > 0) {
                // replace . by - in format to avoid . between base name and extension
                boost::replace_all(saveBackupDateFormat, ".", "-");
                {
                    // Remove all extra backups
                    std::string fn = fi.fileName();
                    Base::FileInfo di(fi.dirPath());
                    std::vector<Base::FileInfo> backup;
                    std::vector<Base::FileInfo> files = di.getDirectoryContent();
                    for (std::vector<Base::FileInfo>::iterator it = files.begin(); it != files.end(); ++it) {
                        if (it->isFile()) {
                            std::string file = it->fileName();
                            std::string fext = it->extension();
                            std::string fextUp = fext;
                            std::transform(fextUp.begin(), fextUp.end(), fextUp.begin(),(int (*)(int))toupper);
                            // re-enforcing identification of the backup file


                            // old case : the name starts with the full name of the project and follows with numbers
                            if ((startsWith(file, fn) &&
                                 (file.length() > fn.length()) &&
                                 checkDigits(file.substr(fn.length()))) ||
                                 // .FCBak case : The bame starts with the base name of the project + "."
                                 // + complement with no "." + ".FCBak"
                                 ((fextUp == "FCBAK") && startsWith(file, pbn) &&
                                 (checkValidComplement(file, pbn, fext)))) {
                                backup.push_back(*it);
                            }
                        }
                    }

                    if (!backup.empty() && (int)backup.size() >= numberOfFiles) {
                        std::sort (backup.begin(), backup.end(), fileComparisonByDate);
                        // delete the oldest backup file we found
                        // Base::FileInfo del = backup.front();
                        int nb = 0;
                        for (std::vector<Base::FileInfo>::iterator it = backup.begin(); it != backup.end(); ++it) {
                            nb++;
                            if (nb >= numberOfFiles) {
                                try {
                                    if (!it->deleteFile()) {
                                        backupManagementError = true;
                                        Base::Console().Warning("Cannot remove backup file : %s\n", it->fileName().c_str());
                                    }
                                }
                                catch (...) {
                                    backupManagementError = true;
                                    Base::Console().Warning("Cannot remove backup file : %s\n", it->fileName().c_str());
                                }
                            }
                        }

                    }
                }  //end remove backup

                // create a new backup file
                {
                    int ext = 1;
                    if (useFCBakExtension) {
                        std::stringstream str;
                        Base::TimeInfo ti = fi.lastModified();
                        time_t s =ti.getSeconds();
                        struct tm * timeinfo = localtime(& s);
                        char buffer[100];

                        strftime(buffer,sizeof(buffer),saveBackupDateFormat.c_str(),timeinfo);
                        str << bn << buffer ;

                        fn = str.str();
                        bool done = false;

                        if ((fn.empty()) || (fn[fn.length()-1] == ' ') || (fn[fn.length()-1] == '-')) {
                            if (fn[fn.length()-1] == ' ') {
                                fn = fn.substr(0,fn.length()-1);
                            }
                        }
                        else {
                            if (!renameFileNoErase(fi, fn+".FCBak")) {
                                fn = fn + "-";
                            }
                            else {
                                done = true;
                            }
                        }

                        if (!done) {
                            while (ext < numberOfFiles + 10) {
                                if (renameFileNoErase(fi, fn+std::to_string(ext)+".FCBak"))
                                    break;
                                ext++;
                            }
                        }
                    }
                    else {
                        // changed but simpler and solves also the delay sometimes introduced by google drive
                        while (ext < numberOfFiles + 10) {
                            // linux just replace the file if exists, and then the existence is to be tested before rename
                            if (renameFileNoErase(fi, fi.filePath()+std::to_string(ext)))
                                break;
                            ext++;
                        }
                    }

                    if (ext >= numberOfFiles + 10) {
                        Base::Console().Error("File not saved: Cannot rename project file to backup file\n");
                        //throw Base::FileException("File not saved: Cannot rename project file to backup file", fi);
                    }
                }
            }
            else {
                try {
                    fi.deleteFile();
                }
                catch (...) {
                    Base::Console().Warning("Cannot remove backup file: %s\n", fi.fileName().c_str());
                    backupManagementError = true;
                }
            }
        }

        Base::FileInfo tmp(sourcename);
        if (!tmp.renameFile(targetname.c_str())) {
            throw Base::FileException(
                "Save interrupted: Cannot rename temporary file to project file", tmp);
        }

        if (numberOfFiles <= 0) {
            try {
                if (fi.isDir())
                    fi.deleteDirectoryRecursive();
                else
                    fi.deleteFile();
            }
            catch (...) {
                Base::Console().Warning("Cannot remove backup file: %s\n", fi.fileName().c_str());
                backupManagementError = true;
           }
        }

        if (backupManagementError) {
            throw Base::FileException("Warning: Save complete, but error while managing backup history.", fi);
        }
    }
    static bool fileComparisonByDate(const Base::FileInfo& i,
                              const Base::FileInfo& j) {
        return (i.lastModified()>j.lastModified());
    }
    bool startsWith(const std::string& st1,
                    const std::string& st2) const {
        return st1.substr(0,st2.length()) == st2;
    }
    bool checkValidString (const std::string& cmpl, const boost::regex& e) const {
        boost::smatch what;
        bool res = boost::regex_search (cmpl,what,e);
        return res;
    }
    bool checkValidComplement(const std::string& file, const std::string& pbn, const std::string& ext) const {
        std::string cmpl = file.substr(pbn.length(),file.length()- pbn.length() - ext.length()-1);
        boost::regex e (R"(^[^.]*$)");
        return checkValidString(cmpl,e);
    }
    bool checkDigits (const std::string& cmpl) const {
        boost::regex e (R"(^[0-9]*$)");
        return checkValidString(cmpl,e);
    }
    bool renameFileNoErase(Base::FileInfo fi, const std::string& newName) {
        // linux just replaces the file if it exists, so the existence is to be tested before rename
        Base::FileInfo nf(newName);
        if (!nf.exists()) {
            return fi.renameFile(newName.c_str());
        }
        return false;
    }

private:
    Policy policy;
    int numberOfFiles;
    bool useFCBakExtension;
    std::string saveBackupDateFormat;
};
}

bool Document::saveToFile(const char* filename) const
{
    ExpressionBlocker::check();

    signalStartSave(*this, filename);

    int compression = DocumentParams::getCompressionLevel();
    compression = Base::clamp<int>(compression, Z_NO_COMPRESSION, Z_BEST_COMPRESSION);

    bool archive = !Base::FileInfo(filename).isDir();
    bool policy = archive?DocumentParams::getBackupPolicy():false;

    std::string _realfile;
    const char *realfile = filename;
    QFileInfo qfi(QString::fromUtf8(filename));
    if (qfi.isSymLink()) {
        _realfile = qfi.symLinkTarget().toUtf8().constData();
        realfile = _realfile.c_str();
    }

    auto canonical_path = [](const char* filename) {
        try {
#ifdef FC_OS_WIN32
            QString utf8Name = QString::fromUtf8(filename);
            auto realpath = fs::weakly_canonical(fs::absolute(fs::path(utf8Name.toStdWString())));
            std::string nativePath = QString::fromStdWString(realpath.native()).toStdString();
#else
            auto realpath = fs::weakly_canonical(fs::absolute(fs::path(filename)));
            std::string nativePath = realpath.native();
#endif
            // In case some folders in the path do not exist
            auto parentPath = realpath.parent_path();
            fs::create_directories(parentPath);

            return nativePath;
        }
        catch (const std::exception&) {
#ifdef FC_OS_WIN32
            QString utf8Name = QString::fromUtf8(filename);
            auto parentPath = fs::absolute(fs::path(utf8Name.toStdWString())).parent_path();
#else
            auto parentPath = fs::absolute(fs::path(filename)).parent_path();
#endif
            fs::create_directories(parentPath);

            return std::string(filename);
        }
    };

    //realpath is canonical filename i.e. without symlink
    std::string nativePath = canonical_path(realfile);

    // make a tmp. file where to save the project data first and then rename to
    // the actual file name. This may be useful if overwriting an existing file
    // fails so that the data of the work up to now isn't lost.
    std::string uuid = Base::Uuid::createUuid();
    std::string fn = nativePath;
    if (policy) {
        fn += ".";
        fn += uuid;
    }
    Base::FileInfo tmp(fn);


    std::vector<std::string> fileNames;

    // open extra scope to close ZipWriter properly
    {
        Base::ofstream file;
        std::unique_ptr<Base::Writer> _writer;
        if(archive) {
            file.open(tmp, std::ios::out | std::ios::binary);
            if (!file.is_open())
                throw Base::FileException("Failed to open file", tmp);
            auto zipwriter = new Base::ZipWriter(file);
            _writer.reset(zipwriter);
            zipwriter->setComment("FreeCAD Document");
            zipwriter->setLevel(compression);
        } else {
            _writer.reset(new Base::FileWriter(tmp.filePath().c_str()));
        }

        save(*_writer, archive);
        fileNames = _writer->getFilenames();
    }


    if (policy) {
        // if saving the project data succeeded rename to the actual file name
        int count_bak = DocumentParams::getCountBackupFiles();
        bool backup = DocumentParams::getCreateBackupFiles();
        if (!backup) {
            count_bak = -1;
        }
        bool useFCBakExtension = DocumentParams::getUseFCBakExtension();
        std::string	saveBackupDateFormat = DocumentParams::getSaveBackupDateFormat();

        BackupPolicy policy;
        if (useFCBakExtension) {
            policy.setPolicy(BackupPolicy::TimeStamp);
            policy.useBackupExtension(useFCBakExtension);
            policy.setDateFormat(saveBackupDateFormat);
        }
        else {
            policy.setPolicy(BackupPolicy::Standard);
        }
        policy.setNumberOfFiles(count_bak);
        policy.apply(fn, nativePath);
    }

    signalFinishSave(*this, filename);

    if(!archive) {
        std::vector<std::pair<std::string,int> > files;
        for(const auto &f : fileNames) {
            auto it = d->files.find(f);
            if(it == d->files.end()) {
                FC_LOG("document " << getName() << " add " << f);
                files.emplace_back(f,1);
            } else {
                files.emplace_back(f,0);
                d->files.erase(it);
            }
        }
        for(const auto &f : d->files) {
            FC_LOG("document " << getName() << " remove " << f);
            files.emplace_back(f,-1);
        }
        d->files.clear();

        GetApplication().signalDocumentFilesSaved(*this, filename, files);

        bool remove = DocumentParams::getAutoRemoveFile();
        std::string path(filename);
        path += "/";
        for(auto &v : files) {
            if(v.second>=0)
                d->files.insert(std::move(v.first));
            else if(remove) 
                Base::FileInfo(path+v.first).deleteFile();
        }
    }

    return true;
}

void Document::save(Base::Writer &writer, bool archive) const {
    if(!archive) {
        writer.setFileVersion(2);
        writer.setForceXML(ForceXML.getValue());
        writer.setSplitXML(SplitXML.getValue());
    }

    writer.putNextEntry("Document.xml");

    if (PreferBinary.getValue()) {
        writer.setMode("BinaryBrep");
        writer.setPreferBinary(true);
    } else if(writer.getFileVersion() > 1)
        writer.setPreferBinary(false);

    writer.Stream() << "<?xml version='1.0' encoding='utf-8'?>\n"
                    << "<!--\n"
                    << " FreeCAD Document, see http://www.freecadweb.org for more information...\n"
                    << "-->\n";
    Document::Save(writer);

    // Special handling for Gui document.
    signalSaveDocument(writer);

    // write additional files
    writer.writeFiles();

    if (writer.hasErrors()) {
        throw Base::FileException("Failed to write all data to file");
    }

    GetApplication().signalSaveDocument(*this);
}

bool Document::isAnyRestoring() {
    return globalIsRestoring;
}

// Open the document
void Document::restore (const char *filename,
        bool delaySignal, const std::vector<std::string> &objNames)
{
    if(!filename)
        filename = FileName.getValue();
    Base::FileInfo fi(filename);
    if(fi.isDir()) {
        fi.setFile(std::string(filename)+'/'+"Document.xml");
        if(!fi.exists()) 
            throw Base::FileException("Project file not found",fi.filePath());
    }

    std::unique_ptr<Base::Reader> _reader;
    std::unique_ptr<Base::XMLReader> _xmlReader;
    std::unique_ptr<zipios::ZipInputStream> zipstream;
    std::string dirname;

    if(fi.fileNamePure() == "Document" && fi.hasExtension("xml")) {
        Base::FileInfo di(fi.dirPath());
        _reader.reset(new Base::FileReader(fi,di.fileName()+"/Document.xml"));
        _xmlReader.reset(new Base::XMLReader(*_reader));
    } else {
        // file.open(fi, std::ios::in | std::ios::binary);
        // std::streambuf* buf = file.rdbuf();
        // std::streamoff size = buf->pubseekoff(0, std::ios::end, std::ios::in);
        // buf->pubseekoff(0, std::ios::beg, std::ios::in);
        // if (size < 22) // an empty zip archive has 22 bytes
        //     throw Base::FileException("Invalid project file",filename);
        zipstream.reset(new zipios::ZipInputStream(filename));
        _reader.reset(new Base::ZipReader(*zipstream,filename));
        _xmlReader.reset(new Base::XMLReader(*_reader));
    }

    restore(*_xmlReader, delaySignal, objNames);
}

void Document::restore(Base::XMLReader &reader,
        bool delaySignal, const std::vector<std::string> &objNames)
{
    if (!reader.isValid())
        throw Base::FileException("Error reading project file", FileName.getValue());

    clearUndos();
    d->files.clear();
    bool signal = false;
    Document *activeDoc = GetApplication().getActiveDocument();
    if (!d->objectArray.empty()) {
        signal = true;
        GetApplication().signalDeleteDocument(*this);
        d->clearDocument();
    }

    Base::FlagToggler<> flag(globalIsRestoring, false);

    setStatus(Document::PartialDoc,false);

    d->clearRecomputeLog();
    d->objectArray.clear();
    d->objectMap.clear();
    d->objectIdMap.clear();
    d->lastObjectId = 0;

    if(signal) {
        GetApplication().signalNewDocument(*this,true);
        if(activeDoc == this)
            GetApplication().setActiveDocument(this);
    }


    GetApplication().signalStartRestoreDocument(*this);
    setStatus(Document::Restoring, true);

    d->partialLoadObjects.clear();
    for(auto &name : objNames)
        d->partialLoadObjects.emplace(name,true);
    try {
        Document::Restore(reader);
    } catch (const Base::XMLParseException &) {
        throw;
    } catch (const Base::Exception& e) {
        Base::Console().Error("Invalid Document.xml: %s\n", e.what());
        setStatus(Document::RestoreError, true);
    }

    d->partialLoadObjects.clear();
    d->programVersion = reader.ProgramVersion;

    // Special handling for Gui document, the view representations must already
    // exist, what is done in Restore().
    // Note: This file doesn't need to be available if the document has been created
    // without GUI. But if available then follow after all data files of the App document.
    signalRestoreDocument(reader);

    reader.readFiles();

    for(auto &f : reader.getFilenames()) {
        FC_TRACE("document " << getName() << " file: " << f);
        d->files.insert(f);
    }

    if (reader.testStatus(Base::XMLReader::ReaderStatus::PartialRestore)) {
        setStatus(Document::PartialRestore, true);
        Base::Console().Error("There were errors while loading the file. Some data might have been modified or not recovered at all. Look above for more specific information about the objects involved.\n");
    }

    if(!delaySignal)
        afterRestore();
}

bool Document::afterRestore(bool checkPartial) {
    Base::FlagToggler<> flag(globalIsRestoring, false);
    if(!afterRestore(d->objectArray,checkPartial)) {
        FC_WARN("Reload partial document " << getName());
        GetApplication().signalPendingReloadDocument(*this);
        return false;
    }
    setStatus(Document::Restoring, false);
    GetApplication().signalFinishRestoreDocument(*this);
    return true;
}

bool Document::afterRestore(const std::vector<DocumentObject *> &objArray, bool checkPartial)
{
    checkPartial = checkPartial && testStatus(Document::PartialDoc);
    if(checkPartial && !d->touchedObjs.empty())
        return false;

    // some link type property cannot restore link information until other
    // objects has been restored. For example, PropertyExpressionEngine and
    // PropertySheet with expression containing label reference. So we add the
    // Property::afterRestore() interface to let them sort it out. Note, this
    // API is not called in object dedpenency order, because the order
    // information is not ready yet.
    std::map<DocumentObject*, std::vector<App::Property*> > propMap;
    for(auto obj : objArray) {
        auto &props = propMap[obj];
        obj->getPropertyList(props);
        for(auto prop : props) {
            try {
                prop->afterRestore();
            } catch (const Base::Exception& e) {
                FC_ERR("Failed to restore " << obj->getFullName()
                        << '.' << prop->getName() << ": " << e.what());
                d->addRecomputeLog(e.what(), obj);
            }
        }
    }

    if(checkPartial && !d->touchedObjs.empty()) {
        // partial document touched, signal full reload
        return false;
    }

    std::set<DocumentObject*> objSet(objArray.begin(),objArray.end());
    auto objs = getDependencyList(objArray.empty()?d->objectArray:objArray,DepSort);
    for (auto obj : objs) {
        if(objSet.find(obj)==objSet.end())
            continue;
        try {
            for(auto prop : propMap[obj])
                prop->onContainerRestored();
            bool touched = false;
            auto returnCode = obj->ExpressionEngine.execute(
                    PropertyExpressionEngine::ExecuteOnRestore,&touched);
            if(returnCode!=DocumentObject::StdReturn) {
                FC_ERR("Expression engine failed to restore " << obj->getFullName() << ": " << returnCode->Why);
                d->addRecomputeLog(returnCode);
            }
            obj->onDocumentRestored();
            if(touched)
                d->touchedObjs.insert(obj);
        }
        catch (const Base::Exception& e) {
            d->addRecomputeLog(e.what(),obj);
            FC_ERR("Failed to restore " << obj->getFullName() << ": " << e.what());
        }
        catch (std::exception &e) {
            d->addRecomputeLog(e.what(),obj);
            FC_ERR("Failed to restore " << obj->getFullName() << ": " << e.what());
        }
        catch (...) {
            d->addRecomputeLog("Unknown exception on restore",obj);
            FC_ERR("Failed to restore " << obj->getFullName() << ": " << "unknown exception");
        }
        if(obj->isValid()) {
            auto &props = propMap[obj];
            props.clear();
            // refresh properties in case the object changes its property list
            obj->getPropertyList(props);
            for(auto prop : props) {
                auto link = Base::freecad_dynamic_cast<PropertyLinkBase>(prop);
                int res;
                std::string errMsg;
                if(link && (res=link->checkRestore(&errMsg))) {
                    d->touchedObjs.insert(obj);
                    if(res==1 || checkPartial) {
                        FC_WARN(obj->getFullName() << '.' << prop->getName() << ": " << errMsg);
                        setStatus(Document::LinkStampChanged, true);
                        if(checkPartial)
                            return false;
                    } else {
                        FC_ERR(obj->getFullName() << '.' << prop->getName() << ": " << errMsg);
                        d->addRecomputeLog(errMsg,obj);
                        setStatus(Document::PartialRestore, true);
                    }
                }
            }
        }

        if(checkPartial && !d->touchedObjs.empty()) {
            // partial document touched, signal full reload
            return false;
        } else if(!obj->isError() && !d->touchedObjs.count(obj))
            obj->purgeTouched();

        signalFinishRestoreObject(*obj);
    }

    d->touchedObjs.clear();
    return true;
}

bool Document::isSaved() const
{
    std::string name = FileName.getValue();
    return !name.empty();
}

/** Label is the visible name of a document shown e.g. in the windows title
 * or in the tree view. The label almost (but not always e.g. if you manually change it)
 * matches with the file name where the document is stored to.
 * In contrast to Label the method getName() returns the internal name of the document that only
 * matches with Label when loading or creating a document because then both are set to the same value.
 * Since the internal name cannot be changed during runtime it must differ from the Label after saving
 * the document the first time or saving it under a new file name.
 * @ note More than one document can have the same label name.
 * @ note The internal is always guaranteed to be unique because @ref Application::newDocument() checks
 * for a document with the same name and makes it unique if needed. Hence you cannot rely on that the
 * internal name matches with the name you passed to Application::newDoument(). You should use the
 * method getName() instead.
 */
const char* Document::getName() const
{
    // return GetApplication().getDocumentName(this);
    return myName.c_str();
}

std::string Document::getFullName(bool python) const {
    if(python) {
        std::ostringstream ss;
        ss << "FreeCAD.getDocument('" << myName << "')";
        return ss.str();
    }
    return myName;
}

App::Document *Document::getOwnerDocument() const {
    return const_cast<App::Document*>(this);
}

const char* Document::getProgramVersion() const
{
    return d->programVersion.c_str();
}

const char* Document::getFileName() const
{
    return testStatus(TempDoc) ? TransientDir.getValue()
                               : FileName.getValue();
}

/// Remove all modifications. After this call The document becomes valid again.
void Document::purgeTouched()
{
    for (std::vector<DocumentObject*>::iterator It = d->objectArray.begin();It != d->objectArray.end();++It)
        (*It)->purgeTouched();
}

bool Document::isTouched() const
{
    for (std::vector<DocumentObject*>::const_iterator It = d->objectArray.begin();It != d->objectArray.end();++It)
        if ((*It)->isTouched())
            return true;
    return false;
}

vector<DocumentObject*> Document::getTouched() const
{
    vector<DocumentObject*> result;

    for (std::vector<DocumentObject*>::const_iterator It = d->objectArray.begin();It != d->objectArray.end();++It)
        if ((*It)->isTouched())
            result.push_back(*It);

    return result;
}

void Document::setClosable(bool c)
{
    setStatus(Document::Closable, c);
}

bool Document::isClosable() const
{
    return testStatus(Document::Closable);
}

int Document::countObjects() const
{
   return static_cast<int>(d->objectArray.size());
}

void Document::getLinksTo(std::set<DocumentObject*> &links,
        const DocumentObject *obj, int options, int maxCount,
        const std::vector<DocumentObject*> &objs) const
{
    std::map<const App::DocumentObject*, std::vector<App::DocumentObject*> > linkMap;

    for(auto o : !objs.empty() ? objs : d->objectArray) {
        if (o == obj)
            continue;
        auto linked = o;
        if (options & GetLinkArrayElement) {
            linked = o->getLinkedObject(false);
        }
        else {
            auto ext = o->getExtensionByType<LinkBaseExtension>(true);
            if(ext)
                linked = ext->getTrueLinkedObject(false,nullptr,0,true);
            else
                linked = o->getLinkedObject(false);
        }

        if(linked && linked!=o) {
            if(options & GetLinkRecursive)
                linkMap[linked].push_back(o);
            else if(linked == obj || !obj) {
                if((options & GetLinkExternal)
                        && linked->getDocument()==o->getDocument())
                    continue;
                else if(options & GetLinkedObject)
                    links.insert(linked);
                else
                    links.insert(o);
                if(maxCount && maxCount<=(int)links.size())
                    return;
            }
        }
    }

    if(!(options & GetLinkRecursive))
        return;

    std::vector<const DocumentObject*> current(1,obj);
    for(int depth=0;!current.empty();++depth) {
        if(!GetApplication().checkLinkDepth(depth, MessageOption::Error))
            break;
        std::vector<const DocumentObject*> next;
        for(const App::DocumentObject *o : current) {
            auto iter = linkMap.find(o);
            if(iter==linkMap.end())
                continue;
            for (App::DocumentObject *link : iter->second) {
                if (links.insert(link).second) {
                    if(maxCount && maxCount<=(int)links.size())
                        return;
                    next.push_back(link);
                }
            }
        }
        current = std::move(next);
    }
    return;
}

bool Document::hasLinksTo(const DocumentObject *obj) const {
    std::set<DocumentObject *> links;
    getLinksTo(links,obj,0,1);
    return !links.empty();
}

std::vector<App::DocumentObject*> Document::getInList(const DocumentObject* me) const
{
    // result list
    std::vector<App::DocumentObject*> result;
    // go through all objects
    for (auto It = d->objectMap.begin(); It != d->objectMap.end();++It) {
        // get the outList and search if me is in that list
        std::vector<DocumentObject*> OutList = It->second->getOutList();
        for (std::vector<DocumentObject*>::const_iterator It2=OutList.begin();It2!=OutList.end();++It2)
            if (*It2 && *It2 == me)
                // add the parent object
                result.push_back(It->second);
    }
    return result;
}

// This function unifies the old _rebuildDependencyList() and
// getDependencyList().  The algorithm basically obtains the object dependency
// by recrusivly visiting the OutList of each object in the given object array.
// It makes sure to call getOutList() of each object once and only once, which
// makes it much more efficient than calling getRecursiveOutList() on each
// individual object.
//
// The problem with the original algorithm is that, it assumes the objects
// inside any OutList are all within the given object array, so it does not
// recursively call getOutList() on those dependent objects inside. This
// assumption is broken by the introduction of PropertyXLink which can link to
// external object.
//
static void _buildDependencyList(const std::vector<App::DocumentObject*> &objectArray,
        int options, std::vector<App::DocumentObject*> *depObjs,
        DependencyList *depList, std::map<DocumentObject*,Vertex> *objectMap,
        bool *touchCheck = nullptr)
{
    std::map<DocumentObject*, std::vector<DocumentObject*> > outLists;
    std::deque<DocumentObject*> objs;

    if(objectMap) objectMap->clear();
    if(depList) depList->clear();

    int op = (options & Document::DepNoXLinked)?DocumentObject::OutListNoXLinked:0;
    for (auto obj : objectArray) {
        objs.push_back(obj);
        while(!objs.empty()) {
            auto obj = objs.front();
            objs.pop_front();
            if(!obj || !obj->getNameInDocument())
                continue;

            auto it = outLists.find(obj);
            if(it!=outLists.end())
                continue;

            if(touchCheck) {
                if(obj->isTouched() || obj->mustExecute()) {
                    // early termination on touch check
                    *touchCheck = true;
                    return;
                }
            }
            if(depObjs) depObjs->push_back(obj);
            if(objectMap && depList)
                (*objectMap)[obj] = add_vertex(*depList);

            auto &outList = outLists[obj];
            outList = obj->getOutList(op);
            objs.insert(objs.end(),outList.begin(),outList.end());
        }
    }

    if(objectMap && depList) {
        for (const auto &v : outLists) {
            for(auto obj : v.second) {
                if(obj && obj->getNameInDocument())
                    add_edge((*objectMap)[v.first],(*objectMap)[obj],*depList);
            }
        }
    }
}

std::vector<App::DocumentObject*> Document::getDependencyList(
    const std::vector<App::DocumentObject*>& objectArray, int options)
{
    std::vector<App::DocumentObject*> ret;
    if(!(options & (DepSort | DepNoCycle))) {
        _buildDependencyList(objectArray,options,&ret,nullptr,nullptr);
        return ret;
    }

    DependencyList depList;
    std::map<DocumentObject*,Vertex> objectMap;
    std::map<Vertex,DocumentObject*> vertexMap;

    _buildDependencyList(objectArray,options,nullptr,&depList,&objectMap);

    for(auto &v : objectMap)
        vertexMap[v.second] = v.first;

    std::list<Vertex> make_order;
    try {
        boost::topological_sort(depList, std::front_inserter(make_order));
    } catch (const std::exception& e) {
        if(options & DepNoCycle) {
            // Use boost::strong_components to find cycles. It groups strongly
            // connected vertices as components, and therefore each component
            // forms a cycle.
            std::vector<int> c(vertexMap.size());
            std::map<int,std::vector<Vertex> > components;
            boost::strong_components(depList,boost::make_iterator_property_map(
                        c.begin(),boost::get(boost::vertex_index,depList),c[0]));
            for(size_t i=0;i<c.size();++i)
                components[c[i]].push_back(i);

            std::ostringstream ss;
            ss << "\nDependency cycles:";
            std::vector<Property*> props;
            std::vector<ObjectIdentifier> identifiers;
            auto findProperty = [&](DocumentObject *obj, DocumentObject *link) {
                props.clear();
                obj->getPropertyList(props);
                bool first = true;
                for (const auto *prop : props) {
                    if (!prop->getName() || prop->getContainer() != obj)
                        continue;
                    if (auto propLink = Base::freecad_dynamic_cast<PropertyLinkBase>(prop)) {
                        identifiers.clear();
                        propLink->getLinksTo(identifiers, link);
                        for (const auto &path : identifiers) {
                            ss << ", ";
                            if (first) {
                                first = false;
                                ss << "Property: ";
                            }
                            ss << path.canonicalPath().toString();
                        }
                    }
                }
            };
            for(auto &v : components) {
                if(v.second.size()==1) {
                    // For components with only one member, we still need to
                    // check if there is self looping.
                    auto it = vertexMap.find(v.second[0]);
                    if(it==vertexMap.end())
                        continue;
                    // Try search the object in its own out list
                    for(auto obj : it->second->getOutList()) {
                        if(obj == it->second) {
                            ss << '\n' << it->second->getFullName();
                            findProperty(obj, obj);
                            ss << '\n';
                            break;
                        }
                    }
                    continue;
                }
                // For components with more than one member, they form a loop together
                ss << '\n';
                DocumentObject *first = nullptr;
                DocumentObject *prev = nullptr;
                for(size_t i=0;i<v.second.size();++i) {
                    auto it = vertexMap.find(v.second[i]);
                    if(it==vertexMap.end())
                        continue;
                    if (prev) {
                        findProperty(prev, it->second);
                        ss << '\n';
                    } else
                        first = it->second;
                    ss << App::SubObjectT(it->second, "").getObjectFullName();
                    prev = it->second;
                }
                if (first != prev)
                    findProperty(prev, first);
                ss << '\n';
            }
            FC_ERR(ss.str());
            FC_THROWM(Base::RuntimeError,
                    "Cyclice dependency detected.\n"
                    "Please check Report View for more details.");
        }
        FC_ERR(e.what());
        ret = DocumentP::partialTopologicalSort(objectArray);
        std::reverse(ret.begin(),ret.end());
        return ret;
    }

    for (std::list<Vertex>::reverse_iterator i = make_order.rbegin();i != make_order.rend(); ++i)
        ret.push_back(vertexMap[*i]);
    return ret;
}

std::vector<App::Document*> Document::getDependentDocuments(bool sort) {
    return getDependentDocuments({this},sort);
}

std::vector<App::Document*> Document::getDependentDocuments(
        std::vector<App::Document*> pending, bool sort)
{
    DependencyList depList;
    std::map<Document*,Vertex> docMap;
    std::map<Vertex,Document*> vertexMap;

    std::vector<App::Document*> ret;
    if(pending.empty())
        return ret;

    auto outLists = PropertyXLink::getDocumentOutList();
    std::set<App::Document*> docs;
    docs.insert(pending.begin(),pending.end());
    if(sort) {
        for(auto doc : pending)
            docMap[doc] = add_vertex(depList);
    }
    while(!pending.empty()) {
        auto doc = pending.back();
        pending.pop_back();

        auto it = outLists.find(doc);
        if(it == outLists.end())
            continue;

        auto &vertex = docMap[doc];
        for(auto depDoc : it->second) {
            if(docs.insert(depDoc).second) {
                pending.push_back(depDoc);
                if(sort)
                    docMap[depDoc] = add_vertex(depList);
            }
            add_edge(vertex,docMap[depDoc],depList);
        }
    }

    if(!sort) {
        ret.insert(ret.end(),docs.begin(),docs.end());
        return ret;
    }

    for(auto &v : docMap)
        vertexMap[v.second] = v.first;

    std::list<Vertex> make_order;
    try {
        boost::topological_sort(depList, std::front_inserter(make_order));
    } catch (const std::exception& e) {
        // Use boost::strong_components to find cycles. It groups strongly
        // connected vertices as components, and therefore each component
        // forms a cycle.
        std::vector<int> c(vertexMap.size());
        std::map<int,std::vector<Vertex> > components;
        boost::strong_components(depList,boost::make_iterator_property_map(
                    c.begin(),boost::get(boost::vertex_index,depList),c[0]));
        for(size_t i=0;i<c.size();++i)
            components[c[i]].push_back(i);

        FC_ERR("Document dependency cycles: ");
        std::ostringstream ss;
        ss << '\n';
        for(auto &v : components) {
            if(v.second.size()<=1)
                continue;
            // For components with more than one member, they form a loop together
            for(size_t i=0;i<v.second.size();++i) {
                auto it = vertexMap.find(v.second[i]);
                if(it==vertexMap.end())
                    continue;
                if(i%6==0)
                    ss << '\n';
                ss << it->second->getName() << ", ";
            }
            ss << '\n';
        }
        FC_ERR(ss.str());
        FC_THROWM(Base::RuntimeError,
                "Cyclice depending documents detected.\n"
                "Please check Report View for more details.");
    }

    for (auto rIt=make_order.rbegin(); rIt!=make_order.rend(); ++rIt)
        ret.push_back(vertexMap[*rIt]);
    return ret;
}

void Document::_rebuildDependencyList(const std::vector<App::DocumentObject*> &objs)
{
#ifdef USE_OLD_DAG
    _buildDependencyList(objs.empty()?d->objectArray:objs,false,0,&d->DepList,&d->VertexObjectList);
#else
    (void)objs;
#endif
}

/**
 * @brief Signal that object identifiers, typically a property or document object has been renamed.
 *
 * This function iterates through all document object in the document, and calls its
 * renameObjectIdentifiers functions.
 *
 * @param paths Map with current and new names
 */

void Document::renameObjectIdentifiers(const std::map<App::ObjectIdentifier, App::ObjectIdentifier> &paths, const std::function<bool(const App::DocumentObject*)> & selector)
{
    std::map<App::ObjectIdentifier, App::ObjectIdentifier> extendedPaths;

    std::map<App::ObjectIdentifier, App::ObjectIdentifier>::const_iterator it = paths.begin();
    while (it != paths.end()) {
        extendedPaths[it->first.canonicalPath()] = it->second.canonicalPath();
        ++it;
    }

    for (std::vector<DocumentObject*>::iterator it = d->objectArray.begin(); it != d->objectArray.end(); ++it)
        if (selector(*it))
            (*it)->renameObjectIdentifiers(extendedPaths);
}

namespace {
int _Recomputing;
class RecomputeCounter {
public:
    RecomputeCounter()
    {
        ++_Recomputing;
    }
    ~RecomputeCounter()
    {
        --_Recomputing;
    }
};
} // anonymous namespace

bool Document::isAnyRecomputing()
{
    return _Recomputing != 0;
}

int Document::recompute(const std::vector<App::DocumentObject*> &objs, bool force, bool *hasError, int options)
{
    RecomputeCounter counter;

    if (d->undoing || d->rollback) {
        if (FC_LOG_INSTANCE.isEnabled(FC_LOGLEVEL_LOG))
            FC_WARN("Ignore document recompute on undo/redo");
        return 0;
    }

    ExpressionParser::clearWarning();

    int objectCount = 0;
    if (testStatus(Document::PartialDoc)) {
        if(mustExecute())
            FC_WARN("Please reload partial document '" << Label.getValue() << "' for recomputation.");
        return 0;
    }
    if (testStatus(Document::Recomputing)) {
        // this is clearly a bug in the calling instance
        FC_ERR("Recursive calling of recompute for document " << getName());
        return 0;
    }
    // The 'SkipRecompute' flag can be (tmp.) set to avoid too many
    // time expensive recomputes
    if(!force && testStatus(Document::SkipRecompute)) {
        signalSkipRecompute(*this,objs);
        return 0;
    }

    // delete recompute log
    d->clearRecomputeLog();

    d->skippedObjs.clear();

    FC_TIME_INIT(t);

    Base::ObjectStatusLocker<Document::Status, Document> exe(Document::Recomputing, this);
    signalBeforeRecompute(*this);

#if 0
    //////////////////////////////////////////////////////////////////////////
    // FIXME Comment by Realthunder:
    // the topologicalSrot() below cannot handle partial recompute, haven't got
    // time to figure out the code yet, simply use back boost::topological_sort
    // for now, that is, rely on getDependencyList() to do the sorting. The
    // downside is, it didn't take advantage of the ready built InList, nor will
    // it report for cyclic dependency.
    //////////////////////////////////////////////////////////////////////////

    // get the sorted vector of all dependent objects and go though it from the end
    auto depObjs = getDependencyList(objs.empty()?d->objectArray:objs);
    vector<DocumentObject*> topoSortedObjects = topologicalSort(depObjs);
    if (topoSortedObjects.size() != depObjs.size()){
        cerr << "App::Document::recompute(): cyclic dependency detected" << endl;
        topoSortedObjects = d->partialTopologicalSort(depObjs);
    }
    std::reverse(topoSortedObjects.begin(),topoSortedObjects.end());
#else
    auto topoSortedObjects = getDependencyList(objs.empty()?d->objectArray:objs,DepSort|options);
#endif
    for(auto obj : topoSortedObjects)
        obj->setStatus(ObjectStatus::PendingRecompute,true);

    bool canAbort = DocumentParams::getCanAbortRecompute();

    std::set<App::DocumentObject *> filter;
    size_t idx = 0;

    FC_TIME_INIT(t2);

    bool aborted = false;
    try {
        // maximum two passes to allow some form of dependency inversion
        for(int passes=0; passes<2 && idx<topoSortedObjects.size(); ++passes) {
            std::unique_ptr<Base::SequencerLauncher> seq;
            if(canAbort)
                seq.reset(new Base::SequencerLauncher("Recompute...", topoSortedObjects.size()));
            FC_LOG("Recompute pass " << passes);
            for (; idx < topoSortedObjects.size(); ++idx) {
                auto obj = topoSortedObjects[idx];
                if(!obj->getNameInDocument() || filter.find(obj)!=filter.end())
                    continue;
                // ask the object if it should be recomputed
                bool doRecompute = false;
                if (obj->mustRecompute()) {
                    doRecompute = true;
                    ++objectCount;
                    int res = _recomputeFeature(obj);
                    if(res) {
                        if(hasError)
                            *hasError = true;
                        if(res < 0) {
                            passes = 2;
                            break;
                        }
                        // if something happened filter all object in its
                        // inListRecursive from the queue then proceed
                        obj->getInListEx(filter,true);
                        filter.insert(obj);
                        continue;
                    }
                }
                if(obj->isTouched() || doRecompute) {
                    signalRecomputedObject(*obj);
                    GetApplication().signalRecomputedObject(*this, *obj);
                    obj->purgeTouched();
                    // Mark all dependent object with ObjectStatus::Enforce.
                    // Note that We don't call enforceRecompute() here in order
                    // to enable recomputation optimization (see
                    // _recomputeFeature())
                    for (auto inObjIt : obj->getInList()) {
                        inObjIt->StatusBits.set(ObjectStatus::Enforce);
                        inObjIt->StatusBits.set(ObjectStatus::Touch);
                        if (obj->getDocument())
                            obj->getDocument()->signalTouchedObject(*obj);
                    }

                    // give the object a chance to revert the above touching,
                    // because for example, new objects are created with
                    // object's execute(), and it will be safe to not touch
                    // those objects.
                    obj->afterRecompute();
                }
                if (seq)
                    seq->next(true);
            }
            // check if all objects are recomputed but still thouched
            for (size_t i=0;i<topoSortedObjects.size();++i) {
                auto obj = topoSortedObjects[i];
                obj->setStatus(ObjectStatus::Recompute2,false);
                if(!filter.count(obj) && obj->isTouched()) {
                    if(passes>0)
                        FC_ERR(obj->getFullName() << " still touched after recompute");
                    else{
                        FC_LOG(obj->getFullName() << " still touched after recompute");
                        if(idx>=topoSortedObjects.size()) {
                            // let's start the next pass on the first touched object
                            idx = i;
                        }
                        obj->setStatus(ObjectStatus::Recompute2,true);
                    }
                }
            }
        }
    }
    catch(Base::AbortException &e){
        aborted = true;
    }
    catch(Base::Exception &e) {
        e.ReportException();
    }

    FC_TIME_LOG(t2, "Recompute");

    for(auto obj : topoSortedObjects) {
        if(!obj->getNameInDocument())
            continue;
        obj->setStatus(ObjectStatus::PendingRecompute,false);
        obj->setStatus(ObjectStatus::Recompute2,false);
    }

    if (aborted)
        throw Base::AbortException();

    signalRecomputed(*this,topoSortedObjects);

    if(!d->skippedObjs.empty())
        signalSkipRecompute(*this,d->skippedObjs);

    FC_TIME_LOG(t,"Recompute total");

    if (!d->_RecomputeLog.empty()) {
        if (!testStatus(Status::IgnoreErrorOnRecompute))
            Base::Console().Error("Recompute failed!\n");
    }

    clearPendingRemove();
    return objectCount;
}

void Document::clearPendingRemove()
{
    for (auto doc : GetApplication().getDocuments()) {
        decltype(doc->d->pendingRemove) objs;
        objs.swap(doc->d->pendingRemove);
        for(auto &o : objs) {
            try {
                auto obj = o.getObject();
                if(obj)
                    obj->getDocument()->removeObject(obj->getNameInDocument());
            } catch (Base::Exception & e) {
                FC_ERR("error when removing object " << o.getDocumentName() << '#' << o.getObjectName()
                        << ": " << e.what());
            }
        }
    }
}

/*!
  Does almost the same as topologicalSort() until no object with an input degree of zero
  can be found. It then searches for objects with an output degree of zero until neither
  an object with input or output degree can be found. The remaining objects form one or
  multiple cycles.
  An alternative to this method might be:
  https://en.wikipedia.org/wiki/Tarjan%E2%80%99s_strongly_connected_components_algorithm
 */
std::vector<App::DocumentObject*> DocumentP::partialTopologicalSort(
        const std::vector<App::DocumentObject*>& objects)
{
    vector < App::DocumentObject* > ret;
    ret.reserve(objects.size());
    // pairs of input and output degree
    map < App::DocumentObject*, std::pair<int, int> > countMap;

    for (auto objectIt : objects) {
        //we need inlist with unique entries
        auto in = objectIt->getInList();
        std::sort(in.begin(), in.end());
        in.erase(std::unique(in.begin(), in.end()), in.end());

        //we need outlist with unique entries
        auto out = objectIt->getOutList();
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());

        countMap[objectIt] = std::make_pair(in.size(), out.size());
    }

    std::list<App::DocumentObject*> degIn;
    std::list<App::DocumentObject*> degOut;

    bool removeVertex = true;
    while (removeVertex) {
        removeVertex = false;

        // try input degree
        auto degInIt = find_if(countMap.begin(), countMap.end(),
                               [](pair< App::DocumentObject*, pair<int, int> > vertex)->bool {
            return vertex.second.first == 0;
        });

        if (degInIt != countMap.end()) {
            removeVertex = true;
            degIn.push_back(degInIt->first);
            degInIt->second.first = degInIt->second.first - 1;

            //we need outlist with unique entries
            auto out = degInIt->first->getOutList();
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end()), out.end());

            for (auto outListIt : out) {
                auto outListMapIt = countMap.find(outListIt);
                if (outListMapIt != countMap.end())
                    outListMapIt->second.first = outListMapIt->second.first - 1;
            }
        }
    }

    // make the output degree negative if input degree is negative
    // to mark the vertex as processed
    for (auto& countIt : countMap) {
        if (countIt.second.first < 0) {
            countIt.second.second = -1;
        }
    }

    removeVertex = degIn.size() != objects.size();
    while (removeVertex) {
        removeVertex = false;

        auto degOutIt = find_if(countMap.begin(), countMap.end(),
                               [](pair< App::DocumentObject*, pair<int, int> > vertex)->bool {
            return vertex.second.second == 0;
        });

        if (degOutIt != countMap.end()) {
            removeVertex = true;
            degOut.push_front(degOutIt->first);
            degOutIt->second.second = degOutIt->second.second - 1;

            //we need inlist with unique entries
            auto in = degOutIt->first->getInList();
            std::sort(in.begin(), in.end());
            in.erase(std::unique(in.begin(), in.end()), in.end());

            for (auto inListIt : in) {
                auto inListMapIt = countMap.find(inListIt);
                if (inListMapIt != countMap.end())
                    inListMapIt->second.second = inListMapIt->second.second - 1;
            }
        }
    }

    // at this point we have no root object any more
    for (auto countIt : countMap) {
        if (countIt.second.first > 0 && countIt.second.second > 0) {
            degIn.push_back(countIt.first);
        }
    }

    ret.insert(ret.end(), degIn.begin(), degIn.end());
    ret.insert(ret.end(), degOut.begin(), degOut.end());

    return ret;
}

std::vector<App::DocumentObject*> DocumentP::topologicalSort(const std::vector<App::DocumentObject*>& objects) const
{
    // topological sort algorithm described here:
    // https://de.wikipedia.org/wiki/Topologische_Sortierung#Algorithmus_f.C3.BCr_das_Topologische_Sortieren
    vector < App::DocumentObject* > ret;
    ret.reserve(objects.size());
    map < App::DocumentObject*,int > countMap;

    for (auto objectIt : objects) {
        // We now support externally linked objects
        // if(!obj->getNameInDocument() || obj->getDocument()!=this)
        if(!objectIt->getNameInDocument())
            continue;
        //we need inlist with unique entries
        auto in = objectIt->getInList();
        std::sort(in.begin(), in.end());
        in.erase(std::unique(in.begin(), in.end()), in.end());

        countMap[objectIt] = in.size();
    }

    auto rootObjeIt = find_if(countMap.begin(), countMap.end(), [](pair < App::DocumentObject*, int > count)->bool {
        return count.second == 0;
    });

    if (rootObjeIt == countMap.end()){
        cerr << "Document::topologicalSort: cyclic dependency detected (no root object)" << endl;
        return ret;
    }

    while (rootObjeIt != countMap.end()){
        rootObjeIt->second = rootObjeIt->second - 1;

        //we need outlist with unique entries
        auto out = rootObjeIt->first->getOutList();
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());

        for (auto outListIt : out) {
            auto outListMapIt = countMap.find(outListIt);
            if (outListMapIt != countMap.end())
                outListMapIt->second = outListMapIt->second - 1;
        }
        ret.push_back(rootObjeIt->first);

        rootObjeIt = find_if(countMap.begin(), countMap.end(), [](pair < App::DocumentObject*, int > count)->bool {
            return count.second == 0;
        });
    }

    return ret;
}

std::vector<App::DocumentObject*> Document::topologicalSort() const
{
    return d->topologicalSort(d->objectArray);
}

const char * Document::getErrorDescription(const App::DocumentObject*Obj) const
{
    return d->findRecomputeLog(Obj);
}

void Document::setErrorDescription(App::DocumentObject *Obj, const char *msg)
{
    if (msg && msg[0] && Obj)
        d->addRecomputeLog(msg, Obj);
}

void Document::setErrorDescription(App::Property *Prop, const char *msg)
{
    if (msg && msg[0] && Prop) {
        auto obj = Base::freecad_dynamic_cast<DocumentObject>(Prop->getContainer());
        if (obj)
            d->addRecomputeLog(msg, obj);
    }
}

// call the recompute of the Feature and handle the exceptions and errors.
int Document::_recomputeFeature(DocumentObject* Feat)
{
    DocumentObjectExecReturn  *returnCode = DocumentObject::StdReturn;

    // delete recompute log
    d->clearRecomputeLog(Feat);

    try {
        returnCode = Feat->ExpressionEngine.execute(PropertyExpressionEngine::ExecuteNonOutput);
        if (returnCode == DocumentObject::StdReturn) {
            bool doRecompute = Feat->isError() || Feat->_enforceRecompute
                                               || !DocumentParams::getOptimizeRecompute()
                                               || testStatus(Status::Restoring);
            if(!doRecompute) {
                static unsigned long long mask = (1<<Property::Output)
                                               | (1<<Property::PropOutput)
                                               | (1<<Property::NoRecompute)
                                               | (1<<Property::PropNoRecompute);
                auto prop = Feat->testPropertyStatus(Property::Touched, mask);
                if(prop) {
                    FC_LOG("recompute on touched " << prop->getFullName());
                    doRecompute = true;
                }
            }

            if(!doRecompute && Feat->skipRecompute()) {
                d->skippedObjs.push_back(Feat);
                FC_LOG("Skip recomputing " << Feat->getFullName());
            } else {
                Feat->_enforceRecompute = false;
                returnCode = Feat->recompute();
            }

            if(returnCode == DocumentObject::StdReturn)
                returnCode = Feat->ExpressionEngine.execute(PropertyExpressionEngine::ExecuteOutput);
        }
    }
    catch(Base::AbortException &e){
        FC_LOG("Failed to recompute " << Feat->getFullName() << ": " << e.what());
        d->addRecomputeLog("User abort",Feat);
        throw;
    }
    catch (const Base::MemoryException& e) {
        FC_ERR("Memory exception in " << Feat->getFullName() << " thrown: " << e.what());
        d->addRecomputeLog("Out of memory exception",Feat);
        return 1;
    }
    catch (Base::Exception &e) {
        e.ReportException();
        FC_LOG("Failed to recompute " << Feat->getFullName() << ": " << e.what());
        d->addRecomputeLog(e.what(),Feat);
        return 1;
    }
    catch (std::exception &e) {
        FC_ERR("exception in " << Feat->getFullName() << " thrown: " << e.what());
        d->addRecomputeLog(e.what(),Feat);
        return 1;
    }
#ifndef FC_DEBUG
    catch (...) {
        FC_ERR("Unknown exception in " << Feat->getFullName() << " thrown");
        d->addRecomputeLog("Unknown exception!",Feat);
        return 1;
    }
#endif

    if (returnCode == DocumentObject::StdReturn) {
        Feat->resetError();
    }
    else {
        returnCode->Which = Feat;
        d->addRecomputeLog(returnCode);
        FC_ERR("Failed to recompute " << Feat->getFullName() << ": " << returnCode->Why);
        return 1;
    }
    return 0;
}

bool Document::recomputeFeature(DocumentObject* Feat, bool recursive)
{
    // verify that the feature is (active) part of the document
    if (Feat->getNameInDocument()) {
        if(recursive) {
            bool hasError = false;
            recompute({Feat},true,&hasError);
            return !hasError;
        } else {
            _recomputeFeature(Feat);
            signalRecomputedObject(*Feat);
            GetApplication().signalRecomputedObject(*this, *Feat);
            return Feat->isValid();
        }
    }else
        return false;
}

DocumentObject * Document::addObject(const char* sType, const char* pObjectName,
                                     bool isNew, const char* viewType, bool isPartial)
{
    Base::Type type = Base::Type::getTypeIfDerivedFrom(sType, App::DocumentObject::getClassTypeId(), true);
    if (type.isBad()) {
        std::stringstream str;
        str << "'" << sType << "' is not a document object type";
        throw Base::TypeError(str.str());
    }

    void* typeInstance = type.createInstance();
    if (!typeInstance)
        return nullptr;

    App::DocumentObject* pcObject = static_cast<App::DocumentObject*>(typeInstance);

    pcObject->setDocument(this);

    // do no transactions if we do a rollback!
    if (!d->rollback) {
        // Undo stuff
        _checkTransaction(nullptr,nullptr,__LINE__);
        if (d->activeUndoTransaction)
            d->activeUndoTransaction->addObjectDel(pcObject);
    }

    // get Unique name
    string ObjectName;

    if (pObjectName && pObjectName[0] != '\0')
        ObjectName = getUniqueObjectName(pObjectName);
    else
        ObjectName = getUniqueObjectName(sType);


    d->activeObject = pcObject;

    // insert in the name map
    d->objectMap[ObjectName] = pcObject;
    // generate object id and add to id map;
    pcObject->_Id = d->addObject(pcObject);
    // cache the pointer to the name string in the Object (for performance of DocumentObject::getNameInDocument())
    pcObject->pcNameInDocument = &(d->objectMap.find(ObjectName)->first);

    // If we are restoring, don't set the Label object now; it will be restored later. This is to avoid potential duplicate
    // label conflicts later.
    if (!d->StatusBits.test(Restoring))
        pcObject->Label.setValue( ObjectName );

    // Call the object-specific initialization
    if (!d->undoing && !d->rollback && isNew) {
        pcObject->TreeRank.setValue(treeRanks().second + 1);
        pcObject->setupObject ();
    }

    // mark the object as new (i.e. set status bit 2) and send the signal
    pcObject->setStatus(ObjectStatus::New, true);

    pcObject->setStatus(ObjectStatus::PartialObject, isPartial);

    if (!viewType || viewType[0] == '\0')
        viewType = pcObject->getViewProviderNameOverride();

    if (viewType && viewType[0] != '\0')
        pcObject->_pcViewProviderName = viewType;

    signalNewObject(*pcObject);

    // do no transactions if we do a rollback!
    if (!d->rollback && d->activeUndoTransaction) {
        signalTransactionAppend(*pcObject, d->activeUndoTransaction);
    }

    // The new object may create object by itself, so set the active object
    // again, before signaling.
    d->activeObject = pcObject;
    signalActivatedObject(*pcObject);

    // return the Object
    return pcObject;
}

std::vector<DocumentObject *> Document::addObjects(const char* sType, const std::vector<std::string>& objectNames, bool isNew)
{
    Base::Type type = Base::Type::getTypeIfDerivedFrom(sType, App::DocumentObject::getClassTypeId(), true);
    if (type.isBad()) {
        std::stringstream str;
        str << "'" << sType << "' is not a document object type";
        throw Base::TypeError(str.str());
    }

    std::vector<DocumentObject *> objects;
    objects.resize(objectNames.size());
    std::generate(objects.begin(), objects.end(),
                  [&]{ return static_cast<App::DocumentObject*>(type.createInstance()); });
    // the type instance could be a null pointer, it is enough to check the first element
    if (!objects.empty() && !objects[0]) {
        objects.clear();
        return objects;
    }

    // get all existing object names
    std::vector<std::string> reservedNames;
    reservedNames.reserve(d->objectMap.size());
    for (auto pos = d->objectMap.begin();pos != d->objectMap.end();++pos) {
        reservedNames.push_back(pos->first);
    }

    for (auto it = objects.begin(); it != objects.end(); ++it) {
        auto index = std::distance(objects.begin(), it);
        App::DocumentObject* pcObject = *it;
        pcObject->setDocument(this);

        // do no transactions if we do a rollback!
        if (!d->rollback) {
            // Undo stuff
            _checkTransaction(nullptr,nullptr,__LINE__);
            if (d->activeUndoTransaction) {
                d->activeUndoTransaction->addObjectDel(pcObject);
            }
        }

        // get unique name
        std::string ObjectName = objectNames[index];
        if (ObjectName.empty())
            ObjectName = sType;
        ObjectName = Base::Tools::getIdentifier(ObjectName);
        if (d->objectMap.find(ObjectName) != d->objectMap.end()) {
            // remove also trailing digits from clean name which is to avoid to create lengthy names
            // like 'Box001001'
            if (!testStatus(KeepTrailingDigits)) {
                std::string::size_type index = ObjectName.find_last_not_of("0123456789");
                if (index+1 < ObjectName.size()) {
                    ObjectName = ObjectName.substr(0,index+1);
                }
            }

            ObjectName = Base::Tools::getUniqueName(ObjectName, reservedNames, 3);
        }

        reservedNames.push_back(ObjectName);

        // insert in the name map
        d->objectMap[ObjectName] = pcObject;
        // generate object id and add to id map;
        pcObject->_Id = d->addObject(pcObject);
        // cache the pointer to the name string in the Object (for performance of DocumentObject::getNameInDocument())
        pcObject->pcNameInDocument = &(d->objectMap.find(ObjectName)->first);

        pcObject->Label.setValue(ObjectName);

        // Call the object-specific initialization
        if (!d->undoing && !d->rollback && isNew) {
            pcObject->TreeRank.setValue(treeRanks().second + 1);
            pcObject->setupObject();
        }

        // mark the object as new (i.e. set status bit 2) and send the signal
        pcObject->setStatus(ObjectStatus::New, true);

        const char *viewType = pcObject->getViewProviderNameOverride();
        pcObject->_pcViewProviderName = viewType ? viewType : "";

        signalNewObject(*pcObject);

        // do no transactions if we do a rollback!
        if (!d->rollback && d->activeUndoTransaction) {
            signalTransactionAppend(*pcObject, d->activeUndoTransaction);
        }
    }

    if (!objects.empty()) {
        d->activeObject = objects.back();
        signalActivatedObject(*objects.back());
    }

    return objects;
}

void Document::addObject(DocumentObject* pcObject, const char* pObjectName, bool activate)
{
    if (pcObject->getDocument()) {
        throw Base::RuntimeError("Document object is already added to a document");
    }

    pcObject->setDocument(this);

    // do no transactions if we do a rollback!
    if (!d->rollback) {
        // Undo stuff
        _checkTransaction(nullptr,nullptr,__LINE__);
        if (d->activeUndoTransaction)
            d->activeUndoTransaction->addObjectDel(pcObject);
    }

    // get unique name
    string ObjectName;
    if (pObjectName && pObjectName[0] != '\0')
        ObjectName = getUniqueObjectName(pObjectName);
    else
        ObjectName = getUniqueObjectName(pcObject->getTypeId().getName());

    if (activate)
        d->activeObject = pcObject;

    // insert in the name map
    d->objectMap[ObjectName] = pcObject;
    // generate object id and add to id map;
    pcObject->_Id = d->addObject(pcObject);
    // cache the pointer to the name string in the Object (for performance of DocumentObject::getNameInDocument())
    pcObject->pcNameInDocument = &(d->objectMap.find(ObjectName)->first);

    pcObject->Label.setValue( ObjectName );

    // mark the object as new (i.e. set status bit 2) and send the signal
    pcObject->setStatus(ObjectStatus::New, true);

    const char *viewType = pcObject->getViewProviderNameOverride();
    pcObject->_pcViewProviderName = viewType ? viewType : "";

    signalNewObject(*pcObject);

    // do no transactions if we do a rollback!
    if (!d->rollback && d->activeUndoTransaction) {
        signalTransactionAppend(*pcObject, d->activeUndoTransaction);
    }

    if (activate)
        d->activeObject = pcObject;
    signalActivatedObject(*pcObject);
}

void Document::_addObject(DocumentObject* pcObject, const char* pObjectName)
{
    std::string ObjectName = getUniqueObjectName(pObjectName);
    d->objectMap[ObjectName] = pcObject;
    // generate object id and add to id map;
    pcObject->_Id = d->addObject(pcObject);
    // cache the pointer to the name string in the Object (for performance of DocumentObject::getNameInDocument())
    pcObject->pcNameInDocument = &(d->objectMap.find(ObjectName)->first);

    // do no transactions if we do a rollback!
    if (!d->rollback) {
        // Undo stuff
        _checkTransaction(nullptr,nullptr,__LINE__);
        if (d->activeUndoTransaction)
            d->activeUndoTransaction->addObjectDel(pcObject);
    }

    const char *viewType = pcObject->getViewProviderNameOverride();
    pcObject->_pcViewProviderName = viewType ? viewType : "";

    // send the signal
    signalNewObject(*pcObject);

    // do no transactions if we do a rollback!
    if (!d->rollback && d->activeUndoTransaction) {
        signalTransactionAppend(*pcObject, d->activeUndoTransaction);
    }

    d->activeObject = pcObject;
    signalActivatedObject(*pcObject);
}

/// Remove an object out of the document
void Document::removeObject(const char* sName)
{
    auto pos = d->objectMap.find(sName);

    // name not found?
    if (pos == d->objectMap.end())
        return;

    if (pos->second->testStatus(ObjectStatus::PendingRecompute)) {
        FC_LOG("pending remove of recomputing object " << pos->second->getFullName());
        d->pendingRemove.emplace_back(pos->second);
        return;
    }

    if (pos->second->testStatus(ObjectStatus::ObjEditing)) {
        FC_LOG("pending remove of editing object " << pos->second->getFullName());
        d->pendingRemove.emplace_back(pos->second);
        return;
    }

    TransactionLocker tlock;

    _checkTransaction(pos->second,nullptr,__LINE__);

    if (d->activeObject == pos->second)
        d->activeObject = nullptr;

    // Mark the object as about to be deleted
    pos->second->setStatus(ObjectStatus::Remove, true);
    if (!d->undoing && !d->rollback) {
        pos->second->unsetupObject();
    }

    signalDeletedObject(*(pos->second));

    // do no transactions if we do a rollback!
    if (!d->rollback && d->activeUndoTransaction) {
        // in this case transaction delete or save the object
        signalTransactionRemove(*pos->second, d->activeUndoTransaction);
    }
    else {
        // if not saved in undo -> delete object
        signalTransactionRemove(*pos->second, 0);
    }

#ifdef USE_OLD_DAG
    if (!d->vertexMap.empty()) {
        // recompute of document is running
        for (std::map<Vertex,DocumentObject*>::iterator it = d->vertexMap.begin(); it != d->vertexMap.end(); ++it) {
            if (it->second == pos->second) {
                it->second = 0; // just nullify the pointer
                break;
            }
        }
    }
#endif //USE_OLD_DAG

    // Before deleting we must nullify all dependent objects
    breakDependency(pos->second, true);

    //and remove the tip if needed
    if (Tip.getValue() && strcmp(Tip.getValue()->getNameInDocument(), sName)==0) {
        Tip.setValue(nullptr);
        TipName.setValue("");
    }

    // remove the ID before possibly deleting the object
    d->objectIdMap.erase(pos->second->_Id);
    // Unset the bit to be on the safe side
    pos->second->setStatus(ObjectStatus::Remove, false);

    // do no transactions if we do a rollback!
    std::unique_ptr<DocumentObject> tobedestroyed;
    if (!d->rollback) {
        // Undo stuff
        if (d->activeUndoTransaction) {
            // in this case transaction delete or save the object
            d->activeUndoTransaction->addObjectNew(pos->second);
        }
        else {
            // if not saved in undo -> delete object later
            std::unique_ptr<DocumentObject> delobj(pos->second);
            tobedestroyed.swap(delobj);
            tobedestroyed->setStatus(ObjectStatus::Destroy, true);
        }
    }

    for (std::vector<DocumentObject*>::iterator obj = d->objectArray.begin(); obj != d->objectArray.end(); ++obj) {
        if (*obj == pos->second) {
            d->objectArray.erase(obj);
            break;
        }
    }

    // In case the object gets deleted the pointer must be nullified
    if (tobedestroyed) {
        tobedestroyed->pcNameInDocument = nullptr;
    }
    d->objectMap.erase(pos);
    ++d->revision;
}

/// Remove an object out of the document (internal)
void Document::_removeObject(DocumentObject* pcObject)
{
    if (pcObject->testStatus(ObjectStatus::PendingRecompute)) {
        FC_LOG("pending remove of recomputing object " << pcObject->getFullName());
        d->pendingRemove.emplace_back(pcObject);
        return;
    }

#if 0 // Allow deleting editing object when undo/redo

    if (pcObject->testStatus(ObjectStatus::ObjEditing)) {
        FC_LOG("pending remove of editing object " << pcObject->getFullName());
        d->pendingRemove.emplace_back(pcObject);
        return;
    }
#endif

    TransactionLocker tlock;

    // TODO Refactoring: share code with Document::removeObject() (2015-09-01, Fat-Zer)
    _checkTransaction(pcObject,nullptr,__LINE__);

    auto pos = d->objectMap.find(pcObject->getNameInDocument());

    if (d->activeObject == pcObject)
        d->activeObject = nullptr;

    // Mark the object as about to be removed
    pcObject->setStatus(ObjectStatus::Remove, true);
    if (!d->undoing && !d->rollback) {
        pcObject->unsetupObject();
    }
    signalDeletedObject(*pcObject);
    // TODO Check me if it's needed (2015-09-01, Fat-Zer)

    //remove the tip if needed
    if (Tip.getValue() == pcObject) {
        Tip.setValue(nullptr);
        TipName.setValue("");
    }

    // do no transactions if we do a rollback!
    if (!d->rollback && d->activeUndoTransaction) {
        // Undo stuff
        signalTransactionRemove(*pcObject, d->activeUndoTransaction);
        breakDependency(pcObject, true);
        d->activeUndoTransaction->addObjectNew(pcObject);
    }
    else {
        // for a rollback delete the object
        signalTransactionRemove(*pcObject, 0);

        breakDependency(pcObject, true);
    }

    // remove from map
    pcObject->setStatus(ObjectStatus::Remove, false); // Unset the bit to be on the safe side
    d->objectIdMap.erase(pcObject->_Id);
    ++d->revision;
    for (std::vector<DocumentObject*>::iterator it = d->objectArray.begin(); it != d->objectArray.end(); ++it) {
        if (*it == pcObject) {
            d->objectArray.erase(it);
            break;
        }
    }

    // for a rollback delete the object
    if (d->rollback) {
        pcObject->setStatus(ObjectStatus::Destroy, true);
        if (!TransactionGuard::addPendingRemove(pcObject))
            delete pcObject;
    }

    // TransactionGuard::addPendingRemove() will call
    // DocumentObject::detachDocument() which will access its name in document.
    // So we have to delay removing object from objectMap, because the name is
    // stored in the map.
    d->objectMap.erase(pos);
}

static bool _RemovingObjects;
static int _RemovingObject;
static std::unordered_map<Property*, int> _PendingProps;
static int _PendingPropIndex;

bool Document::isRemoving(Property *prop)
{
    if (!prop || _RemovingObject == 0)
        return false;
    if (auto obj = Base::freecad_dynamic_cast<DocumentObject>(prop->getContainer())) {
        if (!obj->testStatus(App::Remove))
            _PendingProps.insert(std::make_pair(prop, _PendingPropIndex++));
    }
    return true;
}

void Document::removePendingProperty(Property *prop)
{
    _PendingProps.erase(prop);
}

void Document::removeObjects(const std::vector<std::string> &objs)
{
    if (_RemovingObjects) {
        FC_ERR("recursive calling of Document.removeObjects()");
        return;
    }

    Base::StateLocker guard(_RemovingObjects);

    ++_RemovingObject;

    for (const auto &name : objs)
        removeObject(name.c_str());

    if (--_RemovingObject == 0) {
        std::vector<std::pair<Property*,int> > props;
        props.reserve(_PendingProps.size());
        props.insert(props.end(),_PendingProps.begin(),_PendingProps.end());
        std::sort(props.begin(), props.end(),
            [](const std::pair<Property*,int> &a, const std::pair<Property*,int> &b) {
                return a.second < b.second;
            });

        std::string errMsg;
        for(auto &v : props) {
            auto prop = v.first;
            // double check if the property exists, because it may be removed
            // while we are looping.
            if(_PendingProps.count(prop)) {
                Base::exceptionSafeCall(errMsg, [](Property *prop){prop->touch();}, prop);
                if(errMsg.size()) {
                    FC_ERR("Exception on post object removal "
                            << prop->getFullName() << ": " << errMsg);
                    errMsg.clear();
                }
            }
        }
        _PendingProps.clear();
        _PendingPropIndex = 0;
    }
}

void Document::breakDependency(DocumentObject* pcObject, bool clear)
{
    // Nullify all dependent objects
    PropertyLinkBase::breakLinks(pcObject,d->objectArray,clear);
}

std::vector<DocumentObject*> Document::copyObject(
    const std::vector<DocumentObject*> &objs, bool recursive, bool returnAll)
{
    std::vector<DocumentObject*> deps;
    if(!recursive)
        deps = objs;
    else
        deps = getDependencyList(objs,DepNoXLinked|DepSort);

    if (!testStatus(TempDoc) && !isSaved() && PropertyXLink::hasXLink(deps)) {
        throw Base::RuntimeError(
                "Document must be saved at least once before link to external objects");
    }

    MergeDocuments md(this);
    // if not copying recursively then suppress possible warnings
    md.setVerbose(recursive);

    unsigned int memsize=1000; // ~ for the meta-information
    for (std::vector<App::DocumentObject*>::iterator it = deps.begin(); it != deps.end(); ++it)
        memsize += (*it)->getMemSize();

    // if less than ~10 MB
    bool use_buffer=(memsize < 0xA00000);
    QByteArray res;
    try {
        res.reserve(memsize);
    }
    catch (const Base::MemoryException&) {
        use_buffer = false;
    }

    std::vector<App::DocumentObject*> imported;
    if (use_buffer) {
        Base::ByteArrayOStreambuf obuf(res);
        std::ostream ostr(&obuf);
        exportObjects(deps, ostr);

        Base::ByteArrayIStreambuf ibuf(res);
        std::istream istr(nullptr);
        istr.rdbuf(&ibuf);
        imported = md.importObjects(istr);
    } else {
        static Base::FileInfo fi(App::Application::getTempFileName());
        Base::ofstream ostr(fi, std::ios::out | std::ios::binary);
        exportObjects(deps, ostr);
        ostr.close();

        Base::ifstream istr(fi, std::ios::in | std::ios::binary);
        imported = md.importObjects(istr);
    }

    if (returnAll || imported.size()!=deps.size())
        return imported;

    std::unordered_map<App::DocumentObject*,size_t> indices;
    size_t i=0;
    for(auto o : deps)
        indices[o] = i++;
    std::vector<App::DocumentObject*> result;
    result.reserve(objs.size());
    for(auto o : objs)
        result.push_back(imported[indices[o]]);
    return result;
}

std::vector<App::DocumentObject*>
Document::importLinks(const std::vector<App::DocumentObject*> &objArray)
{
    std::set<App::DocumentObject*> links;
    getLinksTo(links,nullptr,GetLinkExternal,0,objArray);

    std::vector<App::DocumentObject*> objs;
    objs.insert(objs.end(),links.begin(),links.end());
    objs = App::Document::getDependencyList(objs);
    if(objs.empty()) {
        FC_ERR("nothing to import");
        return objs;
    }

    for(auto it=objs.begin();it!=objs.end();) {
        auto obj = *it;
        if(obj->getDocument() == this) {
            it = objs.erase(it);
            continue;
        }
        ++it;
        if(obj->testStatus(App::PartialObject)) {
            throw Base::RuntimeError(
                "Cannot import partial loaded object. Please reload the current document");
        }
    }

    Base::FileInfo fi(App::Application::getTempFileName());
    {
        // save stuff to temp file
        Base::ofstream str(fi, std::ios::out | std::ios::binary);
        MergeDocuments mimeView(this);
        exportObjects(objs, str);
        str.close();
    }
    Base::ifstream str(fi, std::ios::in | std::ios::binary);
    MergeDocuments mimeView(this);
    objs = mimeView.importObjects(str);
    str.close();
    fi.deleteFile();

    const auto &nameMap = mimeView.getNameMap();

    // First, find all link type properties that needs to be changed
    std::map<App::Property*,std::unique_ptr<App::Property> > propMap;
    std::vector<App::Property*> propList;
    for(auto obj : links) {
        propList.clear();
        obj->getPropertyList(propList);
        for(auto prop : propList) {
            auto linkProp = Base::freecad_dynamic_cast<PropertyLinkBase>(prop);
            if(linkProp && !prop->testStatus(Property::Immutable) && !obj->isReadOnly(prop)) {
                auto copy = linkProp->CopyOnImportExternal(nameMap);
                if(copy)
                    propMap[linkProp].reset(copy);
            }
        }
    }

    // Then change them in one go. Note that we don't make change in previous
    // loop, because a changed link property may break other depending link
    // properties, e.g. a link sub referring to some sub object of an xlink, If
    // that sub object is imported with a different name, and xlink is changed
    // before this link sub, it will break.
    for(auto &v : propMap)
        v.first->Paste(*v.second);

    return objs;
}

DocumentObject* Document::moveObject(DocumentObject* obj, bool recursive)
{
    if(!obj)
        return nullptr;
    Document* that = obj->getDocument();
    if (that == this) {
        auto ranks = treeRanks();
        if (obj->TreeRank.getValue() != ranks.second)
            obj->TreeRank.setValue(ranks.second+1);
        return nullptr; // nothing todo
    }

    // True object move without copy is only safe when undo is off on both
    // documents.
    if(!recursive && !d->iUndoMode && !that->d->iUndoMode && !that->d->rollback) {
        // all object of the other document that refer to this object must be nullified
        that->breakDependency(obj, false);
        std::string objname = getUniqueObjectName(obj->getNameInDocument());
        that->_removeObject(obj);
        this->_addObject(obj, objname.c_str());
        obj->setDocument(this);
        return obj;
    }

    std::vector<App::DocumentObject*> deps;
    if(recursive)
        deps = getDependencyList({obj},DepNoXLinked|DepSort);
    else
        deps.push_back(obj);

    auto objs = copyObject(deps,false);
    if(objs.empty())
        return nullptr;
    // Some object may delete its children if deleted, so we collect the IDs
    // or all depending objects for safety reason.
    std::vector<int> ids;
    ids.reserve(deps.size());
    for(auto o : deps)
        ids.push_back(o->getID());

    // We only remove object if it is the moving object or it has no
    // depending objects, i.e. an empty inList, which is why we need to
    // iterate the depending list backwards.
    for(auto iter=ids.rbegin();iter!=ids.rend();++iter) {
        auto o = that->getObjectByID(*iter);
        if(!o) continue;
        if(iter==ids.rbegin()
                || o->getInList().empty())
            that->removeObject(o->getNameInDocument());
    }
    return objs.back();
}

DocumentObject * Document::getActiveObject() const
{
    return d->activeObject;
}

DocumentObject * Document::getObject(const char *Name) const
{
    auto pos = d->objectMap.find(Name);

    if (pos != d->objectMap.end())
        return pos->second;
    else
        return nullptr;
}

DocumentObject * Document::getObjectByID(long id) const
{
    auto it = d->objectIdMap.find(id);
    if(it!=d->objectIdMap.end())
        return it->second;
    return nullptr;
}


// Note: This method is only used in Tree.cpp slotChangeObject(), see explanation there
bool Document::isIn(const DocumentObject *pFeat) const
{
    for (auto o = d->objectMap.begin(); o != d->objectMap.end(); ++o) {
        if (o->second == pFeat)
            return true;
    }

    return false;
}

const char * Document::getObjectName(DocumentObject *pFeat) const
{
    for (auto pos = d->objectMap.begin();pos != d->objectMap.end();++pos) {
        if (pos->second == pFeat)
            return pos->first.c_str();
    }

    return nullptr;
}

std::string Document::getUniqueObjectName(const char *Name) const
{
    if (!Name || *Name == '\0')
        return std::string();
    std::string CleanName = Base::Tools::getIdentifier(Name);

    // name in use?
    auto pos = d->objectMap.find(CleanName);

    if (pos == d->objectMap.end()) {
        // if not, name is OK
        return CleanName;
    }
    else {
        // remove also trailing digits from clean name which is to avoid to create lengthy names
        // like 'Box001001'
        if (!testStatus(KeepTrailingDigits)) {
            std::string::size_type index = CleanName.find_last_not_of("0123456789");
            if (index+1 < CleanName.size()) {
                CleanName = CleanName.substr(0,index+1);
            }
        }

        std::vector<std::string> names;
        names.reserve(d->objectMap.size());
        for (pos = d->objectMap.begin();pos != d->objectMap.end();++pos) {
            names.push_back(pos->first);
        }
        return Base::Tools::getUniqueName(CleanName, names, 3);
    }
}

std::string Document::getStandardObjectName(const char *Name, int d) const
{
    std::vector<App::DocumentObject*> mm = getObjects();
    std::vector<std::string> labels;
    labels.reserve(mm.size());

    for (std::vector<App::DocumentObject*>::const_iterator it = mm.begin(); it != mm.end(); ++it) {
        std::string label = (*it)->Label.getValue();
        labels.push_back(label);
    }
    return Base::Tools::getUniqueName(Name, labels, d);
}

std::vector<DocumentObject*> Document::getDependingObjects() const
{
    return getDependencyList(d->objectArray);
}

const std::vector<DocumentObject*> &Document::getObjects() const
{
    return d->objectArray;
}


std::vector<DocumentObject*> Document::getObjectsOfType(const Base::Type& typeId) const
{
    std::vector<DocumentObject*> Objects;
    for (std::vector<DocumentObject*>::const_iterator it = d->objectArray.begin(); it != d->objectArray.end(); ++it) {
        if ((*it)->getTypeId().isDerivedFrom(typeId))
            Objects.push_back(*it);
    }
    return Objects;
}

std::vector< DocumentObject* > Document::getObjectsWithExtension(const Base::Type& typeId, bool derived) const {

    std::vector<DocumentObject*> Objects;
    for (std::vector<DocumentObject*>::const_iterator it = d->objectArray.begin(); it != d->objectArray.end(); ++it) {
        if ((*it)->hasExtension(typeId, derived))
            Objects.push_back(*it);
    }
    return Objects;
}


std::vector<DocumentObject*> Document::findObjects(const Base::Type& typeId, const char* objname, const char* label) const
{
    boost::cmatch what;
    boost::regex rx_name, rx_label;

    if (objname)
        rx_name.set_expression(objname);

    if (label)
        rx_label.set_expression(label);

    std::vector<DocumentObject*> Objects;
    DocumentObject* found = nullptr;
    for (std::vector<DocumentObject*>::const_iterator it = d->objectArray.begin(); it != d->objectArray.end(); ++it) {
        if ((*it)->getTypeId().isDerivedFrom(typeId)) {
            found = *it;

            if (!rx_name.empty() && !boost::regex_search((*it)->getNameInDocument(), what, rx_name))
                found = nullptr;

            if (!rx_label.empty() && !boost::regex_search((*it)->Label.getValue(), what, rx_label))
                found = nullptr;

            if (found)
                Objects.push_back(found);
        }
    }
    return Objects;
}

int Document::countObjectsOfType(const Base::Type& typeId) const
{
    int ct=0;
    for (auto it = d->objectMap.begin(); it != d->objectMap.end(); ++it) {
        if (it->second->getTypeId().isDerivedFrom(typeId))
            ct++;
    }

    return ct;
}

PyObject * Document::getPyObject()
{
    return Py::new_reference_to(d->DocumentPythonObject);
}

std::vector<App::DocumentObject*> Document::getRootObjects() const
{
    std::vector < App::DocumentObject* > ret;

    for (auto objectIt : d->objectArray) {
        if (objectIt->getInList().empty())
            ret.push_back(objectIt);
    }

    return ret;
}

void DocumentP::findAllPathsAt(const std::vector <Node> &all_nodes, size_t id,
                                std::vector <Path> &all_paths, Path tmp)
{
    if (std::find(tmp.begin(), tmp.end(), id) != tmp.end()) {
        Path tmp2(tmp);
        tmp2.push_back(id);
        all_paths.push_back(tmp2);
        return; // a cycle
    }

    tmp.push_back(id);
    if (all_nodes[id].empty()) {
        all_paths.push_back(tmp);
        return;
    }

    for (size_t i=0; i < all_nodes[id].size(); i++) {
        Path tmp2(tmp);
        findAllPathsAt(all_nodes, all_nodes[id][i], all_paths, tmp2);
    }
}

std::vector<std::list<App::DocumentObject*> >
Document::getPathsByOutList(const App::DocumentObject* from, const App::DocumentObject* to) const
{
    std::map<const DocumentObject*, size_t> indexMap;
    for (size_t i=0; i<d->objectArray.size(); ++i) {
        indexMap[d->objectArray[i]] = i;
    }

    std::vector <Node> all_nodes(d->objectArray.size());
    for (size_t i=0; i<d->objectArray.size(); ++i) {
        DocumentObject* obj = d->objectArray[i];
        std::vector<DocumentObject*> outList = obj->getOutList();
        for (auto it : outList) {
            all_nodes[i].push_back(indexMap[it]);
        }
    }

    std::vector<std::list<App::DocumentObject*> > array;
    if (from == to)
        return array;

    size_t index_from = indexMap[from];
    size_t index_to = indexMap[to];
    Path tmp;
    std::vector<Path> all_paths;
    DocumentP::findAllPathsAt(all_nodes, index_from, all_paths, tmp);

    for (std::vector<Path>::iterator it = all_paths.begin(); it != all_paths.end(); ++it) {
        Path::iterator jt = std::find(it->begin(), it->end(), index_to);
        if (jt != it->end()) {
            std::list<App::DocumentObject*> path;
            for (Path::iterator kt = it->begin(); kt != jt; ++kt) {
                path.push_back(d->objectArray[*kt]);
            }

            path.push_back(d->objectArray[*jt]);
            array.push_back(path);
        }
    }

    // remove duplicates
    std::sort(array.begin(), array.end());
    array.erase(std::unique(array.begin(), array.end()), array.end());

    return array;
}

bool Document::mustExecute() const
{
    if(PropertyXLink::hasXLink(this)) {
        bool touched = false;
        _buildDependencyList(d->objectArray,false,nullptr,nullptr,nullptr,&touched);
        return touched;
    }

    for (std::vector<DocumentObject*>::const_iterator It = d->objectArray.begin();It != d->objectArray.end();++It)
        if ((*It)->isTouched() || (*It)->mustExecute()==1)
            return true;
    return false;
}

long Document::getLastObjectId() const {
    return d->lastObjectId;
}

void Document::setLastObjectId(long id) {
    d->lastObjectId = id;
}

void Document::afterImport(App::DocumentObject *obj) {
    if (obj->Label.getStrValue() == "Unnamed")
        obj->Label.setValue(obj->getNameInDocument());
    obj->onDocumentRestored();
}

std::pair<long, long> Document::treeRanks() const
{
    if (d->objectArray.empty())
        return std::make_pair(0,0);
    if (d->treeRankRevision != d->revision) {
        d->treeRankRevision = d->revision;
        d->treeRanks.second = d->treeRanks.first = d->objectArray.front()->TreeRank.getValue();
        for (auto obj : d->objectArray) {
            long r = obj->TreeRank.getValue();
            if (r < d->treeRanks.first)
                d->treeRanks.first = r;
            else if (r > d->treeRanks.second)
                d->treeRanks.second = r;
        }
    }
    return d->treeRanks;
}

void Document::reorderObjects(const std::vector<DocumentObject*> &_objs, DocumentObject *before)
{
    const char *msg = "Object does not belong to this document";
    if (!before || before->getDocument() != this)
        throw Base::RuntimeError(msg);
        
    for (auto obj : _objs) {
        if (!obj || obj->getDocument() != this)
            throw Base::RuntimeError(msg);
    }
    auto objs = _objs;
    objs.erase(std::unique(objs.begin(), objs.end()), objs.end());
    long beforeRank = before->TreeRank.getValue();
    for (auto obj : d->objectArray) {
        long rank = obj->TreeRank.getValue();
        if (rank >= beforeRank)
            obj->TreeRank.setValue(rank + (long)objs.size());
    }
    for (auto obj : objs)
        obj->TreeRank.setValue(beforeRank++);
}
