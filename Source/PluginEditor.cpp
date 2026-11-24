#include "PluginEditor.h"
#include "Theme.h"

namespace theme = equ::theme;

//==============================================================================
EqualizerLookAndFeel::EqualizerLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::background);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, theme::background.withAlpha (0.6f));
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::ComboBox::backgroundColourId, theme::panelLight);
    setColour (juce::ComboBox::outlineColourId, theme::grid);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::arrowColourId, theme::gridText);
    setColour (juce::PopupMenu::backgroundColourId, theme::panel);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent.withAlpha (0.4f));
    setColour (juce::ToggleButton::textColourId, theme::text);
    setColour (juce::ToggleButton::tickColourId, theme::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, theme::gridText);
    setColour (juce::TextButton::buttonColourId, theme::panelLight);
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc62828));
    setColour (juce::TextButton::textColourOffId, theme::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void EqualizerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                             float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto lineW  = juce::jmax (2.0f, radius * 0.16f);
    const auto arcR   = radius - lineW * 0.5f;
    const auto angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId)
                            .withMultipliedAlpha (slider.isEnabled() ? 1.0f : 0.3f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (theme::grid);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar knobs (gain) fill from the centre, others from the start
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto fromAngle = bipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f : rotaryStartAngle;

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
    g.setColour (fill);
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto knobR = arcR - lineW * 1.2f;
    g.setColour (theme::panelLight);
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre));

    const juce::Point<float> tip (centre.x + knobR * 0.8f * std::sin (angle), centre.y - knobR * 0.8f * std::cos (angle));
    g.setColour (theme::text.withMultipliedAlpha (slider.isEnabled() ? 1.0f : 0.4f));
    g.drawLine ({ centre.getPointOnCircumference (knobR * 0.2f, angle), tip }, 2.0f);
}

//==============================================================================
BandStrip::BandStrip (EqualizerUltimateProcessor& p, int bandIndex)
    : processor (p), band (bandIndex)
{
    const auto colour = theme::bandColour (band);

    enableButton.setButtonText (juce::String (band + 1));
    enableButton.setColour (juce::ToggleButton::tickColourId, colour);
    addAndMakeVisible (enableButton);

    typeBox.addItemList (equ::getBandTypeNames(), 1);
    addAndMakeVisible (typeBox);

    slopeBox.addItemList (equ::getSlopeNames(), 1);
    addAndMakeVisible (slopeBox);

    auto setupKnob = [&] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setColour (juce::Slider::rotarySliderFillColourId, colour);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::FontOptions (11.0f));
        l.setColour (juce::Label::textColourId, theme::gridText);
        addAndMakeVisible (l);
    };

    setupKnob (freqSlider, freqLabel, "Freq");
    setupKnob (gainSlider, gainLabel, "Gain");
    setupKnob (qSlider,    qLabel,    "Q");

    auto& apvts = processor.apvts;
    enableAttachment = std::make_unique<ButtonAttachment> (apvts, equ::ParamIDs::enabled (band),   enableButton);
    typeAttachment   = std::make_unique<ComboAttachment>  (apvts, equ::ParamIDs::type (band),      typeBox);
    slopeAttachment  = std::make_unique<ComboAttachment>  (apvts, equ::ParamIDs::slope (band),     slopeBox);
    freqAttachment   = std::make_unique<SliderAttachment> (apvts, equ::ParamIDs::frequency (band), freqSlider);
    gainAttachment   = std::make_unique<SliderAttachment> (apvts, equ::ParamIDs::gain (band),      gainSlider);
    qAttachment      = std::make_unique<SliderAttachment> (apvts, equ::ParamIDs::q (band),         qSlider);

    gainSlider.setDoubleClickReturnValue (true, 0.0);
    qSlider.setDoubleClickReturnValue (true, 0.707);

    updateControlStates();
}

void BandStrip::updateControlStates()
{
    const auto params = processor.getBandParams (band);
    const bool on = params.enabled;

    const bool usesGain  = params.type == equ::BandType::bell || params.type == equ::BandType::lowShelf
                        || params.type == equ::BandType::highShelf;
    const bool usesSlope = params.isCut();
    const bool usesQ     = ! usesSlope || params.slope == 0;

    typeBox.setEnabled (on);
    freqSlider.setEnabled (on);
    gainSlider.setEnabled (on && usesGain);
    qSlider.setEnabled (on && usesQ);
    slopeBox.setEnabled (on && usesSlope);
}

