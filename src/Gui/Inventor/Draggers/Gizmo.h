#ifndef GUI_GIZMO_H
#define GUI_GIZMO_H

#include <Base/Placement.h>
#include <Gui/ViewProvider.h>
#include <Gui/Inventor/So3DAnnotation.h>

class SoDragger;

namespace Gui
{
class SoLinearDragger;
class SoLinearDraggerContainer;
class PrefQuantitySpinBox;

class LinearGizmo
{
public:
    LinearGizmo();

    void initDragger();
    void uninitDragger();

    // Returns the position and rotation of the base of the dragger
    Base::Placement getDraggerPlacement();
    void setDraggerPlacement(Base::Vector3d pos, Base::Vector3d dir);
    // Returns the drag distance from the base of the feature
    double getDragLength();
    void setDragLength(double dragLength);

    void setProperty(PrefQuantitySpinBox* property);
    void attachViewer(Gui::View3DInventorViewer* viewer, Base::Placement &origin);

private:
    SoLinearDragger* dragger = nullptr;
    SoLinearDraggerContainer* draggerContainer = nullptr;
    CoinPtr<So3DAnnotation> annotation;
    PrefQuantitySpinBox* property = nullptr;
    double initialValue;

    static void dragStartCallback(void *data, SoDragger *d);
    static void dragFinishCallback(void *data, SoDragger *d);
    static void dragMotionCallback(void *data, SoDragger *d);
};

}

#endif /* GUI_GIZMO_H */
