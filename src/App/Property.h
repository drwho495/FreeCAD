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


#ifndef APP_PROPERTY_H
#define APP_PROPERTY_H

#include <Base/Exception.h>
#include <Base/Persistence.h>
#include <Base/Stream.h>
#if 0
#   include <boost/any.hpp>
#else
#   include "stx/any.hpp"
#endif
#include <boost/signals2.hpp>
#include <bitset>
#include <string>
#include <FCGlobal.h>

// WARNING! define this to static thread_local if FreeCAD ever decides to use
// multi-threading recomputation.
#define FC_STATIC static

namespace Py {
class Object;
}

namespace App
{

class PropertyContainer;
class ObjectIdentifier;

/** Base class of all properties
 * This is the father of all properties. Properties are objects which are used
 * in the document tree to parametrize e.g. features and their graphical output.
 * They are also used to gain access from the scripting facility.
 * /par
 * This abstract base class defines all methods shared by all
 * possible properties. It is also possible to define user properties
 * and use them in the framework...
 */
class AppExport Property : public Base::Persistence
{
    TYPESYSTEM_HEADER_WITH_OVERRIDE();

public:
    enum Status
    {
        Touched = 0, // touched property
        Immutable = 1, // can't modify property
        ReadOnly = 2, // for property editor
        Hidden = 3, // for property editor
        Transient = 4, // for property container save
        MaterialEdit = 5, // to turn ON PropertyMaterial edit
        NoMaterialListEdit = 6, // to turn OFF PropertyMaterialList edit
        Output = 7, // same effect as Prop_Output
        LockDynamic = 8, // prevent being removed from dynamic property
        NoModify = 9, // prevent causing Gui::Document::setModified()
        PartialTrigger = 10, // allow change in partial doc
        NoRecompute = 11, // touch owner for recompute on property change
        Single = 12, // for save/load of floating point numbers
        Ordered = 13, // for PropertyLists whether the order of the elements is
                      // relevant for the container using it
        EvalOnRestore = 14, // In case of expression binding, evaluate the
                            // expression on restore and touch the object on value change.
        Busy = 15, // internal use to avoid recursive signaling
        CopyOnChange = 16, // for Link to copy the linked object on change of the property with this flag
        UserEdit = 17, // cause property editor to create button for user defined editing

        // The following bits are corresponding to PropertyType set when the
        // property added. These types are meant to be static, and cannot be
        // changed in runtime. It is mirrored here to save the linear search
        // required in PropertyContainer::getPropertyType()
        //
        PropStaticBegin = 21,
        PropDynamic = 21, // indicating the property is dynamically added
        PropNoPersist = 22, // corresponding to Prop_NoPersist
        PropNoRecompute = 23, // corresponding to Prop_NoRecompute
        PropReadOnly = 24, // corresponding to Prop_ReadOnly
        PropTransient= 25, // corresponding to Prop_Transient
        PropHidden   = 26, // corresponding to Prop_Hidden
        PropOutput   = 27, // corresponding to Prop_Output
        PropStaticEnd = 28,

        User1 = 28, // user-defined status
        User2 = 29, // user-defined status
        User3 = 30, // user-defined status
        User4 = 31  // user-defined status
    };

    using StatusBits = std::bitset<32>;

    Property();
    ~Property() override;

    /// For safe deleting of a dynamic property
    static void destroy(Property *p);

    /** This method is used to get the size of objects
     * It is not meant to have the exact size, it is more or less an estimation
     * which runs fast! Is it two bytes or a GB?
     * This method is defined in Base::Persistence
     * @see Base::Persistence
     */
    unsigned int getMemSize () const override {
        // you have to implement this method in all property classes!
        return sizeof(father) + sizeof(StatusBits);
    }

    /** Get the name of this property in the belonging container
     * With \ref hasName() it can be checked beforehand if a valid name is set.
     * @note If no name is set this function returns an empty string, i.e. "".
     */
    const char* getName() const;
    /** Check if the property has a name set.
     * If no name is set then \ref getName() will return an empty string
     */
    bool hasName() const;
    /** Check if the passed name is valid.
     * If \a name is null or an empty string it's considered invalid,
     * and valid otherwise.
     */
    static bool isValidName(const char* name);

    /** Return a fully qualified property name that include its own's name
     * @param python: if true, then return an expression for accessing this property in Python
     */
    virtual std::string getFullName(bool python=false) const;

    /// Get the class name of the associated property editor item
    virtual const char* getEditorName() const { return ""; }

