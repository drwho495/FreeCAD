/***************************************************************************
 *   Copyright (c) 2008 Jürgen Riegel <juergen.riegel@web.de>              *
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

#ifndef ROBOTGUI_PRECOMPILED_H
#define ROBOTGUI_PRECOMPILED_H

#include <FCConfig.h>

// Importing of App classes
#ifdef FC_OS_WIN32
# define RobotExport    __declspec(dllimport)
# define PartExport     __declspec(dllimport)
# define PartGuiExport  __declspec(dllimport)
# define RobotGuiExport __declspec(dllexport)
#else // for Linux
# define PartExport
# define RobotExport
# define PartGuiExport
# define RobotGuiExport
#endif

#ifdef _MSC_VER
# pragma warning(disable : 4005)
# pragma warning(disable : 4273)
#endif

#ifdef _PreComp_

// STL
#include <sstream>

// Qt
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <qobject.h>
#include <qpalette.h>
#include <QString>
#include <QTimer>

// Inventor
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/draggers/SoJackDragger.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoMarkerSet.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/VRMLnodes/SoVRMLTransform.h>

#endif //_PreComp_

#endif // ROBOTGUI_PRECOMPILED_H
