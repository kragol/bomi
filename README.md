# bomi


## Introduction

bomi is a multimedia player formerly known as CMPlayer,
which is aimed for easy usage but also provides various powerful features and convenience functions.
Just install and enjoy it! There will be already what you expect.
If you don't like, you can configure almost everything.

For more details, please visit [bomi Project Page](http://bomi-player.github.io).

## About this fork

This is a fork of [bm16ton/bomi](https://github.com/bm16ton/bomi), itself a fork of
the original [xylosper/bomi](https://github.com/xylosper/bomi), which stopped in 2016.
It carries the changes needed to build and run on a current Linux distribution
(tested on Arch with gcc 16, Qt 5.15 and Python 3.14) and to work under a
Wayland desktop session.

The history of this fork was rewritten once, on 2026-09-07. bm16ton's
[`e28c64a2`](../../commit/e28c64a235eafba093d2bf5f5459cffe75af6427)
had also committed the output of a Debian package build — `debian/bomi/`, a
489-file copy of the built tree including a 25 MB compiled binary, plus five
debhelper artifacts — and those 494 generated files were stripped from history.
bm16ton remains the author of that commit and his remaining changes are
untouched; the committer field records the rewrite. Commits inherited from
`xylosper/bomi` are unaffected and keep their original hashes.

### A note on authorship

**This port was written by AI.** Every change on top of `bm16ton/bomi` — the
toolchain and build fixes, the Wayland handling, the font drop-down fixes, the Arch
packaging, the commit messages and this README — was produced by Claude (Anthropic)
working from my prompts. I did not write a single line of it myself. It is built and
used on my own machine, which is the entire extent of the testing, and no one has
reviewed the code. Weigh that as you see fit before running it or merging from it.

Two things are worth knowing before you dig in:

* bomi is not an ordinary libmpv client. It vendors a patched mpv (client API
  1.18, early 2016) in `src/mpv`, links it **statically**, includes mpv's
  internal headers, and overrides mpv's own `af_info_dummy` and
  `vf_info_noformat` symbols at link time so that bomi's audio and video
  filters run inside mpv's filter chain. mpv deleted that architecture in
  0.30, so building against a modern system libmpv is a rewrite, not a version
  bump. The vendored mpv and the pinned in-tree FFmpeg are not optional.
* bomi is an X11 client. It uses xcb directly for fullscreen, always-on-top,
  drag-to-move and screensaver inhibition. Under a Wayland session it runs on
  XWayland: it defaults `QT_QPA_PLATFORM` to `xcb` when an X display is
  available. Setting `QT_QPA_PLATFORM` yourself overrides that, and a native
  Wayland run works with reduced functionality (no window-manager
  integration, no screensaver inhibition).

## Requirements

In order to build bomi, you need next tools:

* `g++` or `clang` which supports C++14
* `pkg-config`
* `python` — plus an interpreter older than 3.12 for mpv's bundled waf, which
  predates the removal of `imp` and `distutils`. `./configure` looks for
  `python3.9`/`3.10`/`3.11`, including pyenv installs; override with
  `--python=/path/to/python`.
* `nasm` for FFmpeg's x86 assembly. Without it the in-tree FFmpeg still
  builds, just without hand-written assembly.
* `git` if you try to build from git repository

You have to prepare next libraries, too:

* Qt5 >= 5.2
* OpenGL >= 2.1 with framebuffer object support
* FFmpeg (libav is not supported) (*)
  * libavformat >= 55.12.0 (*)
  * libavcodec >= 55.34.1 (*)
  * libavutil >= 52.48.101 (*)
  * libavfilter (*)
  * libswresample (*)
  * libswscale (*)
* chardet (*)
* libmpg123
* libass
* dvdread dvdnav
* libbluray
* icu-uc
* xcb xcb-icccm x11
* libva libva-glx libva-x11
* vdpau
* alsa

Each item corresponds to its package name for `pkg-config` command except Qt and OpenGL.
Some packages marked with (*) can be in-tree-built.

## Compilation

In the below description, `$` means that you have to input the command in termnal/console
where source code exists.

### Get source code

At first, prepare the source code.

* Download the latest source code tarball and unpack
* Or, clone the git repository if you want

### In-tree build packages

**The in-tree FFmpeg is required, not optional.** It is pinned to 4.0.1, because
the vendored mpv in `src/mpv` carries API fixes written against ffmpeg 4.0 and will
not build against a current system FFmpeg. `./configure` prepends `build/lib/pkgconfig`
to `PKG_CONFIG_PATH`, so build FFmpeg first and **re-run `./configure` afterwards** —
otherwise bomi is configured against your system FFmpeg and nothing will link.

* To build FFmpeg in-tree, run next:
```
$ ./download-ffmpeg
$ ./build-ffmpeg
```

`build-ffmpeg` applies everything in `patches/ffmpeg-*.patch` first, idempotently.
Currently that is a backport of upstream `effadce6`, without which modern binutils
rejects the inline assembly in `libavcodec/x86/mathops.h`.

chardet can also be built in-tree if your distribution lacks it; skip it if
`pkg-config chardet` already works.

* To build chardet in-tree, run next:
```
$ ./download-libchardet
$ ./build-libchardet
```

### Build bomi

If you have any problem when building, please check Troubleshooting section.
It may be helpful to check what you can configure using next command:
```
$ ./configure --help
$ make
```

For a build you can run straight from the source tree, pass `--developer`; it
points the skin, import and translation paths at `src/bomi` instead of the
install prefix. Do not use it when building a package.

Hardware decoding (VA-API and VDPAU) is off in the tested configuration:
```
$ ./configure --disable-vaapi --disable-vdpau
```
Both paths are GLX-based and predate the current drivers. They still compile,
but have not been verified on this fork.

Earlier revisions of this README told you to comment out `extern int pause (void);`
in `/usr/include/unistd.h` before building and put it back afterwards. That is no
longer necessary: the collision was a `static void pause()` in mpv's
`audio/out/ao_pulse.c`, which is now named `audio_pause()`.

#### Test purpose
If you want to try bomi without install, run next commands in order to build bomi:
```
$ ./configure
$ make
```
The executable will be located at `./build/bomi` in source code directory.

#### Install into system

To install bomi, you have to decide the path to install.
It can be spcified by `--prefix` option.
By default, `--prefix=/usr/local` will be applied if you don't specify it which results to locate the executable at `/usr/local/bin/bomi` and other files (skins, translations, etc.) under `/usr/local/share`
For instance, if you want to install into a directory named `bomi` in your home directory, run next:
```
$ ./configure --prefix=${HOME}/bomi
$ make
$ make install
```
You will find the executable at `bomi/bin/bomi` in your home directory.

#### For package builders

Usually, when build a package, you need to specify the fake root system when run `make install`.
This can be accomplished by giving `DEST_DIR` option to `make install`.
Here's a snippet from `PKGBUILD` for Arch Linux as an example:
```bash
build() {
  cd "$srcdir/$pkgname-$pkgver"
  ./configure --prefix=/usr --enable-jack --enable-cdda
  make
}

package() {
  cd "$srcdir/$pkgname-$pkgver"
  make DEST_DIR=$pkgdir install
}
```
where `$pkgdir` is the fake root system. `jack` and `cdda` support is also enabled in this example.

That snippet is upstream's and is incomplete for this fork: it never builds the in-tree
FFmpeg, so `./configure` picks up the system one and nothing links.

#### Arch Linux

`arch/PKGBUILD` builds this fork as a `bomi-git` package. It fetches from this
repository; point `_repo` at a local clone (`file:///path/to/bomi`) if you want to
package work in progress, committing it there first. Then:

```
$ cd arch && makepkg -si
```

It carries an `epoch`, because the AUR `bomi-git` reports a higher commit count than
this branch does and pacman would otherwise read the fork as a downgrade.

Two things it works around, both explained in comments there: the in-tree FFmpeg
tarball is a `source=` entry rather than a `./download-ffmpeg` call, since `build()`
is supposed to run offline; and `./build-ffmpeg` has to run before `./configure`,
not after.

To have `pacman -Syu` offer rebuilds the way it does for repository packages, put the
built package in a [local repository](https://wiki.archlinux.org/title/Pacman/Tips_and_tricks#Custom_local_repository):

```
$ repo-add ~/pkgrepo/local.db.tar.gz ~/pkgrepo/bomi-git-*.pkg.tar.zst
```

and point `/etc/pacman.conf` at it, above the official repositories (pacman does not
expand `~`, so spell the path out):

```ini
[local]
SigLevel = Optional TrustAll
Server = file:///home/YOUR_USER/pkgrepo
```

Rebuilding after a `git pull` bumps `pkgver` from
`git describe`, so the new build sorts above the installed one and shows up as a
normal update.

## Known issues in this fork

* **Scrolling the font drop-down is sluggish.** Every entry previews its own family, so
  Qt loads that family's font engine the first time the row is painted. It is noticeable
  with a few thousand families installed and settles once rows have been visited. This
  is inherent to previewing each family and cannot be moved off the GUI thread, because
  Qt 5's `QFontDatabase` engine loading is not thread-safe. See the comment on
  `FontFamilyModel::fontData()` in `src/bomi/widget/fontcombobox.cpp` for the options if
  it ever becomes worth trading the preview away.
* **Font drop-down rows all take the height of the first row.** No clipping has been
  observed, but a family with unusually tall metrics sorting first could cause it. The
  fix would be an item delegate returning a padded height.
## Known issues after the libmpv port

The `libmpv-port` branch links the system libmpv instead of the vendored mpv ~0.15.
Hardware decoding and display sync work as a result, but bomi ran its own audio and
video filters *inside* mpv's filter chains, and modern mpv has no such chains. Those
filters are disabled rather than removed: the sources are still in the tree, just out
of the build. The items below are what that costs, and what is still wrong.

### Software decoding is sluggish, and that is bomi's fault

With hardware decoding off, playback of demanding content collapses: a 4K HEVC 10-bit
file plays at 8–10fps with bomi burning **843–918% CPU**, where plain `mpv` — same
libmpv, same file, `hwdec=no` — uses **188%**, and a minimal render-API harness with
bomi's exact settings uses **220%** while rendering 535 times a second. So neither
libmpv nor ffmpeg is slow; the cost is in bomi.

Almost all of that CPU is *decoding*: mpv's video output is starved, logs
`mpv_render_context_render() not being called or stuck`, drifts 200–500ms out of A/V
sync, and races the decoder to catch up. The render path is too indirect — mpv's
update callback posts a Qt event to the **GUI thread**, which schedules a scene-graph
update, which eventually renders on the render thread — and cannot service a VO that
wants ~143 presentations a second.

Hardware decoding **masks** this (25–28% CPU, smooth) but does not fix it. Anything
falling back to software decoding hits the same wall.

The fix is to drive rendering from the scene-graph render thread, gated on
`mpv_render_context_update()`, instead of round-tripping the GUI thread. Once that
holds, `MPV_RENDER_PARAM_ADVANCED_CONTROL` becomes safe — it currently deadlocks bomi
— which would also restore direct rendering, logged today as `DR failed - disabling`,
so every decoded 4K frame is copied needlessly.

### Controls that are still shown but do nothing

* **All video colour adjustment.** Brightness, contrast, saturation, hue, the
  per-channel red/green/blue sliders, and the Invert / Grayscale / Remap effects. bomi
  applied these as a 4×4 colour matrix injected into `vo_opengl` as a custom shader
  via the `vo_cmdline` command, and neither survives in modern mpv. Brightness,
  contrast, saturation and hue map directly onto mpv properties; the rest needs a
  user shader (`--glsl-shaders`, `//!HOOK` format). Horizontal/vertical flip still
  works.
* **The audio filter chain.** Volume normalizer, soft clip, channel manipulation,
  equalizer and tempo scaler. `AudioController` is an inert stub. lavfi has
  equivalents for all of them (`dynaudnorm`, `pan`, `anequalizer`, `atempo`), to be
  driven through mpv's `--af`.
* **The spectrum visualizer.** The hardest to bring back: libmpv exposes no way to tap
  decoded PCM, so it would need an out-of-band route.
* **Motion smoothing** is now mpv's GPU frame interpolation, not bomi's own CPU
  interpolator, which is inert.
* **Deinterlacing** is mpv's `yadif` only. Bob, LinearBob and CubicBob all map onto
  it; bomi's own implementations are inert.
* **Video scaling** is mpv's, not bomi's custom GL kernels — generally better, but a
  behaviour change.

### Smaller regressions

* **Snapshots cannot separate video from subtitles.** mpv composites OSD and subtitles
  into the same framebuffer as the video, so "save without subtitles" captures them
  anyway.
* **The cache readout always says Unavailable.** `cache-used` and `cache-size` were
  removed from mpv; the equivalent lives in the `demuxer-cache-state` map and has not
  been rewired. Caching itself is unaffected.
* **DVD menu hit-testing is gone** with the `disc-mouse-on-button` property.
* **`display-fps-override` is set once at startup.** Correct for one display; moving
  the window to a second monitor at a different refresh rate will not re-sync it. Run
  one display at a time, or see the plan for the dynamic version.
* **`vsync-ratio` in the play info panel looks wrong** — it reads 1.4–3.0 where ~6
  would be expected for 24fps content on a 143.84Hz output. It may be counting render
  callbacks rather than vsyncs under `vo=libmpv`. Worth understanding before trusting
  the display-sync figures.

### Cleanup still owed

`src/mpv` and `src/ffmpeg` (174M) remain in the tree but are no longer built or
included; they are kept only so the port stays bisectable. `OS::HwAcc` is a vestigial
stub that no longer enumerates anything but is still referenced by the preferences
code.

The Requirements and Compilation sections above still describe the in-tree ffmpeg and
mpv build, which this branch no longer does: it needs `libmpv >= 2.0` from the system
and builds against whatever ffmpeg that libmpv uses. Those sections need rewriting
before the port is merged.

Build with `make` from the top level, not `make release` inside `src/bomi` — the
latter produces a binary with no skins or imports, which loads no QML at all and looks
like a renderer bug rather than a build mistake.

## Contacts

### [Issue Tracker](https://github.com/xylosper/bomi/issues)
If you have problems or want some features, please report them in English, Korean, or Japanese.

### [E-mail](mailto:darklin20@gmail.com)
If you want to contact me privately, please send me an e-mail.

## License

bomi is distributed under GPLv2.

Copyright (C) 2015 Lee, Byoung-young A.K.A. xylosper

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
