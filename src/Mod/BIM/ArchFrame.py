# SPDX-License-Identifier: LGPL-2.1-or-later

# ***************************************************************************
# *                                                                         *
# *   Copyright (c) 2013 Yorik van Havre <yorik@uncreated.net>              *
# *                                                                         *
# *   This file is part of FreeCAD.                                         *
# *                                                                         *
# *   FreeCAD is free software: you can redistribute it and/or modify it    *
# *   under the terms of the GNU Lesser General Public License as           *
# *   published by the Free Software Foundation, either version 2.1 of the  *
# *   License, or (at your option) any later version.                       *
# *                                                                         *
# *   FreeCAD is distributed in the hope that it will be useful, but        *
# *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      *
# *   Lesser General Public License for more details.                       *
# *                                                                         *
# *   You should have received a copy of the GNU Lesser General Public      *
# *   License along with FreeCAD. If not, see                               *
# *   <https://www.gnu.org/licenses/>.                                      *
# *                                                                         *
# ***************************************************************************

__title__  = "FreeCAD Arch Frame"
__author__ = "Yorik van Havre"
__url__    = "https://www.freecad.org"

## @package ArchFrame
#  \ingroup ARCH
#  \brief The Frame object and tools
#
#  This module provides tools to build Frame objects.
#  Frames are objects made of a profile and an object with
#  edges along which the profile gets extruded

import FreeCAD
import ArchComponent
import Draft
import DraftVecUtils

if FreeCAD.GuiUp:
    from PySide.QtCore import QT_TRANSLATE_NOOP
    import FreeCADGui
    from draftutils.translate import translate
else:
    # \cond
    def translate(ctxt,txt):
        return txt
    def QT_TRANSLATE_NOOP(ctxt,txt):
        return txt
    # \endcond


