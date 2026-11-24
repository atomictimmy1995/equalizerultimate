#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "EQBand.h"

namespace equ
{

/** Parameter IDs. Bands are 1-based in IDs and names ("b1_freq" ... "b8_freq"). */
namespace ParamIDs
{
    inline juce::String enabled   (int band) { return "b" + juce::String (band + 1) + "_on"; }
    inline juce::String type      (int band) { return "b" + juce::String (band + 1) + "_type"; }
    inline juce::String frequency (int band) { return "b" + juce::String (band + 1) + "_freq"; }
    inline juce::String gain      (int band) { return "b" + juce::String (band + 1) + "_gain"; }
    inline juce::String q         (int band) { return "b" + juce::String (band + 1) + "_q"; }
    inline juce::String slope     (int band) { return "b" + juce::String (band + 1) + "_slope"; }

    inline const juce::String output = "output";
    inline const juce::String bypass = "bypass";
}

constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float maxGainDb    = 24.0f;

} // namespace equ

class EqualizerUltimateProcessor final : public juce::AudioProcessor
{
public:
    EqualizerUltimateProcessor();
    ~EqualizerUltimateProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Equalizer Ultimate"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                              { return 1; }
    int getCurrentProgram() override                           { return 0; }
    void setCurrentProgram (int) override                      {}
    const juce::String getProgramName (int) override           { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==============================================================================
    /** Reads the current (unsmoothed) parameter values for a band. Safe from any thread. */
    equ::BandParams getBandParams (int band) const;

    /** Sample rate to use for UI response calculations. */
    double getSampleRateForDisplay() const;

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    static constexpr int maxChannels  = 2;
    static constexpr int subBlockSize = 32; // coefficient update interval while parameters glide

    struct Band
    {
        std::atomic<float>* enabled   = nullptr;
        std::atomic<float>* type      = nullptr;
        std::atomic<float>* frequency = nullptr;
        std::atomic<float>* gain      = nullptr;
        std::atomic<float>* q         = nullptr;
        std::atomic<float>* slope     = nullptr;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedFreq;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         smoothedGain;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedQ;

        std::array<juce::dsp::IIR::Coefficients<float>::Ptr, equ::maxStages> coefficients;
        std::array<std::array<juce::dsp::IIR::Filter<float>, equ::maxStages>, maxChannels> filters;

        bool         active       = false;
        equ::BandType activeType  = equ::BandType::bell;
        int          activeStages = 0;
        bool         dirty        = true;
    };

    void updateBandTargets (Band& band);
    void updateBandCoefficients (Band& band, int numSamplesToSkip);

    std::array<Band, equ::numBands> bands;

    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    juce::dsp::Gain<float> outputGain;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqualizerUltimateProcessor)
};