    /// Get the type of the property in the container
    short getType() const;

    /// Get the group of this property
    const char* getGroup() const;

    /// Get the documentation of this property
    const char* getDocumentation() const;

    /// Is called by the framework to set the father (container)
    void setContainer(PropertyContainer *Father);

    /// Get a pointer to the PropertyContainer derived class the property belongs to
    PropertyContainer *getContainer() const {return father;}

    /// Set value of property
    virtual void setPathValue(const App::ObjectIdentifier & path, const App::any & value);

    /// Get value of property
    virtual App::any getPathValue(const App::ObjectIdentifier & path) const;

    /// Get Python value of property
    virtual bool getPyPathValue(const App::ObjectIdentifier &, Py::Object &) const {
        return false;
    }

    /// Set python value of property
    virtual bool setPyPathValue(const App::ObjectIdentifier &, const Py::Object &) {
        return false;
    }

    /// Convert p to a canonical representation of it
    virtual App::ObjectIdentifier canonicalPath(const App::ObjectIdentifier & p) const;

    /// Get valid paths for this property; used by auto completer
    virtual void getPaths(std::vector<App::ObjectIdentifier> & paths) const;

    /** Called at the beginning of Document::afterRestore()
     *
     * This function is called without dependency sorting, because some
     * types of link property can only reconstructs the linking information
     * inside this function.
     *
     * One example use case of this function is PropertyLinkSub that uses
     * afterRestore() to parse and restore subname references, which may
     * contain sub-object reference from external document, and there will be
     * special mapping required during object import.
     *
     * Another example is PropertyExpressionEngine which only parse the
     * restored expression in afterRestore(). The reason, in addition to
     * subname mapping like PropertyLinkSub, is that it can handle document
     * name adjustment as well. It internally relies on PropertyXLink to store
     * the external document path for external linking. When the external
     * document is restored, its internal name may change due to name conflict
     * with existing documents.  PropertyExpressionEngine can now auto adjust
     * external references without any problem.
     */
    virtual void afterRestore() {}

    /** Called before calling DocumentObject::onDocumentRestored()
     *
     * This function is called after finished calling Property::afterRestore()
     * of all properties of objects. By then, the object dependency information
     * is assumed ready. So, unlike Property::afterRestore(), this function is
     * called on objects with dependency order.
     */
    virtual void onContainerRestored() {}

    /** Property status handling
     */
    //@{
    /// Set the property touched
    virtual void touch();
    /// Test if this property is touched
    virtual bool isTouched() const {
        return _StatusBits.test(Touched);
    }
    /// Reset this property touched
    virtual void purgeTouched() {
        _StatusBits.reset(Touched);
    }
    /// return the status bits
    unsigned long getStatus() const;
    bool testStatus(const StatusBits &bits, const StatusBits &mask = StatusBits()) const;
    bool testStatus(Status pos) const;
    void setStatus(Status pos, bool on);
    void setStatusValue(unsigned long status);
    void setStatus(const StatusBits &bits, bool on);
    ///Sets property editable/grayed out in property editor
    void setReadOnly(bool readOnly);
    inline bool isReadOnly() const {
        return testStatus(App::Property::ReadOnly);
    }
    /// Sets precision of properties using floating point
    /// numbers to single, the default is double.
    void setSinglePrecision(bool single) {
        setStatus(App::Property::Single, single);
    }
    /// Gets precision of properties using floating point numbers
    inline bool isSinglePrecision() const {
        return testStatus(App::Property::Single);
    }
    //@}

    /// Returns a new copy of the property (mainly for Undo/Redo and transactions)
    virtual Property *Copy() const = 0;
    /// Paste the value from the property (mainly for Undo/Redo and transactions)
    virtual void Paste(const Property &from) = 0;

    /// Set value by interpolate between two values
    virtual void interpolate(const Property &from, const Property &to, float t) {
        (void)from;
        (void)to;
        (void)t;
        throw Base::NotImplementedError();
    }

    /// Called when a child property has changed value
    virtual void hasSetChildValue(Property &) {}
    /// Called before a child property changing value
    virtual void aboutToSetChildValue(Property &) {}

    /// Compare if this property has the same content as the given one
    virtual bool isSame(const Property &other) const = 0;
    /// Compare property by comparing their XML content
    bool isSameContent(const Property &other) const;
    /** Returns a new copy before change of the property
     *
     * This function is used internally by property to detect changes.
     */
    virtual Property *copyBeforeChange(void) const {return nullptr;}

    virtual void beforeSave() const {}

