#ifndef X11_HPP
#define X11_HPP

#include "os/os.hpp"

#ifdef Q_OS_LINUX

#include "configure.hpp"
#include "misc/log.hpp"
#include "enum/deintmethod.hpp"
#include <functional>

#ifdef None
#undef None
#endif


namespace OS {

auto createAdapter(QWidget *w) -> WindowAdapter*;

struct X11;

class X11WindowAdapter : public WindowAdapter {
public:
    X11WindowAdapter(QWindow* w);
    auto setFullScreen(bool fs) -> void final;
    auto isAlwaysOnTop() const -> bool final;
    auto setAlwaysOnTop(bool onTop) -> void final;
    auto startMoveByDrag(const QPointF &m) -> void final;
    auto moveByDrag(const QPointF &m) -> void final;
    auto endMoveByDrag() -> void final;
    auto setImeEnabled(bool /*enabled*/) -> void { }
    auto isImeEnabled() const -> bool { return false; }
private:
    auto stopDrag() -> void;
    QTimer m_timer;
};

// Used when there is no X11 connection, i.e. a native Wayland session. The
// base class already implements fullscreen, frameless, move-by-drag and
// snapping through Qt; only the four pure virtuals need filling in.
class QtWindowAdapter : public WindowAdapter {
public:
    QtWindowAdapter(QWindow *w) : WindowAdapter(w) { }
    auto isAlwaysOnTop() const -> bool final;
    auto setAlwaysOnTop(bool onTop) -> void final;
    auto setImeEnabled(bool /*enabled*/) -> void final { }
    auto isImeEnabled() const -> bool final { return false; }
};

}

#endif

#endif // X11_HPP
