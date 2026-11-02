# TODO

Open work since the port to the system libmpv (merged 2026-10-04, released as v0.10.0): what still falls short of the old vendored-mpv bomi, and what could take it further. README.md, "Known issues after the libmpv port", has the background and measurements behind many items.

## Performance

- [ ] **Enable `MPV_RENDER_PARAM_ADVANCED_CONTROL`** (deferred: low payoff, real risk).
  - *What it buys:* direct rendering, meaning the software decoder writes straight
    into GPU-visible memory, saving one copy per frame (logged now as
    `DR failed - disabling`). Also GPU-rendered mpv screenshots, which bomi does not
    use, because its snapshots render into its own FBO.
  - *How much:* plain mpv on the 4K HEVC 10-bit file, software decoding, one run
    each: `--vd-lavc-dr=yes` 180% CPU vs `no` 193%, so about 7%. Expect bomi to go
    from about 185% to about 170%. **Nothing with hardware decoding**, where frames
    are already on the GPU.
  - *What it costs:* with the flag set, any wait from the render thread on a thread
    using libmpv becomes a permanent freeze of the mpv core, not a warning. Fix these
    first:
    - [x] Snapshots no longer make core calls on the render thread: `time-pos` and
      `sub-visibility` are handled on the GUI thread, and the render pass only
      renders the capture and posts it back.
    - [ ] Hidden or minimised window: Qt stops rendering, so nothing calls
      `mpv_render_context_update()`, and the decoder can block waiting for it.
      Service it from the render-thread `Waker` even when no frame is drawn. In this
      mode `update()` allocates textures, so the GL context must be made current
      there.
    - [ ] Audit everything else reachable from `VideoRenderer::render()` for
      non-render libmpv calls.
  - *History:* deadlocked when tried on the old GUI-thread render path. The
    render-thread path it needs is in place (`547add1f`); not retried since.

## Inert controls

- [ ] **Normalizer gain readout** (play info panel) is empty: `dynaudnorm` does not
  report its gain. Drop the readout, or estimate it (for example `astats` before and
  after).
- [ ] **Spectrum visualizer.** libmpv gives no access to decoded PCM, so this needs an
  out-of-band route (for example a lavfi `showspectrum`/`ashowinfo` side branch, or an
  audio-output tap). Hardest item on the list.
- [ ] **"Disable Filters"** (Video > Filter) does nothing. It never did on master
  either. Make it bypass the colour matrix and effects, or remove the menu item.
- [ ] **Deinterlacing preferences.** mpv's `--deinterlace=auto` now does the work
  (`bwdif`, interlaced frames only), so the method and field-doubling choices in
  Preferences are ignored. Prune them to what mpv offers, or map them onto a
  `bwdif`/`yadif` filter that only runs on interlaced frames.
  The base `OS::HwAcc` class survives only to answer `deintcaps.cpp` (and the
  Windows code) with "no API"; remove it as part of this.
- [ ] **bomi's own motion interpolator and GL scaler kernels** are inert, replaced by
  mpv's equivalents. Remove the dead code and any preferences that only drove it.

- [ ] **Channel manipulation → `pan`** (very low priority: not needed for the
  maintainer's own use, where mpv's default downmix is fine, but kept as a feature
  rather than removed). Translate `ChannelLayoutMap` (source → destination speaker
  mixing per layout pair) into a `pan` filter in `af()`. Until then the setting in
  Preferences > Audio is ignored.

## Smaller regressions

- [ ] **DVD menu hit-testing** went with `disc-mouse-on-button`. Check whether modern
  mpv's dvdnav exposes anything equivalent, or drop the feature.
- [ ] **`vsync-ratio` in the play info panel** reads 1.4–3.0 where about 6 is expected
  for 24fps on 143.84Hz. Find out what it counts under `vo=libmpv` before trusting
  the display-sync figures.
- [ ] **Show that display sync improves playback.** It engages, but nothing shows it
  helps yet. Test with high-bitrate 23.976/24fps content, use counter deltas over a
  steady window without touching the UI, and look at a panning shot.
