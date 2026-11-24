#include "ResponseCurve.h"
#include "Theme.h"

namespace
{
    bool hasGain (equ::BandType t)
    {
        return t == equ::BandType::bell || t == equ::BandType::lowShelf || t == equ::BandType::highShelf;
    }

    constexpr float nodeRadius = 9.0f;
}

ResponseCurve::ResponseCurve (EqualizerUltimateProcessor& p)
    : processor (p)
{
    refreshBands();
    startTimerHz (30);
}

ResponseCurve::~ResponseCurve()
{
    stopTimer();
}

//==============================================================================
void ResponseCurve::refreshBands()
{
    sampleRate = processor.getSampleRateForDisplay();

    for (int b = 0; b < equ::numBands; ++b)
    {
        params[(size_t) b]    = processor.getBandParams (b);
        stageSets[(size_t) b] = equ::computeStages (params[(size_t) b], sampleRate);
    }
}

void ResponseCurve::timerCallback()
{
    const auto previous = params;
    const auto previousRate = sampleRate;
    refreshBands();

    bool changed = ! juce::approximatelyEqual (previousRate, sampleRate);

    for (size_t b = 0; b < params.size() && ! changed; ++b)
    {
        const auto& a = previous[b];
        const auto& c = params[b];
        changed = a.enabled != c.enabled || a.type != c.type || a.slope != c.slope
               || ! juce::approximatelyEqual (a.frequency, c.frequency)
               || ! juce::approximatelyEqual (a.gainDb, c.gainDb)
               || ! juce::approximatelyEqual (a.q, c.q);
    }

    if (changed)
        repaint();
}

//==============================================================================
juce::Rectangle<float> ResponseCurve::getPlotArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f).withTrimmedLeft (28.0f).withTrimmedBottom (16.0f);
}

float ResponseCurve::frequencyToX (double hz) const
{
    const auto area = getPlotArea();
    const auto norm = std::log (hz / (double) equ::minFrequency) / std::log ((double) equ::maxFrequency / (double) equ::minFrequency);
    return area.getX() + (float) norm * area.getWidth();
}

double ResponseCurve::xToFrequency (float x) const
{
    const auto area = getPlotArea();
    const auto norm = juce::jlimit (0.0, 1.0, (double) ((x - area.getX()) / area.getWidth()));
    return equ::minFrequency * std::pow ((double) equ::maxFrequency / (double) equ::minFrequency, norm);
}

float ResponseCurve::dbToY (double db) const
{
    const auto area = getPlotArea();
    return (float) juce::jmap (db, -displayRangeDb, displayRangeDb, (double) area.getBottom(), (double) area.getY());
}

double ResponseCurve::yToDb (float y) const
{
    const auto area = getPlotArea();
    return juce::jmap ((double) y, (double) area.getBottom(), (double) area.getY(), -displayRangeDb, displayRangeDb);
}

juce::Point<float> ResponseCurve::getNodePosition (int b) const
{
    const auto& p = params[(size_t) b];
    const auto y = hasGain (p.type) ? dbToY (p.gainDb) : dbToY (0.0);
    return { frequencyToX (p.frequency), y };
}

int ResponseCurve::findNodeAt (juce::Point<float> position) const
{
    int best = -1;
    float bestDistance = nodeRadius * 1.6f;

    for (int b = 0; b < equ::numBands; ++b)
    {
        const auto d = getNodePosition (b).getDistanceFrom (position);

        if (d < bestDistance)
        {
            bestDistance = d;
            best = b;
        }
    }

    return best;
}

