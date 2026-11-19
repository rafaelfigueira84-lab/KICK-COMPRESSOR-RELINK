#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout KickProcessor::layout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&](const char* id, const char* name, float a, float b, float d) {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name,
            juce::NormalisableRange<float>(a, b, 0.1f), d));
    };
    add("enabled", "Correction Applied", 0, 1, 0);
    add("mix", "Correction Amount", 0, 100, 100);
    add("sub", "Sub Cut Hz", 20, 55, 28);
    add("bodyHz", "Body Frequency", 35, 150, 65);
    add("bodyDb", "Body Gain", -6, 6, 0);
    add("mudHz", "Mud Frequency", 150, 900, 300);
    add("mudDb", "Mud Gain", -6, 3, 0);
    add("clickHz", "Attack Frequency", 1000, 9000, 3500);
    add("clickDb", "Attack Gain", -5, 5, 0);
    add("output", "Output Gain", -9, 3, 0);
    add("dynamics", "Smart Dynamics Enabled", 0, 1, 0);
    add("threshold", "Threshold dB", -36, 0, -12);
    add("ratio", "Compression Ratio", 1, 8, 1);
    add("attack", "Compressor Attack ms", 1, 60, 15);
    add("release", "Compressor Release ms", 20, 350, 90);
    add("punch", "Punch Amount", 0, 100, 0);
    add("tight", "Tight Amount", 0, 100, 0);
    return {p.begin(), p.end()};
}

KickProcessor::KickProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", layout()) {}

bool KickProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return (l.getMainInputChannelSet() == juce::AudioChannelSet::mono() ||
            l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()) &&
           l.getMainOutputChannelSet() == l.getMainInputChannelSet();
}

void KickProcessor::prepareToPlay(double sr, int maxBlock) {
    sampleRateHz = sr;
    juce::dsp::ProcessSpec spec {sr, static_cast<juce::uint32>(juce::jmax(1, maxBlock)), 1};
    for (auto& channel : filters)
        for (auto& f : channel) { f.prepare(spec); f.reset(); }
    dynamicStates = {};
    previousParameters.fill(-9999.f);
    updateFilters();
}

void KickProcessor::beginCapture() {
    if (!captureRequested.load() && !captureReady.load()) {
        captureRequested.store(true, std::memory_order_release);
    }
}

void KickProcessor::updateFilters() {
    const char* ids[] = {"sub", "bodyHz", "bodyDb", "mudHz", "mudDb", "clickHz", "clickDb"};
    float v[7]; bool changed = false;
    for (int i = 0; i < 7; ++i) {
        v[i] = parameters.getRawParameterValue(ids[i])->load();
        changed |= (v[i] != previousParameters[static_cast<size_t>(i)]);
        previousParameters[static_cast<size_t>(i)] = v[i];
    }
    if (!changed) return;
    const auto sr = juce::jmax(8000.0, sampleRateHz);
    auto cs = std::array<Coeff::Ptr, 4> {
        Coeff::makeHighPass(sr, v[0], 0.707f),
        Coeff::makePeakFilter(sr, v[1], 0.85f, juce::Decibels::decibelsToGain(v[2])),
        Coeff::makePeakFilter(sr, v[3], 1.0f, juce::Decibels::decibelsToGain(v[4])),
        Coeff::makePeakFilter(sr, v[5], 0.8f, juce::Decibels::decibelsToGain(v[6]))
    };
    for (auto& channel : filters)
        for (size_t i = 0; i < cs.size(); ++i) *channel[i].coefficients = *cs[i];
}

void KickProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    updateFilters();
    const int n = buffer.getNumSamples();
    if (captureRequested.load(std::memory_order_acquire)) {
        if (capturePosition >= captureSize) capturePosition = 0;
        for (int i = 0; i < n && capturePosition < captureSize; ++i) {
            float mono = 0.f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) mono += buffer.getSample(ch, i);
            captured[static_cast<size_t>(capturePosition++)] = mono / juce::jmax(1, buffer.getNumChannels());
        }
        if (capturePosition >= captureSize) {
            captureRequested.store(false, std::memory_order_release);
            captureReady.store(true, std::memory_order_release);
        }
    }
    if (parameters.getRawParameterValue("enabled")->load() < 0.5f) return;
    const auto amount = parameters.getRawParameterValue("mix")->load() * 0.01f;
    const auto out = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("output")->load());
    const bool dynamicsEnabled = parameters.getRawParameterValue("dynamics")->load() >= 0.5f;
    const float threshold = parameters.getRawParameterValue("threshold")->load();
    const float ratio = parameters.getRawParameterValue("ratio")->load();
    const float attackMs = parameters.getRawParameterValue("attack")->load();
    const float releaseMs = parameters.getRawParameterValue("release")->load();
    const float punch = parameters.getRawParameterValue("punch")->load() * 0.01f;
    const float tight = parameters.getRawParameterValue("tight")->load() * 0.01f;
    const float attackCoeff = std::exp(-1.f / (0.001f * attackMs * static_cast<float>(sampleRateHz)));
    const float releaseCoeff = std::exp(-1.f / (0.001f * releaseMs * static_cast<float>(sampleRateHz)));
    const float fastCoeff = std::exp(-1.f / (0.003f * static_cast<float>(sampleRateHz)));
    const float slowCoeff = std::exp(-1.f / (0.050f * static_cast<float>(sampleRateHz)));
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        float* samples = buffer.getWritePointer(ch);
        for (int i = 0; i < n; ++i) {
            const float dry = samples[i]; float wet = dry;
            for (auto& f : filters[static_cast<size_t>(juce::jmin(ch, 1))]) wet = f.processSample(wet);
            if (dynamicsEnabled)
                wet = processDynamicsSample(wet, dynamicStates[static_cast<size_t>(juce::jmin(ch, 1))],
                                            threshold, ratio, attackCoeff, releaseCoeff, fastCoeff, slowCoeff,
                                            punch, tight);
            samples[i] = dry + (wet * out - dry) * amount;
        }
    }
}

