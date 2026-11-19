#pragma once
#include "PluginProcessor.h"

class KickEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit KickEditor(KickProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    KickProcessor& processor;
    juce::TextButton analyzeButton {"ANALYZE KICK"}, applyButton {"APPLY CORRECTION"};
    juce::TextButton bypassButton {"BYPASS"};
    juce::ComboBox mode;
    juce::Slider amount, punchAmount, tightAmount;
    juce::Label status, results, dynamicsResults, amountLabel, modeLabel, punchLabel, tightLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> punchAttachment, tightAttachment;
    KickProcessor::Suggestion pending;
    bool hasPending = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KickEditor)
};
