/***************************************************************************
 *   wasm offscreen-FBO viewport for Quarter — see WasmGLWidget.h.          *
 ***************************************************************************/

#include "PreCompiled.h"
#ifdef __EMSCRIPTEN__

#include "WasmGLWidget.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>
#include <QResizeEvent>
#include <cstring>

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
    const int w = fbo_->width();
    const int h = fbo_->height();
    if (w <= 0 || h <= 0) {
        return {};
    }
    // Reuse a persistent straight-alpha Format_RGBA8888 image (same format as the
    // wasm backing store) so the QuarterWidget Source-mode blit is a plain copy and
    // no per-frame QImage is allocated. QOpenGLFramebufferObject::toImage() instead
    // allocates a premultiplied image every call, which forced Qt's RGBA64 convert.
    if (readback_.width() != w || readback_.height() != h) {
        readback_ = QImage(w, h, QImage::Format_RGBA8888);
    }
    const int stride = w * 4;
    if (static_cast<int>(rbScratch_.size()) < stride * h) {
        rbScratch_.resize(static_cast<size_t>(stride) * h);
    }
    fbo_->bind();
    QOpenGLFunctions* f = context_ ? context_->functions() : nullptr;
    if (!f) {
        return {};
    }
    f->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rbScratch_.data());
    // GL is bottom-up; QImage is top-down. Copy rows in reverse into the reused image.
    for (int row = 0; row < h; ++row) {
        std::memcpy(readback_.scanLine(row),
                    rbScratch_.data() + static_cast<size_t>(h - 1 - row) * stride,
                    stride);
    }
    readback_.setDevicePixelRatio(devicePixelRatioF());
    return readback_;
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
