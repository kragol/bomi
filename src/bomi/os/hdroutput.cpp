#include "hdroutput.hpp"
#include "misc/log.hpp"
#include <QGuiApplication>
#include <QWindow>

DECLARE_LOG_CONTEXT(HDR)

#ifdef Q_OS_LINUX

#include <QtGui/qpa/qplatformwindow_p.h>
#include <wayland-client.h>
#include <unistd.h>
#include "os/color-management-v1-client-protocol.h"

namespace {

struct Description {
    bool ready = false, failed = false;
};

const wp_image_description_v1_listener descriptionListener = {
    .failed = [] (void *d, wp_image_description_v1 *, uint32_t cause, const char *msg) {
        static_cast<Description*>(d)->failed = true;
        _Error("The compositor refused an image description (%%): %%", cause, msg);
    },
    .ready = [] (void *d, wp_image_description_v1 *, uint32_t)
        { static_cast<Description*>(d)->ready = true; },
};

// What the compositor would like the surface to be.
struct Preferred {
    bool done = false;
    uint32_t minLum = 0, maxLum = 0, refLum = 0;
    uint32_t targetMin = 0, targetMax = 0;
    int32_t targetPrim[8] = {};
    bool haveTargetPrim = false;
};

const wp_image_description_info_v1_listener infoListener = {
    .done = [] (void *d, wp_image_description_info_v1 *)
        { static_cast<Preferred*>(d)->done = true; },
    .icc_file = [] (void *, wp_image_description_info_v1 *, int32_t fd, uint32_t)
        { close(fd); },
    .primaries = [] (void *, wp_image_description_info_v1 *, int32_t, int32_t, int32_t,
                     int32_t, int32_t, int32_t, int32_t, int32_t) { },
    .primaries_named = [] (void *, wp_image_description_info_v1 *, uint32_t) { },
    .tf_power = [] (void *, wp_image_description_info_v1 *, uint32_t) { },
    .tf_named = [] (void *, wp_image_description_info_v1 *, uint32_t) { },
    .luminances = [] (void *d, wp_image_description_info_v1 *, uint32_t min, uint32_t max,
                      uint32_t ref) {
        auto p = static_cast<Preferred*>(d);
        p->minLum = min; p->maxLum = max; p->refLum = ref;
    },
    .target_primaries = [] (void *d, wp_image_description_info_v1 *, int32_t rx, int32_t ry,
                            int32_t gx, int32_t gy, int32_t bx, int32_t by,
                            int32_t wx, int32_t wy) {
        auto p = static_cast<Preferred*>(d);
        const int32_t prim[8] = {rx, ry, gx, gy, bx, by, wx, wy};
        std::copy(prim, prim + 8, p->targetPrim);
        p->haveTargetPrim = true;
    },
    .target_luminance = [] (void *d, wp_image_description_info_v1 *, uint32_t min, uint32_t max) {
        auto p = static_cast<Preferred*>(d);
        p->targetMin = min; p->targetMax = max;
    },
    .target_max_cll = [] (void *, wp_image_description_info_v1 *, uint32_t) { },
    .target_max_fall = [] (void *, wp_image_description_info_v1 *, uint32_t) { },
};

const wp_color_manager_v1_listener managerListener = {
    .supported_intent = [] (void *, wp_color_manager_v1 *, uint32_t) { },
    .supported_feature = [] (void *, wp_color_manager_v1 *, uint32_t) { },
    .supported_tf_named = [] (void *, wp_color_manager_v1 *, uint32_t) { },
    .supported_primaries_named = [] (void *, wp_color_manager_v1 *, uint32_t) { },
    .done = [] (void *, wp_color_manager_v1 *) { },
};

}

struct HdrSurface::Data {
    HdrSurface *p = nullptr;
    QWindow *window = nullptr;
    QPointer<QNativeInterface::Private::QWaylandWindow> native;
    wl_display *display = nullptr;
    // Our own queue, so Qt's event dispatching never sees these objects.
    wl_event_queue *queue = nullptr;
    wl_display *wrapper = nullptr;
    wl_registry *registry = nullptr;
    wp_color_manager_v1 *manager = nullptr;
    wl_surface *surface = nullptr;
    wp_color_management_surface_feedback_v1 *feedback = nullptr;
    wp_color_management_surface_v1 *cm = nullptr;
    Preferred preferred;
    bool displayHdr = false;
    double peak = 0;
    QTimer poll;

