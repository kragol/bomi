#pragma once

class QWindow;

struct HdrTarget {
    double peak = 0;      // nits, in PQ's scale (203 = SDR white), for mpv's target-peak
    double displayPeak = 0, reference = 0;
};

// Does what mpv's gpu-next does with --target-colorspace-hint: reads KWin's
// preferred image description for the window's surface, then tags the
// surface PQ/BT.2020 with mastering luminance and max CLL, so the compositor
// knows the content's range. Qt's QColorSpace tag cannot carry luminances.
// Returns peak 0 on failure. Must not be combined with QSurfaceFormat's
// colour space: a wl_surface can have only one colour-management object.
HdrTarget describeSurface(QWindow *window, double peakOverride);
