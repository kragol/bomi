#include "main.hpp"
#include "configure.hpp"
#include "dialog/mbox.hpp"
#include "json/jrserver.hpp"
#include "player/jrplayer.hpp"
#include <QCryptographicHash>
#include <QElapsedTimer>
#ifdef Q_OS_LINUX
#include <signal.h>
#endif

extern "C" void _exit(int);

DECLARE_LOG_CONTEXT(Main)

namespace OGL { auto check() -> QString; }

template<class F>
SIA measure(F func, int loop = 100000) -> quint64
{
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < loop; ++i)
        func(i);
    return timer.nsecsElapsed() / loop;
}

int main(int argc, char **argv) {
#ifdef BOMI_IMPORT_ICU
    Locale::importIcu();
    return 0;
#endif
#ifdef Q_OS_LINUX
    signal(SIGPIPE, SIG_IGN);
    auto gtk_disable_setlocale
            = (void(*)(void))QLibrary::resolve(u"gtk-x11-2.0"_q,
                                               0, "gtk_disable_setlocale");
    if (gtk_disable_setlocale)
        gtk_disable_setlocale();
#endif
#ifdef Q_OS_LINUX
    // bomi is an X11 client: it talks to the window manager over xcb for
    // fullscreen, always-on-top, drag-to-move and screensaver inhibition, none
    // of which have a Qt-level equivalent here. Qt 5 picks the wayland plugin
    // on its own when qt5-wayland is installed and WAYLAND_DISPLAY is set, so
    // default to XWayland instead. An explicit QT_QPA_PLATFORM still wins --
    // OS::initialize() degrades rather than crashing if X11 is unavailable.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")
            && !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")
            && !qEnvironmentVariableIsEmpty("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
#endif
    // Settings, history and app state live here when given; --config-dir
    // wins over BOMI_CONFIG_DIR. Read before anything opens a settings file.
    {
        auto dir = qEnvironmentVariable("BOMI_CONFIG_DIR");
        for (int i = 1; i < argc; ++i) {
            const QByteArray arg(argv[i]);
            if (arg.startsWith("--config-dir="))
                dir = QString::fromLocal8Bit(arg.mid(13));
            else if (arg == "--config-dir" && i + 1 < argc)
                dir = QString::fromLocal8Bit(argv[++i]);
        }
        _SetConfigDir(dir);
    }
    // mpv's render API and bomi's own scene-graph items draw with OpenGL, so
    // keep Qt Quick on its OpenGL backend, and on desktop OpenGL rather than the
    // OpenGL ES that EGL hands out by default.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    {
        auto format = QSurfaceFormat::defaultFormat();
        format.setRenderableType(QSurfaceFormat::OpenGL);
        QSurfaceFormat::setDefaultFormat(format);
    }
    // Matches the installed launcher, bomi.desktop or bomi<suffix>.desktop.
    QGuiApplication::setDesktopFileName(_L("bomi" BOMI_SUFFIX));
    QApplication::setOrganizationName(u"xylosper"_q);
    QApplication::setOrganizationDomain(u"xylosper.net"_q);
    QApplication::setApplicationName(_L(cApp.name()));
    QApplication::setApplicationVersion(_L(cApp.version()));

    registerType();

    QScopedPointer<App> app(new App(argc, argv));

#ifdef Q_OS_WIN
    const char sep = ';';
#else
    const char sep = ':';
#endif
    const auto envPath = qgetenv("PATH");
    qputenv("PATH", envPath + sep + QApplication::applicationDirPath().toLocal8Bit() + "/tools");

    for (auto fmt : QImageWriter::supportedImageFormats())
        writableImageExts.push_back(QString::fromLatin1(fmt));

    if (app->executeToQuit())
        return 0;

    const auto error = OGL::check();
    if (!error.isEmpty()) {
        MBox mbox(nullptr, MBox::Icon::Critical,
                  qApp->translate("OpenGL", "OpenGL Error"),
                  qApp->translate("OpenGL", "Error: %1\n\n"
                                            "Failed to check OpenGL support.\n"
                                            "It may help to update driver of "
                                            "graphics card.").arg(error));
        auto button = mbox.addButton(MBox::Button::Close);
        QObject::connect(button, &QPushButton::clicked, qApp, &QApplication::quit);
        mbox.mbox()->show();
        return app->exec();
    }

    MainWindow *mw = new MainWindow;
    _Debug("Show MainWindow.");
    mw->show();
    app->setMainWindow(mw);
    _Debug("Start main event loop.");

    auto ret = app->exec();
    app->sendPostedEvents(nullptr, QEvent::DeferredDelete);
    app.reset();
    _Debug("Exit...");
    std::_Exit(ret);
    return ret;
}
