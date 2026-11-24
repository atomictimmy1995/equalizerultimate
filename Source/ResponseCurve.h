#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

/**
    Draws the combined frequency response of all bands, plus a draggable node per band.

    - Drag a node: frequency (horizontal) and gain (vertical, for bell/shelf bands)
    - Mouse wheel over a node: Q
    - Double-click a node: toggle the band on/off
*/
class ResponseCurve final : public juce::Component,
                            private juce::Timer
{
public:
    explicit ResponseCurve (EqualizerUltimateProcessor&);
    ~ResponseCurve() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** Band currently selected/hovered, or -1. */
    int getSelectedBand() const noexcept { return selectedBand; }

private:
    void timerCallback() override;
    void refreshBands();

    juce::Rectangle<float> getPlotArea() const;
    float frequencyToX (double hz) const;
    double xToFrequency (float x) const;
    float dbToY (double db) const;
    double yToDb (float y) const;

    juce::Point<float> getNodePosition (int band) const;
    int findNodeAt (juce::Point<float> position) const;

    void setParam (const juce::String& id, float plainValue);

    EqualizerUltimateProcessor& processor;

    std::array<equ::BandParams, equ::numBands> params;
    std::array<equ::StageSet, equ::numBands> stageSets;
    double sampleRate = 48000.0;

    int selectedBand = -1;
    int draggingBand = -1;

    static constexpr double displayRangeDb = 24.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResponseCurve)
};
