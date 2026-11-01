#ifndef OPENGLDRAWITEM_HPP
#define OPENGLDRAWITEM_HPP

#include "geometryitem.hpp"
#include <QOpenGLFunctions>
#include <QSGGeometry>
#include <QSGMaterial>
#include <QSGGeometryNode>
#include <QSGTexture>
#include <QQuickWindow>

class OpenGLDrawItem : public GeometryItem {
public:
    using AttrSet = QSGGeometry::AttributeSet;
    enum UpdateHint {
        UpdateGeometry = 1,
        UpdateMaterial = 2,
        UpdateAll = UpdateGeometry | UpdateMaterial
    };
    OpenGLDrawItem(QQuickItem *parent = nullptr);
    ~OpenGLDrawItem();
    auto devicePixelRatio() const -> double;
    auto logAt(const char *func) const -> QByteArray;
    virtual auto rerender() -> void { reserve(UpdateAll); }
    static auto func() -> QOpenGLFunctions* { return context()->functions(); }
    static auto context() -> QOpenGLContext*;
protected:
    auto isInitialized() const -> bool { return m_init; }
    auto reserve(UpdateHint hint, bool update = true) -> void;
    auto isReserved(UpdateHint update) const -> bool;
    virtual auto initializeGL() -> void { }
    virtual auto finalizeGL() -> void { }
    virtual auto drawingMode() const -> GLenum { return GL_TRIANGLE_STRIP; }
    virtual auto attributeSet() const -> const AttrSet&;
    virtual auto vertexCount() const -> int { return 0; }
    virtual auto createNode() const -> QSGGeometryNode*;
    virtual auto createGeometry() const -> QSGGeometry* = 0;
    virtual auto createMaterial() const -> QSGMaterial* = 0;
    virtual auto updateGeometry(QSGGeometry *geometry) -> QSGGeometry* = 0;
    virtual auto updateMaterial(QSGMaterial *material) -> QSGMaterial* = 0;
    virtual auto afterUpdate() -> void { }
private:
    auto tryInitGL() -> void;
    auto updatePaintNode(QSGNode *old, UpdatePaintNodeData *) -> QSGNode* final;
    QQuickWindow *m_win = nullptr;
    bool m_init = false;
    QSGGeometryNode *m_node = nullptr;
    int m_updates = 0;
};

inline auto OpenGLDrawItem::logAt(const char *func) const -> QByteArray
{ return QByteArray(metaObject()->className()) + "::" + func; }

inline auto OpenGLDrawItem::attributeSet() const -> const AttrSet&
{ return QSGGeometry::defaultAttributes_TexturedPoint2D(); }

inline auto OpenGLDrawItem::context() -> QOpenGLContext*
{ return QOpenGLContext::currentContext(); }

inline auto OpenGLDrawItem::reserve(UpdateHint hint, bool update) -> void
{ m_updates |= hint; if (update) this->update(); }

inline auto OpenGLDrawItem::isReserved(UpdateHint update) const -> bool
{ return m_updates & update; }

inline auto OpenGLDrawItem::tryInitGL() -> void
{ if (!m_init && context()) { initializeGL(); m_init = true; } }

/******************************************************************************/

template <class T>
class VertexDrawItem : public OpenGLDrawItem {
protected:
    using Super = VertexDrawItem<T>;
public:
    using OpenGLDrawItem::OpenGLDrawItem;
    using Vertex = T;
    auto vertices() const -> const QVector<Vertex>& { return m_vertices; }
protected:
    auto vertices() -> QVector<Vertex>& { return m_vertices; }
    auto geometryChange(const QRectF &new_, const QRectF &old) -> void override;
    virtual auto updateVertexOnGeometryChanged() const -> bool { return false; }
private:
    virtual auto initializeVertex(Vertex *vertex) const -> void;
    virtual auto updateVertex(Vertex *vertex) -> void;
    auto attributeSet() const -> const AttrSet& final { return T::info(); }
    auto createGeometry() const -> QSGGeometry* final;
    auto updateGeometry(QSGGeometry *geometry) -> QSGGeometry* final;
    QVector<Vertex> m_vertices;
};

template<class T>
auto VertexDrawItem<T>::geometryChange(const QRectF &new_,
                                               const QRectF &old) -> void
{
    OpenGLDrawItem::geometryChange(new_, old);
    if (updateVertexOnGeometryChanged())
        reserve(UpdateGeometry);
}

template<class T>
auto VertexDrawItem<T>::initializeVertex(Vertex *) const -> void { }

template<class T>
auto VertexDrawItem<T>::updateVertex(Vertex *vertex) -> void
{ memcpy(vertex, m_vertices.data(), sizeof(Vertex)*vertexCount()); }

template<class T>
auto VertexDrawItem<T>::createGeometry() const -> QSGGeometry*
{
    auto geometry = new QSGGeometry(attributeSet(), vertexCount());
    initializeVertex(static_cast<T*>(geometry->vertexData()));
    return geometry;
}

template<class T>
auto VertexDrawItem<T>::updateGeometry(QSGGeometry *geometry) -> QSGGeometry*
{
    geometry->allocate(vertexCount());
    updateVertex(static_cast<T*>(geometry->vertexData()));
    return geometry;
}

/******************************************************************************/

