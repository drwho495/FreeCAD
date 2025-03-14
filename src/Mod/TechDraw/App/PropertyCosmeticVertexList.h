/***************************************************************************
 *   Copyright (c) 2010 Jürgen Riegel <juergen.riegel@web.de>              *
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

#ifndef TECHDRAW_PropertyCosmeticVertexList_H
#define TECHDRAW_PropertyCosmeticVertexList_H

#include <Mod/TechDraw/TechDrawGlobal.h>

#include <vector>
#include <App/Property.h>

#include "CosmeticVertex.h"


namespace Base {
class Writer;
}

namespace TechDraw
{
class CosmeticVertex;

class TechDrawExport PropertyCosmeticVertexList: public App::PropertyLists
{
    TYPESYSTEM_HEADER();

public:
    /**
     * A constructor.
     * A more elaborate description of the constructor.
     */
    PropertyCosmeticVertexList();

    /**
     * A destructor.
     * A more elaborate description of the destructor.
     */
    virtual ~PropertyCosmeticVertexList();

    virtual void setSize(int newSize);
    virtual int getSize(void) const;

    virtual bool isSame(const App::Property &) const {return false;}

    /** Sets the property
     */
    void setValue(CosmeticVertex*);
    void setValues(const std::vector<CosmeticVertex*>&);

    /// index operator
    const CosmeticVertex *operator[] (const int idx) const {
        return _lValueList[idx];
    }

    const std::vector<CosmeticVertex*> &getValues(void) const {
        return _lValueList;
    }

    virtual PyObject *getPyObject(void);
    virtual void setPyObject(PyObject *);

    virtual void Save(Base::Writer &writer) const;
    virtual void Restore(Base::XMLReader &reader);

    virtual App::Property *Copy(void) const;
    virtual void Paste(const App::Property &from);

    virtual unsigned int getMemSize(void) const;

private:
    std::vector<CosmeticVertex*> _lValueList;
};

} // namespace TechDraw


#endif // TECHDRAW_PropertyCosmeticVertexList_H
