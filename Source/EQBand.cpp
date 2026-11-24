#include "EQBand.h"

#include <complex>

namespace equ
{

StageSet computeStages (const BandParams& p, double sampleRate) noexcept
{
    using AC = juce::dsp::IIR::ArrayCoefficients<float>;

    StageSet set;
    set.numStages = getNumStages (p);

    const auto nyquistSafe = (float) (sampleRate * 0.49);
    const auto freq        = juce::jlimit (10.0f, nyquistSafe, p.frequency);
    const auto q           = juce::jmax (0.025f, p.q);
    const auto gain        = juce::Decibels::decibelsToGain (p.gainDb);

    switch (p.type)
    {
        case BandType::lowCut:
        case BandType::highCut:
        {
            const bool high = p.type == BandType::highCut;

            if (set.numStages == 1)
            {
                // 12 dB/oct: a single resonant biquad, Q is user controllable
                set.stages[0] = high ? AC::makeLowPass (sampleRate, freq, q)
                                     : AC::makeHighPass (sampleRate, freq, q);
                break;
            }

            // Steeper slopes: Butterworth cascade of order 2N, built from N biquads
            const auto order = 2 * set.numStages;

            for (int k = 0; k < set.numStages; ++k)
            {
                const auto theta   = juce::MathConstants<double>::pi * (2.0 * k + 1.0) / (2.0 * order);
                const auto stageQ  = (float) (1.0 / (2.0 * std::cos (theta)));
                set.stages[(size_t) k] = high ? AC::makeLowPass (sampleRate, freq, stageQ)
                                              : AC::makeHighPass (sampleRate, freq, stageQ);
            }
            break;
        }

        case BandType::lowShelf:
            set.stages[0] = AC::makeLowShelf (sampleRate, freq, q, gain);
            break;

        case BandType::highShelf:
            set.stages[0] = AC::makeHighShelf (sampleRate, freq, q, gain);
            break;

        case BandType::notch:
            set.stages[0] = AC::makeNotch (sampleRate, freq, q);
            break;

        case BandType::bell:
        default:
            set.stages[0] = AC::makePeakFilter (sampleRate, freq, q, gain);
            break;
    }

    return set;
}

double getMagnitude (const StageSet& set, double frequency, double sampleRate) noexcept
{
    const auto w = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    const std::complex<double> z1 = std::polar (1.0, -w);
    const std::complex<double> z2 = z1 * z1;

    double magnitude = 1.0;

    for (int i = 0; i < set.numStages; ++i)
    {
        const auto& c = set.stages[(size_t) i];
        const auto num = (double) c[0] + (double) c[1] * z1 + (double) c[2] * z2;
        const auto den = (double) c[3] + (double) c[4] * z1 + (double) c[5] * z2;
        magnitude *= std::abs (num) / juce::jmax (1.0e-12, std::abs (den));
    }

    return magnitude;
}

} // namespace equ
