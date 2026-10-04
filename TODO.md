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

- [ ] **`debian/` and `rpm/` packaging** (inherited from upstream and bm16ton,
  untested here) still build the vendored mpv (`debian/build.sh` runs mpv's waf,
  build-depends name `ffmpeg-bomi` and codec libraries). Update them for the
  system libmpv or drop them; `legacy` keeps the working versions.

- [ ] **Find Subtitle: opensubtitles.com and provider settings.** The dialog now
  runs on subliminal (`subtitle/subliminalfinder.cpp` plus the embedded
  `subtitle/subliminal_helper.py`), which fixed the opensubtitles.org advert
  problem: subliminal's own client still gets real subtitles. Still to do:
  - Add opensubtitles.com username/password in Preferences and pass them to
    subliminal: its search works without them, but downloads need an account,
    so it is left out of the provider list for now.
  - Let the user choose providers and search languages. Searches currently use
    the languages ticked in the dialog, plus bomi's language and English.
  - Show progress or allow cancelling: a search takes about 10-15 s, because
    subliminal queries every provider.
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

- [ ] Merge PR #1 (maintainer). `legacy` already preserves the pre-port `master`.

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
