/***************************************************************************
 *   Copyright (c) 2015 Stefan Tröger <stefantroeger@gmx.net>              *
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
# include <Python.h>
# include <vtkCompositeDataSet.h>
# include <vtkMultiBlockDataSet.h>
# include <vtkMultiPieceDataSet.h>
# include <vtkPolyData.h>
# include <vtkRectilinearGrid.h>
# include <vtkStructuredGrid.h>
# include <vtkUnstructuredGrid.h>
# include <vtkUniformGrid.h>
# include <vtkXMLDataSetWriter.h>
# include <vtkXMLImageDataReader.h>
# include <vtkXMLPolyDataReader.h>
# include <vtkXMLRectilinearGridReader.h>
# include <vtkXMLStructuredGridReader.h>
# include <vtkXMLUnstructuredGridReader.h>
#endif

#include <App/Application.h>
#include <App/DocumentObject.h>
#include <Base/Console.h>
#include <Base/FileInfo.h>
#include <Base/Reader.h>
#include <Base/Stream.h>
#include <Base/Writer.h>
#include <CXX/Objects.hxx>

#include "PropertyPostDataObject.h"


using namespace Fem;

TYPESYSTEM_SOURCE(Fem::PropertyPostDataObject , App::Property)

PropertyPostDataObject::PropertyPostDataObject()
{
}

PropertyPostDataObject::~PropertyPostDataObject()
{
}

void PropertyPostDataObject::scaleDataObject(vtkDataObject *dataObject, double s)
{
    auto scalePoints = [](vtkPoints *points, double s) {
        for (vtkIdType i = 0; i < points->GetNumberOfPoints(); i++) {
            double xyz[3];
            points->GetPoint(i, xyz);
            for (double & j : xyz)
                j *= s;
            points->SetPoint(i, xyz);
        }
    };

    if (dataObject->GetDataObjectType() == VTK_POLY_DATA) {
        vtkPolyData *dataSet = vtkPolyData::SafeDownCast(dataObject);
        scalePoints(dataSet->GetPoints(), s);
    }
    else if (dataObject->GetDataObjectType() == VTK_STRUCTURED_GRID) {
        vtkStructuredGrid *dataSet = vtkStructuredGrid::SafeDownCast(dataObject);
        scalePoints(dataSet->GetPoints(), s);
    }
    else if (dataObject->GetDataObjectType() == VTK_UNSTRUCTURED_GRID) {
        vtkUnstructuredGrid *dataSet = vtkUnstructuredGrid::SafeDownCast(dataObject);
        scalePoints(dataSet->GetPoints(), s);
    }
    else if (dataObject->GetDataObjectType() == VTK_MULTIBLOCK_DATA_SET) {
        vtkMultiBlockDataSet *dataSet = vtkMultiBlockDataSet::SafeDownCast(dataObject);
        for (unsigned int i = 0; i < dataSet->GetNumberOfBlocks(); i++)
            scaleDataObject(dataSet->GetBlock(i), s);
    }
    else if (dataObject->GetDataObjectType() == VTK_MULTIPIECE_DATA_SET) {
        vtkMultiPieceDataSet *dataSet = vtkMultiPieceDataSet::SafeDownCast(dataObject);
        for (unsigned int i = 0; i < dataSet->GetNumberOfPieces(); i++)
            scaleDataObject(dataSet->GetPiece(i), s);
    }
}

void PropertyPostDataObject::scale(double s)
{
    if (m_dataObject) {
        aboutToSetValue();
        scaleDataObject(m_dataObject, s);
        hasSetValue();
    }
}

void PropertyPostDataObject::setValue(const vtkSmartPointer<vtkDataObject> &ds)
{
    aboutToSetValue();

    if (ds) {
        createDataObjectByExternalType(ds);
        m_dataObject->DeepCopy(ds);
    }
    else {
        m_dataObject = nullptr;
    }

    hasSetValue();
}

const vtkSmartPointer<vtkDataObject> &PropertyPostDataObject::getValue() const
{
    return m_dataObject;
}

bool PropertyPostDataObject::isComposite()
{

    return m_dataObject && !m_dataObject->IsA("vtkDataSet");
}

bool PropertyPostDataObject::isDataSet()
{

    return m_dataObject && m_dataObject->IsA("vtkDataSet");
}

int PropertyPostDataObject::getDataType()
{

    if (!m_dataObject)
        return -1;

    return m_dataObject->GetDataObjectType();
}


PyObject *PropertyPostDataObject::getPyObject()
{
    //TODO: fetch the vtk python object from the data set and return it
    return Py::new_reference_to(Py::None());
}

void PropertyPostDataObject::setPyObject(PyObject * /*value*/)
{
}

