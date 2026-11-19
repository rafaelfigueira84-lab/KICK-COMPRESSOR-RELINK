
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class KickProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int fftOrder = 14;
    static constexpr int captureSize = 1 << fftOrder;

    struct Suggestion
    {
        float fundamental = 60.f;
        float subCut = 28.f;

        float bodyHz = 65.f;
        float bodyDb = 0.f;

        float mudHz = 300.f;
        float mudDb = 0.f;

        float clickHz = 3500.f;
        float clickDb = 0.f;

        float confidence = 0.f;

        float attackMs = 0.f;
        float decayMs = 0.f;
        float crestDb = 0.f;

        float thresholdDb = -12.f;
        float ratio = 1.f;

        float compressorAttackMs = 15.f;
        float compressorReleaseMs = 90.f;

        float dynamicsNeed = 0.f;
        float punch = 0.f;
        float tight = 0.f;
    };

    KickProcessor();

    const juce::String getName() const override
    {
        return "RELINK Kick Enhancer";
    }

    void prepareToPlay(double, int) override;
    void releaseResources() override {}

    void processBlock(
        juce::AudioBuffer<float>&,
        juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;

    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override
    {
        return 0.0;
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }

    void setCurrentProgram(int) override {}

    const juce::String getProgramName(int) override
    {
        return {};
    }

    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout&) const override;

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void beginCapture();

    bool consumeCapture(Suggestion&, bool club);

    void applySuggestion(const Suggestion&);

    juce::AudioProcessorValueTreeState parameters;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeff = juce::dsp::IIR::Coefficients<float>;

    static juce::AudioProcessorValueTreeState::ParameterLayout
        layout();

    static float bandEnergy(
        const std::array<float, captureSize / 2>&,
        float,
        float,
        double);

    Suggestion analyze(bool club) const;

    void updateFilters();

    struct DynamicState
    {
        float detector = 0.f;
        float fast = 0.f;
        float slow = 0.f;
        float gain = 1.f;
    };

    float processDynamicsSample(
        float sample,
        DynamicState&,
        float threshold,
        float ratio,
        float attackCoeff,
        float releaseCoeff,
        float fastCoeff,
        float slowCoeff,
        float punch,
        float tight);

    std::array<float, captureSize> captured {};

    std::atomic<bool> captureRequested {false};
    std::atomic<bool> captureReady {false};

    int capturePosition = 0;

    double sampleRateHz = 44100.0;

    std::array<std::array<Filter, 4>, 2> filters;

    std::array<DynamicState, 2> dynamicStates {};

    std::array<float, 10> previousParameters {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KickProcessor)
};