    void SetRestoreError(const char *) override;

    /** Return a unique ID for the property
     *
     * The ID of a property is generated from a monotonically increasing
     * internal counter. The intention of the ID is to be used as a key for
     * mapping, instead of using the raw pointer. Because, it is possible for
     * the runtime memory allocator to reuse just deleted memory, which will
     * cause hard to debug problem if use pointer as key. 
     */
    int64_t getID() const {return _id;}

    friend class PropertyContainer;
    friend struct PropertyData;
    friend class DynamicProperty;

private:
    /** Status bits of the property
     * The first 8 bits are used for the base system the rest can be used in
     * descendent classes to mark special statuses on the objects.
     * The bits and their meaning are listed below:
     * 0 - object is marked as 'touched'
     * 1 - object is marked as 'immutable'
     * 2 - object is marked as 'read-only' (for property editor)
     * 3 - object is marked as 'hidden' (for property editor)
     */
    StatusBits _StatusBits;

protected:
    /// Gets called by all setValue() methods after the value has changed
    virtual void hasSetValue();
    /// Gets called by all setValue() methods before the value has changed
    virtual void aboutToSetValue();

    /// Verify a path for the current property
    virtual void verifyPath(const App::ObjectIdentifier & p) const;

    /// Return a file name suitable for saving this property
    std::string getFileName(const char *postfix=0, const char *prefix=0) const;

private:
    // forbidden
    Property(const Property&);
    Property& operator = (const Property&);

    // Sync status with Property_Type
    void syncType(unsigned type);

private:
    PropertyContainer *father;
    const char *myName;
    std::unique_ptr<Property> _old;
    int64_t _id;

public:
    boost::signals2::signal<void (const App::Property&)> signalChanged;
};


/** A template class that is used to inhibit multiple nested calls to aboutToSetValue/hasSetValue for properties.
 *
 * A template class that is used to inhibit multiple nested calls to
 * aboutToSetValue/hasSetValue for properties, and only invoke it on change and
 * last time it is needed. This is useful in cases where you want to change multiple
 * values in a property "atomically", using possibly multiple primitive functions
 * that normally would trigger aboutToSetValue/hasSetValue calls on their own.
 *
 * To use, inherit privately from the AtomicPropertyChangeInterface class, using
 * your class name as the template argument. In all cases where you normally would
 * call aboutToSetValue/hasSetValue before and after a change, create an
 * AtomicPropertyChange object. The default constructor assume you are about to
 * change the property and will call property's aboutToSetValue() if the
 * property has not been marked as changed before by any other
 * AtomicPropertyChange instances in current call stack. You can pass 'false'
 * as the a second argument to the constructor, and manually call
 * AtomicPropertyChange::aboutToChange() before actual change, this enables you
 * to prevent unnecessary property copy for undo/redo where there is actual
 * changes. AtomicPropertyChange will guaranetee calling hasSetValue() when the
 * last instance in the current call stack is destroyed.
 *
 * One thing to take note is that, because C++ does not allow throwing
 * exception in destructor, any exception thrown when calling property's
 * hasSetValue() will be caught and swallowed. To allow exception propagation,
 * you can manually call AtomicPropertyChange::tryInvoke(). If the condition is
 * satisfied, it will call hasSetValue() that allows exception propagation.
 */
template<class P> class AtomicPropertyChangeInterface {
protected:
    AtomicPropertyChangeInterface() : signalCounter(0), hasChanged(false) { }

public:

    class AtomicPropertyChange {
    public:
        /** Constructor
         *
         * @param prop: the property
         * @param markChange: If true, marks the property as changed if it
         *                    hasn't been marked before, and calls its
         *                    aboutToSetValue().
         */
        explicit AtomicPropertyChange(P & prop, bool markChange=true) : mProp(prop) {
            mProp.signalCounter++;
            if (markChange)
                aboutToChange();
        }

        /** Mark the property as changed
         *
         * It will mark the property as changed only if it has been marked
         * before, and only then will it call the property's aboutToSetValue().
         */
        void aboutToChange() {
            if (!mProp.hasChanged) {
                mProp.hasChanged = true;
                mProp.aboutToSetValue();
            }
        }

        /** Destructor
         *
         * If the property is marked as changed, and this is the last instance
         * of the class in current call stack, it will call property's
         * hasSetValue()
         */
        ~AtomicPropertyChange() {
            // Signal counter == 1? meaning we are the last one. Invoke
            // hasSetValue() before decrease counter to prevent recursive call
            // triggered by another AtomicPropertyChange created inside
            // hasSetValue(), as it has now been changed to a virtual function.
            if (mProp.signalCounter == 1 && mProp.hasChanged) {
                // Must make sure to not throw in a destructor
                try {
                    mProp.hasSetValue();
                } catch(Base::Exception &e) {
                    e.ReportException();
                } catch(...) {}
                mProp.hasChanged = false;
            }
            if (mProp.signalCounter>0)
                mProp.signalCounter--;
        }

        /** Check and invoke property's hasSetValue()
         *
         * Check if this is the last instance and the property has been marked
         * as changed. If so, invoke property's hasSetValue().
         */
        // Destructor cannot throw. So we provide this function to allow error
        // propagation.
        void tryInvoke() {
            if (mProp.signalCounter==1 && mProp.hasChanged) {
                mProp.hasSetValue();
                if (mProp.signalCounter>0)
                    --mProp.signalCounter;
                mProp.hasChanged = false;
            }
        }

    private:
        P & mProp; /**< Referenced to property we work on */
    };

protected:
    int signalCounter; /**< Counter for invoking transaction start/stop */
    bool hasChanged;
};


/** Helper class to construct list like properties
 *
 * This class is not derived from Property so that we can have more that one
 * base class for list like properties, e.g. see PropertyList, and
 * PropertyLinkListBase
 */
class AppExport PropertyListsBase
{
public:
    virtual void setSize(int newSize)=0;
    virtual int getSize() const =0;

