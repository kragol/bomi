#ifndef SUBLIMINALFINDER_HPP
#define SUBLIMINALFINDER_HPP

class Mrl;

struct SubtitleLink {
    QString language, fileName, provider, langCode;
    QString id; // subliminal's "<provider>:<subtitle id>", for download()
};

// Finds and downloads subtitles through subliminal, which keeps up with the
// subtitle providers (opensubtitles.org, podnapisi, addic7ed, ...). It runs a
// small Python helper (subliminal_helper.py, compiled in as a resource) with
// the system python3, the way bomi runs yt-dlp for streaming sites, so
// subliminal is an optional runtime dependency.
class SubliminalFinder : public QObject {
    Q_OBJECT
public:
    enum State {
        Unavailable = 1, Connecting = 8, Available = 2, Finding = 4,
        Downloading = 32
    };
    SubliminalFinder(QObject *parent = nullptr);
    ~SubliminalFinder();
    // Languages to search for, as ISO 639 codes ("en", "pt-br").
    auto setLanguages(const QStringList &codes) -> void;
    auto find(const Mrl &mrl) -> bool;
    auto find(const QString &name) -> bool;
    auto find(const QString &query, int season, int episode) -> bool;
    auto download(const QString &id) -> bool;
    auto state() const -> State;
    auto isAvailable() const -> bool { return state() == Available; }
    auto error() const -> QString;
signals:
    void stateChanged();
    void found(const QVector<SubtitleLink> &links);
    void downloaded(const QString &id, const QByteArray &data, const QString &format);
    void failed(const QString &error);
private:
    struct Data;
    Data *d;
};

#endif // SUBLIMINALFINDER_HPP