//==============================================================================
void ResponseCurve::paint (juce::Graphics& g)
{
    namespace theme = equ::theme;

    const auto bounds = getLocalBounds().toFloat();
    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto area = getPlotArea();
    g.setFont (juce::FontOptions (11.0f));

    // Frequency grid
    for (const double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const auto x = frequencyToX (hz);
        g.setColour (theme::grid);
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());

        const auto label = hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz);
        g.setColour (theme::gridText);
        g.drawFittedText (label, juce::Rectangle<int> (juce::roundToInt (x) - 20, (int) area.getBottom() + 2, 40, 14),
                          juce::Justification::centred, 1);
    }

    // Gain grid
    for (double db = -displayRangeDb; db <= displayRangeDb; db += 6.0)
    {
        const auto y = dbToY (db);
        g.setColour (juce::approximatelyEqual (db, 0.0) ? theme::grid.brighter (0.4f) : theme::grid);
        g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());

        g.setColour (theme::gridText);
        g.drawFittedText ((db > 0 ? "+" : "") + juce::String ((int) db),
                          juce::Rectangle<int> ((int) bounds.getX() + 4, juce::roundToInt (y) - 7, 30, 14),
                          juce::Justification::centredLeft, 1);
    }

    const auto width = juce::jmax (1, (int) area.getWidth());
    std::vector<double> frequencies ((size_t) width + 1);

    for (int i = 0; i <= width; ++i)
        frequencies[(size_t) i] = xToFrequency (area.getX() + (float) i);

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    // Individual band responses
    for (int b = 0; b < equ::numBands; ++b)
    {
        const auto& p = params[(size_t) b];

        if (! p.enabled)
            continue;

        juce::Path bandPath;
        bandPath.startNewSubPath (area.getX(), dbToY (0.0));

        for (int i = 0; i <= width; ++i)
        {
            const auto mag = equ::getMagnitude (stageSets[(size_t) b], frequencies[(size_t) i], sampleRate);
            bandPath.lineTo (area.getX() + (float) i, dbToY (juce::Decibels::gainToDecibels (mag, -100.0)));
        }

        bandPath.lineTo (area.getRight(), dbToY (0.0));
        bandPath.closeSubPath();

        const auto colour = equ::theme::bandColour (b);
        const bool highlighted = b == selectedBand || b == draggingBand;
        g.setColour (colour.withAlpha (highlighted ? 0.28f : 0.12f));
        g.fillPath (bandPath);
    }

    // Combined response
    juce::Path total;

    for (int i = 0; i <= width; ++i)
    {
        double mag = 1.0;

        for (int b = 0; b < equ::numBands; ++b)
            if (params[(size_t) b].enabled)
                mag *= equ::getMagnitude (stageSets[(size_t) b], frequencies[(size_t) i], sampleRate);

        const auto x = area.getX() + (float) i;
        const auto y = dbToY (juce::Decibels::gainToDecibels (mag, -100.0));

        if (i == 0)
            total.startNewSubPath (x, y);
        else
            total.lineTo (x, y);
    }

    g.setColour (theme::curve);
    g.strokePath (total, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.restoreState();

    // Nodes
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

    for (int b = 0; b < equ::numBands; ++b)
    {
        const auto& p = params[(size_t) b];
        const auto centre = getNodePosition (b);
        const auto colour = equ::theme::bandColour (b);
        const auto node = juce::Rectangle<float> (nodeRadius * 2.0f, nodeRadius * 2.0f).withCentre (centre);
        const bool highlighted = b == selectedBand || b == draggingBand;

        g.setColour (p.enabled ? colour : colour.withSaturation (0.0f).withAlpha (0.45f));
        g.fillEllipse (node);

        if (highlighted)
        {
            g.setColour (theme::curve);
            g.drawEllipse (node.expanded (2.0f), 1.5f);
        }

        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawFittedText (juce::String (b + 1), node.toNearestInt(), juce::Justification::centred, 1);
    }

    // Readout for the selected band
    if (const auto b = draggingBand >= 0 ? draggingBand : selectedBand; b >= 0)
    {
        const auto& p = params[(size_t) b];
        juce::String text = "Band " + juce::String (b + 1) + "  "
                          + equ::getBandTypeNames()[(int) p.type] + "  "
                          + (p.frequency >= 1000.0f ? juce::String (p.frequency / 1000.0f, 2) + " kHz"
                                                    : juce::String (juce::roundToInt (p.frequency)) + " Hz");

        if (hasGain (p.type))
            text << "  " << juce::String (p.gainDb, 1) << " dB";

        if (! p.isCut() || p.slope == 0)
            text << "  Q " << juce::String (p.q, 2);

        g.setColour (theme::text);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (text, area.reduced (6.0f).removeFromTop (16.0f), juce::Justification::topRight);
    }
}

void ResponseCurve::resized()
{
    repaint();
}

//==============================================================================
void ResponseCurve::setParam (const juce::String& id, float plainValue)
{
    if (auto* param = processor.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
}

void ResponseCurve::mouseMove (const juce::MouseEvent& e)
{
    const auto hovered = findNodeAt (e.position);
    setMouseCursor (hovered >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);

    if (hovered >= 0 && hovered != selectedBand)
    {
        selectedBand = hovered;
        repaint();
    }
}

void ResponseCurve::mouseDown (const juce::MouseEvent& e)
{
    draggingBand = findNodeAt (e.position);

    if (draggingBand < 0)
        return;

    selectedBand = draggingBand;

    for (const auto& id : { equ::ParamIDs::frequency (draggingBand), equ::ParamIDs::gain (draggingBand) })
        if (auto* param = processor.apvts.getParameter (id))
            param->beginChangeGesture();

    repaint();
}

void ResponseCurve::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingBand < 0)
        return;

    const auto area = getPlotArea();
    const auto pos  = e.position.withX (juce::jlimit (area.getX(), area.getRight(), e.position.x))
                                .withY (juce::jlimit (area.getY(), area.getBottom(), e.position.y));

    setParam (equ::ParamIDs::frequency (draggingBand), (float) xToFrequency (pos.x));

    if (hasGain (params[(size_t) draggingBand].type))
        setParam (equ::ParamIDs::gain (draggingBand),
                  juce::jlimit (-equ::maxGainDb, equ::maxGainDb, (float) yToDb (pos.y)));

    refreshBands();
    repaint();
}

void ResponseCurve::mouseUp (const juce::MouseEvent&)
{
    if (draggingBand < 0)
        return;

    for (const auto& id : { equ::ParamIDs::frequency (draggingBand), equ::ParamIDs::gain (draggingBand) })
        if (auto* param = processor.apvts.getParameter (id))
            param->endChangeGesture();

    draggingBand = -1;
    repaint();
}

void ResponseCurve::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto b = findNodeAt (e.position);

    if (b < 0)
        return;

    if (auto* param = processor.apvts.getParameter (equ::ParamIDs::enabled (b)))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        param->endChangeGesture();
    }

    refreshBands();
    repaint();
}

void ResponseCurve::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto b = findNodeAt (e.position) >= 0 ? findNodeAt (e.position) : selectedBand;

    if (b < 0)
        return;

    if (auto* param = processor.apvts.getParameter (equ::ParamIDs::q (b)))
    {
        const auto delta = (wheel.isReversed ? -wheel.deltaY : wheel.deltaY) * 0.1f;
        param->beginChangeGesture();
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, param->getValue() + delta));
        param->endChangeGesture();
    }

    refreshBands();
    repaint();
}
