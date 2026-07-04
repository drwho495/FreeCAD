/***************************************************************************
 *   wasm offscreen-FBO viewport for Quarter — see WasmGLWidget.h.          *
 ***************************************************************************/

#include "PreCompiled.h"
#ifdef __EMSCRIPTEN__

#include "WasmGLWidget.h"

#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>
#include <QResizeEvent>

using namespace Gui;

WasmGLWidget::WasmGLWidget(QWidget* parent)
    : QWidget(parent)
{
    // We paint the whole widget from the FBO readback; skip the backing-store
    // clear and let Quarter drive when we render.
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
}

WasmGLWidget::~WasmGLWidget() = default;

bool WasmGLWidget::ensureContext()
{
    if (context_) {
        return true;
    }
    surface_ = std::make_unique<QOffscreenSurface>();
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setMajorVersion(3);
    fmt.setMinorVersion(0);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    surface_->setFormat(fmt);
    surface_->create();

    context_ = std::make_unique<QOpenGLContext>();
    context_->setFormat(fmt);
    if (!context_->create()) {
        context_.reset();
        surface_.reset();
        return false;
    }
    return true;
}

bool WasmGLWidget::ensureFbo(const QSize& sizeDevPx)
{
    if (fbo_ && fboSize_ == sizeDevPx) {
        return true;
    }
    fbo_.reset();
    QOpenGLFramebufferObjectFormat fboFmt;
    fboFmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    fbo_ = std::make_unique<QOpenGLFramebufferObject>(sizeDevPx, fboFmt);
    fboSize_ = sizeDevPx;
    return fbo_->isValid();
}

void WasmGLWidget::makeCurrent()
{
    if (!ensureContext()) {
        return;
    }
    if (!context_->makeCurrent(surface_.get())) {
        return;
    }
    const qreal dpr = devicePixelRatioF();
    const QSize sz(qMax(1, int(width() * dpr)), qMax(1, int(height() * dpr)));
    if (ensureFbo(sz)) {
        fbo_->bind();  // Coin's SoGLRenderAction renders into this FBO
    }
}

void WasmGLWidget::doneCurrent()
{
    // Intentionally a no-op: QWasmOpenGLContext::isValid() reports false whenever
    // the context is not current, which would make Quarter's isValid() checks
    // fail. The context is tied to our offscreen surface anyway.
}

bool WasmGLWidget::isValid() const
{
    return const_cast<WasmGLWidget*>(this)->ensureContext();
}

QOpenGLContext* WasmGLWidget::context() const
{
    return context_.get();
}

QSurfaceFormat WasmGLWidget::format() const
{
    return context_ ? context_->format() : QSurfaceFormat::defaultFormat();
}

GLuint WasmGLWidget::defaultFramebufferObject() const
{
    return fbo_ ? fbo_->handle() : 0;
}

QImage WasmGLWidget::readbackImage()
{
    if (!fbo_) {
        return {};
    }
    QImage img = fbo_->toImage();  // handles GL readback + vertical flip
    img.setDevicePixelRatio(devicePixelRatioF());
    return img;
}

QImage WasmGLWidget::grabFramebuffer()
{
    return readbackImage();
}

void WasmGLWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // FBO is recreated lazily on the next makeCurrent(); trigger a repaint so
    // Quarter re-renders at the new size.
    update();
}

#endif  // __EMSCRIPTEN__
