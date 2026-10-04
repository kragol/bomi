#include "subliminalfinder.hpp"
#include "player/mrl.hpp"
#include "misc/log.hpp"
#include <QProcess>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

DECLARE_LOG_CONTEXT(Subtitle)

struct SubliminalFinder::Data {
    SubliminalFinder *p = nullptr;
    State state = Unavailable;
    QString error, helper, cache, out, pendingId;
    QStringList languages;
    QVector<SubliminalProvider> providers;
    QString configFile;
    bool hasConfigFile = false;
    QTemporaryDir temp;
    QProcess proc;
    enum Command { None, Check, Search, Download } command = None;

    auto setState(State s) -> void
    {
        if (_Change(state, s))
            emit p->stateChanged();
    }
    // A failed check means subliminal is unusable; a failed search or download
    // is reported and the finder stays usable.
    auto fail(Command cmd, const QString &e) -> void
    {
        error = e;
        _Error("subliminal: %%", e);
        if (cmd == Check) {
            setState(Unavailable);
        } else {
            setState(Available);
            emit p->failed(e);
        }
    }
    auto run(Command cmd, const QStringList &args, State busy) -> bool
    {
        if (proc.state() != QProcess::NotRunning)
            return false;
        command = cmd;
        error.clear();
        setState(busy);
        proc.start(u"python3"_q, QStringList() << helper << args);
        return true;
    }
    auto finished() -> void
    {
        const auto cmd = command;
        command = None;
        const auto stdOut = proc.readAllStandardOutput();
        const auto stdErr = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (!stdErr.isEmpty())
            _Debug("subliminal stderr:\n%%", stdErr);
        const auto doc = QJsonDocument::fromJson(stdOut);
        const auto failure = doc.isObject() ? doc.object()[u"error"_q].toString()
                                            : QString();
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() || doc.isNull()
                || !failure.isEmpty()) {
            if (!failure.isEmpty())
                fail(cmd, failure);
            else if (proc.error() == QProcess::FailedToStart)
                fail(cmd, tr("Cannot run python3."));
            else // python itself failed; its stderr is in the debug log above
                fail(cmd, tr("python3 or subliminal failed; see the log for details."));
            return;
        }
        switch (cmd) {
        case Check: {
            const auto o = doc.object();
            _Info("Using subliminal %%.", o[u"version"_q].toString());
            configFile = o[u"config"_q].toString();
            hasConfigFile = o[u"configFound"_q].toBool();
            providers.clear();
            for (const auto &value : o[u"providers"_q].toArray()) {
                const auto po = value.toObject();
                SubliminalProvider provider;
                provider.name = po[u"name"_q].toString();
                provider.error = po[u"error"_q].toString();
                for (const auto &option : po[u"options"_q].toArray())
                    provider.options.push_back(option.toString());
                for (const auto &entry : po[u"configured"_q].toArray())
                    provider.configured.push_back(entry.toString());
                provider.used = po[u"used"_q].toBool();
                providers.push_back(provider);
            }
            setState(Available);
            break;
        }
        case Search: {
            QVector<SubtitleLink> links;
            for (const auto &value : doc.array()) {
                const auto o = value.toObject();
                SubtitleLink link;
                link.id = o[u"id"_q].toString();
                link.provider = o[u"provider"_q].toString();
                if (o[u"hearingImpaired"_q].toBool())
                    link.provider += u" (HI)"_q; // tells otherwise identical rows apart
                link.langCode = o[u"language"_q].toString();
                link.fileName = o[u"fileName"_q].toString();
                links.push_back(link);
            }
            setState(Available);
            emit p->found(links);
            break;
        } case Download: {
            QFile file(out);
            QByteArray data;
            if (file.open(QFile::ReadOnly))
                data = file.readAll();
            file.close();
            file.remove();
            setState(Available);
            emit p->downloaded(pendingId, data, doc.object()[u"format"_q].toString());
            break;
        } default:
            break;
        }
    }
    auto search(const QStringList &target) -> bool
    {
        if (state != Available)
            return false;
        return run(Search, QStringList() << u"search"_q << u"--cache"_q << cache
                   << u"--languages"_q << languages.join(','_q) << target, Finding);
    }
};

SubliminalFinder::SubliminalFinder(QObject *parent)
    : QObject(parent), d(new Data)
{
    d->p = this;
    connect(&d->proc, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>
            (&QProcess::finished), this, [this] () { d->finished(); });
    connect(&d->proc, &QProcess::errorOccurred, this, [this] (QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            d->finished();
    });
    // The helper ships as a resource; python needs a real file to run.
    d->helper = d->temp.filePath(u"subliminal_helper.py"_q);
    d->cache = d->temp.filePath(u"search.pickle"_q);
    d->out = d->temp.filePath(u"download.bin"_q);
    if (!d->temp.isValid() || !QFile::copy(u":/subtitle/subliminal_helper.py"_q, d->helper)) {
        d->fail(Data::Check, tr("Cannot prepare the subliminal helper."));
        return;
    }
    d->run(Data::Check, QStringList() << u"check"_q, Connecting);
}

SubliminalFinder::~SubliminalFinder()
{
    if (d->proc.state() != QProcess::NotRunning) {
        d->proc.kill();
        d->proc.waitForFinished(1000);
    }
    delete d;
}

auto SubliminalFinder::setLanguages(const QStringList &codes) -> void
{
    d->languages = codes;
}

auto SubliminalFinder::find(const Mrl &mrl) -> bool
{
    if (mrl.isLocalFile())
        return d->search(QStringList() << u"--video"_q << mrl.toLocalFile());
    const auto name = mrl.displayName();
    return !name.isEmpty() && d->search(QStringList() << u"--name"_q << name);
}

auto SubliminalFinder::find(const QString &name) -> bool
{
    return !name.isEmpty() && d->search(QStringList() << u"--name"_q << name);
}

auto SubliminalFinder::find(const QString &query, int season, int episode) -> bool
{
    if (query.isEmpty())
        return false;
    // subliminal guesses everything from a release-like name.
    auto name = query;
    if (season > 0 && episode > 0)
        name += u" S%1E%2"_q.arg(season, 2, 10, '0'_q).arg(episode, 2, 10, '0'_q);
    return d->search(QStringList() << u"--name"_q << name);
}

auto SubliminalFinder::download(const QString &id) -> bool
{
    if (d->state != Available || id.isEmpty())
        return false;
    d->pendingId = id;
    return d->run(Data::Download, QStringList() << u"download"_q << u"--cache"_q << d->cache
                  << u"--id"_q << id << u"--out"_q << d->out, Downloading);
}

auto SubliminalFinder::state() const -> State
{
    return d->state;
}

auto SubliminalFinder::error() const -> QString
{
    return d->error;
}

auto SubliminalFinder::providers() const -> QVector<SubliminalProvider>
{
    return d->providers;
}

auto SubliminalFinder::configFile() const -> QString
{
    return d->configFile;
}

auto SubliminalFinder::hasConfigFile() const -> bool
{
    return d->hasConfigFile;
}
