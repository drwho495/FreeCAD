#include "Gizmo.h"

#include <Inventor/draggers/SoDragger.h>
#include <Inventor/So3DAnnotation.h>
#include <Base/Converter.h>
#include <Base/Console.h>

#include <algorithm>
#include "Document.h"
#include "Gui/Utilities.h"

#include "PrefWidgets.h"
#include "SoLinearDragger.h"
#include "View3DInventorViewer.h"

using namespace Gui;

LinearGizmo::LinearGizmo()
{
}

void LinearGizmo::initDragger()
{
    assert(!annotation && "Forgot to call LinearGizmo::uninitDragger?");
    annotation = new So3DAnnotation;

    draggerContainer = new SoLinearDraggerContainer;
    annotation->addChild(draggerContainer);

    draggerContainer->color.setValue(1, 0, 0);
    dragger = draggerContainer->getDragger();

    dragger->addStartCallback(dragStartCallback, this);
    dragger->addFinishCallback(dragFinishCallback, this);
    dragger->addMotionCallback(dragMotionCallback, this);
    dragger->setLabelVisibility(false);
    dragger->cylinderRadius = 1.0f;
    dragger->cylinderHeight = 25.0f;

    setDragLength(property->value().getValue());
}

void LinearGizmo::uninitDragger()
{
    annotation.reset();
    dragger = nullptr;
    draggerContainer = nullptr;
}

Base::Placement LinearGizmo::getDraggerPlacement()
{
    return {
        Base::convertTo<Base::Vector3d>(draggerContainer->translation.getValue()),
        Base::convertTo<Base::Rotation>(draggerContainer->rotation.getValue())
    };
}

void LinearGizmo::setDraggerPlacement(Base::Vector3d pos, Base::Vector3d dir)
{
    draggerContainer->translation = Base::convertTo<SbVec3f>(pos);
    draggerContainer->setPointerDirection(dir);
}

double LinearGizmo::getDragLength()
{
    double dragLength = dragger->translationIncrementCount.getValue()
        * dragger->translationIncrement.getValue();

    return dragLength;
}

void LinearGizmo::setDragLength(double dragLength)
{
    dragger->translation = {0, static_cast<float>(dragLength), 0};
}

void LinearGizmo::setProperty(PrefQuantitySpinBox* property)
{
    this->property = property;
}

void LinearGizmo::attachViewer(Gui::View3DInventorViewer* viewer, Base::Placement &origin) {
    if (draggerContainer && viewer) {
        auto mat = origin.toMatrix();

        viewer->getDocument()->setEditingTransform(mat);
        viewer->setupEditingRoot(annotation.get(), &mat);
    }
}

void LinearGizmo::dragStartCallback(void *data, [[maybe_unused]] SoDragger *d)
{
    Base::Console().message("Started dragging\n");

    auto sudoThis = static_cast<LinearGizmo*>(data);
    sudoThis->initialValue = sudoThis->property->value().getValue();
    sudoThis->dragger->translationIncrementCount.setValue(0);
}

void LinearGizmo::dragFinishCallback(void *data, [[maybe_unused]] SoDragger *d)
{
    Base::Console().message("Finished dragging\n");
}

void LinearGizmo::dragMotionCallback(void *data, [[maybe_unused]] SoDragger *d)
{
    auto sudoThis = static_cast<LinearGizmo*>(data);

    double value = sudoThis->initialValue + sudoThis->getDragLength();
    value = std::max(value, sudoThis->dragger->translationIncrement.getValue());

    sudoThis->property->setValue(value);
    sudoThis->setDragLength(value);

    Base::Console().message("Continuing dragging, value: %lf\n", value);
}
