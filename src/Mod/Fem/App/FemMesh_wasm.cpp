/***************************************************************************
 *   Copyright (c) 2009 Jürgen Riegel <juergen.riegel@web.de>              *
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

// ===========================================================================
// WebAssembly (emscripten) stub implementation of Fem::FemMesh.
//
// The desktop implementation of FemMesh lives in FemMesh.cpp and links against
// SMESH / SMDS / SMESHDS / StdMeshers / Driver / VTK / MED / HDF5. NONE of
// those libraries are compiled for the wasm target, so this translation unit
// replaces FemMesh.cpp there (see CMakeLists.txt) and provides a completely
// self-contained implementation that:
//
//   * lets a FEM document (e.g. FEMExample.FCStd) LOAD with its mesh objects
//     present as EMPTY SHELLS (no geometry, no nodes, no elements), and
//   * preserves the FreeCAD file-restore plumbing so the document loader stays
//     in sync (Save registers the doc-file, Restore re-registers it so the
//     loader still calls RestoreDocFile, which then drops the mesh payload).
//
// Meshing, solving and mesh IO never run in the browser build, so every
// operation that would need SMESH throws Base::NotImplementedError. Read-only
// query/geometry accessors degrade gracefully by returning empty/default
// values (never throw) because inspection and rendering code may touch them.
//
// IMPORTANT: this file must NOT include anything from SMESH/SMDS/SMESHDS/
// StdMeshers/Driver/VTK/MED/HDF5. The pointer members (myMesh, _mesh_gen) are
// forward-declared incomplete types from FemMesh.h and are always null here;
// they are never dereferenced or deleted.
// ===========================================================================

#include "PreCompiled.h"

#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <App/ComplexGeoData.h>
#include <Base/BoundBox.h>
#include <Base/Exception.h>
#include <Base/Matrix.h>
#include <Base/Quantity.h>
#include <Base/Reader.h>
#include <Base/Unit.h>
#include <Base/Writer.h>

#include "FemMesh.h"


using namespace Fem;

// SMESH_Gen is an incomplete forward-declared type in the wasm build; a null
// pointer definition is all that is needed. It is never dereferenced/deleted.
SMESH_Gen* FemMesh::_mesh_gen = nullptr;

TYPESYSTEM_SOURCE(Fem::FemMesh, Base::Persistence)

namespace
{
// Central helper so every unavailable operation reports the same message.
[[noreturn]] void femMeshUnavailable()
{
    throw Base::NotImplementedError(
        "FemMesh: meshing/IO is not available in the WebAssembly build"
    );
}
}  // namespace


// ==== Construction / destruction ===========================================
// No SMESH generator, no CreateMesh(): myMesh stays null, _Mtrx default-inits.

FemMesh::FemMesh()
    : myMesh(nullptr)
{}

FemMesh::FemMesh(const FemMesh& mesh)
    : myMesh(nullptr)
{
    // Only the placement matrix is copyable without SMESH; there is no mesh
    // data to duplicate in the wasm build.
    _Mtrx = mesh._Mtrx;
}

FemMesh::~FemMesh()
{
    // myMesh is an incomplete, always-null pointer here: nothing to delete.
}

FemMesh& FemMesh::operator=(const FemMesh& mesh)
{
    if (this != &mesh) {
        _Mtrx = mesh._Mtrx;
        // myMesh remains nullptr.
    }
    return *this;
}

void FemMesh::copyMeshData(const FemMesh& /*mesh*/)
{
    // Not reachable on the load path; copying mesh data needs SMESH.
    femMeshUnavailable();
}


// ==== SMESH accessors (always null in wasm) ================================

const SMESH_Mesh* FemMesh::getSMesh() const
{
    return myMesh;
}

SMESH_Mesh* FemMesh::getSMesh()
{
    return myMesh;
}

SMESH_Gen* FemMesh::getGenerator()
{
    // No SMESH generator exists in the wasm build.
    return nullptr;
}


// ==== Meshing / hypotheses (unavailable) ===================================

void FemMesh::addHypothesis(const TopoDS_Shape& /*aSubShape*/, SMESH_HypothesisPtr /*hyp*/)
{
    femMeshUnavailable();
}

