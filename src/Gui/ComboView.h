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

#ifndef GUI_DOCKWND_COMBOVIEW_H
#define GUI_DOCKWND_COMBOVIEW_H

#include <Base/Parameter.h>
#include "DockWindow.h"


class QTabWidget;
class QTreeView;

namespace App {
  class PropertyContainer;
}

namespace Gui {
    class TreeWidget;
    class PropertyView;
    class ProjectWidget;
    class TreePanel;
namespace PropertyEditor {
class EditableListView;
class EditableItem;
class PropertyEditor;
} // namespace PropertyEditor

namespace TaskView {
class TaskView;
class TaskDialog;
} // namespace TaskView
} // namespace Gui


namespace Gui {
    class ControlSingleton;
namespace DockWnd {

/** Combo View
  * is a combination of a tree, property and TaskPanel for
  * integrated user action.
 */
class GuiExport ComboView : public Gui::DockWindow
{
    Q_OBJECT

public:
    /**
     * A constructor.
     * A more elaborate description of the constructor.
     */
    ComboView(bool showModel, Gui::Document*  pcDocument, QWidget *parent=nullptr);

    void setShowModel(bool);

    /**
     * A destructor.
     * A more elaborate description of the destructor.
    */
    ~ComboView() override;

    Gui::TaskView::TaskView *getTaskPanel(){return taskPanel;}
    QTabWidget* getTabPanel() const { return tabs;}


    friend class Gui::ControlSingleton;

    void showTreeView();
    void showTaskView();

    bool hasTreeView() const;

private Q_SLOTS:
    void onCurrentTabChanged(int index);
    void onSplitterMoved();

protected:
    void showDialog(Gui::TaskView::TaskDialog *dlg);
    void closeDialog();
    void closedDialog();
    void changeEvent(QEvent *e) override;

private:
    int oldTabIndex;
    int modelIndex = -1;
    int taskIndex;
    QTabWidget                         * tabs = nullptr;
    Gui::PropertyView                  * prop = nullptr;
    Gui::TreePanel                     * tree = nullptr;
    Gui::TaskView::TaskView            * taskPanel = nullptr;

    ParameterGrp::handle hGrp;

  //Gui::ProjectWidget                 * projectView;
};

} // namespace DockWnd
} // namespace Gui

#endif // GUI_DOCKWND_SELECTIONVIEW_H