App::Property *PropertyPostDataObject::Copy() const
{
    PropertyPostDataObject *prop = new PropertyPostDataObject();
    if (m_dataObject) {

        prop->createDataObjectByExternalType(m_dataObject);
        prop->m_dataObject->DeepCopy(m_dataObject);
    }

    return prop;
}

void PropertyPostDataObject::createDataObjectByExternalType(vtkSmartPointer<vtkDataObject> ex)
{

    switch (ex->GetDataObjectType()) {

        case VTK_POLY_DATA:
            m_dataObject = vtkSmartPointer<vtkPolyData>::New();
            break;
        case VTK_STRUCTURED_GRID:
            m_dataObject = vtkSmartPointer<vtkStructuredGrid>::New();
            break;
        case VTK_RECTILINEAR_GRID:
            m_dataObject = vtkSmartPointer<vtkRectilinearGrid>::New();
            break;
        case VTK_UNSTRUCTURED_GRID:
            m_dataObject = vtkSmartPointer<vtkUnstructuredGrid>::New();
            break;
        case VTK_UNIFORM_GRID:
            m_dataObject = vtkSmartPointer<vtkUniformGrid>::New();
            break;
        case VTK_COMPOSITE_DATA_SET:
            m_dataObject = vtkCompositeDataSet::New();
            break;
        case VTK_MULTIBLOCK_DATA_SET:
            m_dataObject = vtkSmartPointer<vtkMultiBlockDataSet>::New();
            break;
        case VTK_MULTIPIECE_DATA_SET:
            m_dataObject = vtkSmartPointer<vtkMultiPieceDataSet>::New();
            break;
        default:
            break;
    };
}


void PropertyPostDataObject::Paste(const App::Property &from)
{
    aboutToSetValue();
    m_dataObject = dynamic_cast<const PropertyPostDataObject &>(from).m_dataObject;
    hasSetValue();
}

unsigned int PropertyPostDataObject::getMemSize() const
{
    return m_dataObject ? m_dataObject->GetActualMemorySize() : 0;
}

void PropertyPostDataObject::getPaths(std::vector<App::ObjectIdentifier>& /*paths*/) const
{
 /* paths.push_back(App::ObjectIdentifier(getContainer())
                    << App::ObjectIdentifier::Component::SimpleComponent(getName())
                    << App::ObjectIdentifier::Component::SimpleComponent(
                           App::ObjectIdentifier::String("ShapeType")));
    paths.push_back(App::ObjectIdentifier(getContainer())
                    << App::ObjectIdentifier::Component::SimpleComponent(getName())
                    << App::ObjectIdentifier::Component::SimpleComponent(
                           App::ObjectIdentifier::String("Orientation")));
    paths.push_back(App::ObjectIdentifier(getContainer())
                    << App::ObjectIdentifier::Component::SimpleComponent(getName())
                    << App::ObjectIdentifier::Component::SimpleComponent(
                           App::ObjectIdentifier::String("Length")));
    paths.push_back(App::ObjectIdentifier(getContainer())
                    << App::ObjectIdentifier::Component::SimpleComponent(getName())
                    << App::ObjectIdentifier::Component::SimpleComponent(
                           App::ObjectIdentifier::String("Area")));
    paths.push_back(App::ObjectIdentifier(getContainer())
                    << App::ObjectIdentifier::Component::SimpleComponent(getName())
                    << App::ObjectIdentifier::Component::SimpleComponent(
                           App::ObjectIdentifier::String("Volume")));
    */
}

