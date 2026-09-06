#include "fontcombobox.hpp"
#include "misc/simplelistmodel.hpp"
#include <QFontDatabase>

static QFontDatabase::WritingSystem writingSystemFromScript(QLocale::Script script)
{
    switch (script) {
    case QLocale::ArabicScript:
        return QFontDatabase::Arabic;
    case QLocale::CyrillicScript:
        return QFontDatabase::Cyrillic;
    case QLocale::GurmukhiScript:
        return QFontDatabase::Gurmukhi;
    case QLocale::SimplifiedHanScript:
        return QFontDatabase::SimplifiedChinese;
    case QLocale::TraditionalHanScript:
        return QFontDatabase::TraditionalChinese;
    case QLocale::LatinScript:
        return QFontDatabase::Latin;
    case QLocale::ArmenianScript:
        return QFontDatabase::Armenian;
    case QLocale::BengaliScript:
        return QFontDatabase::Bengali;
    case QLocale::DevanagariScript:
        return QFontDatabase::Devanagari;
    case QLocale::GeorgianScript:
        return QFontDatabase::Georgian;
    case QLocale::GreekScript:
        return QFontDatabase::Greek;
    case QLocale::GujaratiScript:
        return QFontDatabase::Gujarati;
    case QLocale::HebrewScript:
        return QFontDatabase::Hebrew;
    case QLocale::JapaneseScript:
        return QFontDatabase::Japanese;
    case QLocale::KhmerScript:
        return QFontDatabase::Khmer;
    case QLocale::KannadaScript:
        return QFontDatabase::Kannada;
    case QLocale::KoreanScript:
        return QFontDatabase::Korean;
    case QLocale::LaoScript:
        return QFontDatabase::Lao;
    case QLocale::MalayalamScript:
        return QFontDatabase::Malayalam;
    case QLocale::MyanmarScript:
        return QFontDatabase::Myanmar;
    case QLocale::TamilScript:
        return QFontDatabase::Tamil;
    case QLocale::TeluguScript:
        return QFontDatabase::Telugu;
    case QLocale::ThaanaScript:
        return QFontDatabase::Thaana;
    case QLocale::ThaiScript:
        return QFontDatabase::Thai;
    case QLocale::TibetanScript:
        return QFontDatabase::Tibetan;
    case QLocale::SinhalaScript:
        return QFontDatabase::Sinhala;
    case QLocale::SyriacScript:
        return QFontDatabase::Syriac;
    case QLocale::OriyaScript:
        return QFontDatabase::Oriya;
    case QLocale::OghamScript:
        return QFontDatabase::Ogham;
    case QLocale::RunicScript:
        return QFontDatabase::Runic;
    case QLocale::NkoScript:
        return QFontDatabase::Nko;
    default:
        return QFontDatabase::Any;
    }
}

static QFontDatabase::WritingSystem writingSystemFromLocale()
{
    QStringList uiLanguages = QLocale::system().uiLanguages();
    QLocale::Script script;
    if (!uiLanguages.isEmpty())
        script = QLocale(uiLanguages.at(0)).script();
    else
        script = QLocale::system().script();

    return writingSystemFromScript(script);
}

static QFontDatabase::WritingSystem writingSystemForFont(QFontDatabase *db, const QFont &font, bool *hasLatin)
{
    QList<QFontDatabase::WritingSystem> writingSystems = db->writingSystems(font.family());

    // this just confuses the algorithm below. Vietnamese is Latin with lots of special chars
    writingSystems.removeOne(QFontDatabase::Vietnamese);
    *hasLatin = writingSystems.removeOne(QFontDatabase::Latin);

    if (writingSystems.isEmpty())
        return QFontDatabase::Any;

    QFontDatabase::WritingSystem system = writingSystemFromLocale();

    if (writingSystems.contains(system))
        return system;

    if (system == QFontDatabase::TraditionalChinese
            && writingSystems.contains(QFontDatabase::SimplifiedChinese)) {
        return QFontDatabase::SimplifiedChinese;
    }

    if (system == QFontDatabase::SimplifiedChinese
            && writingSystems.contains(QFontDatabase::TraditionalChinese)) {
        return QFontDatabase::TraditionalChinese;
    }

    system = writingSystems.last();

    if (!*hasLatin) {
        // we need to show something
        return system;
    }

    if (writingSystems.count() == 1 && system > QFontDatabase::Cyrillic)
        return system;

    if (writingSystems.count() <= 2 && system > QFontDatabase::Armenian && system < QFontDatabase::Vietnamese)
        return system;

    if (writingSystems.count() <= 5 && system >= QFontDatabase::SimplifiedChinese && system <= QFontDatabase::Korean)
        return system;

    return QFontDatabase::Any;
}

