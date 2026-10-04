# TODO: libmpv port

What remains before the `libmpv-port` branch matches the old vendored-mpv bomi, and
what could take it further. README.md, "Known issues after the libmpv port", has the
background and measurements behind each item.

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
- [ ] **bomi's own motion interpolator and GL scaler kernels** are inert, replaced by
  mpv's equivalents. Remove the dead code and any preferences that only drove it.

- [ ] **Channel manipulation → `pan`** (very low priority: not needed for the
  maintainer's own use, where mpv's default downmix is fine, but kept as a feature
  rather than removed). Translate `ChannelLayoutMap` (source → destination speaker
  mixing per layout pair) into a `pan` filter in `af()`. Until then the setting in
  Preferences > Audio is ignored.

## Smaller regressions

- [ ] **MPRIS album art** (`Mpris` → `PlayEngine::snapshot(bool)`) uses
  `screenshot-raw`, which fails under hardware decoding without advanced control
  ("Input image format cuda not supported by libswscale" in the log), so album art
  is empty with hwdec on. Reuse the render-path snapshot capture instead.
- [ ] **DVD menu hit-testing** went with `disc-mouse-on-button`. Check whether modern
  mpv's dvdnav exposes anything equivalent, or drop the feature.
- [ ] **`vsync-ratio` in the play info panel** reads 1.4–3.0 where about 6 is expected
  for 24fps on 143.84Hz. Find out what it counts under `vo=libmpv` before trusting
  the display-sync figures.
- [ ] **Show that display sync improves playback.** It engages, but nothing shows it
  helps yet. Test with high-bitrate 23.976/24fps content, use counter deltas over a
  steady window without touching the UI, and look at a panning shot.

## Beyond the old bomi

- [ ] **HDR output.** HDR sources are already tone-mapped to SDR correctly, which the
  old bomi could not do. Real HDR output to an HDR display needs an HDR surface, which
  Qt 5 cannot provide. That means Qt 6 plus Wayland colour management (KDE supports
  it). The colour shader hooks `OUTPUT`, so it would need revisiting, because OUTPUT
  would then be PQ.
- [ ] **Native Wayland / Qt 6** in general (currently out of scope).
- [ ] **Expose mpv's HDR tone-mapping options** (`tone-mapping`, `hdr-compute-peak`,
  `target-peak`) in preferences, since mpv now does the work.

## Before merging into master

- [ ] Delete `src/mpv` and `src/ffmpeg` (174M). They are no longer built and are kept
  only for bisecting.
- [ ] Remove the vestigial `OS::HwAcc` stub and its references in the preferences code.
- [ ] Rewrite README's Requirements and Compilation sections: the port needs system
  `libmpv >= 2.0` and no longer builds ffmpeg/mpv in-tree.
- [ ] Switch `_branch` in `arch/PKGBUILD` back to `master`.
- [ ] Remove the disabled filter-layer sources once their replacements are in.

## Done since the port started

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
- [x] Audio chain as one lavfi graph in `af`: `dynaudnorm` (normalizer), `volume`
  (volume × amp, so the soft clip sees the full gain as in bomi's mixer),
  10 × `equalizer`, `asoftclip=type=sin`; tempo scaler via `audio-pitch-correction`
