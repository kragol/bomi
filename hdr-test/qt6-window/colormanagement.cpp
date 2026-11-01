#include "colormanagement.h"

#include <QGuiApplication>
#include <QWindow>
#include <QtGui/qpa/qplatformwindow_p.h>
#include <cstring>
#include <unistd.h>
#include <wayland-client.h>
#include "color-management-v1-client-protocol.h"

namespace {

struct Info {
    bool done = false;
    uint32_t minLum = 0, maxLum = 0, refLum = 0;           // luminances
    uint32_t targetMin = 0, targetMax = 0;                 // target_luminance
    int32_t targetPrim[8] = {};
    bool haveTargetPrim = false;
};

struct Desc {
    bool ready = false, failed = false;
};

const wp_color_manager_v1_listener managerListener = {
    [](void *, wp_color_manager_v1 *, uint32_t) {},
    [](void *, wp_color_manager_v1 *, uint32_t) {},
    [](void *, wp_color_manager_v1 *, uint32_t) {},
    [](void *, wp_color_manager_v1 *, uint32_t) {},
    [](void *, wp_color_manager_v1 *) {},
};

const wp_image_description_v1_listener descListener = {
    [](void *d, wp_image_description_v1 *, uint32_t cause, const char *msg) {
        static_cast<Desc *>(d)->failed = true;
        qWarning("image description failed (%u): %s", cause, msg);
    },
    [](void *d, wp_image_description_v1 *, uint32_t) { static_cast<Desc *>(d)->ready = true; },
};

const wp_image_description_info_v1_listener infoListener = {
    [](void *d, wp_image_description_info_v1 *) { static_cast<Info *>(d)->done = true; },
    [](void *, wp_image_description_info_v1 *, int32_t fd, uint32_t) { close(fd); },
    [](void *, wp_image_description_info_v1 *, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t) {},
    [](void *, wp_image_description_info_v1 *, uint32_t) {},
    [](void *, wp_image_description_info_v1 *, uint32_t) {},
    [](void *, wp_image_description_info_v1 *, uint32_t) {},
    [](void *d, wp_image_description_info_v1 *, uint32_t min, uint32_t max, uint32_t ref) {
        auto *i = static_cast<Info *>(d);
        i->minLum = min; i->maxLum = max; i->refLum = ref;
    },
    [](void *d, wp_image_description_info_v1 *, int32_t rx, int32_t ry, int32_t gx, int32_t gy,
       int32_t bx, int32_t by, int32_t wx, int32_t wy) {
        auto *i = static_cast<Info *>(d);
        const int32_t p[8] = {rx, ry, gx, gy, bx, by, wx, wy};
        std::memcpy(i->targetPrim, p, sizeof p);
        i->haveTargetPrim = true;
    },
    [](void *d, wp_image_description_info_v1 *, uint32_t min, uint32_t max) {
        auto *i = static_cast<Info *>(d);
        i->targetMin = min; i->targetMax = max;
    },
    [](void *, wp_image_description_info_v1 *, uint32_t) {},
    [](void *, wp_image_description_info_v1 *, uint32_t) {},
};

const wp_color_management_surface_feedback_v1_listener feedbackListener = {
    [](void *, wp_color_management_surface_feedback_v1 *, uint32_t) {},
};

struct Globals {
    wp_color_manager_v1 *manager = nullptr;
};

const wl_registry_listener registryListener = {
    [](void *d, wl_registry *registry, uint32_t name, const char *interface, uint32_t) {
        if (std::strcmp(interface, wp_color_manager_v1_interface.name) == 0)
            static_cast<Globals *>(d)->manager = static_cast<wp_color_manager_v1 *>(
                wl_registry_bind(registry, name, &wp_color_manager_v1_interface, 1));
    },
    [](void *, wl_registry *, uint32_t) {},
};

} // namespace

HdrTarget describeSurface(QWindow *window, double peakOverride)
{
    HdrTarget result;
    auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    auto *win = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    if (!app || !win || !win->surface()) {
        qWarning("describeSurface: not a Wayland window");
        return result;
    }
    wl_display *display = app->display();
    wl_surface *surface = win->surface();

    // A private queue, so Qt's event dispatching never sees our objects.
    wl_event_queue *queue = wl_display_create_queue(display);
    auto *wrapper = static_cast<wl_display *>(wl_proxy_create_wrapper(display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapper), queue);

    Globals globals;
    wl_registry *registry = wl_display_get_registry(wrapper);
    wl_registry_add_listener(registry, &registryListener, &globals);
    wl_display_roundtrip_queue(display, queue);
    if (!globals.manager) {
        qWarning("describeSurface: compositor has no wp_color_manager_v1");
        return result;
    }
    wp_color_manager_v1_add_listener(globals.manager, &managerListener, nullptr);

    // What the compositor would like this surface to be.
    auto *feedback = wp_color_manager_v1_get_surface_feedback(globals.manager, surface);
    wp_color_management_surface_feedback_v1_add_listener(feedback, &feedbackListener, nullptr);
    Desc preferredState;
    auto *preferred = wp_color_management_surface_feedback_v1_get_preferred(feedback);
    wp_image_description_v1_add_listener(preferred, &descListener, &preferredState);
    while (!preferredState.ready && !preferredState.failed)
        wl_display_roundtrip_queue(display, queue);
    if (preferredState.failed)
        return result;
    Info info;
    auto *infoObj = wp_image_description_v1_get_information(preferred);
    wp_image_description_info_v1_add_listener(infoObj, &infoListener, &info);
    while (!info.done)
        wl_display_roundtrip_queue(display, queue);

    result.displayPeak = info.targetMax ? info.targetMax : info.maxLum;
    result.reference = info.refLum;
    // The compositor maps PQ's 203-nit SDR white to its own reference
    // luminance, so express the display peak in PQ's scale, as mpv does.
    result.peak = peakOverride > 0 ? peakOverride : result.displayPeak * 203.0 / result.reference;
    const uint32_t peak = uint32_t(result.peak + 0.5);
    qInfo("preferred: luminances min %.4f max %u ref %u, target %.4f-%u nits -> PQ peak %u",
          info.minLum / 10000.0, info.maxLum, info.refLum, info.targetMin / 10000.0,
          info.targetMax, peak);

    auto *params = wp_color_manager_v1_create_parametric_creator(globals.manager);
    wp_image_description_creator_params_v1_set_primaries_named(params, WP_COLOR_MANAGER_V1_PRIMARIES_BT2020);
    wp_image_description_creator_params_v1_set_tf_named(params, WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ);
    if (info.haveTargetPrim) {
        const int32_t *p = info.targetPrim;
        wp_image_description_creator_params_v1_set_mastering_display_primaries(
            params, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
    }
    wp_image_description_creator_params_v1_set_mastering_luminance(params, info.targetMin, peak);
    wp_image_description_creator_params_v1_set_max_cll(params, peak);
    Desc ownState;
    auto *own = wp_image_description_creator_params_v1_create(params);
    wp_image_description_v1_add_listener(own, &descListener, &ownState);
    while (!ownState.ready && !ownState.failed)
        wl_display_roundtrip_queue(display, queue);
    if (ownState.failed) {
        result.peak = 0;
        return result;
    }

    // Kept alive on purpose: destroying it would unset the description.
    auto *cmSurface = wp_color_manager_v1_get_surface(globals.manager, surface);
    wp_color_management_surface_v1_set_image_description(cmSurface, own,
                                                         WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
    wl_display_flush(display);
    return result; // applied on Qt's next commit of the surface
}
