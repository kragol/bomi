// Qt 6 HDR test window: does a Qt 6 Wayland window tagged BT.2020/PQ, with
// libmpv's vo_gpu rendering PQ through the OpenGL render API, give real HDR
// on KWin? See "HDR output" in bomi's TODO.md.
//
//   hdrwindow [--sdr | --describe] [--peak NITS] [--bits 8|16] [FILE]
//
// Default: window tagged PQ/BT.2020 through Qt (QSurfaceFormat colour space),
//          mpv told target-trc=pq, target-prim=bt.2020.
// --describe: tag the surface ourselves like mpv's gpu-next does: read KWin's
//          preferred description, declare mastering luminance and max CLL, and
//          set mpv's target-peak to the display peak in PQ's scale.
// --sdr:   untagged window, mpv tone-maps to SDR as bomi does today (for A/B).
// --peak:  mpv target-peak in nits (default: mpv's auto, i.e. no tone mapping
//          in mpv; KWin maps to the display instead).
// --bits:  window colour depth (default 8). NVIDIA's Wayland EGL offers only
//          8-bit and 16-bit float window configs, no 10-bit, and Qt 6.12's EGL
//          config chooser never asks for float, so 16 gets no surface there.

#include "colormanagement.h"
#include "mpvitem.h"

#include <QColorSpace>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <clocale>

int main(int argc, char *argv[])
{
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C"); // mpv requires it

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption sdr("sdr", "Untagged SDR window, mpv tone-maps to SDR.");
    QCommandLineOption describe("describe", "Tag the surface with luminances, as mpv does.");
    QCommandLineOption peak("peak", "mpv target-peak in nits.", "nits");
    QCommandLineOption bits("bits", "Window bits per channel: 8 or 16 (default 8).", "n", "8");
    parser.addOptions({sdr, describe, peak, bits});
    parser.addPositionalArgument("file", "Video to play (default: ../hdr-test.mkv).");
    parser.process(app);

    const bool hdr = !parser.isSet(sdr);
    QString file = parser.positionalArguments().value(0);
    if (file.isEmpty())
        file = QCoreApplication::applicationDirPath() + "/../../hdr-test.mkv";
    MpvItem::s_file = file;

    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    const int depth = parser.value(bits).toInt();
    format.setRedBufferSize(depth);
    format.setGreenBufferSize(depth);
    format.setBlueBufferSize(depth);
    format.setAlphaBufferSize(depth == 16 ? 16 : -1);
    if (hdr) {
        if (!parser.isSet(describe))
            format.setColorSpace(QColorSpace(QColorSpace::Primaries::Bt2020,
                                             QColorSpace::TransferFunction::St2084));
        MpvItem::s_options << "target-trc=pq" << "target-prim=bt.2020";
        if (parser.isSet(peak) && !parser.isSet(describe))
            MpvItem::s_options << "target-peak=" + parser.value(peak);
    }
    // mpv renders into a float FBO, so it would not dither; Qt then truncates
    // to the window depth, which bands badly in PQ.
    MpvItem::s_options << "dither-depth=" + QString::number(depth);
    QSurfaceFormat::setDefaultFormat(format);

    QQmlApplicationEngine engine;
    const QString mode = !hdr ? "SDR (untagged window)"
                       : parser.isSet(describe) ? "HDR (own description with luminances)"
                                                : "HDR (Qt PQ/BT.2020 tag)";
    engine.setInitialProperties({{"mode", mode}});
    engine.loadFromModule("HdrWindow", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QObject::connect(window, &QQuickWindow::frameSwapped, window, [window] {
        const QSurfaceFormat f = window->format();
        qInfo("window format: R%d G%d B%d A%d, colour space %s",
              f.redBufferSize(), f.greenBufferSize(), f.blueBufferSize(), f.alphaBufferSize(),
              qPrintable(f.colorSpace().description().isEmpty()
                             ? QStringLiteral("(none)") : f.colorSpace().description()));
    }, Qt::SingleShotConnection);
    if (hdr && parser.isSet(describe)) {
        const double override = parser.isSet(peak) ? parser.value(peak).toDouble() : 0;
        QObject::connect(window, &QQuickWindow::frameSwapped, window, [window, override] {
            const HdrTarget t = describeSurface(window, override);
            if (t.peak > 0)
                window->findChild<MpvItem *>()->setMpvProperty("target-peak", QString::number(qRound(t.peak)));
        }, Qt::SingleShotConnection);
    }

    return app.exec();
}
