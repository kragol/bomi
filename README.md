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
* **Hardware decoding is off, deliberately.** Builds are configured with
  `--disable-vaapi --disable-vdpau`. bomi's VA-API path hardcodes the GLX interop
  (`vaGetDisplayGLX`), which the current NVIDIA VA driver — an EGL/NVDEC bridge — does not
  implement, so only VDPAU is reachable. VDPAU still works on the NVIDIA blob, but Mesa
  removed it in 25.3.0 and NVIDIA has deprecated it for NVDEC/NVENC, and the vendored mpv
  is far too old to offer nvdec, CUDA, Vulkan or EGL VA-API instead. Software decoding
  keeps up fine, so enabling it buys little.
* **No display sync, so frame pacing is approximate.** `--video-sync` and `interpolation`
  do not exist in the vendored mpv; upstream added them in 0.18 and this tree is
  0.14/0.15. Playback is audio-synced, which judders whenever the frame interval does not
  divide the refresh interval — choosing a display mode that divides evenly into your
  content's frame rate helps more than anything in this codebase. bomi's own motion
  interpolation (Preferences > Video Processing) is the built-in mitigation. A proper fix
  means porting to a modern libmpv.

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
