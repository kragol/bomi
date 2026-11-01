#ifndef MAIN_HPP
#define MAIN_HPP

#include "app.hpp"
#include "stdafx.hpp"
#include "mainwindow.hpp"
#include "player/playlistmodel.hpp"
#include "player/historymodel.hpp"
#include "player/avinfoobject.hpp"
#include "player/playengine.hpp"
#include "pref/pref.hpp"
#include "audio/audioformat.hpp"
#include "video/interpolatorparams.hpp"
#include "video/videopreview.hpp"
#include "video/videorenderer.hpp"
#include "misc/downloader.hpp"
#include "quick/algorithmobject.hpp"
#include "quick/circularimageitem.hpp"
#include "quick/maskareaitem.hpp"
#include "quick/toplevelitem.hpp"
#include "quick/appobject.hpp"
#include "quick/settingsobject.hpp"
#include "quick/formatobject.hpp"
#include "quick/themeobject.hpp"
#include "quick/windowobject.hpp"
#include "quick/busyiconitem.hpp"
#include "quick/buttonboxitem.hpp"
#include "quick/triangleitem.hpp"
#include "audio/visualizer.hpp"
#include <QImageWriter>

template<class T>
SIA _QmlSingleton(QQmlEngine *, QJSEngine *) -> QObject* { return new T; }

auto registerType() -> void
{
    qmlRegisterType<PlayEngine>("bomi", 1, 0, "Engine");
    qmlRegisterType<MediaObject>("bomi", 1, 0, "Media");
    qmlRegisterType<BusyIconItem>("bomi", 1, 0, "BusyIcon");
    qmlRegisterType<ButtonBoxItem>("bomi", 1, 0, "ButtonBox");
    qmlRegisterType<OsdStyleObject>("bomi", 1, 0, "OsdStyleTheme");
    qmlRegisterType<TimelineThemeObject>("bomi", 1, 0, "TimelineTheme");
    qmlRegisterType<CircularImageItem>("bomi", 1, 0, "CircularImage");
    qmlRegisterType<MaskAreaItem>("bomi", 1, 0, "MaskArea");
    qmlRegisterType<EditionChapterObject>("bomi", 1, 0, "Chapter");
    qmlRegisterType<EditionChapterObject>("bomi", 1, 0, "Edition");
    qmlRegisterType<TriangleItem>("bomi", 1, 0, "Triangle");
    qmlRegisterType<AudioVisualizer>("bomi", 1, 0, "Visualizer");
    qmlRegisterAnonymousType<TopLevelItem>("bomi", 1);
    qmlRegisterAnonymousType<Downloader>("bomi", 1);
    qmlRegisterAnonymousType<HistoryModel>("bomi", 1);
    qmlRegisterAnonymousType<VideoObject>("bomi", 1);
    qmlRegisterAnonymousType<AvTrackObject>("bomi", 1);
    qmlRegisterAnonymousType<VideoFormatObject>("bomi", 1);
    qmlRegisterAnonymousType<VideoToolObject>("bomi", 1);
    qmlRegisterAnonymousType<AudioFormatObject>("bomi", 1);
    qmlRegisterAnonymousType<AudioObject>("bomi", 1);
    qmlRegisterAnonymousType<CodecObject>("bomi", 1);
    qmlRegisterAnonymousType<SubtitleObject>("bomi", 1);
    qmlRegisterAnonymousType<CacheInfoObject>("bomi", 1);
    qmlRegisterAnonymousType<PlaylistModel>("bomi", 1);
    qmlRegisterAnonymousType<WindowObject>("bomi", 1);
    qmlRegisterAnonymousType<MemoryObject>("bomi", 1);
    qmlRegisterAnonymousType<CpuObject>("bomi", 1);
    qmlRegisterAnonymousType<MouseObject>("bomi", 1);
    qmlRegisterAnonymousType<ThemeObject>("bomi", 1);
    qmlRegisterAnonymousType<ControlsThemeObject>("bomi", 1);
    qmlRegisterAnonymousType<OsdThemeObject>("bomi", 1);
    qmlRegisterAnonymousType<MessageThemeObject>("bomi", 1);
    qmlRegisterAnonymousType<MouseEventObject>("bomi", 1);
    qmlRegisterAnonymousType<VideoPreview>("bomi", 1);
    qmlRegisterAnonymousType<VideoRenderer>("bomi", 1);

    qmlRegisterSingletonType<AppObject>("bomi", 1, 0, "App", _QmlSingleton<AppObject>);
    qmlRegisterSingletonType<FormatObject>("bomi", 1, 0, "Format", _QmlSingleton<FormatObject>);
    qmlRegisterSingletonType<SettingsObject>("bomi", 1, 0, "Settings", _QmlSingleton<SettingsObject>);
    qmlRegisterSingletonType<AlgorithmObject>("bomi", 1, 0, "Alg", _QmlSingleton<AlgorithmObject>);

    qRegisterMetaType<PlayEngine::State>("State");
    qRegisterMetaType<Mrl>("Mrl");
    qRegisterMetaType<QVector<int>>("QVector<int>");
    qRegisterMetaType<StreamList>("StreamList");
    qRegisterMetaType<AudioFormat>("AudioFormat");
    qRegisterMetaType<QVector<QMetaProperty>>();
    qRegisterMetaType<ChannelLayoutMap>();
    qRegisterMetaType<QList<MatchString>>();
    qRegisterMetaType<MouseActionMap>();
    qRegisterMetaType<AudioNormalizerOption>();
    qRegisterMetaType<DeintCaps>();
    qRegisterMetaType<ShortcutMap>();
    qRegisterMetaType<OsdStyle>();
    qRegisterMetaType<OS::HwAcc::Api>();
    qRegisterMetaType<IntrplParamSetMap>("IntrplParamSetMap");
    qRegisterMetaType<AudioVisualizer::Type>();
    qRegisterMetaType<AudioVisualizer::Scale>();
    // MrlState signals name this typedef; Qt 6 looks argument types up by name.
    qRegisterMetaType<VideoEffects>("VideoEffects");
    // Types QSettings stores as QVariants; Qt 6 finds their stream operators
    // through the meta type, which has to be registered before loading.
    qRegisterMetaType<Mrl>();
    qRegisterMetaType<Playlist>();
    qRegisterMetaType<EncodingInfo>();
    qRegisterMetaType<Locale>();
    qRegisterMetaType<QMap<QString, QString>>();

}

namespace Global {
extern QStringList writableImageExts;
}

#endif // MAIN_HPP
