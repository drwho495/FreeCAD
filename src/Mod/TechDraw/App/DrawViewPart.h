/***************************************************************************
 *   Copyright (c) 2007 Jürgen Riegel <juergen.riegel@web.de>              *
 *   Copyright (c) 2013 Luke Parry <l.parry@warwick.ac.uk>                 *
 *   Copyright (c) 2016 WandererFan <wandererfan@gmail.com>                *
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

#ifndef DrawViewPart_h_
#define DrawViewPart_h_

#include <QFuture>
#include <QFutureWatcher>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>

#include <App/DocumentObject.h>
#include <App/FeaturePython.h>
#include <App/PropertyLinks.h>
#include <Base/BoundBox.h>
#include <Mod/TechDraw/TechDrawGlobal.h>

#include "CosmeticExtension.h"
#include "DrawView.h"

class Message_ProgressIndicator;

class gp_Pnt;
class gp_Pln;
class gp_Ax2;
class TopoDS_Shape;

namespace Base
{
class SequencerLauncher;
}

namespace App
{
class Part;
}

namespace Part
{
class ProgressIndicator;
}

namespace TechDraw
{
class GeometryObject;
using GeometryObjectPtr = std::shared_ptr<GeometryObject>;
class Vertex;
class BaseGeom;
class Face;
}// namespace TechDraw

namespace TechDraw
{
class DrawHatch;
class DrawGeomHatch;
class DrawViewDimension;
class DrawProjectSplit;
class DrawViewSection;
class DrawViewDetail;
class DrawViewBalloon;
class CosmeticVertex;
class CosmeticEdge;
class CenterLine;
class GeomFormat;
}// namespace TechDraw

namespace TechDraw
{

class DrawViewSection;

class TechDrawExport DrawViewPart: public DrawView, public CosmeticExtension
{
    PROPERTY_HEADER_WITH_EXTENSIONS(TechDraw::DrawViewPart);

public:
    DrawViewPart();
    ~DrawViewPart() override;

    App::PropertyLinkList Source;
    App::PropertyXLinkList XSource;
    App::PropertyVector
        Direction;//TODO: Rename to YAxisDirection or whatever this actually is  (ProjectionDirection)
    App::PropertyVector XDirection;
    App::PropertyBool Perspective;
    App::PropertyDistance Focus;

    App::PropertyBool CoarseView;
    App::PropertyBool SeamVisible;
    App::PropertyBool SmoothVisible;
    //App::PropertyBool   OutlinesVisible;
    App::PropertyBool IsoVisible;

    App::PropertyBool HardHidden;
    App::PropertyBool SmoothHidden;
    App::PropertyBool SeamHidden;
    //App::PropertyBool   OutlinesHidden;
    App::PropertyBool IsoHidden;
    App::PropertyInteger IsoCount;

    App::PropertyInteger ScrubCount;

    short mustExecute() const override;
    App::DocumentObjectExecReturn* execute() override;
    const char* getViewProviderName() const override { return "TechDrawGui::ViewProviderViewPart"; }
    PyObject* getPyObject() override;

    static TopoDS_Shape centerScaleRotate(DrawViewPart* dvp, TopoDS_Shape& inOutShape,
                                          Base::Vector3d centroid);
    std::vector<TechDraw::DrawHatch*> getHatches() const;
    std::vector<TechDraw::DrawGeomHatch*> getGeomHatches() const;
    std::vector<TechDraw::DrawViewDimension*> getDimensions() const;
    std::vector<TechDraw::DrawViewBalloon*> getBalloons() const;

    const std::vector<TechDraw::VertexPtr> getVertexGeometry() const;
    const BaseGeomPtrVector getEdgeGeometry() const;
    const BaseGeomPtrVector getVisibleFaceEdges() const;
    const std::vector<TechDraw::FacePtr> getFaceGeometry() const;

    bool hasGeometry() const;
    TechDraw::GeometryObjectPtr getGeometryObject() const;

    TechDraw::VertexPtr getVertex(std::string vertexName) const;
    TechDraw::BaseGeomPtr getEdge(std::string edgeName) const;
    TechDraw::FacePtr getFace(std::string faceName) const;

    TechDraw::BaseGeomPtr
    getGeomByIndex(int idx) const;//get existing geom for edge idx in projection
    TechDraw::VertexPtr
    getProjVertexByIndex(int idx) const;//get existing geom for vertex idx in projection

    TechDraw::VertexPtr getProjVertexByCosTag(std::string cosTag);
    std::vector<TechDraw::BaseGeomPtr>
    getFaceEdgesByIndex(int idx) const;//get edges for face idx in projection

    virtual Base::BoundBox3d getBoundingBox() const;
    double getBoxX() const;
    double getBoxY() const;
    QRectF getRect() const override;
    virtual std::vector<DrawViewSection*>
    getSectionRefs() const;//are there ViewSections based on this ViewPart?
    virtual std::vector<DrawViewDetail*> getDetailRefs() const;


    virtual Base::Vector3d projectPoint(const Base::Vector3d& pt, bool invert = true) const;
    virtual Base::Vector3d inverseProjectPoint(const Base::Vector3d& pt, bool invert=true) const;
    virtual BaseGeomPtr projectEdge(const TopoDS_Edge& e) const;
    virtual BaseGeomPtrVector projectWire(const TopoDS_Wire& inWire) const;

    virtual gp_Ax2 getViewAxis(const Base::Vector3d& pt, const Base::Vector3d& direction,
                               const bool flip = true) const;
    virtual gp_Ax2 getProjectionCS(Base::Vector3d pt = Base::Vector3d(0.0, 0.0, 0.0)) const;
    virtual gp_Ax2 getRotatedCS(Base::Vector3d basePoint = Base::Vector3d(0.0, 0.0, 0.0)) const;
    virtual Base::Vector3d getXDirection() const;//don't use XDirection.getValue()
    virtual Base::Vector3d getOriginalCentroid() const;
    virtual Base::Vector3d getCurrentCentroid() const;
    virtual Base::Vector3d getLegacyX(const Base::Vector3d& pt, const Base::Vector3d& axis,
                                      const bool flip = true) const;
    gp_Ax2 localVectorToCS(const Base::Vector3d localUnit) const;
    Base::Vector3d localVectorToDirection(const Base::Vector3d localUnit) const;

    Base::Vector3d getLocalOrigin3d() const;
    Base::Vector3d getLocalOrigin2d() const;

    static bool handleFaces();
    static bool newFaceFinder();

    static TopoDS_Shape shapeShapeIntersect(const TopoDS_Shape& shape0,
                                            const TopoDS_Shape& shape1,
                                            Handle(Message_ProgressIndicator) pi = nullptr);
    static bool isTrulyEmpty(TopoDS_Shape inShape);

    bool isUnsetting() { return nowUnsetting; }

    virtual std::vector<TopoDS_Wire> getWireForFace(int idx) const;

    virtual TopoDS_Shape getSourceShape() const;
    virtual TopoDS_Shape getSourceShapeFused() const;
    virtual std::vector<TopoDS_Shape> getSourceShape2d() const;
    virtual TopoDS_Shape getShapeForDetail() const;

    TopoDS_Shape getShape() const;
    double getSizeAlongVector(Base::Vector3d alignmentVector);

    virtual void postHlrTasks();
    virtual void postFaceExtractionTasks();

    bool isIso() const;

    void clearCosmeticVertexes();
    void refreshCVGeoms();
    void addCosmeticVertexesToGeom();
    int add1CVToGV(std::string tag);
    int getCVIndex(std::string tag);

    void clearCosmeticEdges();
    void refreshCEGeoms();
    void addCosmeticEdgesToGeom();
    int add1CEToGE(std::string tag);

    void clearCenterLines();
    void refreshCLGeoms();
    void addCenterLinesToGeom();
    int add1CLToGE(std::string tag);

    void clearGeomFormats();

    void dumpVerts(const std::string text);
    void dumpCosVerts(const std::string text);
    void dumpCosEdges(const std::string text);

    std::string addReferenceVertex(Base::Vector3d v);
    void addReferencesToGeom();
    void removeReferenceVertex(std::string tag);
    void updateReferenceVert(std::string tag, Base::Vector3d loc2d);
    void removeAllReferencesFromGeom();
    void resetReferenceVerts();

    std::vector<App::DocumentObject*> getAllSources() const;

    bool waitingForFaces() const { return m_waitingForFaces; }
    bool waitingForHlr() const { return m_waitingForHlr; }
    virtual bool waitingForResult() const;

protected:
    void onHlrFinished(GeometryObjectPtr result);
    void onFacesFinished(std::shared_ptr<std::vector<FacePtr>> faces);
    void abortMakeGeometry();
    void waitingForFaces(bool s) { m_waitingForFaces = s; }
    void waitingForHlr(bool s) { m_waitingForHlr = s; }

    bool checkXDirection() const;

    TechDraw::GeometryObjectPtr m_geometryObject;
    Base::BoundBox3d bbox;

    void onChanged(const App::Property* prop) override;
    void unsetupObject() override;

    void buildGeometryObject(TopoDS_Shape& shape, const gp_Ax2& viewAxis);
    void makeGeometryForShape(TopoDS_Shape& shape);//const??
    void partExec(TopoDS_Shape& shape);
    void addShapes2d();

    struct ExtractFaceParams {
        std::string featureName;
        std::shared_ptr<Base::SequencerLauncher> progress;
        std::vector<BaseGeomPtr> goEdges;
        std::shared_ptr<std::vector<FacePtr>> faces;
    };
    static void extractFaces(const ExtractFaceParams &params);

    Base::Vector3d shapeCentroid;
    void getRunControl();

    bool m_handleFaces;

    TopoDS_Shape m_saveShape;     //TODO: make this a Property.  Part::TopoShapeProperty??
    Base::Vector3d m_saveCentroid;//centroid before centering shape in origin

    void handleChangedPropertyName(Base::XMLReader& reader, const char* TypeName,
                                   const char* PropName) override;

    bool prefHardViz();
    bool prefSeamViz();
    bool prefSmoothViz();
    bool prefIsoViz();
    bool prefHardHid();
    bool prefSeamHid();
    bool prefSmoothHid();
    bool prefIsoHid();
    int prefIsoCount();

    std::vector<TechDraw::VertexPtr> m_referenceVerts;

private:
    bool nowUnsetting = false;
    bool m_waitingForFaces = false;
    bool m_waitingForHlr = false;

    std::unique_ptr<QFutureWatcher<void>> m_hlrWatcher;
    std::unique_ptr<QFutureWatcher<void>> m_faceWatcher;
    std::shared_ptr<Base::SequencerLauncher> m_progress;
};

using DrawViewPartPython = App::FeaturePythonT<DrawViewPart>;

}//namespace TechDraw

#endif// #ifndef DrawViewPart_h_