void PropertyPostDataObject::Save(Base::Writer &writer) const
{
    std::string extension;
    if(!m_dataObject) {
        writer.Stream() << writer.ind() << "<Data/>\n";
        return;
    }

    bool forceXML = writer.isForceXML() > 1;
    if(!forceXML)
        extension = ".";

    switch (m_dataObject->GetDataObjectType()) {

        case VTK_POLY_DATA:
            extension += "vtp";
            break;
        case VTK_STRUCTURED_GRID:
            extension += "vts";
            break;
        case VTK_RECTILINEAR_GRID:
            extension += "vtr";
            break;
        case VTK_UNSTRUCTURED_GRID:
            extension += "vtu";
            break;
        case VTK_UNIFORM_GRID:
            extension += "vti"; //image data
            break;
            //TODO:multi-datasets use multiple files, this needs to be implemented specially
//         case VTK_COMPOSITE_DATA_SET:
//             prop->m_dataObject = vtkCompositeDataSet::New();
//             break;
//         case VTK_MULTIBLOCK_DATA_SET:
//             prop->m_dataObject = vtkMultiBlockDataSet::New();
//             break;
//         case VTK_MULTIPIECE_DATA_SET:
//             prop->m_dataObject = vtkMultiPieceDataSet::New();
//             break;
        default:
            break;
    };

    if(forceXML) {
        writer.Stream() << writer.ind() << "<Data cdata=\""
            << extension << "\"/>\n";
        save(writer.beginCharStream(false) << '\n', writer);
        writer.endCharStream() << '\n' << writer.ind() << "</Data>\n";
    } else {
        writer.Stream() << writer.ind() << "<Data file=\""
                        << writer.addFile(getFileName(extension.c_str()), this)
                        << "\"/>\n";
    }
}

void PropertyPostDataObject::Restore(Base::XMLReader &reader)
{
    reader.readElement("Data");

    std::string ext = reader.getAttribute("cdata","");
    if(ext.size()) {
        restore(reader.beginCharStream(false),ext);
        return;
    } else if(!reader.hasAttribute("file"))
        return;

    std::string file(reader.getAttribute("file"));

    if (!file.empty()) {
        // initiate a file read
        reader.addFile(file.c_str(), this);
    }
}

void PropertyPostDataObject::SaveDocFile(Base::Writer &writer) const
{
    save(writer.Stream(),writer);
}

