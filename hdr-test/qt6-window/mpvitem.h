#pragma once

#include <QQuickFramebufferObject>
#include <QVariant>
#include <mpv/client.h>

// Renders libmpv through the OpenGL render API into a float FBO, the way
// bomi does, so the test exercises the same path a Qt 6 bomi would take.
class MpvItem : public QQuickFramebufferObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit MpvItem(QQuickItem *parent = nullptr);
    ~MpvItem() override;

    Renderer *createRenderer() const override;
    mpv_handle *handle() const { return m_mpv; }

    Q_INVOKABLE void command(const QStringList &args);
    Q_INVOKABLE QVariant getProperty(const QString &name) const;
    void setMpvProperty(const char *name, const QString &value);

    // Set from main() before QML loads.
    static QStringList s_options; // name=value pairs passed to mpv
    static QString s_file;

signals:
    void frameSwapped();

private:
    mpv_handle *m_mpv = nullptr;
};