struct FontData {
    DECL_EQ(FontData, &T::font);
    QFont font;
    QString display;
};

class FontFamilyModel : public SimpleListModel<FontData> {
    auto displayData(int row, int) const -> QVariant
    {
        auto &data = at(row);
        return data.display.isEmpty() ? data.font.family() : data.display;
    }
    auto fontData(int row, int) const -> QFont { return at(row).font; }
};

static auto generateList(bool fixedOnly) -> QList<FontData>
{
    QList<FontData> list;
    QFontDatabase db;
    const auto type = fixedOnly ? QFontDatabase::FixedFont : QFontDatabase::GeneralFont;
    const auto def = QFontDatabase::systemFont(type);
    for (auto &family : db.families()) {
        if (fixedOnly && !db.isFixedPitch(family))
            continue;
        FontData data;
        data.font = def;
        data.font.setFamily(family);
        // Deliberately no QFontInfo(data.font).family() here. That resolves the
        // family through the font engine, forcing Qt to load and cache an engine
        // for every installed family; with ~3700 families it costs ~18s, and
        // because each QFont is kept alive in this list the engines can never be
        // evicted, which pushes QFontCache::decreaseCache() into thrashing and
        // takes it past 24s and climbing. Skipping it makes this loop ~4ms.
        // setCurrentFont() resolves the one font it actually needs instead.
        bool hasLatin = false;
        const auto system = writingSystemForFont(&db, data.font, &hasLatin);
        const auto sample = db.writingSystemSample(system);
        if (!sample.isEmpty())
            data.display = data.font.family() % " ("_a % sample % ")"_a;
        list.push_back(data);
    }
    return list;
}

static auto getFontDataList(bool fixedOnly) -> QList<FontData>
{
    if (fixedOnly) {
        static const auto list = generateList(true);
        return list;
    } else {
        static const auto all = generateList(false);
        return all;
    }
}

struct FontComboBox::Data {
    FontComboBox *p = nullptr;
    FontFamilyModel *model = nullptr;
    bool fixedOnly = false;
    auto generateList() -> QList<FontData> { return getFontDataList(fixedOnly); }
};

FontComboBox::FontComboBox(QWidget *parent)
    : QComboBox(parent), d(new Data)
{
    d->p = this;
    setSizeAdjustPolicy(AdjustToMinimumContentsLengthWithIcon);
    setMinimumContentsLength(10);
    connect(SIGNAL_VT(this, currentIndexChanged, int), this, [=] (int idx) {
        emit currentFontChanged();
        Q_UNUSED(idx);
        // This used to setFont(d->model->at(idx).font) so the closed combo box
        // showed the family in its own typeface. QComboBox::changeEvent()
        // answers a FontChange by relaying out the entire popup, and because
        // each row draws in its own family that measures -- and loads a font
        // engine for -- every installed family. With ~3700 fonts the
        // preferences dialog never finished opening. The popup entries still
        // render in their own families via FontFamilyModel::fontData().
    });
    d->model = new FontFamilyModel;
    d->model->setList(d->generateList());
    setModel(d->model);
    setCurrentIndex(0);
}

FontComboBox::~FontComboBox()
{
    delete d->model;
    delete d;
}

auto FontComboBox::setFixedFontOnly(bool fixed) -> void
{
    if (_Change(d->fixedOnly, fixed)) {
        d->model->setList(d->generateList());
        setCurrentIndex(0);
    }
}

auto FontComboBox::setCurrentFont(const QFont &font) -> void
{
    auto find = [this] (const QString &family) -> bool {
        if (family.isEmpty())
            return false;
        for (int i = 0; i < d->model->size(); ++i) {
            if (!d->model->at(i).font.family().compare(family, Qt::CaseInsensitive)) {
                setCurrentIndex(i);
                return true;
            }
        }
        return false;
    };
    // The requested family usually names an installed family outright, so try
    // it before paying for QFontInfo, which has to load the font engine. Fall
    // back to the resolved name for aliases such as "Sans Serif".
    if (!find(font.family()))
        find(QFontInfo(font).family());
}

auto FontComboBox::currentFont() const -> QFont
{
    return d->model->value(currentIndex()).font;
}