    // Blocks until the description is ready or refused; only our queue runs.
    auto wait(wp_image_description_v1 *desc) -> bool
    {
        Description state;
        wp_image_description_v1_add_listener(desc, &descriptionListener, &state);
        while (!state.ready && !state.failed)
            if (wl_display_roundtrip_queue(display, queue) < 0)
                return false;
        return state.ready;
    }

    auto queryPreferred() -> void
    {
        if (!feedback)
            return;
        auto desc = wp_color_management_surface_feedback_v1_get_preferred(feedback);
        Preferred info;
        if (wait(desc)) {
            auto obj = wp_image_description_v1_get_information(desc);
            wp_image_description_info_v1_add_listener(obj, &infoListener, &info);
            while (!info.done)
                if (wl_display_roundtrip_queue(display, queue) < 0)
                    break;
        }
        wp_image_description_v1_destroy(desc);
        preferred = info;
        const auto peak = info.targetMax ? info.targetMax : info.maxLum;
        // BOMI_HDR_DISPLAY=1 pretends the display is in HDR mode, for testing
        // on a compositor or output without HDR (KWin then maps PQ to SDR).
        static const bool force = qEnvironmentVariableIntValue("BOMI_HDR_DISPLAY");
        const bool hdr = force || (info.refLum > 0 && peak > info.refLum);
        _Info("Display: peak %% nits, SDR white %% nits -> %%", peak, info.refLum,
              hdr ? "HDR" : "SDR");
        if (_Change(displayHdr, hdr))
            emit p->displayHdrChanged(hdr);
    }

    auto attach() -> void
    {
        if (!native) {
            native = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
            if (!native)
                return;
            using W = QNativeInterface::Private::QWaylandWindow;
            QObject::connect(native, &W::surfaceCreated, p, [this] () { attachSurface(); });
            QObject::connect(native, &W::surfaceDestroyed, p, [this] () { detachSurface(); });
        }
        attachSurface();
    }

    auto attachSurface() -> void
    {
        if (!native || !native->surface() || surface == native->surface())
            return;
        detachSurface();
        surface = native->surface();
        feedback = wp_color_manager_v1_get_surface_feedback(manager, surface);
        static const wp_color_management_surface_feedback_v1_listener listener = {
            .preferred_changed = [] (void *d, wp_color_management_surface_feedback_v1 *, uint32_t) {
                auto data = static_cast<Data*>(d);
                // Not from inside the dispatch: queryPreferred() runs our queue.
                QMetaObject::invokeMethod(data->p, [data] () { data->queryPreferred(); },
                                          Qt::QueuedConnection);
            },
        };
        wp_color_management_surface_feedback_v1_add_listener(feedback, &listener, this);
        queryPreferred();
        if (peak > 0)
            apply();
    }

    // The objects are inert once the surface is gone; Qt reports that after
    // destroying it.
    auto detachSurface() -> void
    {
        if (cm)
            wp_color_management_surface_v1_destroy(cm);
        if (feedback)
            wp_color_management_surface_feedback_v1_destroy(feedback);
        cm = nullptr;
        feedback = nullptr;
        surface = nullptr;
        wl_display_flush(display);
    }