void FemMesh::setStandardHypotheses()
{
    femMeshUnavailable();
}

void FemMesh::compute()
{
    femMeshUnavailable();
}


// ==== Search / retrieval accessors (degrade gracefully, never throw) =======

std::set<long> FemMesh::getSurfaceNodes(long /*ElemId*/, short /*FaceId*/, float /*Angle*/) const
{
    return {};
}

std::list<std::pair<int, int>> FemMesh::getVolumesByFace(const TopoDS_Face& /*face*/) const
{
    return {};
}

std::list<int> FemMesh::getFacesByFace(const TopoDS_Face& /*face*/) const
{
    return {};
}

std::list<int> FemMesh::getEdgesByEdge(const TopoDS_Edge& /*edge*/) const
{
    return {};
}

std::map<int, int> FemMesh::getccxVolumesByFace(const TopoDS_Face& /*face*/) const
{
    return {};
}

std::set<int> FemMesh::getNodesBySolid(const TopoDS_Solid& /*solid*/) const
{
    return {};
}

std::set<int> FemMesh::getNodesByFace(const TopoDS_Face& /*face*/) const
{
    return {};
}

std::set<int> FemMesh::getNodesByEdge(const TopoDS_Edge& /*edge*/) const
{
    return {};
}

std::set<int> FemMesh::getNodesByVertex(const TopoDS_Vertex& /*vertex*/) const
{
    return {};
}

std::list<int> FemMesh::getElementNodes(int /*id*/) const
{
    return {};
}

std::list<int> FemMesh::getNodeElements(int /*id*/, SMDSAbs_ElementType /*type*/) const
{
    return {};
}

std::set<int> FemMesh::getEdgesOnly() const
{
    return {};
}

std::set<int> FemMesh::getFacesOnly() const
{
    return {};
}


// ==== Persistence ==========================================================
// The document-file registration plumbing is IDENTICAL to the desktop build.
//
// FemMesh (not PropertyFemMesh) owns the doc-file registration: PropertyFemMesh
// merely forwards Save/Restore/SaveDocFile/RestoreDocFile straight through to
// this FemMesh. We therefore MUST keep Save's writer.addFile(...) and Restore's
// reader.addFile(...) so that at load time the document reader still schedules
// and invokes RestoreDocFile for this object. Only the SMESH mesh-payload
// read/write (previously done in SaveDocFile/RestoreDocFile) is dropped.

unsigned int FemMesh::getMemSize() const
{
    return 0;
}

void FemMesh::Save(Base::Writer& writer) const
{
    // Verbatim from the desktop build: no SMESH is touched here. Registering the
    // doc-file keeps the save/restore round-trip symmetric so RestoreDocFile is
    // invoked on load.
    if (!writer.isForceXML()) {
        // See SaveDocFile(), RestoreDocFile()
        writer.Stream() << writer.ind() << "<FemMesh file=\"";
        writer.Stream() << writer.addFile("FemMesh.unv", this) << "\"";
        writer.Stream() << " a11=\"" << _Mtrx[0][0] << "\" a12=\"" << _Mtrx[0][1] << "\" a13=\""
                        << _Mtrx[0][2] << "\" a14=\"" << _Mtrx[0][3] << "\"";
        writer.Stream() << " a21=\"" << _Mtrx[1][0] << "\" a22=\"" << _Mtrx[1][1] << "\" a23=\""
                        << _Mtrx[1][2] << "\" a24=\"" << _Mtrx[1][3] << "\"";
        writer.Stream() << " a31=\"" << _Mtrx[2][0] << "\" a32=\"" << _Mtrx[2][1] << "\" a33=\""
                        << _Mtrx[2][2] << "\" a34=\"" << _Mtrx[2][3] << "\"";
        writer.Stream() << " a41=\"" << _Mtrx[3][0] << "\" a42=\"" << _Mtrx[3][1] << "\" a43=\""
                        << _Mtrx[3][2] << "\" a44=\"" << _Mtrx[3][3] << "\"";
        writer.Stream() << "/>" << std::endl;
    }
    else {
        writer.Stream() << writer.ind() << "<FemMesh file=\"\"";
        writer.Stream() << " a11=\"" << _Mtrx[0][0] << "\" a12=\"" << _Mtrx[0][1] << "\" a13=\""
                        << _Mtrx[0][2] << "\" a14=\"" << _Mtrx[0][3] << "\"";
        writer.Stream() << " a21=\"" << _Mtrx[1][0] << "\" a22=\"" << _Mtrx[1][1] << "\" a23=\""
                        << _Mtrx[1][2] << "\" a24=\"" << _Mtrx[1][3] << "\"";
        writer.Stream() << " a31=\"" << _Mtrx[2][0] << "\" a32=\"" << _Mtrx[2][1] << "\" a33=\""
                        << _Mtrx[2][2] << "\" a34=\"" << _Mtrx[2][3] << "\"";
        writer.Stream() << " a41=\"" << _Mtrx[3][0] << "\" a42=\"" << _Mtrx[3][1] << "\" a43=\""
                        << _Mtrx[3][2] << "\" a44=\"" << _Mtrx[3][3] << "\"";
        writer.Stream() << "/>" << std::endl;
    }
}

