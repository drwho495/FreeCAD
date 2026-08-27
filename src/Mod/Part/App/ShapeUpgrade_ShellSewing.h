// SPDX-License-Identifier: LGPL-2.1-or-later

/***************************************************************************
 *   Copyright (c) 2026 FreeCAD Project Association                       *
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

#pragma once

#include <Mod/Part/PartGlobal.h>

#include <NCollection_IndexedMap.hxx>
#include <ShapeBuild_ReShape.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Shape.hxx>


namespace Part
{
/*!
 * \brief The ShapeUpgrade_ShellSewing class
 * OCCT removed its own ShapeUpgrade_ShellSewing class (declared dead code
 * upstream) even though FreeCAD relies on it to heal shells built from
 * loose faces. This is a drop-in reimplementation kept in FreeCAD so that
 * removing it from OCCT does not require touching every call site.
 *
 * It provides a tool for applying the sewing algorithm from
 * BRepBuilderAPI: it takes a shape, calls sewing for each shell,
 * and then replaces sewed shells with use of ShapeBuild_ReShape.
 */
class PartExport ShapeUpgrade_ShellSewing
{
public:
    ShapeUpgrade_ShellSewing();

    //! Builds a new shape from a former one, by calling Sewing from
    //! BRepBuilderAPI. Rebuilt solids are oriented to be "not infinite".
    //!
    //! If <tol> is not given (i.e. value 0. by default), it is
    //! computed as the mean tolerance recorded in <shape>.
    //!
    //! If no shell has been sewed, this method returns a null shape.
    TopoDS_Shape ApplySewing(const TopoDS_Shape& shape, const double tol = 0.0);

private:
    void init(const TopoDS_Shape& shape);
    int prepare(const double tol);
    TopoDS_Shape apply(const TopoDS_Shape& shape, const double tol);

    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> myShells;
    Handle(ShapeBuild_ReShape) myReShape;
};

}  // namespace Part
