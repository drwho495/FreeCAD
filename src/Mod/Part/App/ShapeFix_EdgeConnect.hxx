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

// OCCT removed its own ShapeFix_EdgeConnect class (declared dead code
// upstream) even though FreeCAD's Part.ShapeFix.EdgeConnect Python binding
// relies on it. This is a drop-in reimplementation kept in FreeCAD, named
// and scoped (global namespace) to match the original OCCT class exactly,
// so that removing it from OCCT does not require touching the generated
// Python binding glue that expects `#include <ShapeFix_EdgeConnect.hxx>`.

#pragma once

#include <NCollection_DataMap.hxx>
#include <NCollection_List.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Shape.hxx>

class TopoDS_Edge;
class TopoDS_Shape;

//! Rebuilds edges to connect with new vertices, was moved from ShapeBuild.
//! Makes vertices to be shared to connect edges,
//! updates positions and tolerances for shared vertices.
//! Accepts edges bounded by two vertices each.
class ShapeFix_EdgeConnect
{
public:
    ShapeFix_EdgeConnect() = default;

    //! Adds information on connectivity between start vertex
    //! of second edge and end vertex of first edge,
    //! taking edges orientation into account
    void Add(const TopoDS_Edge& aFirst, const TopoDS_Edge& aSecond);

    //! Adds connectivity information for the whole shape.
    //! Note: edges in wires must be well ordered
    //! Note: flag Closed should be set for closed wires
    void Add(const TopoDS_Shape& aShape);

    //! Builds shared vertices, updates their positions and tolerances
    void Build();

    //! Clears internal data structure
    void Clear();

private:
    NCollection_DataMap<TopoDS_Shape, TopoDS_Shape, TopTools_ShapeMapHasher> myVertices;
    NCollection_DataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>
        myLists;
};