void FemMesh::Restore(Base::XMLReader& reader)
{
    // Verbatim from the desktop build: no SMESH is touched here. Re-registering
    // the referenced doc-file causes the loader to call RestoreDocFile below.
    reader.readElement("FemMesh");
    std::string file(reader.getAttribute<const char*>("file"));

    if (!file.empty()) {
        // initiate a file read
        reader.addFile(file.c_str(), this);
    }
    if (reader.hasAttribute("a11")) {
        _Mtrx[0][0] = reader.getAttribute<double>("a11");
        _Mtrx[0][1] = reader.getAttribute<double>("a12");
        _Mtrx[0][2] = reader.getAttribute<double>("a13");
        _Mtrx[0][3] = reader.getAttribute<double>("a14");

        _Mtrx[1][0] = reader.getAttribute<double>("a21");
        _Mtrx[1][1] = reader.getAttribute<double>("a22");
        _Mtrx[1][2] = reader.getAttribute<double>("a23");
        _Mtrx[1][3] = reader.getAttribute<double>("a24");

        _Mtrx[2][0] = reader.getAttribute<double>("a31");
        _Mtrx[2][1] = reader.getAttribute<double>("a32");
        _Mtrx[2][2] = reader.getAttribute<double>("a33");
        _Mtrx[2][3] = reader.getAttribute<double>("a34");

        _Mtrx[3][0] = reader.getAttribute<double>("a41");
        _Mtrx[3][1] = reader.getAttribute<double>("a42");
        _Mtrx[3][2] = reader.getAttribute<double>("a43");
        _Mtrx[3][3] = reader.getAttribute<double>("a44");
    }
}

void FemMesh::SaveDocFile(Base::Writer& /*writer*/) const
{
    // wasm build: the SMESH mesh payload is dropped. There is no mesh data to
    // export (the desktop build would ExportUNV() here), so we write an empty
    // doc-file entry. This keeps the save path crash-free; the resulting file
    // is empty and reloads as an empty mesh.
}

void FemMesh::RestoreDocFile(Base::Reader& /*reader*/)
{
    // wasm build: no-op. We intentionally read nothing and leave the mesh empty.
    // The document loader's readFiles() advances to the next zip entry via
    // getNextEntry() regardless of how much of this entry was consumed, and it
    // explicitly tolerates entries it cannot fully read, so skipping the SMESH
    // mesh payload here is safe.
}


// ==== Placement / geometry =================================================

void FemMesh::transformGeometry(const Base::Matrix4D& /*rclTrf*/)
{
    // No geometry (nodes) to bake the transform into in the wasm build.
}

void FemMesh::setTransform(const Base::Matrix4D& rclTrf)
{
    // Placement handling, no geometric transformation
    _Mtrx = rclTrf;
}

Base::Matrix4D FemMesh::getTransform() const
{
    return _Mtrx;
}

