/***************************************************************************
 *   wasm replacement viewport for Quarter's QOpenGLWidget.                 *
 *                                                                         *
 *   QOpenGLWidget cannot composite on Qt for WebAssembly: a WebGL context  *
 *   is tied to a single surface with no cross-context resource sharing, so  *
 *   the window compositor cannot sample a child widget's FBO texture and    *
 *   nothing paints (the 3D region stays black). This raster QWidget gives    *
 *   Quarter the QOpenGLWidget API it needs but renders into an offscreen     *
 *   FBO; QuarterWidget::paintEvent reads it back and QPainter::drawImage()s  *
 *   it through the ordinary raster backing store, which Qt-wasm DOES show.   *
 *   Approach mirrors OpenSCAD's WasmGLWidget.                                *
 ***************************************************************************/

#ifndef GUI_WASMGLWIDGET_H
#define GUI_WASMGLWIDGET_H

#ifdef __EMSCRIPTEN__

#include <QWidget>
#include <QImage>
#include <QSize>
#include <QSurfaceFormat>
#include <QtGui/qopengl.h>
#include <memory>

class QOpenGLContext;
class QOffscreenSurface;
class QOpenGLFramebufferObject;

namespace Gui {

/// QOpenGLWidget-compatible raster widget backed by an offscreen GLES3 FBO.
/// No Q_OBJECT: it uses no signals/slots of its own (avoids moc on a header that
/// is compiled only under __EMSCRIPTEN__).
class WasmGLWidget : public QWidget
{
public:
    explicit WasmGLWidget(QWidget* parent = nullptr);
    ~WasmGLWidget() override;

    // --- QOpenGLWidget API subset used by Quarter's QuarterWidget ---
    /// Make the offscreen context current AND bind the FBO so subsequent GL
    /// (Coin's SoGLRenderAction) renders into it. Recreates the FBO on resize.
    void makeCurrent();
    void doneCurrent();
    bool isValid() const;
    QOpenGLContext* context() const;
    QSurfaceFormat format() const;
    GLuint defaultFramebufferObject() const;
    QImage grabFramebuffer();

    /// Read the current FBO contents back into a top-down QImage (for painting).
    QImage readbackImage();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    bool ensureContext();
    bool ensureFbo(const QSize& sizeDevPx);

    std::unique_ptr<QOpenGLContext> context_;
    std::unique_ptr<QOffscreenSurface> surface_;
    std::unique_ptr<QOpenGLFramebufferObject> fbo_;
    QSize fboSize_;
};

}  // namespace Gui

#endif  // __EMSCRIPTEN__
#endif  // GUI_WASMGLWIDGET_H