void BandStrip::setHighlighted (bool shouldBeHighlighted)
{
    if (highlighted != shouldBeHighlighted)
    {
        highlighted = shouldBeHighlighted;
        repaint();
    }
}

void BandStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (theme::bandColour (band).withAlpha (highlighted ? 0.9f : 0.35f));
    g.drawRoundedRectangle (bounds, 6.0f, highlighted ? 1.5f : 1.0f);
}

void BandStrip::resized()
{
    auto area = getLocalBounds().reduced (6);

    enableButton.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    typeBox.setBounds (area.removeFromTop (22));
    slopeBox.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (4);

    auto placeKnob = [] (juce::Rectangle<int> r, juce::Slider& s, juce::Label& l)
    {
        l.setBounds (r.removeFromTop (14));
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmin (r.getWidth(), 72), 16);
        s.setBounds (r);
    };

    // Frequency gets the big knob, gain and Q share the row below it
    auto top = area.removeFromTop (juce::roundToInt ((float) area.getHeight() * 0.55f));
    placeKnob (top, freqSlider, freqLabel);

    area.removeFromTop (2);
    auto left = area.removeFromLeft (area.getWidth() / 2);
    placeKnob (left, gainSlider, gainLabel);
    placeKnob (area, qSlider, qLabel);
}

//==============================================================================
EqualizerUltimateEditor::EqualizerUltimateEditor (EqualizerUltimateProcessor& p)
    : AudioProcessorEditor (&p), processor (p), responseCurve (p)
{
    addAndMakeVisible (responseCurve);

    for (int b = 0; b < equ::numBands; ++b)
    {
        strips.push_back (std::make_unique<BandStrip> (processor, b));
        addAndMakeVisible (*strips.back());
    }

    outputSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    outputSlider.setColour (juce::Slider::rotarySliderFillColourId, theme::accent);
    outputSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (outputSlider);

    outputLabel.setText ("Output", juce::dontSendNotification);
    outputLabel.setJustificationType (juce::Justification::centred);
    outputLabel.setColour (juce::Label::textColourId, theme::gridText);
    addAndMakeVisible (outputLabel);

    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);

    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, equ::ParamIDs::output, outputSlider);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, equ::ParamIDs::bypass, bypassButton);

    // Set after all children exist so they all pick it up (slider text boxes take colours at creation)
    setLookAndFeel (&lookAndFeel);

    setResizable (true, true);
    setResizeLimits (820, 500, 1800, 1100);
    setSize (1000, 600);

    startTimerHz (15);
}

EqualizerUltimateEditor::~EqualizerUltimateEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EqualizerUltimateEditor::timerCallback()
{
    const auto selected = responseCurve.getSelectedBand();

    for (int b = 0; b < (int) strips.size(); ++b)
    {
        strips[(size_t) b]->updateControlStates();
        strips[(size_t) b]->setHighlighted (b == selected);
    }
}

void EqualizerUltimateEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    auto header = getLocalBounds().removeFromTop (34).reduced (12, 0);
    g.setColour (theme::text);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("EQUALIZER ULTIMATE", header, juce::Justification::centredLeft);

    g.setColour (theme::gridText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("8-band parametric EQ", header, juce::Justification::centredRight);
}

void EqualizerUltimateEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (34);
    area.reduce (10, 0);
    area.removeFromBottom (10);

    const auto stripHeight = juce::jlimit (250, 330, (int) (area.getHeight() * 0.48f));
    auto bottom = area.removeFromBottom (stripHeight);
    area.removeFromBottom (8);
    responseCurve.setBounds (area);

    auto masterColumn = bottom.removeFromRight (96);
    bottom.removeFromRight (6);

    masterColumn.removeFromTop (10);
    outputLabel.setBounds (masterColumn.removeFromTop (16));
    outputSlider.setBounds (masterColumn.removeFromTop (110));
    masterColumn.removeFromTop (12);
    bypassButton.setBounds (masterColumn.removeFromTop (30).reduced (6, 0));

    const auto stripWidth = bottom.getWidth() / (int) strips.size();

    for (auto& strip : strips)
        strip->setBounds (bottom.removeFromLeft (stripWidth).reduced (2, 0));
}