// Hands a texture bomi manages itself in OpenGL (an OpenGLTexture2D, mpv's
// FBO) to the Qt 6 scene graph, which only samples QSGTextures. The wrapper is
// rebuilt when the GL texture is recreated or resized; an empty texture is
// replaced by a transparent 1x1 one, because every sampler needs a texture.
class SGTextureWrapper {
public:
    SGTextureWrapper() = default;
    SGTextureWrapper(const SGTextureWrapper&) = delete;
    SGTextureWrapper &operator = (const SGTextureWrapper&) = delete;
    ~SGTextureWrapper();
    auto get(QQuickWindow *window, GLuint id, const QSize &size) -> QSGTexture*;
private:
    QSGTexture *m_texture = nullptr, *m_empty = nullptr;
    GLuint m_id = 0;
    QSize m_size;
};

template<class T>
class ShaderRenderItem : public VertexDrawItem<T> {
protected:
    using Super = ShaderRenderItem<T>;
public:
    using Type = QSGMaterialType;
    using VertexDrawItem<T>::VertexDrawItem;
    struct ShaderData {
        virtual ~ShaderData() = default;
        // Set on every sync; QSGTextures are created for this window.
        QQuickWindow *window = nullptr;
    };
    struct ShaderIface {
        virtual ~ShaderIface() = default;
        // Compiled shaders (.qsb) in the resources, e.g. ":/shaders/x.vert.qsb".
        // Their uniform block (binding 0, std140) starts with mat4 qt_Matrix and
        // float qt_Opacity; any uniforms of their own start at UserUniforms.
        QString vertexShader, fragmentShader;
        static constexpr int UserUniforms = 80;
    private:
        // Writes the shader's own uniforms into the block; true if it did.
        virtual auto updateUniforms(char *block, ShaderData *data) -> bool
            { Q_UNUSED(block); Q_UNUSED(data); return false; }
        // The texture for the sampler at this binding.
        virtual auto texture(int binding, ShaderData *data) -> QSGTexture*
            { Q_UNUSED(binding); Q_UNUSED(data); return nullptr; }
        friend class ShaderRenderItem;
    };
    virtual auto isOpaque() const -> bool { return false; }
protected:
    static auto shaderData(QSGMaterial *m) -> ShaderData*
        { return static_cast<Material*>(m)->data(); }
private:
    virtual auto type() const -> Type* = 0;
    virtual auto createShader() const -> ShaderIface* = 0;
    virtual auto createData() const -> ShaderData* = 0;
    virtual auto updateData(ShaderData *data) -> void = 0;
private:
    struct Material : public QSGMaterial {
        Material(const ShaderRenderItem *item);
        ~Material() { delete m_data; }
        auto type() const -> QSGMaterialType* final { return m_item->type(); }
        auto createShader(QSGRendererInterface::RenderMode) const -> QSGMaterialShader* final;
        auto data() -> ShaderData* { return m_data; }
        auto data() const -> const ShaderData* { return m_data; }
    private:
        const ShaderRenderItem *m_item = nullptr;
        ShaderData *m_data = nullptr;
    };
    struct Shader : public QSGMaterialShader {
        ShaderIface *m_iface = nullptr;
        Shader(ShaderIface *iface);
        ~Shader() { delete m_iface; }
        auto updateUniformData(RenderState &state, QSGMaterial *new_,
                               QSGMaterial *old) -> bool final;
        auto updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                                QSGMaterial *new_, QSGMaterial *old) -> void final;
    };
    auto createMaterial() const -> QSGMaterial* final;
    auto updateMaterial(QSGMaterial *material) -> QSGMaterial* final;
};

template<class T>
ShaderRenderItem<T>::Material::Material(const ShaderRenderItem *item)
    : m_item(item)
{
    setFlag(Blending, !item->isOpaque());
    m_data = m_item->createData();
}

template<class T>
auto ShaderRenderItem<T>::Material::createShader(QSGRendererInterface::RenderMode) const -> QSGMaterialShader*
{ return new Shader(m_item->createShader()); }

template<class T>
ShaderRenderItem<T>::Shader::Shader(ShaderIface *iface)
    : m_iface(iface)
{
    setShaderFileName(VertexStage, m_iface->vertexShader);
    setShaderFileName(FragmentStage, m_iface->fragmentShader);
}

template<class T>
auto ShaderRenderItem<T>::Shader::updateUniformData(RenderState &state,
                                                    QSGMaterial *new_,
                                                    QSGMaterial *) -> bool
{
    auto block = state.uniformData()->data();
    bool changed = false;
    if (state.isMatrixDirty()) {
        memcpy(block, state.combinedMatrix().constData(), 64);
        changed = true;
    }
    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        memcpy(block + 64, &opacity, 4);
        changed = true;
    }
    auto m = static_cast<Material*>(new_);
    return m_iface->updateUniforms(block, m->data()) || changed;
}

template<class T>
auto ShaderRenderItem<T>::Shader::updateSampledImage(RenderState &state, int binding,
                                                     QSGTexture **texture,
                                                     QSGMaterial *new_,
                                                     QSGMaterial *) -> void
{
    auto m = static_cast<Material*>(new_);
    if (auto t = m_iface->texture(binding, m->data())) {
        t->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        *texture = t;
    }
}

template<class T>
auto ShaderRenderItem<T>::createMaterial() const -> QSGMaterial*
{ return new Material(this); }

template<class T>
auto ShaderRenderItem<T>::updateMaterial(QSGMaterial *material) -> QSGMaterial*
{
    auto m = static_cast<Material*>(material);
    m->data()->window = this->window();
    updateData(m->data());
    return m;
}

#endif // OPENGLDRAWITEM_HPP
