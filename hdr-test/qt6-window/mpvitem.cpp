#include "mpvitem.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>
#include <mpv/render_gl.h>

#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif

QStringList MpvItem::s_options;
QString MpvItem::s_file;

static void *getProcAddress(void *, const char *name)
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    return ctx ? reinterpret_cast<void *>(ctx->getProcAddress(name)) : nullptr;
}

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvRenderer(MpvItem *item) : m_item(item)
    {
        mpv_opengl_init_params gl{getProcAddress, nullptr};
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        if (mpv_render_context_create(&m_ctx, item->handle(), params) < 0)
            qFatal("mpv_render_context_create failed");
        mpv_render_context_set_update_callback(m_ctx, [](void *ctx) {
            QMetaObject::invokeMethod(static_cast<MpvItem *>(ctx), "update", Qt::QueuedConnection);
        }, item);
        // vo=libmpv fails to initialise if a file starts before the render
        // context exists, so only load it now.
        QMetaObject::invokeMethod(item, [item] { item->command({"loadfile", MpvItem::s_file}); },
                                  Qt::QueuedConnection);
    }

    ~MpvRenderer() override { mpv_render_context_free(m_ctx); }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        // Float, so PQ code values are not quantised to 8 bits before Qt
        // composites the FBO into the 10-bit window.
        QOpenGLFramebufferObjectFormat format;
        format.setInternalTextureFormat(GL_RGBA16F);
        return new QOpenGLFramebufferObject(size, format);
    }

    void render() override
    {
        QOpenGLFramebufferObject *fbo = framebufferObject();
        mpv_opengl_fbo mfbo{static_cast<int>(fbo->handle()), fbo->width(), fbo->height(), GL_RGBA16F};
        int flipY = 0;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mfbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context_render(m_ctx, params);
    }

private:
    MpvItem *m_item;
    mpv_render_context *m_ctx = nullptr;
};

MpvItem::MpvItem(QQuickItem *parent) : QQuickFramebufferObject(parent)
{
    m_mpv = mpv_create();
    if (!m_mpv)
        qFatal("mpv_create failed");
    mpv_set_option_string(m_mpv, "vo", "libmpv");
    mpv_set_option_string(m_mpv, "terminal", "yes");
    mpv_set_option_string(m_mpv, "msg-level", "all=status,vo=v");
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "loop-file", "inf");
    for (const QString &opt : std::as_const(s_options)) {
        const qsizetype eq = opt.indexOf('=');
        const QByteArray name = opt.left(eq).toUtf8(), value = opt.mid(eq + 1).toUtf8();
        if (mpv_set_option_string(m_mpv, name.constData(), value.constData()) < 0)
            qWarning("mpv rejected option %s", qPrintable(opt));
    }
    if (mpv_initialize(m_mpv) < 0)
        qFatal("mpv_initialize failed");
    connect(this, &MpvItem::windowChanged, this, [this](QQuickWindow *w) {
        if (w)
            connect(w, &QQuickWindow::frameSwapped, this, &MpvItem::frameSwapped);
    });
}

MpvItem::~MpvItem()
{
    // The renderer (and its render context) is gone by now: Qt Quick
    // destroys it on the render thread before the item is deleted.
    mpv_terminate_destroy(m_mpv);
}

QQuickFramebufferObject::Renderer *MpvItem::createRenderer() const
{
    window()->setPersistentGraphics(true);
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(const_cast<MpvItem *>(this));
}

void MpvItem::command(const QStringList &args)
{
    QList<QByteArray> bytes;
    for (const QString &a : args)
        bytes << a.toUtf8();
    QList<const char *> argv;
    for (const QByteArray &b : std::as_const(bytes))
        argv << b.constData();
    argv << nullptr;
    mpv_command_async(m_mpv, 0, argv.data());
}

QVariant MpvItem::getProperty(const QString &name) const
{
    char *s = mpv_get_property_string(m_mpv, name.toUtf8().constData());
    if (!s)
        return {};
    QString v = QString::fromUtf8(s);
    mpv_free(s);
    return v;
}

void MpvItem::setMpvProperty(const char *name, const QString &value)
{
    if (mpv_set_property_string(m_mpv, name, value.toUtf8().constData()) < 0)
        qWarning("mpv rejected %s=%s", name, qPrintable(value));
}