void PropertyPostDataObject::save(std::ostream &s, Base::Writer &writer) const
{
    // If the shape is empty we simply store nothing. The file size will be 0 which
    // can be checked when reading in the data.
    if (!m_dataObject)
        return;

    Base::FileInfo fi(App::Application::getTempFileName(), true);

    vtkSmartPointer<vtkXMLDataSetWriter> xmlWriter = vtkSmartPointer<vtkXMLDataSetWriter>::New();
    xmlWriter->SetInputDataObject(m_dataObject);
    xmlWriter->SetFileName(fi.filePath().c_str());
    if(writer.isPreferBinary())
        xmlWriter->SetDataModeToBinary();
    else
        xmlWriter->SetDataModeToAscii();

#ifdef VTK_CELL_ARRAY_V2
    // Looks like an invalid data object that causes a crash with vtk9
    vtkUnstructuredGrid *dataGrid = vtkUnstructuredGrid::SafeDownCast(m_dataObject);
    if (dataGrid && (dataGrid->GetPiece() < 0 || dataGrid->GetNumberOfPoints() <= 0)) {
        std::cerr << "PropertyPostDataObject::SaveDocFile: ignore empty vtkUnstructuredGrid\n";
        return;
    }
#endif

    if (xmlWriter->Write() != 1) {
        // Note: Do NOT throw an exception here because if the tmp. file could
        // not be created we should not abort.
        // We only print an error message but continue writing the next files to the
        // stream...
        App::PropertyContainer *father = this->getContainer();
        if (father && father->isDerivedFrom(App::DocumentObject::getClassTypeId())) {
            App::DocumentObject *obj = static_cast<App::DocumentObject *>(father);
            Base::Console().Error("Dataset of '%s' cannot be written to vtk file '%s'\n",
                                  obj->Label.getValue(), fi.filePath().c_str());
        }
        else {
            Base::Console().Error("Cannot save vtk file '%s'\n", fi.filePath().c_str());
        }

        std::stringstream ss;
        ss << "Cannot save vtk file '" << fi.filePath() << "'";
        writer.addError(ss.str());
    }

    Base::ifstream file(fi, std::ios::in | std::ios::binary);
    if (file){
        std::streambuf* buf = file.rdbuf();
        s << buf;
    }

    file.close();
}

void PropertyPostDataObject::RestoreDocFile(Base::Reader &reader)
{
    Base::FileInfo xml(reader.getFileName());
    restore(reader,xml.extension());
}

void PropertyPostDataObject::restore(std::istream &reader, const std::string &extension) {

    // create a temporary file and copy the content from the zip stream
    Base::FileInfo fi(App::Application::getTempFileName());

    // read in the ASCII file and write back to the file stream
    Base::ofstream file(fi, std::ios::out | std::ios::binary);
    unsigned long ulSize = 0;
    if (reader) {
        std::streambuf *buf = file.rdbuf();
        reader >> buf;
        file.flush();
        ulSize = buf->pubseekoff(0, std::ios::cur, std::ios::in);
    }
    file.close();

    // Read the data from the temp file
    if (ulSize > 0) {
        // TODO: read in of composite data structures need to be coded,
        // including replace of "GetOutputAsDataSet()"
        vtkSmartPointer<vtkXMLReader> xmlReader;
        if (extension == "vtp")
            xmlReader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
        else if (extension == "vts")
            xmlReader = vtkSmartPointer<vtkXMLStructuredGridReader>::New();
        else if (extension == "vtr")
            xmlReader = vtkSmartPointer<vtkXMLRectilinearGridReader>::New();
        else if (extension == "vtu")
            xmlReader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
        else if (extension == "vti")
            xmlReader = vtkSmartPointer<vtkXMLImageDataReader>::New();

        xmlReader->SetFileName(fi.filePath().c_str());
        xmlReader->Update();

        if (!xmlReader->GetOutputAsDataSet()) {
            // Note: Do NOT throw an exception here because if the tmp. created file could
            // not be read it's NOT an indication for an invalid input stream 'reader'.
            // We only print an error message but continue reading the next files from the
            // stream...
            App::PropertyContainer *father = this->getContainer();
            if (father && father->isDerivedFrom(App::DocumentObject::getClassTypeId())) {
                App::DocumentObject *obj = static_cast<App::DocumentObject *>(father);
                Base::Console().Error("Dataset file '%s' with data of '%s' seems to be empty\n",
                                      fi.filePath().c_str(), obj->Label.getValue());
            }
            else {
                Base::Console().Warning("Loaded Dataset file '%s' seems to be empty\n",
                                        fi.filePath().c_str());
            }
        }
        else {
            aboutToSetValue();
            createDataObjectByExternalType(xmlReader->GetOutputAsDataSet());
            m_dataObject->DeepCopy(xmlReader->GetOutputAsDataSet());
            hasSetValue();
        }
    }

    // delete the temp file
    fi.deleteFile();
}