float KickProcessor::processDynamicsSample(float sample, DynamicState& s, float threshold, float ratio,
                                           float attackCoeff, float releaseCoeff, float fastCoeff, float slowCoeff,
                                           float punch, float tight) {
    const float rectified = std::abs(sample);
    const float coeff = rectified > s.detector ? attackCoeff : releaseCoeff;
    s.detector = coeff * s.detector + (1.f - coeff) * rectified;
    const float levelDb = juce::Decibels::gainToDecibels(s.detector, -120.f);
    const float reductionDb = levelDb > threshold ? (levelDb - threshold) * (1.f - 1.f / juce::jmax(1.f, ratio)) : 0.f;
    const float targetGain = juce::Decibels::decibelsToGain(-reductionDb);
    s.gain = 0.98f * s.gain + 0.02f * targetGain;
    // Two envelopes provide simple transient-versus-sustain classification.
    s.fast = fastCoeff * s.fast + (1.f - fastCoeff) * rectified;
    s.slow = slowCoeff * s.slow + (1.f - slowCoeff) * rectified;
    const float transient = juce::jlimit(0.f, 1.f, (s.fast - s.slow) / (s.fast + 0.0001f));
    const float sustain = juce::jlimit(0.f, 1.f, (s.slow - s.fast) / (s.slow + 0.0001f));
    const float shaping = (1.f + 0.7f * punch * transient) * (1.f - 0.65f * tight * sustain);
    return sample * s.gain * shaping;
}

float KickProcessor::bandEnergy(const std::array<float, captureSize / 2>& m, float lo, float hi, double sr) {
    float total = 0.f; int count = 0;
    for (int i = 1; i < captureSize / 2; ++i) {
        const float hz = static_cast<float>(i * sr / captureSize);
        if (hz >= lo && hz < hi) { total += m[static_cast<size_t>(i)]; ++count; }
    }
    return count ? total / count : 0.f;
}

