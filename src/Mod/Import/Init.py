# SPDX-License-Identifier: LGPL-2.1-or-later

# FreeCAD init script of the Import module
# (c) 2001 Juergen Riegel

# ***************************************************************************
# *   Copyright (c) 2002 Juergen Riegel <juergen.riegel@web.de>             *
# *                                                                         *
# *   This file is part of the FreeCAD CAx development system.              *
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU Lesser General Public License (LGPL)    *
# *   as published by the Free Software Foundation; either version 2 of     *
# *   the License, or (at your option) any later version.                   *
# *   for detail see the LICENCE text file.                                 *
# *                                                                         *
# *   FreeCAD is distributed in the hope that it will be useful,            *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU Lesser General Public License for more details.                   *
# *                                                                         *
# *   You should have received a copy of the GNU Library General Public     *
# *   License along with FreeCAD; if not, write to the Free Software        *
# *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
# *   USA                                                                   *
# *                                                                         *
# ***************************************************************************/

import FreeCAD

translate = FreeCAD.Qt.translate

# Append the open handler
# FreeCAD.addImportType("STEP 214 (*.step *.stp)","ImportGui")
# FreeCAD.addExportType("STEP 214 (*.step *.stp)","ImportGui")
# FreeCAD.addExportType("IGES files (*.iges *.igs)","ImportGui")
FreeCAD.addImportType("PLMXML files (*.plmxml *.PLMXML)", "PlmXmlParser")
FreeCAD.addImportType("STEPZ Zip File Type (*.stpZ *.stpz *.STPZ)", "stepZ")
FreeCAD.addImportType("glTF (*.gltf *.GLTF *.glb *.GLB)", "ImportGui")
FreeCAD.addTranslatableExportType(
    translate("FileFormat", "STEPZ (Zipped STEP)"), ["stpZ", "stpz"], "stepZ"
)
FreeCAD.addExportType("glTF (*.gltf *.glb)", "ImportGui")

# Native DXF import/export.
#
# The DXF reader/writer (Import.readDXF / Import.writeDXFShape / writeDXFObject)
# is compiled into this Import module (src/Mod/Import/App/dxf). The actual
# open/insert/export dispatch for *.dxf is registered by Draft/Init.py
# ("importDXF"), whose modern path calls Import.readDXF / ImportGui.readDXF.
# Import.open()/insert() themselves only handle STEP/IGES/glTF, so we do not
# re-register *.dxf -> "Import" here (that would create a non-functional,
# duplicate handler that raises "no supported file format").
#
# What we DO enforce is that the native C++ importer is used: the legacy
# pure-Python DXF importer (dxfUseLegacyImporter=True) calls getDXFlibs(),
# which downloads the dxf library over the network at import time. That is
# impossible in the wasm build (no network / no subprocess), so pin the
# preference to the native reader.
_dxfGrp = FreeCAD.ParamGet("User parameter:BaseApp/Preferences/Mod/Draft")
_dxfGrp.SetBool("dxfUseLegacyImporter", False)
_dxfGrp.SetBool("dxfUseLegacyExporter", False)
