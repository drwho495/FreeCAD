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

// Ported from the OCCT implementation of ShapeFix_EdgeConnect (removed
// upstream, see ShapeFix_EdgeConnect.hxx for context).

#include <BRep_Builder.hxx>
#include <BRep_GCurve.hxx>
#include <BRep_TEdge.hxx>
#include <BRep_Tool.hxx>
#include <NCollection_Sequence.hxx>
#include <Precision.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>
#include <gp_XYZ.hxx>

#include "ShapeFix_EdgeConnect.hxx"

void ShapeFix_EdgeConnect::Add(const TopoDS_Edge& aFirst, const TopoDS_Edge& aSecond)
{
    // Select vertices to connect
    TopoDS_Vertex theFirstVertex = TopExp::LastVertex(aFirst, true);
    TopoDS_Vertex theSecondVertex = TopExp::FirstVertex(aSecond, true);

    // Make necessary bindings
    if (myVertices.IsBound(theFirstVertex)) {
        // First vertex is bound - find shared vertex
        TopoDS_Vertex theFirstShared = TopoDS::Vertex(myVertices(theFirstVertex));
        if (myVertices.IsBound(theSecondVertex)) {
            // Second vertex is bound - find shared vertex
            TopoDS_Vertex theSecondShared = TopoDS::Vertex(myVertices(theSecondVertex));
            if (!theFirstShared.IsSame(theSecondShared)) {
                // Concatenate lists
                NCollection_List<TopoDS_Shape>& theFirstList = myLists(theFirstShared);
                NCollection_List<TopoDS_Shape>& theSecondList = myLists(theSecondShared);
                for (NCollection_List<TopoDS_Shape>::Iterator theIterator(theSecondList);
                     theIterator.More();
                     theIterator.Next()) {
                    // Rebind shared vertex for current one
                    myVertices(theIterator.Value()) = theFirstShared;
                    // Skip the following edge
                    theIterator.Next();
                }
                // Append second list to the first one
                theFirstList.Append(theSecondList);
                // Unbind the second shared vertex
                myLists.UnBind(theSecondShared);
            }
        }
        else {
            // Bind second vertex with shared vertex of the first one
            myVertices.Bind(theSecondVertex, theFirstShared);
            // Add second vertex and second edge to the list
            NCollection_List<TopoDS_Shape>& theFirstList = myLists(theFirstShared);
            theFirstList.Append(theSecondVertex);
            theFirstList.Append(aSecond);
        }
    }
    else {
        if (myVertices.IsBound(theSecondVertex)) {
            // Second vertex is bound - find shared vertex
            TopoDS_Vertex& theSecondShared = TopoDS::Vertex(myVertices(theSecondVertex));
            // Bind first vertex with shared vertex of the second one
            myVertices.Bind(theFirstVertex, theSecondShared);
            // Add first vertex and first edge to the list
            NCollection_List<TopoDS_Shape>& theSecondList = myLists(theSecondShared);
            theSecondList.Append(theFirstVertex);
            theSecondList.Append(aFirst);
        }
        else {
            // None is bound - create new bindings
            myVertices.Bind(theFirstVertex, theFirstVertex);
            myVertices.Bind(theSecondVertex, theFirstVertex);
            NCollection_List<TopoDS_Shape> theNewList;
            theNewList.Append(theFirstVertex);
            theNewList.Append(aFirst);
            theNewList.Append(theSecondVertex);
            theNewList.Append(aSecond);
            myLists.Bind(theFirstVertex, theNewList);
        }
    }
}

void ShapeFix_EdgeConnect::Add(const TopoDS_Shape& aShape)
{
    for (TopExp_Explorer expw(aShape, TopAbs_WIRE); expw.More(); expw.Next()) {
        TopoDS_Wire theWire = TopoDS::Wire(expw.Current());
        TopExp_Explorer expe(theWire, TopAbs_EDGE);
        if (expe.More()) {
            // Obtain the first edge and remember it
            TopoDS_Edge theEdge = TopoDS::Edge(expe.Current());
            TopoDS_Edge theFirst = theEdge;
            expe.Next();
            for (; expe.More(); expe.Next()) {
                // Obtain second edge and connect it
                TopoDS_Edge theNext = TopoDS::Edge(expe.Current());
                Add(theEdge, theNext);
                theEdge = theNext;
            }
            // Connect first and last edges if wire is closed
            if (theWire.Closed()) {
                Add(theEdge, theFirst);
            }
        }
    }
}

