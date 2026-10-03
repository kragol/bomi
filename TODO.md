# TODO: libmpv port

What remains before the `libmpv-port` branch matches the old vendored-mpv bomi, and
what could take it further. README.md, "Known issues after the libmpv port", has the
background and measurements behind each item.

## Performance

- [ ] **Enable `MPV_RENDER_PARAM_ADVANCED_CONTROL`.** Brings back direct rendering
  (logged now as `DR failed - disabling`), so decoded frames are no longer copied.
  It deadlocked on the old GUI-thread render path. The render-thread path it needs
  is now in place (`547add1f`), but it has not been retried. Watch two things: a
  hidden or minimised window (Qt stops rendering, so `mpv_render_context_update()`
  stops being called), and `update()` needing a current GL context in this mode.

## Inert controls

- [ ] **Audio filters** via mpv's `af` property with lavfi filters. `AudioController`
  is an inert stub.
  - [ ] Volume normalizer → `dynaudnorm`
  - [ ] Equalizer → `anequalizer` (or `superequalizer`)
  - [ ] Channel manipulation → `pan`
  - [ ] Tempo scaler → `scaletempo2` / `atempo`
  - [ ] Soft clip → `asoftclip`
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

## Smaller regressions

- [ ] **Cache readout** always says Unavailable. Rewire it from the removed
  `cache-used`/`cache-size` to the `demuxer-cache-state` map.
- [ ] **Snapshot without subtitles** captures them anyway. Try `screenshot video`, or
  toggle `sub-visibility` around the capture.
- [ ] **DVD menu hit-testing** went with `disc-mouse-on-button`. Check whether modern
  mpv's dvdnav exposes anything equivalent, or drop the feature.
- [ ] **`display-fps-override` is set once at startup.** Update it when the window
  moves to a monitor with a different refresh rate.
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
- [ ] Update `arch/PKGBUILD` for the libmpv build (`/usr/bin/bomi` is still the old
  vendored build).
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
