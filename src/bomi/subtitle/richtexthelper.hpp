#ifndef RICHTEXTHELPER_HPP
#define RICHTEXTHELPER_HPP

class RichTextHelper {
public:
    static constexpr const auto QCI = Qt::CaseInsensitive;

    virtual ~RichTextHelper() {}

    SIA _Same(const QString &str, const char *latin1) -> bool
    { return !str.compare(_L(latin1), QCI); }

    SIA _Same(const QStringView &str, const char *latin1) -> bool
    { return !str.compare(_L(latin1), QCI); }

    static auto toInt(const QStringView &text) -> int;
    static auto toColor(const QStringView &text) -> QColor;
    static auto isRightBracket(ushort c) -> bool {return c == '>';}
    static auto isSeparator(ushort c) -> bool
        { return _IsOneOf(c, ' ', '\t', '\r', '\n'); }
    static auto isWhitespace(ushort c) -> bool { return _IsOneOf(c, ' ', '\t'); }
    static auto isNewLine(ushort c) -> bool { return _IsOneOf(c, '\r', '\n'); }
    static auto replace(const QStringView &str, const QString &from,
                        const QString &to, Qt::CaseSensitivity s = QCI) -> QString;
    static auto trim(const QStringView &text) -> QStringView
    {
        if (text.isEmpty()) return QStringView();
        int start = 0, end = text.size();
        while (start < end && isSeparator(text.at(start).unicode())) ++start;
        while (end > start && isSeparator(text.at(end-1).unicode())) --end;
        return start < end ? text.mid(start, end-start) : QStringView();
    }
    static auto skipSeparator(int &pos, const QStringView &text) -> bool
    {
        for (; pos < text.size() && isSeparator(text.at(pos).unicode()); ++pos) ;
        return pos >= text.size(); // true for end
    }
    static auto skipSeparator(int &pos, const QString &text) -> bool
        { return skipSeparator(pos, QStringView(text).mid(0)); }

    static auto entityCharacter(const QStringView &entity) -> QChar;
    static auto toFontPixelSize(const QStringView &size) -> int;
    static auto pixelSizeToPointSize(double pt) -> int;

    struct Tag {
        struct Attr {
            Attr() { }
            Attr(const QStringView &name, const QStringView &value)
                : name(name), value(value) { }
            QStringView name, value;
        };
        auto index(const char *attr) const -> int
        {
            for (int i = 0; i < this->attr.size(); ++i) {
                if (_Same(this->attr[i].name, attr))
                    return i;
            }
            return -1;
        }
        auto value(const char *attr) const -> QStringView
        {
            const int idx = index(attr);
            return idx < 0 ? QStringView() : this->attr[idx].value;
        }
        QMap<int, QVariant> style() const;
        int pos = -1;
        QStringView name;
        QList<Attr> attr;
    };
    static auto parseTag(const QStringView &text, int &pos) -> Tag;
    static auto parseTag(const QString &text, int &pos) -> Tag
        { return parseTag(QStringView(text).mid(0, -1), pos); }
    static auto innerText(const QString &open, const QString &close,
                          const QStringView &text, QStringView &block,
                          int &pos, Tag &tag) -> int;
    static auto innerText(const QString &open, const QString &close,
                          const QString &text, QStringView &block,
                          int &pos, Tag &tag) -> int
        { return innerText(open, close, QStringView(text).mid(0), block, pos, tag); }
};

#endif // RICHTEXTHELPER_HPP