    const std::set<int> &getTouchList() const {
        return _touchList;
    }

    void clearTouchList() {
        _touchList.clear();
    }

protected:
    virtual void setPyValues(const std::vector<PyObject*> &vals, const std::vector<int> &indices) {
        (void)vals;
        (void)indices;
        throw Base::NotImplementedError();
    }
    void _setPyObject(PyObject *);

protected:
    std::set<int> _touchList;
};

/** Base class of all property lists.
 * The PropertyLists class is the base class for properties which can contain
 * multiple values, not only a single value.
 * All property types which may contain more than one value inherits this class.
 */
class AppExport PropertyLists : public Property, public PropertyListsBase

{
    TYPESYSTEM_HEADER_WITH_OVERRIDE();
public:
    void setPyObject(PyObject *obj) override {
        _setPyObject(obj);
    }

    // if the order of the elements in the list relevant?
    // if yes, certain operations, like restoring must make sure that the
    // order is kept despite errors.
    inline void setOrderRelevant(bool on) { this->setStatus(Status::Ordered,on); }
    inline bool isOrderRelevant() const { return this->testStatus(Status::Ordered);}

    virtual void Save(Base::Writer &writer) const override;
    virtual void Restore(Base::XMLReader &reader) override;
    virtual void SaveDocFile (Base::Writer &writer) const override;
    virtual void RestoreDocFile(Base::Reader &reader) override;

protected:
    /** Returns the XML element name when saving into document XML
     *
     * The default implementation here is the type name stripped with prefix 'Property'
     */
    virtual const char *xmlName() const;

    /// Called to restore the content from the document XML
    virtual void restoreXML(Base::XMLReader &) {
        throw Base::NotImplementedError();
    }
    /** Called to save the content from the document XML
     *
     * @return Return true if the current tag is closed with '/>'.
     *
     * Note that the on input, the current tag for storing this property is not
     * closed, i.e. without ending '>', so that the implementation can choose
     * whether to store value as attribute or child elements.
     */
    virtual bool saveXML(Base::Writer &) const {
        throw Base::NotImplementedError();
        return false;
    }

    /// Return whether allow to save content to a separate (file) stream
    virtual bool canSaveStream(Base::Writer &) const {
        return false;
    }

    /// Called to save the content into a separate (file) stream
    virtual void restoreStream(Base::InputStream &s, unsigned count) {
        (void)s;
        (void)count;
        throw Base::NotImplementedError();
    }

