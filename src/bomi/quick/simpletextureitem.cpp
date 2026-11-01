#include "simpletextureitem.hpp"
#include "opengl/opengltexture2d.hpp"

struct SimpleTextureData : public SimpleTextureItem::ShaderData {
    const OpenGLTexture2D *texture = nullptr;
    SGTextureWrapper sgTexture;
};

struct SimpleTextureShader : public SimpleTextureItem::ShaderIface {
    SimpleTextureShader() {
        vertexShader = u":/shaders/texture.vert.qsb"_q;
        fragmentShader = u":/shaders/simpletexture.frag.qsb"_q;
    }
    auto texture(int binding, SimpleTextureItem::ShaderData *data) -> QSGTexture* override {
        Q_UNUSED(binding);
        auto d = static_cast<SimpleTextureData*>(data);
        return d->sgTexture.get(d->window, d->texture->id(), d->texture->size());
    }
};

SimpleTextureItem::SimpleTextureItem(QQuickItem *parent)
    : ShaderRenderItem<OGL::TextureVertex>(parent)
{
    m_texture = new OpenGLTexture2D;
}

SimpleTextureItem::~SimpleTextureItem()
{
    delete m_texture;
}

auto SimpleTextureItem::createShader() const -> SimpleTextureItem::ShaderIface*
{
    return new SimpleTextureShader;
}

auto SimpleTextureItem::createData() const -> SimpleTextureItem::ShaderData*
{
    auto data = new SimpleTextureData;
    data->texture = m_texture;
    return data;
}

auto SimpleTextureItem::updateData(ShaderData *data) -> void
{
    Q_ASSERT(static_cast<SimpleTextureData*>(data)->texture == m_texture);
    Q_UNUSED(data);
    updateTexture(m_texture);
}

auto SimpleTextureItem::updateVertex(Vertex *vertex) -> void
{
    OGL::CoordAttr::fillTriangleStrip(vertex, &Vertex::position,
                                      {0, 0}, {width(), height()});
}

auto SimpleTextureItem::setTexture(const OpenGLTexture2D &texture) -> void
{
    *m_texture = texture;
    reserve(UpdateMaterial);
}
