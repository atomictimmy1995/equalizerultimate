#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ResponseCurve.h"

/** Dark look-and-feel with coloured rotary knobs. */
class EqualizerLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    EqualizerLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
};

/** Controls for a single EQ band. */
class BandStrip final : public juce::Component
{
public:
    BandStrip (EqualizerUltimateProcessor&, int bandIndex);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Enables/disables controls depending on band type (called from the editor's timer). */
    void updateControlStates();

    void setHighlighted (bool shouldBeHighlighted);

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    EqualizerUltimateProcessor& processor;
    const int band;
    bool highlighted = false;

    juce::ToggleButton enableButton;
    juce::ComboBox typeBox, slopeBox;
    juce::Slider freqSlider, gainSlider, qSlider;
    juce::Label freqLabel, gainLabel, qLabel;

    std::unique_ptr<ButtonAttachment> enableAttachment;
    std::unique_ptr<ComboAttachment>  typeAttachment, slopeAttachment;
    std::unique_ptr<SliderAttachment> freqAttachment, gainAttachment, qAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandStrip)
};

class EqualizerUltimateEditor final : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit EqualizerUltimateEditor (EqualizerUltimateProcessor&);
    ~EqualizerUltimateEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    EqualizerUltimateProcessor& processor;
    EqualizerLookAndFeel lookAndFeel;

    ResponseCurve responseCurve;
    std::vector<std::unique_ptr<BandStrip>> strips;

    juce::Slider outputSlider;
    juce::Label outputLabel;
    juce::TextButton bypassButton { "Bypass" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqualizerUltimateEditor)
};