    /// Called to restore the content from a separate (file) stream
    virtual void saveStream(Base::OutputStream &) const {
        throw Base::NotImplementedError();
    }
};

/** Helper class to implement PropertyLists */
template<class T, class ListT = std::vector<T>, class ParentT = PropertyLists >
class PropertyListsT: public ParentT
                    , public AtomicPropertyChangeInterface<PropertyListsT<T,ListT,ParentT> >
{
public:
    using const_reference = typename ListT::const_reference;
    using list_type = ListT;
    using parent_type = ParentT;
    using this_type = PropertyListsT<T, ListT, ParentT>;
    using atomic_change = typename AtomicPropertyChangeInterface<
        PropertyListsT<T,ListT,ParentT> >::AtomicPropertyChange;

    friend atomic_change;

    virtual void setSize(int newSize, const_reference def) {
        if(newSize != (int)_lValueList.size()) {
            atomic_change guard(*this);
            _lValueList.resize(newSize,def);
            ParentT::clearTouchList();
            guard.tryInvoke();
        }
    }

    virtual void setSize(int newSize) override {
        if(newSize != (int)_lValueList.size()) {
            atomic_change guard(*this);
            _lValueList.resize(newSize);
            ParentT::clearTouchList();
            guard.tryInvoke();
        }
    }

    int getSize() const override {
        return static_cast<int>(_lValueList.size());
    }

    void setValue(const_reference value) {
        ListT vals;
        vals.resize(1,value);
        setValues(std::move(vals));
    }

    virtual void setValues(ListT &&newValues) {
        atomic_change guard(*this);
        this->_touchList.clear();
        this->_lValueList = std::move(newValues);
        guard.tryInvoke();
    }

    void setValues(const ListT &newValues = ListT()) {
        setValues(ListT(newValues));
    }

    void setValue(const ListT &newValues = ListT()) {
        setValues(newValues);
    }

    const ListT &getValues() const{return _lValueList;}

    // alias to getValues
    const ListT &getValue() const{return getValues();}

    const_reference operator[] (int idx) const {return _lValueList[idx];} 

    bool isSame(const Property &_other) const override {
        if (&_other == this)
            return true;
        auto other = Base::freecad_dynamic_cast<const this_type>(&_other);
        return other && this->getValues() == other->getValues();
    }

    Property *copyBeforeChange(void) const override {
        return this->Copy();
    }

    void setPyObject(PyObject *value) override {
        try {
            setValue(getPyValue(value));
            return;
        }catch(...){}
        parent_type::setPyObject(value);
    }

    virtual void set1Value(int index, const_reference value) {
        int size = getSize();
        if (index<-1 || index>size)
            throw Base::RuntimeError("index out of bound");

        atomic_change guard(*this, false);
        if (index==-1 || index == size) {
            guard.aboutToChange();
            index = size;
            setSize(index+1,value);
        } else if (value == _lValueList[index]) {
            return;
        } else {
            guard.aboutToChange();
            _lValueList[index] = value;
        }
        this->_touchList.insert(index);
        guard.tryInvoke();
    }

    virtual void interpolateValue(int index, const_reference from, const_reference to, float t) {
        (void)index;
        (void)from;
        (void)to;
        (void)t;
        throw Base::NotImplementedError();
    }

    virtual unsigned int getMemSize (void) const override {
        return static_cast<unsigned int>(_lValueList.size() * sizeof(T));
    }

    template<class F>
    int removeIf(F f) {
        ListT vals;
        int count=0;
        for(auto it=_lValueList.cbegin();it!=_lValueList.cend();++it) {
            const T &v = *it;
            if(f(v)) {
                if(!count)
                    vals.insert(vals.end(),_lValueList.cbegin(),it);
                ++count;
            } else if (count) {
                vals.push_back(v);
            }
        }
        if(count)
            setValues(std::move(vals));
        return count;
    }

    void interpolate(const Property &from, const Property &to, float t) override
    {
        const auto &fromList = dynamic_cast<const this_type &>(from);
        const auto &toList = dynamic_cast<const this_type&>(to);
        atomic_change guard(*this, false);
        int count = std::min(getSize(), std::min(fromList.getSize(), toList.getSize()));
        for (int i=0; i<count; ++i) {
            interpolateValue(i, fromList[i], toList[i], t);
        }
        guard.tryInvoke();
    }

protected:

    void setPyValues(const std::vector<PyObject*>& vals, const std::vector<int>& indices) override
    {
        if (indices.empty()) {
            ListT values {};
            values.reserve(vals.size());
            for (auto *valsContent : vals) {
                values.push_back(getPyValue(valsContent));
            }
            setValues(std::move(values));
            return;
        }
        assert(vals.size() == indices.size());
        atomic_change guard(*this);
        int i {0};
        for (auto index : indices) {
            set1Value(index, getPyValue(vals[i]));
            i++;
        }
        guard.tryInvoke();
    }

    virtual T getPyValue(PyObject* item) const = 0;

protected:
    ListT _lValueList;
};

} // namespace App

#endif // APP_PROPERTY_H
