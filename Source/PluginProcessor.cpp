#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::NormalisableRange<float> makeLogRange (float start, float end)
    {
        return { start, end,
                 [] (float s, float e, float t) { return s * std::pow (e / s, t); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
    }

    constexpr std::array<float, equ::numBands> defaultFrequencies { 30.0f, 80.0f, 200.0f, 500.0f,
                                                                     1200.0f, 3000.0f, 7000.0f, 16000.0f };

    juce::String formatFrequency (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz"
                             : juce::String (juce::roundToInt (hz)) + " Hz";
    }

    float parseFrequency (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        const auto value = t.getFloatValue();
        return t.endsWith ("k") || t.endsWith ("khz") ? value * 1000.0f : value;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EqualizerUltimateProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (int b = 0; b < equ::numBands; ++b)
    {
        const auto prefix  = "Band " + String (b + 1) + " ";
        const bool isFirst = b == 0;
        const bool isLast  = b == equ::numBands - 1;

        const auto defaultType = isFirst ? equ::BandType::lowCut
                               : isLast  ? equ::BandType::highCut
                                         : equ::BandType::bell;

        auto group = std::make_unique<AudioProcessorParameterGroup> ("band" + String (b + 1),
                                                                    "Band " + String (b + 1), "|");

        // Outer bands (cuts) start disabled so the default state is fully transparent.
        group->addChild (std::make_unique<AudioParameterBool> (ParameterID { equ::ParamIDs::enabled (b), 1 },
                                                               prefix + "On", ! (isFirst || isLast)));

        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { equ::ParamIDs::type (b), 1 },
                                                                 prefix + "Type", equ::getBandTypeNames(),
                                                                 (int) defaultType));

        group->addChild (std::make_unique<AudioParameterFloat> (
            ParameterID { equ::ParamIDs::frequency (b), 1 }, prefix + "Frequency",
            makeLogRange (equ::minFrequency, equ::maxFrequency), defaultFrequencies[(size_t) b],
            AudioParameterFloatAttributes()
                .withLabel ("Hz")
                .withStringFromValueFunction ([] (float v, int) { return formatFrequency (v); })
                .withValueFromStringFunction ([] (const String& s) { return parseFrequency (s); })));

        group->addChild (std::make_unique<AudioParameterFloat> (
            ParameterID { equ::ParamIDs::gain (b), 1 }, prefix + "Gain",
            NormalisableRange<float> (-equ::maxGainDb, equ::maxGainDb, 0.1f), 0.0f,
            AudioParameterFloatAttributes()
                .withLabel ("dB")
                .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

        group->addChild (std::make_unique<AudioParameterFloat> (
            ParameterID { equ::ParamIDs::q (b), 1 }, prefix + "Q",
            makeLogRange (0.1f, 18.0f), 0.707f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));

        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { equ::ParamIDs::slope (b), 1 },
                                                                 prefix + "Slope", equ::getSlopeNames(),
                                                                 isFirst || isLast ? 1 : 0));

        layout.add (std::move (group));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { equ::ParamIDs::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { equ::ParamIDs::bypass, 1 }, "Bypass", false));

    return layout;
}

//==============================================================================
EqualizerUltimateProcessor::EqualizerUltimateProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EqualizerUltimate", createParameterLayout())
{
    for (int b = 0; b < equ::numBands; ++b)
    {
        auto& band     = bands[(size_t) b];
        band.enabled   = apvts.getRawParameterValue (equ::ParamIDs::enabled (b));
        band.type      = apvts.getRawParameterValue (equ::ParamIDs::type (b));
        band.frequency = apvts.getRawParameterValue (equ::ParamIDs::frequency (b));
        band.gain      = apvts.getRawParameterValue (equ::ParamIDs::gain (b));
        band.q         = apvts.getRawParameterValue (equ::ParamIDs::q (b));
        band.slope     = apvts.getRawParameterValue (equ::ParamIDs::slope (b));

        for (auto& c : band.coefficients)
            c = new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);

        for (auto& channelFilters : band.filters)
            for (size_t s = 0; s < channelFilters.size(); ++s)
                channelFilters[s] = juce::dsp::IIR::Filter<float> (band.coefficients[s]);
    }

    outputParam = apvts.getRawParameterValue (equ::ParamIDs::output);
    bypassParam = apvts.getRawParameterValue (equ::ParamIDs::bypass);
}

//==============================================================================
bool EqualizerUltimateProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void EqualizerUltimateProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    const juce::dsp::ProcessSpec monoSpec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    constexpr double smoothingSeconds = 0.05;

    for (auto& band : bands)
    {
        for (auto& channelFilters : band.filters)
            for (auto& f : channelFilters)
                f.prepare (monoSpec);

        band.smoothedFreq.reset (sampleRate, smoothingSeconds);
        band.smoothedGain.reset (sampleRate, smoothingSeconds);
        band.smoothedQ.reset    (sampleRate, smoothingSeconds);

        band.smoothedFreq.setCurrentAndTargetValue (band.frequency->load());
        band.smoothedGain.setCurrentAndTargetValue (band.gain->load());
        band.smoothedQ.setCurrentAndTargetValue    (band.q->load());

        band.active = false;
        band.dirty  = true;
    }

    outputGain.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) maxChannels });
    outputGain.setRampDurationSeconds (smoothingSeconds);
    outputGain.setGainDecibels (outputParam->load());
    outputGain.reset();
}