Base::BoundBox3d FemMesh::getBoundBox() const
{
    // Empty mesh -> invalid/empty bounding box.
    return Base::BoundBox3d();
}


// ==== Subelement / point accessors (degrade gracefully) ====================

std::vector<const char*> FemMesh::getElementTypes() const
{
    // Same static type list as the desktop build (no SMESH needed).
    std::vector<const char*> temp;
    temp.push_back("Vertex");
    temp.push_back("Edge");
    temp.push_back("Face");
    temp.push_back("Volume");

    return temp;
}

unsigned long FemMesh::countSubElements(const char* /*Type*/) const
{
    return 0;
}

Data::Segment* FemMesh::getSubElement(const char* /*Type*/, unsigned long /*n*/) const
{
    return nullptr;
}

void FemMesh::getPoints(
    std::vector<Base::Vector3d>& Points,
    std::vector<Base::Vector3d>& Normals,
    double /*Accuracy*/,
    uint16_t /*flags*/
) const
{
    // Empty mesh: no points/normals.
    Points.clear();
    Normals.clear();
}

struct Fem::FemMesh::FemMeshInfo FemMesh::getInfo() const
{
    // Zero-filled: an empty mesh has no nodes/elements of any kind.
    struct FemMeshInfo rtrn {};
    rtrn.numFaces = 0;
    rtrn.numNode = 0;
    rtrn.numTria = 0;
    rtrn.numQuad = 0;
    rtrn.numPoly = 0;
    rtrn.numVolu = 0;
    rtrn.numTetr = 0;
    rtrn.numHexa = 0;
    rtrn.numPyrd = 0;
    rtrn.numPris = 0;
    rtrn.numHedr = 0;

    return rtrn;
}

Base::Quantity FemMesh::getVolume() const
{
    return Base::Quantity(0.0, Base::Unit::Volume);
}


// ==== Group management (unavailable) =======================================

int FemMesh::addGroup(const std::string /*TypeString*/, const std::string /*Name*/, const int /*theId*/)
{
    femMeshUnavailable();
}

void FemMesh::addGroupElements(int /*GroupId*/, const std::set<int>& /*ElementIds*/)
{
    femMeshUnavailable();
}

bool FemMesh::removeGroup(int /*GroupId*/)
{
    femMeshUnavailable();
}

void FemMesh::renameGroup(int /*id*/, const std::string& /*name*/)
{
    femMeshUnavailable();
}


// ==== Import / export (unavailable) ========================================

void FemMesh::read(const char* /*FileName*/)
{
    femMeshUnavailable();
}

void FemMesh::readVTKWithGroups(const char* /*FileName*/, const char* /*vtk_group_cell_array*/)
{
    femMeshUnavailable();
}

void FemMesh::write(const char* /*FileName*/) const
{
    femMeshUnavailable();
}

void FemMesh::writeABAQUS(
    const std::string& /*Filename*/,
    int /*elemParam*/,
    bool /*groupParam*/,
    ABAQUS_VolumeVariant /*volVariant*/,
    ABAQUS_FaceVariant /*faceVariant*/,
    ABAQUS_EdgeVariant /*edgeVariant*/
) const
{
    femMeshUnavailable();
}

void FemMesh::writeVTK(const std::string& /*FileName*/, bool /*highest*/) const
{
    femMeshUnavailable();
}

void FemMesh::writeVTKWithGroups(
    const std::string& /*FileName*/,
    const std::string& /*vtk_group_cell_array*/,
    std::map<std::string, int> /*name_to_id*/,
    bool /*highest*/
)
{
    femMeshUnavailable();
}

void FemMesh::writeZ88(const std::string& /*FileName*/) const
{
    femMeshUnavailable();
}

void FemMesh::readNastran(const std::string& /*Filename*/)
{
    femMeshUnavailable();
}

void FemMesh::readNastran95(const std::string& /*Filename*/)
{
    femMeshUnavailable();
}

void FemMesh::readZ88(const std::string& /*Filename*/)
{
    femMeshUnavailable();
}

void FemMesh::readAbaqus(const std::string& /*Filename*/)
{
    femMeshUnavailable();
}
