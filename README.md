# bomi


## Introduction

bomi is a multimedia player formerly known as CMPlayer,
which is aimed for easy usage but also provides various powerful features and convenience functions.
Just install and enjoy it! There will be already what you expect.
If you don't like, you can configure almost everything.

The original [bomi project page](http://bomi-player.github.io) and
[xylosper's repository](https://github.com/xylosper/bomi) describe bomi as it was in
2016. They are kept here for reference but are no longer maintained, and they do not
cover this fork; see "Legacy links" at the end.

## About this fork

This is a fork of [bm16ton/bomi](https://github.com/bm16ton/bomi), itself a fork of
the original [xylosper/bomi](https://github.com/xylosper/bomi), which stopped in 2016.
It carries the changes needed to build and run on a current Linux distribution
(tested on Arch with gcc 16, Qt 5.15 and Python 3.14) and to work under a
Wayland desktop session, and it ports bomi from its vendored 2016 mpv to the
system libmpv.

### Branches

* **`master`** (0.10.x) builds against the system **libmpv** (>= 2.0, i.e. mpv 0.30 or
  later) through mpv's render API, and against whatever FFmpeg that libmpv uses.
  Nothing is vendored. This is where development happens.
* **`legacy`** (0.9.12, tag `v0.9.12`) is bomi as it was before the libmpv port: the vendored, patched
  mpv (client API 1.18, early 2016) in `src/mpv`, linked statically against a
  pinned in-tree FFmpeg 4.0.1, with bomi's own audio and video filters running
  inside mpv's filter chains. It is kept on purpose, for two reasons:
  * **Regression testing.** It is the reference for how bomi behaved before the
    port: when something sounds, looks or performs differently on `master`,
    build `legacy` and compare.
  * **A base for contributors** who want to work on the vendored version, or
    need code the port removed (the audio mixer, normalizer and channel
    mapping, the motion interpolator, the software deinterlacers, bomi's own
    VA-API/VDPAU support).

  It receives no new features. Its README describes how to build it (it needs a
  Python older than 3.12 and `nasm`); its `arch/PKGBUILD` packages it as
  `bomi-git` too, so the two cannot be installed side by side.

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

* bomi used to be no ordinary libmpv client: it vendored a patched 2016 mpv,
  linked it statically and ran its own audio and video filters inside mpv's
  filter chains, an architecture mpv deleted in 0.30. On `master` it is an
  ordinary render-API client of the system libmpv, and what those filters did
  is now done by mpv (a lavfi audio graph, `--deinterlace`, GPU interpolation,
  user shaders for colour adjustment). See "Known issues after the libmpv
  port" below for what that changed, and the `legacy` branch for the old code.
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
* Qt 5 `qmake` and `lrelease`
* `git` if you try to build from git repository

You have to prepare next libraries, too:

* Qt5 >= 5.2, with QtQuick and QtQuick.Controls 1.x
* OpenGL >= 2.1 with framebuffer object support
* libmpv (`mpv` >= 2.0, i.e. mpv 0.30 or later)
* FFmpeg (libav is not supported): libavformat, libavcodec, libavutil,
  libavfilter, libswresample, libswscale -- the ones your libmpv uses
* libass >= 0.12.1
* dvdread, dvdnav
* libbluray
* chardet (*)
* glib-2.0, gobject-2.0
* xcb, xcb-icccm, xcb-screensaver, xcb-randr, xcb-xtest, x11
* alsa

Each item corresponds to its package name for `pkg-config` command except Qt and OpenGL.
chardet, marked with (*), can be built in-tree.

Optional at runtime, run as external tools:

* `yt-dlp` -- playing streaming sites such as YouTube
* [`subliminal`](https://github.com/Diaoul/subliminal) with `python3` -- Tools >
  Find Subtitle. bomi runs a small helper script against subliminal's Python API;
  without subliminal the dialog reports itself unavailable.

## Compilation

In the below description, `$` means that you have to input the command in termnal/console
where source code exists.

### Get source code

At first, prepare the source code.

* Download the latest source code tarball and unpack
* Or, clone the git repository if you want

### In-tree build packages

chardet can be built in-tree if your distribution lacks it; skip it if
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

Hardware decoding is mpv's: enable it in Preferences > Video > Hardware
acceleration, and mpv picks the backend (nvdec, VA-API, Vulkan...). There is no
build option for it.

Build with `make` from the top level, not `make release` inside `src/bomi` -- the
latter produces a binary with no skins or imports, which loads no QML at all and
looks like a renderer bug rather than a build mistake.

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

#### Arch Linux

`arch/PKGBUILD` builds this fork as a `bomi-git` package. It fetches from this
repository; point `_repo` at a local clone (`file:///path/to/bomi`) if you want to
package work in progress, committing it there first. Then:

```
$ cd arch && makepkg -si
```

It carries an `epoch`, because the AUR `bomi-git` reports a higher commit count than
this branch does and pacman would otherwise read the fork as a downgrade.

It fetches `master` and builds against the system `mpv` and `ffmpeg`; nothing is
vendored. Their libraries are declared by soname, so pacman holds back an mpv or
ffmpeg update that bumps a soname until you rebuild `bomi-git`. The `legacy`
branch has its own `arch/PKGBUILD`, which fetches `legacy`.

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

Rebuilding after new commits bumps `pkgver` (commits since the 0.10.0 base, counted
from a fixed commit hash so clones without tags work too), so the new build sorts
above the installed one and shows up as a normal update.

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

`master` links the system libmpv instead of the vendored mpv (the `legacy` branch).
Hardware decoding and display sync work as a result, but bomi ran its own audio and
video filters *inside* mpv's filter chains, and modern mpv has no such chains. Most
of what they did is now done by mpv; the items below are what is still missing or
different. `TODO.md` tracks them.

### Direct rendering is still off

Software decoding now costs what plain `mpv` costs (4K HEVC 10-bit: about 185% CPU,
a new frame every vsync). The port's earlier 850–1000% came from running yadif on
every frame, not from the render path. But mpv still logs `DR failed - disabling`,
so each decoded frame is copied once more than necessary. Direct rendering needs
`MPV_RENDER_PARAM_ADVANCED_CONTROL`. New frames are now rendered from the scene-graph
render thread, with `mpv_render_context_update()` called there, which that mode
requires. It deadlocked when tried on the old GUI-thread path and has not been
retried since.

### Controls that are still shown but do nothing

* **Channel manipulation** (Preferences > Audio). Not ported yet, and low priority:
  mpv's own downmix is used, and the custom speaker mapping is ignored. The equalizer,
  normalizer, soft clip, amplifier and tempo scaler are back as a lavfi graph in
  mpv's `--af`. `AudioController` remains an inert stub.
* **The spectrum visualizer.** The hardest to bring back: libmpv exposes no way to tap
  decoded PCM, so it would need an out-of-band route.
* **Motion smoothing** is now mpv's GPU frame interpolation, not bomi's own CPU
  interpolator, which is inert.
* **Deinterlacing** is mpv's `--deinterlace=auto`: interlaced frames only, with
  `bwdif` (`bwdif_cuda` under nvdec). The method choices in Preferences (Bob,
  LinearBob, CubicBob, Yadif, field doubling) are ignored; bomi's own
  implementations are inert.
* **Video scaling** is mpv's, not bomi's custom GL kernels — generally better, but a
  behaviour change.

### Smaller regressions

* **The normalizer gain readout in the play info panel is empty.** `dynaudnorm` does
  the normalizing now and does not report its current gain.
* **DVD menu hit-testing is gone** with the `disc-mouse-on-button` property.
* **`vsync-ratio` in the play info panel looks wrong** — it reads 1.4–3.0 where ~6
  would be expected for 24fps content on a 143.84Hz output. It may be counting render
  callbacks rather than vsyncs under `vo=libmpv`. Worth understanding before trusting
  the display-sync figures.

## Issues

Issues for this fork go to its own tracker:
**[github.com/kragol/bomi/issues](https://github.com/kragol/bomi/issues)**.

Please read this before posting. This is a personal fork, maintained for my own use.
There is no support commitment: I do not plan to work through user reports, and I
do not commit to reading them either, so an issue may sit unanswered. I may look
through them once in a while and fix some, most likely when they affect me too. Issues that come with clear reproduction steps,
the bomi version (Help > About bomi, or `pacman -Q bomi-git`) and the relevant log
output stand the best chance. If something worked in 0.9.x and broke in 0.10, say
whether the `legacy` branch still behaves the old way.

Please do not report problems with this fork to xylosper or on the original
tracker; he has not worked on bomi since 2016 and this code is not his.

## Legacy links

These belong to the original bomi by xylosper and are inactive since 2016. They are
useful for history (old issues and discussions, the 0.9.x documentation), not for
this fork:

* Project page: [bomi-player.github.io](http://bomi-player.github.io)
* Repository: [github.com/xylosper/bomi](https://github.com/xylosper/bomi)
* Issue tracker: [github.com/xylosper/bomi/issues](https://github.com/xylosper/bomi/issues)
* The original README listed the author's e-mail address for private contact; it
  is not repeated here, since he no longer maintains bomi.

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
