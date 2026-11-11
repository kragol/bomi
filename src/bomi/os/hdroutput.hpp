#ifndef HDROUTPUT_HPP
#define HDROUTPUT_HPP

class QWindow;

// HDR output through the Wayland colour-management protocol: tells whether the
// window's display is in HDR mode, and tags the window's surface PQ/BT.2020
// with the video's peak luminance, so that the compositor (KWin) maps it to
// the display. Inert outside native Wayland, or when the compositor has no
// wp_color_manager_v1. The window must not also be given a QSurfaceFormat
// colour space: a wl_surface takes only one colour-management object.
class HdrSurface : public QObject {
    Q_OBJECT
public:
    HdrSurface(QWindow *window);
    ~HdrSurface();
    auto isAvailable() const -> bool;
    // The compositor would like this surface brighter than SDR white, i.e.
    // its output is in HDR mode.
    auto isDisplayHdr() const -> bool;
    // Tag the surface PQ/BT.2020 with mastering peak and MaxCLL in nits, or
    // remove the tag with 0. Applied with the window's next frame.
    auto setPq(double peak) -> void;
signals:
    void displayHdrChanged(bool hdr);
private:
    auto eventFilter(QObject *o, QEvent *e) -> bool final;
    struct Data;
    Data *d;
};

#endif // HDROUTPUT_HPP
