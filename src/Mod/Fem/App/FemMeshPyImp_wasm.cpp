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

// =============================================================================
// WebAssembly (emscripten) replacement for FemMeshPyImp.cpp
// -----------------------------------------------------------------------------
// The real FemMeshPyImp.cpp implements every FemMeshPy method by calling into
// SMESH/SMDS/SMESHDS (the mesher), none of which is compiled for the wasm build
// (no VTK/MED/HDF5). This translation unit provides self-contained stubs for the
// exact same set of methods with byte-identical signatures so that the FemMeshPy
// Python type links and the Fem module loads. FEM documents restore as empty
// shell meshes; these Python methods are never invoked on the restore path, so
// each simply raises NotImplementedError (mutating/query methods) or returns an
// empty/zero value (attribute getters).
//
// NOTE: no SMESH / SMDS / SMESHDS / StdMeshers / Driver / VTK / MED / HDF5
// headers are included here (FemMesh.h itself is already __EMSCRIPTEN__-guarded).
// =============================================================================

#include <Python.h>

#include <stdexcept>
#include <string>

#include <Base/Exception.h>

#include "Mod/Fem/App/FemMesh.h"

// clang-format off
// inclusion of the generated files (generated out of FemMesh.pyi)
#include "FemMeshPy.h"
#include "FemMeshPy.cpp"
// clang-format on


using namespace Fem;

namespace
{
// Common error raised by every unavailable mutating/query method.
inline PyObject* raiseUnavailable()
{
    PyErr_SetString(
        PyExc_NotImplementedError,
        "FemMesh Python API is unavailable in the WebAssembly build"
    );
    return nullptr;
}
}  // namespace

// returns a string which represents the object e.g. when printed in python
std::string FemMeshPy::representation() const
{
    return std::string("<FemMesh object>");
}

PyObject* FemMeshPy::PyMake(struct _typeobject*, PyObject*, PyObject*)  // Python wrapper
{
    // create a new instance of FemMeshPy and the Twin object
    return new FemMeshPy(new FemMesh);
}

// constructor method
int FemMeshPy::PyInit(PyObject* args, PyObject* /*kwd*/)
{
    PyObject* pcObj = nullptr;
    if (!PyArg_ParseTuple(args, "|O", &pcObj)) {
        return -1;
    }

    try {
        // if no mesh is given
        if (!pcObj) {
            return 0;
        }
        if (PyObject_TypeCheck(pcObj, &(FemMeshPy::Type))) {
            getFemMeshPtr()->operator=(*static_cast<FemMeshPy*>(pcObj)->getFemMeshPtr());
        }
        else {
            PyErr_Format(
                PyExc_TypeError,
                "Cannot create a FemMesh out of a '%s'",
                pcObj->ob_type->tp_name
            );
            return -1;
        }
    }
    catch (const Base::Exception& e) {
        e.setPyException();
        return -1;
    }
    catch (const std::exception& e) {
        PyErr_SetString(Base::PyExc_FC_GeneralError, e.what());
        return -1;
    }
    catch (const Py::Exception&) {
        return -1;
    }

    return 0;
}


// ===== Methods ============================================================
// All mutating / query methods are unavailable without SMESH; raise
// NotImplementedError. Signatures are kept byte-identical to FemMeshPyImp.cpp.

PyObject* FemMeshPy::setShape(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addHypothesis(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::setStandardHypotheses(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::compute(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addNode(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addEdge(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addFace(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addQuad(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addVolume(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addEdgeList(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addFaceList(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addVolumeList(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::copy(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::read(PyObject* /*args*/, PyObject* /*kwds*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::write(PyObject* /*args*/, PyObject* /*kwds*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::writeABAQUS(PyObject* /*args*/, PyObject* /*kwd*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::setTransform(PyObject* /*args*/)
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getFacesByFace(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getEdgesByEdge(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getVolumesByFace(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getccxVolumesByFace(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getNodeById(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getNodesBySolid(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getNodesByFace(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getNodesByEdge(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getNodesByVertex(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getElementNodes(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getNodeElements(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getGroupName(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getGroupElementType(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getGroupElements(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addGroup(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::addGroupElements(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::removeGroup(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::renameGroup(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getElementType(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

PyObject* FemMeshPy::getIdByElementType(PyObject* /*args*/) const
{
    return raiseUnavailable();
}

// ===== Attributes ============================================================
// Getters return the zero/empty value of the declared Py type. They must not
// throw (the generated getset code does not expect an exception here).

Py::Dict FemMeshPy::getNodes() const
{
    return Py::Dict();
}

Py::Long FemMeshPy::getNodeCount() const
{
    return Py::Long(0);
}

Py::Tuple FemMeshPy::getEdges() const
{
    return Py::Tuple();
}

Py::Tuple FemMeshPy::getEdgesOnly() const
{
    return Py::Tuple();
}

Py::Long FemMeshPy::getEdgeCount() const
{
    return Py::Long(0);
}

Py::Tuple FemMeshPy::getFaces() const
{
    return Py::Tuple();
}

Py::Tuple FemMeshPy::getFacesOnly() const
{
    return Py::Tuple();
}

Py::Long FemMeshPy::getFaceCount() const
{
    return Py::Long(0);
}

Py::Long FemMeshPy::getTriangleCount() const
{
    return Py::Long(0);
}

Py::Long FemMeshPy::getQuadrangleCount() const
{
    return Py::Long(0);
}

Py::Long FemMeshPy::getPolygonCount() const
{
    return Py::Long(0);
}

Py::Tuple FemMeshPy::getVolumes() const
{
    return Py::Tuple();
}

Py::Long FemMeshPy::getVolumeCount() const
{
    return Py::Long(0);
}

Py::Long FemMeshPy::getTetraCount() const
{
    return Py::Long(0);
}

Py::Long FemMeshPy::getHexaCount() const
{
    return Py::Long(0);
}

Py::Long FemMeshPy::getPyramidCount() const
{
    return Py::Long(0);
}

Py::Long FemMeshPy::getPrismCount() const
{
    return Py::Long(0);
}

Py::Long FemMeshPy::getPolyhedronCount() const
{
    return Py::Long(0);
}

Py::Long FemMeshPy::getSubMeshCount() const
{
    return Py::Long(0);
}

Py::Long FemMeshPy::getGroupCount() const
{
    return Py::Long(0);
}

Py::Tuple FemMeshPy::getGroups() const
{
    return Py::Tuple();
}

Py::Object FemMeshPy::getVolume() const
{
    return Py::Object();
}

// ===== custom attributes ============================================================

PyObject* FemMeshPy::getCustomAttributes(const char* /*attr*/) const
{
    return nullptr;
}

int FemMeshPy::setCustomAttributes(const char* /*attr*/, PyObject* /*obj*/)
{
    return 0;
}
