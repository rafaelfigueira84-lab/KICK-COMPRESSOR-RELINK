#include "PluginEditor.h"

KickEditor::KickEditor(KickProcessor& p) : AudioProcessorEditor(&p), processor(p) {
    setSize(650, 535);
    mode.addItem("NATURAL", 1);
    mode.addItem("CLUB", 2);
    mode.setSelectedId(1);
    addAndMakeVisible(mode);
    modeLabel.setText("MODE", juce::dontSendNotification);
    addAndMakeVisible(modeLabel);

    amount.setSliderStyle(juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 25);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "mix", amount);
    addAndMakeVisible(amount);
    amountLabel.setText("CORRECTION AMOUNT", juce::dontSendNotification);
    addAndMakeVisible(amountLabel);
    for (auto* s : {&punchAmount, &tightAmount}) {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 24);
        addAndMakeVisible(*s);
    }
    punchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "punch", punchAmount);
    tightAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "tight", tightAmount);
    punchLabel.setText("PUNCH", juce::dontSendNotification);
    tightLabel.setText("TIGHT", juce::dontSendNotification);
    addAndMakeVisible(punchLabel);
    addAndMakeVisible(tightLabel);

    analyzeButton.onClick = [this] {
        hasPending = false;
        applyButton.setEnabled(false);
        processor.beginCapture();
        status.setText("Listening... play the kick for at least one second.", juce::dontSendNotification);
    };
    applyButton.setEnabled(false);
    applyButton.onClick = [this] {
        if (!hasPending || pending.confidence < 1.f) return;
        processor.applySuggestion(pending);
        status.setText("Correction applied. Compare using BYPASS.", juce::dontSendNotification);
    };
    bypassButton.onClick = [this] {
        if (auto* param = processor.parameters.getParameter("enabled")) {
            const bool enabled = processor.parameters.getRawParameterValue("enabled")->load() > 0.5f;
            param->setValueNotifyingHost(enabled ? 0.f : 1.f);
        }
    };
    for (auto* b : {&analyzeButton, &applyButton, &bypassButton}) {
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263c56));
        addAndMakeVisible(*b);
    }
    status.setText("Select a mode, play the kick, then click ANALYZE.", juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    status.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(status);
    results.setText("FUNDAMENTAL  -- Hz       SUB CUT  -- Hz\nBODY  -- dB       MID  -- dB       ATTACK  -- dB", juce::dontSendNotification);
    results.setJustificationType(juce::Justification::centred);
    results.setColour(juce::Label::textColourId, juce::Colour(0xff69dbc4));
    addAndMakeVisible(results);
    dynamicsResults.setText("ATTACK  -- ms       DECAY  -- ms       CREST  -- dB\nCOMPRESSOR  --       PUNCH  --       TIGHT  --", juce::dontSendNotification);
    dynamicsResults.setJustificationType(juce::Justification::centred);
    dynamicsResults.setColour(juce::Label::textColourId, juce::Colour(0xfff4c67d));
    addAndMakeVisible(dynamicsResults);
    startTimerHz(15);
}

void KickEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff101923));
    g.setColour(juce::Colour(0xff59d5c0));
    g.setFont(juce::Font(juce::FontOptions(30.f, juce::Font::bold)));
    g.drawText("RELINK  KICK ENHANCER", 25, 18, getWidth() - 50, 42, juce::Justification::centred);
    g.setColour(juce::Colours::lightgrey);
    g.setFont(13.f);
    g.drawText("SMART SPECTRUM + DYNAMICS  •  v1.1", 25, 59, getWidth()-50, 22, juce::Justification::centred);
    g.setColour(juce::Colour(0xff1b2a38));
    g.fillRoundedRectangle(24.f, 106.f, getWidth()-48.f, 205.f, 12.f);
    g.setColour(juce::Colour(0xff334b5e));
    g.drawRoundedRectangle(24.f, 106.f, getWidth()-48.f, 205.f, 12.f, 1.f);
}

void KickEditor::resized() {
    modeLabel.setBounds(33, 87, 55, 24);
    mode.setBounds(91, 88, 140, 24);
    results.setBounds(34, 117, getWidth()-68, 74);
    dynamicsResults.setBounds(34, 204, getWidth()-68, 88);
    amountLabel.setBounds(30, 320, 165, 30);
    amount.setBounds(195, 320, 420, 30);
    punchLabel.setBounds(30, 358, 95, 26);
    punchAmount.setBounds(115, 358, 205, 27);
    tightLabel.setBounds(345, 358, 70, 26);
    tightAmount.setBounds(410, 358, 205, 27);
    analyzeButton.setBounds(30, 423, 180, 40);
    applyButton.setBounds(230, 423, 210, 40);
    bypassButton.setBounds(460, 423, 160, 40);
    status.setBounds(25, 480, getWidth()-50, 30);
}

void KickEditor::timerCallback() {
    KickProcessor::Suggestion s;
    if (!processor.consumeCapture(s, mode.getSelectedId() == 2)) return;
    pending = s;
    hasPending = true;
    if (s.confidence < 1.f) {
        status.setText("No kick detected. Play audio and analyze again.", juce::dontSendNotification);
        return;
    }
    results.setText(juce::String::formatted("FUNDAMENTAL  %.1f Hz       SUB CUT  %.1f Hz\nBODY  %+.1f dB @ %.0f Hz       MID  %+.1f dB       ATTACK  %+.1f dB",
        s.fundamental, s.subCut, s.bodyDb, s.bodyHz, s.mudDb, s.clickDb), juce::dontSendNotification);
    dynamicsResults.setText(juce::String::formatted("ATTACK  %.1f ms       DECAY  %.0f ms       CREST  %.1f dB\nCOMPRESSOR  %.1f:1 @ %.1f dB       PUNCH  %.0f%%       TIGHT  %.0f%%",
        s.attackMs, s.decayMs, s.crestDb, s.ratio, s.thresholdDb, s.punch, s.tight), juce::dontSendNotification);
    status.setText("Analysis finished. Review the suggestions, then click APPLY.", juce::dontSendNotification);
    applyButton.setEnabled(true);
}
