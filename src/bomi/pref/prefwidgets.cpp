#include "prefwidgets.hpp"
#include "enum/codecid.hpp"
#include "os/os.hpp"
#include "pref.hpp"
#include <QListWidget>

HwAccCodecBox::HwAccCodecBox(QWidget *parent)
    : QGroupBox(parent)
{
    // The candidates come from libmpv at runtime (see Pref::defaultHwAccCodecs),
    // so this follows an mpv upgrade rather than bomi's own CodecId enum, which
    // never knew VP9 or AV1. Whether a codec can actually be accelerated depends
    // on the GPU and driver and is only known once decoding starts, so no
    // "not supported" marking is attempted here -- the play info panel reports
    // what mpv ended up using.
    //
    // A checkable QListWidget rather than a column of QCheckBoxes: the list
    // grows whenever mpv gains a codec, and this scrolls on its own once it
    // outgrows the box. Note that putting a QScrollArea inside this promoted
    // group box instead leaves the whole preferences dialog unpainted.
    m_list = new QListWidget;
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    // Tall enough that today's ten codecs fit without scrolling, capped so a
    // much longer list from a future mpv cannot push the dialog off-screen.
    m_list->setMaximumHeight(330);
    // Codec names are short; without this they get elided to "h2..." because the
    // view sizes items before the group box has been given its final width.
    m_list->setTextElideMode(Qt::ElideNone);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (const auto &codec : Pref::defaultHwAccCodecs()) {
        auto item = new QListWidgetItem(codec, m_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    connect(m_list, &QListWidget::itemChanged,
            this, &HwAccCodecBox::valueChanged);

    auto outer = new QVBoxLayout;
    outer->addWidget(m_list);
    setLayout(outer);
    // Hug the list instead of stretching to fill the page.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
}

auto HwAccCodecBox::value() const -> QStringList
{
    QStringList list;
    for (int i = 0; i < m_list->count(); ++i) {
        auto item = m_list->item(i);
        if (item->checkState() == Qt::Checked)
            list.push_back(item->text());
    }
    return list;
}

auto HwAccCodecBox::setValue(const QStringList &list) -> void
{
    for (int i = 0; i < m_list->count(); ++i) {
        auto item = m_list->item(i);
        item->setCheckState(list.contains(item->text()) ? Qt::Checked
                                                        : Qt::Unchecked);
    }
}

/******************************************************************************/

DataButtonGroup::DataButtonGroup(QObject *parent)
    : QButtonGroup(parent)
{
    connect(this, static_cast<Signal<QButtonGroup, int>>(&QButtonGroup::buttonClicked), this, [=] () {
        if (_Change(m_button, checkedButton()))
            emit currentDataChanged(currentData());
    });
}

auto DataButtonGroup::addButton(QAbstractButton *button, const QVariant &data) -> void
{
    QButtonGroup::addButton(button);
    m_data[button] = data;
}

auto DataButtonGroup::button(const QVariant &data) const -> QAbstractButton*
{
    for (auto it = m_data.begin(); it != m_data.end(); ++it) {
        if (*it == data)
            return it.key();
    }
    return nullptr;
}

auto DataButtonGroup::setCurrentData(const QVariant &data) -> void
{
    auto button = this->button(data);
    if (button)
        button->setChecked(true);
    if (_Change(m_button, button))
        emit currentDataChanged(currentData());
}
