// Inert AudioController for the libmpv port.
//
// The real implementation (audiocontroller.cpp and the audio filter classes
// under audio/, kept on the legacy branch) ran bomi's own DSP *inside* mpv's
// audio filter chain by overriding mpv's af_info_dummy symbol at link time.
// Modern mpv removed the af chain entirely in 0.30, so there is no chain left
// to inject into and no way to see decoded PCM through libmpv.
//
// The normalizer, gain, equalizer and soft clip are now a lavfi graph in mpv's
// --af (PlayEngine::Data::af()). This class keeps its interface so PlayEngine
// and the preferences UI are untouched, and discards what is left: channel
// manipulation and the spectrum analyzer.

#include "audiocontroller.hpp"
#include "audioformat.hpp"
#include "visualizer.hpp"
#include "enum/channellayout.hpp"

struct AudioController::Data {
    AudioVisualizer visualizer;
};

AudioController::AudioController(QObject *parent)
    : QObject(parent), d(new Data)
{
}

AudioController::~AudioController()
{
    delete d;
}

auto AudioController::gain() const -> double { return -1.0; }
auto AudioController::isTempoScalerActivated() const -> bool { return false; }
auto AudioController::isNormalizerActivated() const -> bool { return false; }
auto AudioController::setNormalizerOption(const AudioNormalizerOption &) -> void { }
auto AudioController::setSoftClip(bool) -> void { }
auto AudioController::setChannelLayoutMap(const ChannelLayoutMap &) -> void { }
auto AudioController::setOutputChannelLayout(ChannelLayout) -> void { }
auto AudioController::setEqualizer(const AudioEqualizer &) -> void { }
auto AudioController::chmap() const -> mp_chmap* { return nullptr; }
auto AudioController::inputFormat() const -> AudioFormat { return AudioFormat(); }
auto AudioController::outputFormat() const -> AudioFormat { return AudioFormat(); }
auto AudioController::samplerate() const -> int { return 0; }
auto AudioController::setAnalyzeSpectrum(bool) -> void { }

// Kept non-null: the QML side binds to this object's properties.
auto AudioController::visualizer() const -> AudioVisualizer*
{
    return &d->visualizer;
}