void EqualizerUltimateProcessor::updateBandTargets (Band& band)
{
    const bool enabled = band.enabled->load() > 0.5f;
    const auto freq    = band.frequency->load();
    const auto gain    = band.gain->load();
    const auto q       = band.q->load();

    if (! enabled)
    {
        band.active = false;
        band.smoothedFreq.setCurrentAndTargetValue (freq);
        band.smoothedGain.setCurrentAndTargetValue (gain);
        band.smoothedQ.setCurrentAndTargetValue (q);
        return;
    }

    equ::BandParams p;
    p.type  = (equ::BandType) juce::jlimit (0, 5, (int) band.type->load());
    p.slope = juce::jlimit (0, 3, (int) band.slope->load());
    const auto numStages = equ::getNumStages (p);

    if (! band.active || p.type != band.activeType || numStages != band.activeStages)
    {
        for (auto& channelFilters : band.filters)
            for (auto& f : channelFilters)
                f.reset();

        if (! band.active)
        {
            // When a band is switched on, start straight at its settings rather than gliding.
            band.smoothedFreq.setCurrentAndTargetValue (freq);
            band.smoothedGain.setCurrentAndTargetValue (gain);
            band.smoothedQ.setCurrentAndTargetValue (q);
        }

        band.active       = true;
        band.activeType   = p.type;
        band.activeStages = numStages;
        band.dirty        = true;
    }

    band.smoothedFreq.setTargetValue (freq);
    band.smoothedGain.setTargetValue (gain);
    band.smoothedQ.setTargetValue (q);
}

void EqualizerUltimateProcessor::updateBandCoefficients (Band& band, int numSamplesToSkip)
{
    equ::BandParams p;
    p.type      = band.activeType;
    p.slope     = band.activeStages - 1;
    p.frequency = band.smoothedFreq.skip (numSamplesToSkip);
    p.gainDb    = band.smoothedGain.skip (numSamplesToSkip);
    p.q         = band.smoothedQ.skip (numSamplesToSkip);

    const auto set = equ::computeStages (p, currentSampleRate);

    for (int s = 0; s < set.numStages; ++s)
        *band.coefficients[(size_t) s] = set.stages[(size_t) s];

    band.dirty = false;
}

void EqualizerUltimateProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numInputs  = getTotalNumInputChannels();
    const auto numOutputs = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto ch = numInputs; ch < numOutputs; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (bypassParam->load() > 0.5f)
        return;

    for (auto& band : bands)
        updateBandTargets (band);

    const auto numChannels = juce::jmin (buffer.getNumChannels(), maxChannels);
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);

    for (int start = 0; start < numSamples; start += subBlockSize)
    {
        const auto n = juce::jmin (subBlockSize, numSamples - start);
        auto subBlock = block.getSubBlock ((size_t) start, (size_t) n);

        for (auto& band : bands)
        {
            if (! band.active)
                continue;

            if (band.dirty || band.smoothedFreq.isSmoothing() || band.smoothedGain.isSmoothing()
                || band.smoothedQ.isSmoothing())
                updateBandCoefficients (band, n);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto channelBlock = subBlock.getSingleChannelBlock ((size_t) ch);
                juce::dsp::ProcessContextReplacing<float> context (channelBlock);

                for (int s = 0; s < band.activeStages; ++s)
                    band.filters[(size_t) ch][(size_t) s].process (context);
            }
        }
    }

    outputGain.setGainDecibels (outputParam->load());
    juce::dsp::ProcessContextReplacing<float> outputContext (block);
    outputGain.process (outputContext);
}

//==============================================================================
equ::BandParams EqualizerUltimateProcessor::getBandParams (int b) const
{
    const auto& band = bands[(size_t) b];

    equ::BandParams p;
    p.enabled   = band.enabled->load() > 0.5f;
    p.type      = (equ::BandType) juce::jlimit (0, 5, (int) band.type->load());
    p.frequency = band.frequency->load();
    p.gainDb    = band.gain->load();
    p.q         = band.q->load();
    p.slope     = juce::jlimit (0, 3, (int) band.slope->load());
    return p;
}

double EqualizerUltimateProcessor::getSampleRateForDisplay() const
{
    const auto sr = getSampleRate();
    return sr > 0.0 ? sr : 48000.0;
}

juce::AudioProcessorParameter* EqualizerUltimateProcessor::getBypassParameter() const
{
    return apvts.getParameter (equ::ParamIDs::bypass);
}

//==============================================================================
void EqualizerUltimateProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EqualizerUltimateProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* EqualizerUltimateProcessor::createEditor()
{
    return new EqualizerUltimateEditor (*this);
}

//==============================================================================
#ifdef JucePlugin_Name // only defined when building the plugin itself (not the test app)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EqualizerUltimateProcessor();
}
#endif
