/***************************************************************************
 *   Copyright (c) 2013 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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

#include "DlgSettingsObjectColor.h"
#include "ui_DlgSettingsObjectColor.h"
#include <App/Material.h>
#include <Gui/PrefWidgets.h>
#include "PartParams.h"

using namespace PartGui;

/* TRANSLATOR PartGui::DlgSettingsObjectColor */

/**
 *  Constructs a DlgSettingsObjectColor which is a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'
 */
DlgSettingsObjectColor::DlgSettingsObjectColor(QWidget* parent)
    : PreferencePage(parent)
    , ui(new Ui_DlgSettingsObjectColor)
{
    ui->setupUi(this);
    ui->DefaultShapeColor->setDisabled(ui->checkRandomColor->isChecked());
    ui->DressUpColor->setAllowTransparency(true);
    ui->AdditiveColor->setAllowTransparency(true);
    ui->SubtractiveColor->setAllowTransparency(true);
    ui->IntersectingColor->setAllowTransparency(true);

    App::Color c;
    QColor qc;
    c.setPackedValue(PartParams::getPreviewAddColor());
    qc.setRgbF(c.r, c.g, c.b, c.a);
    ui->AdditiveColor->setColor(qc);

    c.setPackedValue(PartParams::getPreviewSubColor());
    qc.setRgbF(c.r, c.g, c.b, c.a);
    ui->SubtractiveColor->setColor(qc);

    c.setPackedValue(PartParams::getPreviewIntersectColor());
    qc.setRgbF(c.r, c.g, c.b, c.a);
    ui->IntersectingColor->setColor(qc);

    c.setPackedValue(PartParams::getPreviewDressColor());
    qc.setRgbF(c.r, c.g, c.b, c.a);
    ui->DressUpColor->setColor(qc);
}

/**
 *  Destroys the object and frees any allocated resources
 */
DlgSettingsObjectColor::~DlgSettingsObjectColor()
{
    // no need to delete child widgets, Qt does it all for us
}

void DlgSettingsObjectColor::saveSettings()
{
    // Part
    ui->DefaultShapeColor->onSave();
    ui->checkRandomColor->onSave();
    ui->DefaultShapeTransparency->onSave();
    ui->DefaultShapeLineColor->onSave();
    ui->DefaultShapeLineWidth->onSave();
    ui->DefaultShapeVertexColor->onSave();
    ui->DefaultShapeVertexSize->onSave();
    ui->BoundingBoxColor->onSave();
    ui->BoundingBoxFontSize->onSave();
    ui->twosideRendering->onSave();
    // Annotations
    ui->AnnotationTextColor->onSave();
    // Preview
    ui->AdditiveColor->onSave();
    ui->SubtractiveColor->onSave();
    ui->IntersectingColor->onSave();
    ui->DressUpColor->onSave();

    PartParams::setRespectSystemDPI(ui->RespectSystemDPI->isChecked());
}

void DlgSettingsObjectColor::loadSettings()
{
    // Part
    ui->DefaultShapeColor->onRestore();
    ui->checkRandomColor->onRestore();
    ui->DefaultShapeTransparency->onRestore();
    ui->DefaultShapeLineColor->onRestore();
    ui->DefaultShapeLineWidth->onRestore();
    ui->DefaultShapeVertexColor->onRestore();
    ui->DefaultShapeVertexSize->onRestore();
    ui->BoundingBoxColor->onRestore();
    ui->BoundingBoxFontSize->onRestore();
    ui->twosideRendering->onRestore();
    // Annotations
    ui->AnnotationTextColor->onRestore();
    // Preview
    ui->AdditiveColor->onRestore();
    ui->SubtractiveColor->onRestore();
    ui->IntersectingColor->onRestore();
    ui->DressUpColor->onRestore();

    ui->RespectSystemDPI->setChecked(PartParams::getRespectSystemDPI());
}

/**
 * Sets the strings of the subwidgets using the current language.
 */
void DlgSettingsObjectColor::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    else {
        QWidget::changeEvent(e);
    }
}

#include "moc_DlgSettingsObjectColor.cpp"

