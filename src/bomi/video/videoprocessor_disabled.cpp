// Inert VideoProcessor for the libmpv port.
//
// The real implementation (videoprocessor.cpp, still on disk but out of the
// build) ran bomi's motion interpolator, software deinterlacers and colour
// conversion *inside* mpv's video filter chain, by overriding mpv's
// vf_info_noformat symbol at link time. Modern mpv replaced the vf chain with
// libavfilter, so there is nothing to inject into.
//
// The class keeps its interface so PlayEngine and the preferences UI are
// untouched. Deinterlacing has already moved to mpv's own --vf=yadif (see
// PlayEngine::Data::vf); motion interpolation is deferred, with mpv's native
// GPU `interpolation` the intended replacement.

#include "videoprocessor.hpp"
#include "enum/colorspace.hpp"
#include "enum/colorrange.hpp"

struct VideoProcessor::Data {
    ColorSpace spaceOut = ColorSpace::Auto;
    ColorRange rangeOut = ColorRange::Auto;
};

VideoProcessor::VideoProcessor()
    : d(new Data)
{
}

VideoProcessor::~VideoProcessor()
{
    delete d;
}

auto VideoProcessor::isInputInterlaced() const -> bool { return false; }
auto VideoProcessor::isOutputInterlaced() const -> bool { return false; }
auto VideoProcessor::skipToNextBlackFrame() -> void { }
auto VideoProcessor::stopSkipping() -> void { }
auto VideoProcessor::isSkipping() const -> bool { return false; }
auto VideoProcessor::hwdec() const -> QString { return QString(); }
auto VideoProcessor::setMotionIntrplOption(const MotionIntrplOption &) -> void { }
auto VideoProcessor::inputColorSpace() const -> ColorSpace { return ColorSpace::Auto; }
auto VideoProcessor::inputColorRange() const -> ColorRange { return ColorRange::Auto; }
auto VideoProcessor::outputColorSpace() const -> ColorSpace { return d->spaceOut; }
auto VideoProcessor::outputColorRange() const -> ColorRange { return d->rangeOut; }

auto VideoProcessor::setOutputColorSpace(ColorSpace space) -> void
{
    if (_Change(d->spaceOut, space))
        emit outputColorSpaceChanged(space);
}

auto VideoProcessor::setOutputColorRange(ColorRange range) -> void
{
    if (_Change(d->rangeOut, range))
        emit outputColorRangeChanged(range);
}
