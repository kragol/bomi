#include "opengldrawitem.hpp"
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QtQuick/qsgtexture_platform.h>

OpenGLDrawItem::OpenGLDrawItem(QQuickItem *parent)
    : GeometryItem(parent)
{
    setFlag(ItemHasContents, true);
    connect(this, &QQuickItem::windowChanged, [this] (QQuickWindow *window) {
        m_win = window;
        if (window) {
            connect(window, &QQuickWindow::sceneGraphInitialized,
                    this, &OpenGLDrawItem::tryInitGL, Qt::DirectConnection);
            connect(window, &QQuickWindow::beforeRendering,
                    this, &OpenGLDrawItem::tryInitGL, Qt::DirectConnection);
            connect(window, &QQuickWindow::sceneGraphInvalidated,
                    this, &OpenGLDrawItem::finalizeGL, Qt::DirectConnection);
        }
    });
}

OpenGLDrawItem::~OpenGLDrawItem() {

}

auto OpenGLDrawItem::devicePixelRatio() const -> double
{
    return m_win ? m_win->devicePixelRatio() : 1.0;
}

auto OpenGLDrawItem::createNode() const -> QSGGeometryNode*
{
    return new QSGGeometryNode;
}

auto OpenGLDrawItem::updatePaintNode(QSGNode *old,
                                     UpdatePaintNodeData *) -> QSGNode*
{
    tryInitGL();
    m_node = static_cast<QSGGeometryNode*>(old);
    if (!m_node) {
        m_node = createNode();
        m_node->setGeometry(createGeometry());
        m_node->setMaterial(createMaterial());
        m_node->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
        m_node->geometry()->setDrawingMode(drawingMode());
        reserve(UpdateAll, false);

    }
    if (isReserved(UpdateMaterial)) {
        auto m = m_node->material();
        if (_Change(m, updateMaterial(m)))
            m_node->setMaterial(m);
        m_node->markDirty(QSGNode::DirtyMaterial);
    }
    if (isReserved(UpdateGeometry)) {
        auto g = m_node->geometry();
        if (_Change(g, updateGeometry(g)))
            m_node->setGeometry(g);
        m_node->markDirty(QSGNode::DirtyGeometry);
    }
    m_updates = 0;
    afterUpdate();
    return m_node;
}

SGTextureWrapper::~SGTextureWrapper()
{
    delete m_texture;
    delete m_empty;
}

auto SGTextureWrapper::get(QQuickWindow *window, GLuint id,
                           const QSize &size) -> QSGTexture*
{
    if (!window)
        return nullptr;
    if (!id || size.isEmpty()) {
        if (!m_empty) {
            QImage image(1, 1, QImage::Format_RGBA8888_Premultiplied);
            image.fill(Qt::transparent);
            m_empty = window->createTextureFromImage(image);
        }
        return m_empty;
    }
    if (!m_texture || id != m_id || size != m_size) {
        delete m_texture;
        m_texture = QNativeInterface::QSGOpenGLTexture::fromNative(id, window, size,
                                                                   QQuickWindow::TextureHasAlphaChannel);
        m_texture->setFiltering(QSGTexture::Linear);
        m_texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        m_texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
        m_id = id;
        m_size = size;
    }
    return m_texture;
}
