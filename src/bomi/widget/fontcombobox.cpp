#include "fontcombobox.hpp"
#include "misc/simplelistmodel.hpp"
#include <QFontDatabase>
#include <QListView>
#include <QStylePainter>
#include <QStyleOptionComboBox>

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
    // Known cost: returning a per-row font means Qt loads that family's font
    // engine the first time the row is painted, so scrolling the drop-down is
    // sluggish until the visited rows are cached. Everything cheap has already
    // been done -- the engine load is inherent to previewing each family, and
    // it cannot be moved off the GUI thread because QFontDatabase engine
    // loading is not thread-safe in Qt 5. Dropping it would mean giving up the
    // preview, or previewing only the sample text and drawing the family name
    // in the UI font, which roughly halves the work but changes the look.
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
    // Every row carries its own family through FontFamilyModel::fontData(), so
    // laying the popup out measures each row in that family and loads a font
    // engine for it. Opening the drop-down took ~24s with ~3700 families
    // installed. Uniform sizes make QListView measure one row and reuse it,
    // which brings that down to ~0.1s; only the visible rows then load an
    // engine, when they are painted.
    //
    // Batched layout then keeps the *first* open cheap too. Uniform sizes
    // alone still leave a one-off per-row pass -- ~900ms for 3700 rows -- and
    // because that is longer than QApplication::doubleClickInterval() (400ms),
    // QComboBox stops blocking the mouse release that follows the click, so
    // the drop-down opened and immediately closed again. Laying out in batches
    // brings the first open to ~27ms and it stays open.
    if (auto view = qobject_cast<QListView*>(this->view())) {
        view->setUniformItemSizes(true);
        view->setLayoutMode(QListView::Batched);
    }
    setCurrentIndex(0);
}

void FontComboBox::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);
    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    painter.drawComplexControl(QStyle::CC_ComboBox, opt);
    // Draw the label in the family it names. Doing it on the painter rather
    // than through setFont() is the whole point: setFont() sends a FontChange,
    // and QComboBox answers that by relaying out the entire drop-down, which
    // measures every row in its own family and loads that many font engines.
    // Only the one selected family is loaded here, and only to paint a label.
    const auto font = currentFont();
    opt.fontMetrics = QFontMetrics(font);
    painter.setFont(font);
    painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
}

void FontComboBox::showPopup()
{
    QComboBox::showPopup();
    // With batched layout the rows past the first batch are not laid out yet
    // when QComboBox does its own scroll-to-current, so the popup opens at the
    // top of the list rather than at the selected family. Nudge it until the
    // row has been laid out and the scroll actually lands.
    const auto idx = model()->index(currentIndex(), modelColumn());
    if (!idx.isValid())
        return;
    auto v = view();
    auto timer = new QTimer(v);
    timer->setInterval(10);
    auto tries = std::make_shared<int>(0);
    connect(timer, &QTimer::timeout, v, [v, idx, timer, tries] () {
        v->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        const bool done = v->visualRect(idx).intersects(v->viewport()->rect());
        if (done || !v->isVisible() || ++*tries > 50)
            timer->deleteLater();
    });
    timer->start();
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