- [ ] **`bomi --action <id>` does nothing to a running bomi.** Sent from a second instance (same `TMPDIR`, so the single-instance socket is shared), `--action video/snapshot/quick` and `--action window/toggle-fs` had no effect, while the same actions work from their keyboard shortcuts. Seen 2026-11-02 with both the Qt 5 build (master) and the Qt 6 port, so it predates the port; not checked on `legacy`. `--action tool/pref` given at startup does open the dialog, so the problem is forwarding to the running instance (`App`'s `LocalConnection` message handling in `player/app.cpp`) or executing the action once it arrives.

- [ ] **`debian/` and `rpm/` packaging** (inherited from upstream and bm16ton,
  untested here) still build the vendored mpv (`debian/build.sh` runs mpv's waf,
  build-depends name `ffmpeg-bomi` and codec libraries). Update them for the
  system libmpv or drop them; `legacy` keeps the working versions.

- [x] **Main checkout no longer builds master** (`MPV_EXPORT does not name a type` in `/usr/include/mpv/render.h`). This is not the branch's code: a fresh checkout of master builds, as the `libmpv-port` worktree does. The checkout's generated top-level `Makefile` comes from the legacy `configure`. It still has the `build-mpv` target and puts `build/lib/pkgconfig` first in `PKG_CONFIG_PATH`, so pkg-config finds the vendored `mpv.pc`, and the vendored `build/include/mpv/client.h` shadows the system one.
  - [x] Fix the checkout: done 2026-10-08 (`git clean -fdx -e .claude`, then `./configure` and `make`).
  - [x] Fix master's `configure` too: the `/opt/ffmpeg-bomi` and `/opt/libass-bomi` `PKG_CONFIG_PATH` entries are gone, and `bomi.pro` stops qmake with an explanation when the vendored `build/include/mpv/client.h` is present. The guard is in qmake rather than `configure` because a stale `legacy` Makefile still runs master's `bomi.pro`; it checks the mpv header, not `build/lib/pkgconfig`, because `build-libchardet` legitimately installs there.
- [ ] **Find Subtitle: opensubtitles.com and provider settings.** The dialog now
  runs on subliminal (`subtitle/subliminalfinder.cpp` plus the embedded
  `subtitle/subliminal_helper.py`), which fixed the opensubtitles.org advert
  problem: subliminal's own client still gets real subtitles. Still to do:
  - [x] opensubtitles.com accounts: read from subliminal's own `subliminal.toml` and listed read-only in Preferences > Subtitle > Find. Downloads with a real account verified 2026-10-07.
  - Let the user choose providers and search languages. Searches currently use
    the languages ticked in the dialog, plus bomi's language and English.
  - Show progress or allow cancelling: a search takes about 10-15 s, because
    subliminal queries every provider.

## Playback history drawer

- [ ] **Readable columns.**
  - Size columns as fractions of the drawer width instead of fixed pixels (`HistoryView.qml:62-66`); the fixed widths are far too narrow on HiDPI. Drop the 40% width cap (`BaseApp.qml:98`). The Qt 6 port follows the display scale, so fixed pixel widths grow with it there; re-check under Qt 6 before fixing.
  - Location: elide in the middle, so the start (which drive) and the end (which folder) both stay visible. Drop the filename, which repeats Name (`historymodel.cpp:284`).
  - Latest Playback (`historymodel.cpp:282`, now `Qt::ISODate`): time only for today (`14:23`), `Yesterday 23:40` for yesterday (late-night viewing crosses midnight), the date alone for anything older.
  - Hover tooltip with the full name and full path.
- [ ] **Make resizing and horizontal scrolling discoverable.** Both already work (header dots; wheel tilt or Alt+wheel). Show a resize cursor over the separators, show a horizontal scroll bar when content overflows, and save dragged widths (as fractions) in the settings. `ModelView.qml` is shared with the playlist drawer.
- [ ] **Follow-up: date format preference** for Latest Playback (relative, ISO, locale short/long).
- [ ] **Stars** (low priority; most useful for streaming URLs such as TV channels). They are favourites: starred entries sort first and survive "Clear history", but nothing in the UI says so. Clicking one turns every star white. Leads: `setStarred()` resets the whole model instead of emitting `dataChanged`, and the icon binds to the non-reactive `history.isStarred(row)` instead of the `star` role. The "off" icon (`img/fav-off.png`) is almost fully transparent. Add a tooltip that explains what a star does.

## Dialogs

- [ ] **Hard-coded left column widths are too narrow on HiDPI.** Several dialogs fix the width of their left column (labels or page list) in pixels, so text is cut off or cramped on a HiDPI screen. Let the layout size these from their contents (or scale by font metrics) instead. Leads, not yet checked one by one: Preferences page tree fixed at 200 px min and max (`ui/prefdialog.ui:18-24`); 150 px label columns in `ui/osdstyle_widget.ui` and `ui/subtitlefinddialog.ui`; 100 px in `ui/subtitleviewer.ui`; 75 px in `ui/opendvddialog.ui`. Also check the `setFixedWidth`/`setMinimumWidth` calls in `dialog/audioequalizerdialog.cpp`, `dialog/videocolordialog.cpp` and `misc/logviewer.cpp`. Under the Qt 6 port these pixel sizes scale with the display (1.6× here), so this may already be fine there; re-check under Qt 6 first.

## Beyond the old bomi

- [ ] **HDR output.** HDR sources are already tone-mapped to SDR correctly, which the old bomi could not do. Real HDR output to an HDR display needs an HDR surface, which Qt 5 cannot provide. Until then, `mpv --vo=gpu-next --target-colorspace-hint=yes` with Plasma HDR on is the way to watch HDR files.
  - Plan: the third step after the Qt 6 port and native Wayland (see below). Decided: HDR output switches on automatically when the display is in HDR mode (KWin's preferred image description has a peak above its reference luminance) and the file is PQ or HLG, with an Auto/Off preference; it follows Plasma's HDR toggle and moving between monitors live.
  - First step: prototype the UI-in-PQ problem in the test window (the UI root item as a layer with an sRGB → PQ shader at 203-nit SDR white); fall back to putting the video in a Wayland subsurface below an SDR surface if blending or quality is poor.
  - Proven with a Qt 6 test window (`hdr-test/qt6-window/`, 2026-11-01, Plasma 6.8, NVIDIA): real HDR works with the `vo_gpu` renderer libmpv already offers, and looks the same as `mpv --vo=gpu-next --target-colorspace-hint=yes` with `--hdr-compute-peak=no` or `--tone-mapping=bt.2390`. `gpu-next` is not needed.
  - Requires the Qt 6 / native Wayland item below; an XWayland window cannot use Wayland colour management.
  - What works: mpv outputs PQ untouched (`target-trc=pq`, `target-prim=bt.2020`, `target-peak` = the source peak), and the surface is tagged PQ/BT.2020 with mastering luminance and max CLL set to the source peak, so KWin does the tone mapping to the display. KWin's curve keeps everything up to 4000 nits distinguishable on a 580-nit display.
  - Qt's own tag is not enough: `QSurfaceFormat::setColorSpace(QColorSpace(Bt2020, St2084))` reaches KWin, but `QColorSpace` carries no luminances, and without them KWin clips everything above about 1000 nits to white. bomi needs its own small `wp_color_management_v1` client (about 150 lines in the test, using Qt's private `QNativeInterface::Private::QWaylandWindow::surface()`), and must not also set the Qt colour space: a `wl_surface` takes only one colour-management object.
  - Letting `vo_gpu` tone-map instead does not work well: KWin maps PQ's 203-nit SDR white to its own reference luminance (the SDR brightness setting), so the target peak would be display peak × 203 / reference, and `vo_gpu`'s curves then squeeze 1000–4000 nits into one or two 8-bit steps.
  - 8-bit window only: NVIDIA's Wayland EGL offers 8-bit and 16-bit float window configs, no 10-bit, and Qt 6.12's EGL config chooser never asks for float. Have mpv dither (`dither-depth=8`); gradients may still band.
  - The whole window becomes PQ, not just the video: the OSD, playlist and other QML overlays must be drawn at SDR white (203 nits) in PQ, and SDR files need mapping up too, or a per-file switch of the window colour space.
  - The colour shader hooks `OUTPUT`, so it needs revisiting, because `OUTPUT` would then be PQ.
  - The libmpv render API only offers the `vo_gpu` renderer, not `gpu-next` (libplacebo) (mpv issue #10810, PR #16818). It would add mpv's own compositor negotiation and per-frame tone mapping, but is no longer a prerequisite.
- [ ] **Qt 6 and native Wayland.** Prerequisite for HDR output. Qt 5 support is dropped; the port becomes the next major version.
  - [x] Qt 6 port, on branch `qt6` (pushed 2026-11-02, not merged). Still runs on XWayland. Qt Quick is forced onto OpenGL for mpv's render API; bomi's scene-graph shaders are GLSL 440 compiled to `.qsb` by qmake; bomi has its own `Slider`/`SliderStyle` in place of Controls 1; skins wrap relative icon paths in `Qt.resolvedUrl`. Checked against the Qt 5 build: all 15 skins, subtitles, seek preview, snapshots, playlist, preferences, settings from a Qt 5 configuration.
  - Installed next to the Qt 5 package while it is being tried: `configure --suffix=-qt6` names everything `bomi-qt6` (executable, launcher, data and settings folders, single-instance socket, MPRIS name), and the settings folder `~/.config/xylosper/bomi-qt6` starts as a copy of bomi's. The branch's PKGBUILD builds `bomi-qt6-git`. **When merging into master:** drop `_suffix` and restore the `bomi-git` package name, provides and conflicts in `arch/PKGBUILD`.
  - [ ] Native Wayland: drop the forced `QT_QPA_PLATFORM=xcb` (`player/main.cpp`); drag-move through `QWindow::startSystemMove()`; `OS::usingMemory()` returns 0 without X11 although it only reads `/proc`; remove the `<GL/glx.h>` include; check fullscreen, screensaver inhibition, refresh rate and fractional scaling.
  - [ ] Qt 6 follow-ups: about 100 compiler warnings left; the Windows and macOS code paths are untested; the UI now follows the display scale (1.6× here, unlike the Qt 5 build), keep or make it a setting; once Qt 6 has rewritten the fonts in `pref.json` (19-field format), a Qt 5 build cannot read them; one unreproduced segfault inside SQLite in `HistoryModel::update()` (2026-11-02, seven reruns clean).
- [ ] **Expose mpv's HDR tone-mapping options** (`tone-mapping`, `hdr-compute-peak`,
  `target-peak`) in preferences, since mpv now does the work.
  - These only apply to SDR output. With HDR output, KWin tone-maps (see "HDR output") and mpv passes PQ through, so they do nothing. Offering "mpv tone-maps" as an HDR alternative is not worth it with `vo_gpu`: even with `hdr-compute-peak`, its curves squeeze 1000–4000 nits into one or two 8-bit steps. gpu-next's per-scene spline (darker mid-tones on mostly dark frames, highlight detail kept) would make it a real choice, if libmpv ever offers gpu-next.
  - Gamma only partly overlaps a curve choice: it reshapes mid-tones for a static look but cannot adapt per scene.

## Done since the port started

- [x] Merged into master (PR #1, 2026-10-04) and released as v0.10.0; `legacy` preserves the pre-port `master`.

- [x] Playback through system libmpv, correct aspect, subtitles on letterbox
- [x] Hardware decoding, configurable, codec list read from libmpv at runtime
- [x] Display sync toggle (Ctrl+Y)
- [x] Video colour adjustment and Invert/Grayscale/Remap as an `OUTPUT` user shader,
  correct on HDR sources (`8486a288`)
- [x] Software decoding CPU back to plain-mpv levels: yadif no longer runs on every
  frame, and deinterlacing is mpv's auto mode (`ae8fb42f`)
- [x] New frames rendered from the scene-graph render thread, so GUI stalls no
  longer drop video frames (`547add1f`)
- [x] `arch/PKGBUILD` builds the libmpv port against system mpv/ffmpeg, with
  soname-versioned dependencies and a tag-independent `pkgver()`
- [x] Cache settings applied again (`cache`, `demuxer-max-bytes`, `cache-secs`,
  `cache-on-disk`; of the old KiB-based options only `cache-secs` was still
  accepted), and the cache readout reads `demuxer-cache-state`
- [x] Snapshot without subtitles: two captures through bomi's render path,
  the second with `sub-visibility` briefly off. (`screenshot-raw` fails under
  hardware decoding without advanced control.)
- [x] `display-fps-override` follows the window's screen and that screen's
  refresh-rate changes (only the startup path verified; single monitor here)
- [x] MPRIS album art captured through the render path (`PlayEngine::grabFrame()`),
  so it works under hardware decoding; it includes subtitles if visible
- [x] Merge prep: vendored mpv, FFmpeg build scripts, the unbuilt filter layer and
  bomi's VA-API/VDPAU code removed; README build sections rewritten; PKGBUILD
  fetches `master`; pre-port `master` kept as the `legacy` branch
- [x] Find Subtitle runs on subliminal (optional dependency), replacing the
  opensubtitles.org XML-RPC client that only got adverts
- [x] Audio chain as one lavfi graph in `af`: `dynaudnorm` (normalizer), `volume`
  (volume × amp, so the soft clip sees the full gain as in bomi's mixer),
  10 × `equalizer`, `asoftclip=type=sin`; tempo scaler via `audio-pitch-correction`
- [x] Letterbox bars follow fullscreen switches instantly: the item-sized framebuffer was resized behind a 300 ms debounce, so the old bars lingered stretched (`6e41ec8b`)
- [x] Gamma in the colour dialog and the Adjust Color step actions, applied in the colour shader after tone mapping with mpv's curve (same -100..100 scale as mpv and SMPlayer). mpv's own `gamma` property was tried first and failed on HDR: on two Spider-Man frames, -50 took bright blinds from 251 to 41 (to 11 with HQ upscaling) where the shader keeps them at 244, and +50 blew out highlights instead of lifting midtones
