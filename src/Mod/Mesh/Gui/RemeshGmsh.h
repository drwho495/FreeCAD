// SPDX-License-Identifier: LGPL-2.1-or-later

/***************************************************************************
 *   Copyright (c) 2020 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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

#pragma once

#include <Mod/Mesh/MeshGlobal.h>

#include <memory>
#include <QDialog>
#ifndef __EMSCRIPTEN__
// The gmsh-based remesh feature drives an external gmsh process via QProcess,
// which is unavailable in the WebAssembly build (no subprocess in the browser).
#include <QProcess>
#endif

#include <Gui/TaskView/TaskDialog.h>
#include <Gui/TaskView/TaskView.h>
#include <Mod/Mesh/App/Core/MeshKernel.h>


namespace Mesh
{
class Feature;
}

namespace Gui
{
class StatusWidget;
}

#ifndef __EMSCRIPTEN__
// The entire gmsh-remesh UI relies on QProcess (implemented in RemeshGmsh.cpp,
// which is excluded from the WebAssembly build). Guarding the declarations keeps
// `#include "RemeshGmsh.h"` harmless under wasm for consumers such as MeshPart/Gui.
namespace MeshGui
{

/**
 * Non-modal dialog to remesh an existing mesh.
 * @author Werner Mayer
 */
class MeshGuiExport GmshWidget: public QWidget
{
    Q_OBJECT

public:
    explicit GmshWidget(QWidget* parent = nullptr, Qt::WindowFlags fl = Qt::WindowFlags());
    ~GmshWidget() override;
    void accept();
    void reject();

protected:
    void changeEvent(QEvent* e) override;
    int meshingAlgorithm() const;
    double getAngle() const;
    double getMaxSize() const;
    double getMinSize() const;
    virtual bool writeProject(QString& inpFile, QString& outFile);
    virtual bool loadOutput();

private:
    void setupConnections();
    void started();
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void errorOccurred(QProcess::ProcessError error);
    void onKillButtonClicked();
    void onClearButtonClicked();

    void readyReadStandardError();
    void readyReadStandardOutput();

private:
    class Private;
    std::unique_ptr<Private> d;

    Q_DISABLE_COPY_MOVE(GmshWidget)
};

/**
 * Non-modal dialog to remesh an existing mesh.
 * @author Werner Mayer
 */
class MeshGuiExport RemeshGmsh: public GmshWidget
{
    Q_OBJECT

public:
    explicit RemeshGmsh(
        Mesh::Feature* mesh,
        QWidget* parent = nullptr,
        Qt::WindowFlags fl = Qt::WindowFlags()
    );
    ~RemeshGmsh() override;

protected:
    bool writeProject(QString& inpFile, QString& outFile) override;
    bool loadOutput() override;

private:
    class Private;
    std::unique_ptr<Private> d;

    Q_DISABLE_COPY_MOVE(RemeshGmsh)
};

/**
 * Embed the panel into a task dialog.
 */
class TaskRemeshGmsh: public Gui::TaskView::TaskDialog
{
    Q_OBJECT

public:
    explicit TaskRemeshGmsh(Mesh::Feature* mesh);

public:
    void clicked(int) override;

    QDialogButtonBox::StandardButtons getStandardButtons() const override
    {
        return QDialogButtonBox::Apply | QDialogButtonBox::Close;
    }
    bool isAllowedAlterDocument() const override
    {
        return true;
    }

private:
    RemeshGmsh* widget;
};

}  // namespace MeshGui
#endif  // __EMSCRIPTEN__
