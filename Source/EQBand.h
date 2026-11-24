#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace equ
{

constexpr int numBands  = 8;
constexpr int maxStages = 4; // 48 dB/oct cut = 4 cascaded biquads

enum class BandType
{
    lowCut = 0,
    lowShelf,
    bell,
    notch,
    highShelf,
    highCut
};

inline juce::StringArray getBandTypeNames()
{
    return { "Low Cut", "Low Shelf", "Bell", "Notch", "High Shelf", "High Cut" };
}

inline juce::StringArray getSlopeNames()
{
    return { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
}

/** Plain-value snapshot of one band's parameters. */
struct BandParams
{
    bool     enabled   = true;
    BandType type      = BandType::bell;
    float    frequency = 1000.0f; // Hz
    float    gainDb    = 0.0f;
    float    q         = 0.707f;
    int      slope     = 0;       // 0..3 -> 12/24/36/48 dB/oct

    bool isCut() const noexcept { return type == BandType::lowCut || type == BandType::highCut; }
};

/** Un-normalised biquad coefficients {b0, b1, b2, a0, a1, a2} for each stage of a band. */
struct StageSet
{
    std::array<std::array<float, 6>, maxStages> stages {};
    int numStages = 0;
};

/** Computes biquad coefficients for a band. Allocation-free, so safe on the audio thread. */
StageSet computeStages (const BandParams& params, double sampleRate) noexcept;

/** Linear magnitude of a set of stages at the given frequency. */
double getMagnitude (const StageSet& set, double frequency, double sampleRate) noexcept;

/** Number of cascaded biquads a band uses (depends on type and slope). */
inline int getNumStages (const BandParams& p) noexcept
{
    return p.isCut() ? juce::jlimit (1, maxStages, p.slope + 1) : 1;
}

} // namespace equ