void ShapeFix_EdgeConnect::Build()
{
    NCollection_List<TopoDS_Shape>::Iterator theLIterator;
    NCollection_List<Handle(BRep_CurveRepresentation)>::Iterator theCIterator;

    NCollection_Sequence<gp_XYZ> thePositions;
    gp_XYZ thePosition;
    double theMaxDev;
    BRep_Builder theBuilder;

    // Iterate on shared vertices
    for (NCollection_DataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>::
             Iterator theSIterator(myLists);
         theSIterator.More();
         theSIterator.Next()) {
        TopoDS_Vertex theSharedVertex = TopoDS::Vertex(theSIterator.Key());
        const NCollection_List<TopoDS_Shape>& theList = theSIterator.Value();

        thePositions.Clear();

        // Iterate on edges, accumulating positions
        for (theLIterator.Initialize(theList); theLIterator.More(); theLIterator.Next()) {
            const TopoDS_Vertex& theVertex = TopoDS::Vertex(theLIterator.Value());
            theLIterator.Next();
            TopoDS_Edge& theEdge = TopoDS::Edge(theLIterator.ChangeValue());

            // Determine usage of curve bound points
            TopoDS_Vertex theStart, theEnd;
            theEdge.Orientation(TopAbs_FORWARD);
            TopExp::Vertices(theEdge, theStart, theEnd);
            bool use_start = (theVertex.IsSame(theStart));
            bool use_end = (theVertex.IsSame(theEnd));

            // Iterate on edge curves, accumulating positions
            for (theCIterator.Initialize(
                     (*((Handle(BRep_TEdge)*)&theEdge.TShape()))->ChangeCurves());
                 theCIterator.More();
                 theCIterator.Next()) {
                Handle(BRep_GCurve) GC = occ::down_cast<BRep_GCurve>(theCIterator.Value());
                if (GC.IsNull()) {
                    continue;
                }
                // Calculate vertex position for this curve
                double theFParam, theLParam;
                GC->Range(theFParam, theLParam);
                gp_Pnt thePoint;
                if (use_start) {
                    GC->D0(theFParam, thePoint);
                    thePositions.Append(thePoint.XYZ());
                }
                if (use_end) {
                    GC->D0(theLParam, thePoint);
                    thePositions.Append(thePoint.XYZ());
                }
            }
        }

        int i, theNbPos = thePositions.Length();

        // Calculate vertex position (bounding-box midpoint of accumulated positions)
        thePosition = gp_XYZ(0., 0., 0.);
        gp_XYZ theLBound(0., 0., 0.), theRBound(0., 0., 0.);
        for (i = 1; i <= theNbPos; i++) {
            thePosition = thePositions.Value(i);
            if (i == 1) {
                theLBound = theRBound = thePosition;
            }
            double val = thePosition.X();
            if (val < theLBound.X()) {
                theLBound.SetX(val);
            }
            else if (val > theRBound.X()) {
                theRBound.SetX(val);
            }
            val = thePosition.Y();
            if (val < theLBound.Y()) {
                theLBound.SetY(val);
            }
            else if (val > theRBound.Y()) {
                theRBound.SetY(val);
            }
            val = thePosition.Z();
            if (val < theLBound.Z()) {
                theLBound.SetZ(val);
            }
            else if (val > theRBound.Z()) {
                theRBound.SetZ(val);
            }
        }
        if (theNbPos > 1) {
            thePosition = (theLBound + theRBound) / 2.;
        }

        // Calculate maximal deviation
        theMaxDev = 0.;

        for (i = 1; i <= theNbPos; i++) {
            double theDeviation = (thePosition - thePositions.Value(i)).Modulus();
            if (theDeviation > theMaxDev) {
                theMaxDev = theDeviation;
            }
        }
        theMaxDev *= 1.0001;  // To avoid numerical roundings
        if (theMaxDev < Precision::Confusion()) {
            theMaxDev = Precision::Confusion();
        }

        // Update shared vertex
        theBuilder.UpdateVertex(theSharedVertex, gp_Pnt(thePosition), theMaxDev);

        // Iterate on edges, adding shared vertex
        for (theLIterator.Initialize(theList); theLIterator.More(); theLIterator.Next()) {
            const TopoDS_Vertex& theVertex = TopoDS::Vertex(theLIterator.Value());
            theLIterator.Next();
            TopoDS_Edge& theEdge = TopoDS::Edge(theLIterator.ChangeValue());

            // Determine usage of old vertices
            TopoDS_Vertex theStart, theEnd;
            theEdge.Orientation(TopAbs_FORWARD);
            TopExp::Vertices(theEdge, theStart, theEnd);
            bool use_start = (theVertex.IsSame(theStart));
            bool use_end = (theVertex.IsSame(theEnd));

            // Prepare vertex to remove
            TopoDS_Vertex theOldVertex;
            if (use_start) {
                theOldVertex = theStart;  // start is preferred for closed edges
            }
            else {
                theOldVertex = theEnd;
            }

            // Prepare vertex to add
            TopoDS_Vertex theNewVertex;
            if (use_start) {
                TopoDS_Shape tmpshapeFwd = theSharedVertex.Oriented(TopAbs_FORWARD);
                theNewVertex = TopoDS::Vertex(tmpshapeFwd);
            }
            else {
                TopoDS_Shape tmpshapeRev = theSharedVertex.Oriented(TopAbs_REVERSED);
                theNewVertex = TopoDS::Vertex(tmpshapeRev);
            }
            if (!theOldVertex.IsSame(theNewVertex)) {
                // Replace vertices
                bool freeflag = theEdge.Free();
                theEdge.Free(true);
                theBuilder.Remove(theEdge, theOldVertex);
                theBuilder.Add(theEdge, theNewVertex);
                if (use_start && use_end) {
                    // process special case for closed edge
                    theBuilder.Remove(theEdge,
                                       theOldVertex.Oriented(TopAbs_REVERSED));  // remove reversed
                    theBuilder.Add(theEdge,
                                   theNewVertex.Oriented(TopAbs_REVERSED));  // add reversed
                }
                theEdge.Free(freeflag);
            }
        }
    }

    // Clear maps after build
    Clear();
}

void ShapeFix_EdgeConnect::Clear()
{
    myVertices.Clear();
    myLists.Clear();
}
