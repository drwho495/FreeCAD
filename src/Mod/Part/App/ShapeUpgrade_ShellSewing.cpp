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

#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <ShapeAnalysis_ShapeTolerance.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>

#include "ShapeUpgrade_ShellSewing.h"

using namespace Part;

ShapeUpgrade_ShellSewing::ShapeUpgrade_ShellSewing()
{
    myReShape = new ShapeBuild_ReShape;
}

void ShapeUpgrade_ShellSewing::init(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) {
        return;
    }
    if (shape.ShapeType() == TopAbs_SHELL) {
        myShells.Add(shape);
    }
    else {
        for (TopExp_Explorer exs(shape, TopAbs_SHELL); exs.More(); exs.Next()) {
            myShells.Add(exs.Current());
        }
    }
}

int ShapeUpgrade_ShellSewing::prepare(const double tol)
{
    int nb = myShells.Extent();
    int ns = 0;
    for (int i = 1; i <= nb; i++) {
        TopoDS_Shell sl = TopoDS::Shell(myShells.FindKey(i));
        BRepBuilderAPI_Sewing ss(tol);
        for (TopExp_Explorer exp(sl, TopAbs_FACE); exp.More(); exp.Next()) {
            ss.Add(exp.Current());
        }
        ss.Perform();
        TopoDS_Shape newsh = ss.SewedShape();
        if (!newsh.IsNull()) {
            myReShape->Replace(sl, newsh);
            ns++;
        }
    }
    return ns;
}

TopoDS_Shape ShapeUpgrade_ShellSewing::apply(const TopoDS_Shape& shape, const double tol)
{
    if (shape.IsNull() || myShells.Extent() == 0) {
        return shape;
    }

    TopoDS_Shape res = myReShape->Apply(shape, TopAbs_FACE, 2);

    // Now orient the solids correctly.
    myReShape->Clear();
    int ns = 0;
    for (TopExp_Explorer exd(shape, TopAbs_SOLID); exd.More(); exd.Next()) {
        TopoDS_Solid sd = TopoDS::Solid(exd.Current());
        BRepClass3d_SolidClassifier bsc3d(sd);
        bsc3d.PerformInfinitePoint(tol);
        if (bsc3d.State() == TopAbs_IN) {
            myReShape->Replace(sd, sd.Reversed());
            ns++;
        }
    }

    if (ns != 0) {
        res = myReShape->Apply(res, TopAbs_SHELL, 2);
    }

    return res;
}

TopoDS_Shape ShapeUpgrade_ShellSewing::ApplySewing(const TopoDS_Shape& shape, const double tol)
{
    if (shape.IsNull()) {
        return shape;
    }

    double t = tol;
    if (t <= 0.) {
        ShapeAnalysis_ShapeTolerance stu;
        t = stu.Tolerance(shape, 0);  // average tolerance
    }

    init(shape);
    if (prepare(t)) {
        return apply(shape, t);
    }

    return TopoDS_Shape();
}
