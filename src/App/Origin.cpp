/***************************************************************************
 *   Copyright (c) 2015 Stefan Tröger <stefantroeger@gmx.net>              *
 *   Copyright (c) 2015 Alexander Golubev (Fat-Zer) <fatzer2@gmail.com>    *
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

#include "PreCompiled.h"

#ifndef _PreComp_
# include <string>
#endif

#include <boost/range.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <App/Application.h>
#include <App/Document.h>
#include <Base/Exception.h>
#include <Base/Placement.h>

#include "Origin.h"

#include "GroupParams.h"
#include "OriginFeature.h"

#ifndef M_PI
# define M_PI       3.14159265358979323846
#endif

using namespace App;

typedef boost::iterator_range<const char*> CharRange;

PROPERTY_SOURCE(App::Origin, App::DocumentObject)

Origin::Origin() : extension(this) {
    ADD_PROPERTY_TYPE ( OriginFeatures, (nullptr), 0, (PropertyType)(Prop_Hidden|Prop_Output),
            "Axis and baseplanes controlled by the origin" );

    setStatus(App::NoAutoExpand,true);
    extension.initExtension(this);
    Visibility.setValue(false);
}

Origin::~Origin() = default;

App::OriginFeature *Origin::getOriginFeature( const char *role) const {
    initObjects();
    const auto & features = OriginFeatures.getValues ();
    auto featIt = std::find_if (features.begin(), features.end(),
            [role] (App::DocumentObject *obj) {
                return obj->isDerivedFrom ( App::OriginFeature::getClassTypeId () ) &&
                    strcmp (static_cast<App::OriginFeature *>(obj)->Role.getValue(), role) == 0;
            } );
    if (featIt != features.end()) {
        return static_cast<App::OriginFeature *>(*featIt);
    } else {

        std::stringstream err;
        err << "Origin \"" << getFullName () << "\" doesn't contain feature with role \""
            << role << '"';
        throw Base::RuntimeError ( err.str().c_str () );
    }
}

App::Property *Origin::getPropertyByName(const char *name) const
{
    if (getDocument() && !getDocument()->testStatus(App::Document::Restoring)
            && name && strcmp(name, OriginFeatures.getName())==0)
    {
        initObjects();
        return &const_cast<Origin*>(this)->OriginFeatures;
    }
    return DocumentObject::getPropertyByName(name);
}

App::Line *Origin::getAxis( const char *role ) const {
    App::OriginFeature *feat = getOriginFeature (role);
    if ( feat->isDerivedFrom(App::Line::getClassTypeId () ) ) {
        return static_cast<App::Line *> (feat);
    } else {
        std::stringstream err;
        err << "Origin \"" << getFullName () << "\" contains bad Axis object for role \""
            << role << '"';
        throw Base::RuntimeError ( err.str().c_str () );
    }
}

App::Plane *Origin::getPlane( const char *role ) const {
    App::OriginFeature *feat = getOriginFeature (role);
    if ( feat->isDerivedFrom(App::Plane::getClassTypeId () ) ) {
        return static_cast<App::Plane *> (feat);
    } else {
        std::stringstream err;
        err << "Origin \"" << getFullName () << "\" contains bad Plane object for role \""
            << role << '"';
        throw Base::RuntimeError ( err.str().c_str () );
    }
}

bool Origin::hasObject (const DocumentObject *obj) const {
    const auto & features = OriginFeatures.getValues ();
    return std::find (features.begin(), features.end(), obj) != features.end ();
}

short Origin::mustExecute() const {
    return DocumentObject::mustExecute();
}

App::DocumentObjectExecReturn *Origin::execute() {
    return DocumentObject::execute ();
}

void Origin::setupObject () {
    DocumentObject::setupObject();
    if(GroupParams::getCreateOrigin())
        initObjects();
}

void Origin::initObjects() const {
    if (testStatus(App::ObjectStatus::Remove)
            || !getDocument()
            || getDocument()->testStatus(App::Document::Restoring)
            || getDocument()->isPerformingTransaction())
        return;

    const static struct {
        const Base::Type type;
        const char *role;
	const QString label;
        Base::Rotation rot;
    } setupData [] = {
        {App::Line::getClassTypeId(),  AxisRoles[0],  tr("X-axis"),   Base::Rotation()},
        {App::Line::getClassTypeId(),  AxisRoles[1],  tr("Y-axis"),   Base::Rotation(Base::Vector3d(1,1,1), M_PI*2/3)},
        {App::Line::getClassTypeId(),  AxisRoles[2],  tr("Z-axis"),   Base::Rotation(Base::Vector3d(1,-1,1), M_PI*2/3)},
        {App::Plane::getClassTypeId(), PlaneRoles[0], tr("XY-plane"), Base::Rotation()},
        {App::Plane::getClassTypeId(), PlaneRoles[1], tr("XZ-plane"), Base::Rotation(1.0, 0.0, 0.0, 1.0 )},
        {App::Plane::getClassTypeId(), PlaneRoles[2], tr("YZ-plane"), Base::Rotation(Base::Vector3d(1,1,1), M_PI*2/3)}
    };

    if(OriginFeatures.getSize())
        return;

    App::Document *doc = getDocument ();

    std::vector<App::DocumentObject *> links;
    for (auto data: setupData) {
        std::string objName = doc->getUniqueObjectName ( data.role );
        App::DocumentObject *featureObj = doc->addObject ( data.type.getName(), objName.c_str () );

        assert ( featureObj && featureObj->isDerivedFrom ( App::OriginFeature::getClassTypeId () ) );

	QByteArray byteArray = data.label.toUtf8();
        featureObj->Label.setValue(byteArray.constData());

        App::OriginFeature *feature = static_cast <App::OriginFeature *> ( featureObj );
        feature->Placement.setValue ( Base::Placement ( Base::Vector3d (), data.rot ) );
        feature->Role.setValue ( data.role );
        feature->purgeTouched();

        links.push_back (feature);
    }

    const_cast<Origin*>(this)->OriginFeatures.setValues (links);
}

void Origin::unsetupObject () {
    const auto &objsLnk = OriginFeatures.getValues ();
    // Copy to set to assert we won't call methode more then one time for each object
    std::set<App::DocumentObject *> objs (objsLnk.begin(), objsLnk.end());
    // Remove all controlled objects
    for (auto obj: objs ) {
        // Check that previous deletes wasn't inderectly removed one of our objects
        const auto &objsLnk = OriginFeatures.getValues ();
        if ( std::find(objsLnk.begin(), objsLnk.end(), obj) != objsLnk.end()) {
            if ( ! obj->isRemoving() ) {
                obj->getDocument()->removeObject (obj->getNameInDocument());
            }
        }
    }
}

// ----------------------------------------------------------------------------

Origin::OriginExtension::OriginExtension(Origin* obj)
    : obj(obj)
{
    Group.setStatus(Property::Transient, true);
}

void Origin::OriginExtension::initExtension(ExtensionContainer* obj) {
    App::GroupExtension::initExtension(obj);
}

bool Origin::OriginExtension::extensionGetSubObject(DocumentObject *&ret, const char *subname,
                                                    PyObject **pyobj, Base::Matrix4D *mat, bool, int depth) const {
    if (!subname || subname[0] == '\0') {
        return false;
    }

    const char *dot = strchr(subname, '.');
    if (!dot)
        return false;

    CharRange name(subname, dot);
    const char *found = nullptr;

    // mapping of object name to role name
    for (int i=0; i<3; i++) {
        if (boost::starts_with(name, Origin::AxisRoles[i])) {
            found = Origin::AxisRoles[i];
            break;
        }
        if (boost::starts_with(name, Origin::PlaneRoles[i])) {
            found = Origin::PlaneRoles[i];
            break;
        }
    }

    if (!found)
        return false;

    try {
        ret = obj->getOriginFeature(found);
        if (!ret)
            return false;
        ret = ret->getSubObject(dot+1, pyobj, mat, true, depth+1);
        return true;
    }
    catch (const Base::Exception& e) {
        e.ReportException();
        return false;
    }
}

const App::PropertyLinkList& Origin::OriginExtension::getExportGroupProperty(int reason) const
{
    return obj && reason == GS_SELECT ? obj->OriginFeatures : _ExportChildren;
}

bool Origin::canSaveExtension(Extension *ext) const
{
    if (ext)
        return ext!=&extension;
    return foreachExtension<Extension>(
                [this](Extension *ext) {
                    return ext != &this->extension;
                });
}

void Origin::Restore(Base::XMLReader &reader)
{
    // Disabled for now, as a mistake has been made several weeks ago causing
    // restore error. If it is really necessary to enable extension saving on
    // Origin, uncomment the following line.
    //
    // return App::ExtensionContainer::Restore(reader);

    return App::PropertyContainer::Restore(reader);
}