KickProcessor::Suggestion KickProcessor::analyze(bool club) const {
    std::array<float, captureSize * 2> data {};
    for (int i = 0; i < captureSize; ++i) {
        const float w = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (captureSize - 1));
        data[static_cast<size_t>(i)] = captured[static_cast<size_t>(i)] * w;
    }
    juce::dsp::FFT fft(fftOrder);
    fft.performRealOnlyForwardTransform(data.data());
    std::array<float, captureSize / 2> m {};
    for (int i = 1; i < captureSize / 2; ++i) {
        const float re = data[static_cast<size_t>(i * 2)], im = data[static_cast<size_t>(i * 2 + 1)];
        m[static_cast<size_t>(i)] = std::sqrt(re * re + im * im);
    }
    int best = 1; float peak = 0;
    for (int i = 1; i < captureSize / 2; ++i) {
        const auto hz = i * sampleRateHz / captureSize;
        if (hz < 35 || hz > 145) continue;
        const float smoothed = (m[static_cast<size_t>(i - 1)] + m[static_cast<size_t>(i)] + m[static_cast<size_t>(i + 1)]) / 3.f;
        if (smoothed > peak) { peak = smoothed; best = i; }
    }
    Suggestion s;
    s.fundamental = static_cast<float>(best * sampleRateHz / captureSize);
    const float sub = bandEnergy(m, 20, 35, sampleRateHz);
    const float body = bandEnergy(m, 45, 130, sampleRateHz);
    const float mud = bandEnergy(m, 180, 600, sampleRateHz);
    const float click = bandEnergy(m, 2000, 7500, sampleRateHz);
    const float lowReference = juce::jmax(body, 0.00001f);
    s.confidence = juce::jlimit(0.f, 100.f, 100.f * peak / (peak + 5.f));
    s.bodyHz = juce::jlimit(35.f, 150.f, s.fundamental);
    s.subCut = juce::jlimit(22.f, 40.f, s.fundamental * 0.52f);
    s.bodyDb = juce::jlimit(-2.f, club ? 3.5f : 2.f,
        (club ? 2.0f : 0.8f) - 2.f * std::log10(1.f + body / juce::jmax(peak, 0.00001f)));
    s.mudHz = 320.f;
    s.mudDb = juce::jlimit(-4.f, 0.f, -1.5f * mud / lowReference);
    s.clickHz = 3500.f;
    s.clickDb = juce::jlimit(-2.f, club ? 2.5f : 1.f, (club ? 1.7f : 0.5f) - click / lowReference);
    if (sub > body * 1.5f) s.subCut = juce::jmin(45.f, s.subCut + 5.f);
    if (peak < 0.001f) s.confidence = 0.f;
    // Time-domain diagnostics: strongest transient, preceding onset and short-window decay.
    int peakIndex = 0; float peakSample = 0.f;
    for (int i = 0; i < captureSize; ++i) {
        const float a = std::abs(captured[static_cast<size_t>(i)]);
        if (a > peakSample) { peakSample = a; peakIndex = i; }
    }
    const int lookback = juce::jmax(1, static_cast<int>(sampleRateHz * 0.060));
    int onset = peakIndex;
    while (onset > juce::jmax(0, peakIndex - lookback) &&
           std::abs(captured[static_cast<size_t>(onset - 1)]) > peakSample * 0.08f) --onset;
    // If the waveform crosses zero in the attack, detect energy via RMS windows instead.
    const int window = juce::jmax(8, static_cast<int>(sampleRateHz * 0.005));
    float rmsSum = 0.f; int rmsCount = 0;
    const int end = juce::jmin(captureSize, peakIndex + static_cast<int>(sampleRateHz * 0.18));
    for (int i = onset; i < end; ++i) {
        const float v = captured[static_cast<size_t>(i)];
        rmsSum += v * v; ++rmsCount;
    }
    const float rms = std::sqrt(rmsSum / juce::jmax(1, rmsCount));
    s.attackMs = juce::jlimit(0.f, 60.f, 1000.f * (peakIndex - onset) / static_cast<float>(sampleRateHz));
    s.crestDb = juce::Decibels::gainToDecibels((peakSample + 1.e-6f) / (rms + 1.e-6f));
    int decayEnd = peakIndex;
    for (int i = peakIndex + window; i + window < captureSize; i += window) {
        float sum = 0.f;
        for (int j = i; j < i + window; ++j) {
            const float v = captured[static_cast<size_t>(j)]; sum += v * v;
        }
        if (std::sqrt(sum / window) < peakSample * 0.12f) { decayEnd = i; break; }
        decayEnd = i;
    }
    s.decayMs = juce::jlimit(0.f, 350.f, 1000.f * (decayEnd - peakIndex) / static_cast<float>(sampleRateHz));
    // Compression is recommended only for unusually high crest factor.
    s.dynamicsNeed = juce::jlimit(0.f, 1.f, (s.crestDb - 11.f) / 10.f);
    s.ratio = s.dynamicsNeed < 0.3f ? 1.f : juce::jlimit(1.5f, 3.5f, 1.5f + s.dynamicsNeed * 1.8f);
    s.thresholdDb = juce::jlimit(-30.f, -4.f,
        juce::Decibels::gainToDecibels(rms + 1.e-6f) + 3.f);
    s.compressorAttackMs = club ? 18.f : 25.f;
    s.compressorReleaseMs = juce::jlimit(45.f, 180.f, s.decayMs * 0.65f);
    s.punch = juce::jlimit(0.f, 60.f, (club ? 27.f : 12.f) + (8.f - s.crestDb) * 1.5f);
    s.tight = juce::jlimit(0.f, 70.f, (s.decayMs - (club ? 95.f : 125.f)) * 0.4f);
    if (peakSample < 0.005f) s.confidence = 0.f;
    return s;
}

bool KickProcessor::consumeCapture(Suggestion& suggestion, bool club) {
    if (!captureReady.exchange(false, std::memory_order_acq_rel)) return false;
    suggestion = analyze(club);
    return true;
}

void KickProcessor::applySuggestion(const Suggestion& s) {
    auto set = [&](const char* id, float value) {
        if (auto* p = parameters.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    set("sub", s.subCut); set("bodyHz", s.bodyHz); set("bodyDb", s.bodyDb);
    set("mudHz", s.mudHz); set("mudDb", s.mudDb);
    set("clickHz", s.clickHz); set("clickDb", s.clickDb);
    set("output", juce::jlimit(-5.f, 0.f, -(juce::jmax(0.f, s.bodyDb) + juce::jmax(0.f, s.clickDb)) * 0.35f));
    set("threshold", s.thresholdDb); set("ratio", s.ratio);
    set("attack", s.compressorAttackMs); set("release", s.compressorReleaseMs);
    set("punch", s.punch); set("tight", s.tight);
    set("dynamics", 1.f);
    set("enabled", 1.f);
}

void KickProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void KickProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* KickProcessor::createEditor() { return new KickEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KickProcessor(); }