    auto apply() -> void
    {
        if (!surface)
            return;
        if (peak <= 0) {
            if (cm)
                wp_color_management_surface_v1_unset_image_description(cm);
        } else {
            const auto nits = uint32_t(peak + 0.5);
            auto params = wp_color_manager_v1_create_parametric_creator(manager);
            wp_image_description_creator_params_v1_set_primaries_named(
                params, WP_COLOR_MANAGER_V1_PRIMARIES_BT2020);
            wp_image_description_creator_params_v1_set_tf_named(
                params, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ);
            if (preferred.haveTargetPrim) {
                const auto *pr = preferred.targetPrim;
                wp_image_description_creator_params_v1_set_mastering_display_primaries(
                    params, pr[0], pr[1], pr[2], pr[3], pr[4], pr[5], pr[6], pr[7]);
            }
            // Declaring the content's range is what makes KWin tone-map it
            // instead of clipping above ~1000 nits.
            wp_image_description_creator_params_v1_set_mastering_luminance(
                params, preferred.targetMin, nits);
            wp_image_description_creator_params_v1_set_max_cll(params, nits);
            auto desc = wp_image_description_creator_params_v1_create(params);
            if (wait(desc)) {
                if (!cm)
                    cm = wp_color_manager_v1_get_surface(manager, surface);
                wp_color_management_surface_v1_set_image_description(
                    cm, desc, WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
                _Info("Surface tagged PQ/BT.2020, peak %% nits.", nits);
            }
            wp_image_description_v1_destroy(desc);
        }
        wl_display_flush(display);
        window->requestUpdate(); // the description applies with the next commit
    }
};

HdrSurface::HdrSurface(QWindow *window)
    : d(new Data)
{
    d->p = this;
    d->window = window;
    auto app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!app || !app->display())
        return;
    d->display = app->display();
    d->queue = wl_display_create_queue(d->display);
    d->wrapper = static_cast<wl_display*>(wl_proxy_create_wrapper(d->display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(d->wrapper), d->queue);
    d->registry = wl_display_get_registry(d->wrapper);
    static const wl_registry_listener listener = {
        .global = [] (void *d, wl_registry *registry, uint32_t name, const char *iface, uint32_t) {
            if (qstrcmp(iface, wp_color_manager_v1_interface.name) == 0)
                static_cast<Data*>(d)->manager = static_cast<wp_color_manager_v1*>(
                    wl_registry_bind(registry, name, &wp_color_manager_v1_interface, 1));
        },
        .global_remove = [] (void *, wl_registry *, uint32_t) { },
    };
    wl_registry_add_listener(d->registry, &listener, d);
    wl_display_roundtrip_queue(d->display, d->queue);
    if (!d->manager) {
        _Info("The compositor has no colour management: no HDR output.");
        return;
    }
    wp_color_manager_v1_add_listener(d->manager, &managerListener, nullptr);

    // Qt reads every event from the socket, but those for our queue wait
    // there until dispatched. Only "preferred changed" arrives unasked.
    d->poll.setInterval(500);
    connect(&d->poll, &QTimer::timeout, this,
            [this] () { wl_display_dispatch_queue_pending(d->display, d->queue); });
    d->poll.start();

    // QWindow::visibleChanged comes before the platform window exists.
    window->installEventFilter(this);
    if (window->handle())
        d->attach();
}

auto HdrSurface::eventFilter(QObject *o, QEvent *e) -> bool
{
    if (o == d->window && (e->type() == QEvent::PlatformSurface || e->type() == QEvent::Expose))
        QMetaObject::invokeMethod(this, [this] () { d->attach(); }, Qt::QueuedConnection);
    return QObject::eventFilter(o, e);
}

HdrSurface::~HdrSurface()
{
    if (d->manager) {
        d->detachSurface();
        wp_color_manager_v1_destroy(d->manager);
    }
    if (d->registry)
        wl_registry_destroy(d->registry);
    if (d->wrapper)
        wl_proxy_wrapper_destroy(d->wrapper);
    if (d->queue)
        wl_event_queue_destroy(d->queue);
    delete d;
}

auto HdrSurface::isAvailable() const -> bool
{
    return d->manager;
}

auto HdrSurface::isDisplayHdr() const -> bool
{
    return d->displayHdr;
}

auto HdrSurface::setPq(double peak) -> void
{
    if (!d->manager || !_Change(d->peak, qMax(0.0, peak)))
        return;
    d->apply();
}

#else

struct HdrSurface::Data { };
auto HdrSurface::eventFilter(QObject *o, QEvent *e) -> bool { return QObject::eventFilter(o, e); }
HdrSurface::HdrSurface(QWindow *) : d(new Data) { }
HdrSurface::~HdrSurface() { delete d; }
auto HdrSurface::isAvailable() const -> bool { return false; }
auto HdrSurface::isDisplayHdr() const -> bool { return false; }
auto HdrSurface::setPq(double) -> void { }

#endif