class _Frame(ArchComponent.Component):

    "A parametric frame object"

    def __init__(self,obj):
        ArchComponent.Component.__init__(self,obj)
        self.Type = "Frame"
        self.setProperties(obj)
        obj.IfcType = "Railing"

    def setProperties(self,obj):

        pl = obj.PropertiesList
        if not "Profile" in pl:
            obj.addProperty("App::PropertyLink","Profile","Frame",QT_TRANSLATE_NOOP("App::Property","The profile used to build this frame"), locked=True)
        if not "Align" in pl:
            obj.addProperty("App::PropertyBool","Align","Frame",QT_TRANSLATE_NOOP("App::Property","Specifies if the profile must be aligned with the extrusion wires"), locked=True)
            obj.Align = True
        if not "Offset" in pl:
            obj.addProperty("App::PropertyVectorDistance","Offset","Frame",QT_TRANSLATE_NOOP("App::Property","An offset vector between the base sketch and the frame"), locked=True)
        if not "BasePoint" in pl:
            obj.addProperty("App::PropertyInteger","BasePoint","Frame",QT_TRANSLATE_NOOP("App::Property","Crossing point of the path on the profile."), locked=True)
        if not "ProfilePlacement" in pl:
            obj.addProperty("App::PropertyPlacement","ProfilePlacement","Frame",QT_TRANSLATE_NOOP("App::Property","An optional additional placement to add to the profile before extruding it"), locked=True)
        if not "Rotation" in pl:
            obj.addProperty("App::PropertyAngle","Rotation","Frame",QT_TRANSLATE_NOOP("App::Property","The rotation of the profile around its extrusion axis"), locked=True)
        if not "Edges" in pl:
            obj.addProperty("App::PropertyEnumeration","Edges","Frame",QT_TRANSLATE_NOOP("App::Property","The type of edges to consider"), locked=True)
            obj.Edges = ["All edges","Vertical edges","Horizontal edges","Bottom horizontal edges","Top horizontal edges"]
        if not "Fuse" in pl:
            obj.addProperty("App::PropertyBool","Fuse","Frame",QT_TRANSLATE_NOOP("App::Property","If true, geometry is fused, otherwise a compound"), locked=True)

    def onDocumentRestored(self,obj):

        ArchComponent.Component.onDocumentRestored(self,obj)
        self.setProperties(obj)

    def loads(self,state):

        self.Type = "Frame"

    def execute(self,obj):

        if self.clone(obj):
            return
        if not self.ensureBase(obj):
            return

        if not obj.Base:
            return
        if not obj.Base.Shape:
            return
        if not obj.Base.Shape.Wires:
            return

        pl = obj.Placement
        if obj.Base.Shape.Solids:
            obj.Shape = obj.Base.Shape.copy()
            if not pl.isNull():
                obj.Placement = obj.Shape.Placement.multiply(pl)
        else:
            if not obj.Profile:
                return
            if not obj.Profile.Shape:
                return
            if obj.Profile.Shape.findPlane() is None:
                return
            if not obj.Profile.Shape.Wires:
                return
            if not obj.Profile.Shape.Faces:
                for w in obj.Profile.Shape.Wires:
                    if not w.isClosed():
                        return
            import math
            import DraftGeomUtils
            import Part
            baseprofile = obj.Profile.Shape.copy()
            if hasattr(obj,"ProfilePlacement"):
                if not obj.ProfilePlacement.isNull():
                    baseprofile.Placement = obj.ProfilePlacement.multiply(baseprofile.Placement)
            if not baseprofile.Faces:
                f = []
                for w in baseprofile.Wires:
                    f.append(Part.Face(w))
                if len(f) == 1:
                    baseprofile = f[0]
                else:
                    baseprofile = Part.makeCompound(f)
            shapes = []
            normal = DraftGeomUtils.getNormal(obj.Base.Shape)
            edges = obj.Base.Shape.Edges
            if hasattr(obj,"Edges"):
                if obj.Edges == "Vertical edges":
                    rv = obj.Base.Placement.Rotation.multVec(FreeCAD.Vector(0,1,0))
                    edges = [e for e in edges if round(rv.getAngle(e.tangentAt(e.FirstParameter)),4) in [0,3.1416]]
                elif obj.Edges == "Horizontal edges":
                    rv = obj.Base.Placement.Rotation.multVec(FreeCAD.Vector(1,0,0))
                    edges = [e for e in edges if round(rv.getAngle(e.tangentAt(e.FirstParameter)),4) in [0,3.1416]]
                elif obj.Edges == "Top horizontal edges":
                    rv = obj.Base.Placement.Rotation.multVec(FreeCAD.Vector(1,0,0))
                    edges = [e for e in edges if round(rv.getAngle(e.tangentAt(e.FirstParameter)),4) in [0,3.1416]]
                    edges = sorted(edges,key=lambda x: x.CenterOfMass.z,reverse=True)
                    z = edges[0].CenterOfMass.z
                    edges = [e for e in edges if abs(e.CenterOfMass.z-z) < 0.00001]
                elif obj.Edges == "Bottom horizontal edges":
                    rv = obj.Base.Placement.Rotation.multVec(FreeCAD.Vector(1,0,0))
                    edges = [e for e in edges if round(rv.getAngle(e.tangentAt(e.FirstParameter)),4) in [0,3.1416]]
                    edges = sorted(edges,key=lambda x: x.CenterOfMass.z)
                    z = edges[0].CenterOfMass.z
                    edges = [e for e in edges if abs(e.CenterOfMass.z-z) < 0.00001]
            for e in edges:
                bvec = DraftGeomUtils.vec(e)
                bpoint = e.Vertexes[0].Point
                profile = baseprofile.copy()
                rot = None # New rotation.
                # Supplying FreeCAD.Rotation() with two parallel vectors and
                # a null vector may seem strange, but the function is perfectly
                # able to handle this. Its algorithm will use default axes in
                # such cases.
                if obj.Align:
                    if normal is None:
                        rot = FreeCAD.Rotation(FreeCAD.Vector(), bvec, bvec, "ZYX")
                    else:
                        rot = FreeCAD.Rotation(FreeCAD.Vector(), normal, bvec, "ZYX")
                    profile.Placement.Rotation = rot
                if hasattr(obj, "BasePoint"):
                    edges = Part.__sortEdges__(profile.Edges)
                    basepointliste = [profile.Placement.Base]
                    for edge in edges:
                        basepointliste.append(DraftGeomUtils.findMidpoint(edge))
                        basepointliste.append(edge.Vertexes[-1].Point)
                    try:
                        basepoint = basepointliste[obj.BasePoint]
                    except IndexError:
                        FreeCAD.Console.PrintMessage(translate("Arch", "Crossing point not found in profile.")+"\n")
                        basepoint = basepointliste[0]
                else:
                    basepoint = profile.Placement.Base
                delta = bpoint.sub(basepoint) # Translation vector.
                if obj.Offset and (not DraftVecUtils.isNull(obj.Offset)):
                    if rot is None:
                        delta = delta + obj.Offset
                    else:
                        delta = delta + rot.multVec(obj.Offset)
                profile.translate(delta)
                if obj.Rotation:
                    profile.rotate(bpoint, bvec, obj.Rotation)
                # profile = wire.makePipeShell([profile], True, False, 2) TODO buggy
                profile = profile.extrude(bvec)
                shapes.append(profile)
            if shapes:
                if hasattr(obj,"Fuse"):
                    if obj.Fuse:
                        if len(shapes) > 1:
                            s = shapes[0].multiFuse(shapes[1:])
                            s = s.removeSplitter()
                            obj.Shape = s
                            obj.Placement = pl
                            return
                obj.Shape = Part.makeCompound(shapes)
                obj.Placement = pl


class _ViewProviderFrame(ArchComponent.ViewProviderComponent):

    "A View Provider for the Frame object"

    def __init__(self,vobj):

        ArchComponent.ViewProviderComponent.__init__(self,vobj)

    def getIcon(self):

        import Arch_rc
        return ":/icons/Arch_Frame_Tree.svg"

    def claimChildren(self):

        p = []
        if hasattr(self,"Object"):
            if self.Object.Profile:
                p = [self.Object.Profile]
        return ArchComponent.ViewProviderComponent.claimChildren(self)+p
