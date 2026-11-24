// Headless tests for Equalizer Ultimate: runs the real processor offline and checks its response.

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

#include <iostream>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& description)
    {
        std::cout << (condition ? "[PASS] " : "[FAIL] ") << description << std::endl;

        if (! condition)
            ++failures;
    }

    void setParam (EqualizerUltimateProcessor& p, const juce::String& id, float plainValue)
    {
        auto* param = p.apvts.getParameter (id);
        jassert (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
    }

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    /** Feeds a stereo sine through the processor and returns the gain in dB over the second half. */
    double measureGainDb (EqualizerUltimateProcessor& p, double frequency, double seconds = 1.0)
    {
        p.prepareToPlay (sampleRate, blockSize);

        const auto totalSamples = (int) (seconds * sampleRate);
        double inSum = 0.0, outSum = 0.0;
        double phase = 0.0;
        const auto inc = juce::MathConstants<double>::twoPi * frequency / sampleRate;

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        for (int start = 0; start < totalSamples; start += blockSize)
        {
            const auto n = juce::jmin (blockSize, totalSamples - start);
            buffer.setSize (2, n, false, false, true);

            std::vector<float> input ((size_t) n);

            for (int i = 0; i < n; ++i)
            {
                input[(size_t) i] = (float) (0.25 * std::sin (phase));
                phase += inc;
            }

            for (int ch = 0; ch < 2; ++ch)
                buffer.copyFrom (ch, 0, input.data(), n);

            p.processBlock (buffer, midi);

            if (start >= totalSamples / 2)
            {
                for (int i = 0; i < n; ++i)
                {
                    inSum  += (double) input[(size_t) i] * input[(size_t) i];
                    outSum += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
                }
            }
        }

        p.releaseResources();
        return 10.0 * std::log10 (juce::jmax (1.0e-30, outSum) / inSum);
    }

    bool near (double value, double target, double tolerance)
    {
        return std::abs (value - target) <= tolerance;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // --- Default state is transparent ------------------------------------------------
    {
        EqualizerUltimateProcessor p;

        for (const double f : { 50.0, 1000.0, 12000.0 })
        {
            const auto g = measureGainDb (p, f);
            check (near (g, 0.0, 0.01), "Default settings are transparent at " + juce::String (f) + " Hz ("
                                        + juce::String (g, 4) + " dB)");
        }
    }

    // --- Bell boost -----------------------------------------------------------------
    {
        EqualizerUltimateProcessor p;
        setParam (p, equ::ParamIDs::frequency (3), 1000.0f);
        setParam (p, equ::ParamIDs::gain (3), 12.0f);
        setParam (p, equ::ParamIDs::q (3), 1.0f);

        const auto atCentre = measureGainDb (p, 1000.0);
        const auto farAway  = measureGainDb (p, 100.0); // a decade below the centre
        check (near (atCentre, 12.0, 0.3), "+12 dB bell at 1 kHz boosts 1 kHz by ~12 dB (" + juce::String (atCentre, 2) + " dB)");
        check (farAway < 1.5, "Bell has little effect a decade away (" + juce::String (farAway, 2) + " dB)");
    }

    // --- Notch ------------------------------------------------------------------------
    {
        EqualizerUltimateProcessor p;
        setParam (p, equ::ParamIDs::type (4), (float) equ::BandType::notch);
        setParam (p, equ::ParamIDs::frequency (4), 2000.0f);
        setParam (p, equ::ParamIDs::q (4), 2.0f);

        const auto g = measureGainDb (p, 2000.0);
        check (g < -40.0, "Notch at 2 kHz removes 2 kHz (" + juce::String (g, 1) + " dB)");
    }

    // --- High cut ---------------------------------------------------------------------
    {
        EqualizerUltimateProcessor p;
        setParam (p, equ::ParamIDs::enabled (7), 1.0f);
        setParam (p, equ::ParamIDs::frequency (7), 1000.0f);
        setParam (p, equ::ParamIDs::slope (7), 3.0f); // 48 dB/oct

        const auto pass = measureGainDb (p, 100.0);
        const auto stop = measureGainDb (p, 10000.0);
        check (near (pass, 0.0, 0.1), "48 dB/oct high cut at 1 kHz passes 100 Hz (" + juce::String (pass, 3) + " dB)");
        check (stop < -80.0, "48 dB/oct high cut at 1 kHz strongly attenuates 10 kHz (" + juce::String (stop, 1) + " dB)");

        const auto corner = measureGainDb (p, 1000.0);
        check (near (corner, -3.01, 0.3), "Butterworth high cut is -3 dB at the corner (" + juce::String (corner, 2) + " dB)");
    }

    // --- Low cut slopes ---------------------------------------------------------------
    {
        double previous = 0.0;

        for (int slope = 0; slope < 4; ++slope)
        {
            EqualizerUltimateProcessor p;
            setParam (p, equ::ParamIDs::enabled (0), 1.0f);
            setParam (p, equ::ParamIDs::frequency (0), 400.0f);
            setParam (p, equ::ParamIDs::slope (0), (float) slope);

            const auto g = measureGainDb (p, 100.0, 2.0);
            const auto expected = -12.0 * (slope + 1) * 2.0; // two octaves below the corner
            check (near (g, expected, 3.0), "Low cut " + juce::String (12 * (slope + 1)) + " dB/oct: two octaves below = "
                                            + juce::String (g, 1) + " dB (expected ~" + juce::String (expected, 0) + ")");
            check (slope == 0 || g < previous, "Steeper slope attenuates more");
            previous = g;
        }
    }

    // --- Shelves ----------------------------------------------------------------------
    {
        EqualizerUltimateProcessor p;
        setParam (p, equ::ParamIDs::type (1), (float) equ::BandType::lowShelf);
        setParam (p, equ::ParamIDs::frequency (1), 200.0f);
        setParam (p, equ::ParamIDs::gain (1), -6.0f);
        setParam (p, equ::ParamIDs::type (6), (float) equ::BandType::highShelf);
        setParam (p, equ::ParamIDs::frequency (6), 5000.0f);
        setParam (p, equ::ParamIDs::gain (6), 6.0f);

        const auto low  = measureGainDb (p, 30.0, 2.0);
        const auto mid  = measureGainDb (p, 1000.0);
        const auto high = measureGainDb (p, 18000.0);
        check (near (low, -6.0, 0.5),  "Low shelf -6 dB (" + juce::String (low, 2) + " dB)");
        check (near (mid, 0.0, 0.5),   "Mid band untouched between shelves (" + juce::String (mid, 2) + " dB)");
        check (near (high, 6.0, 0.5),  "High shelf +6 dB (" + juce::String (high, 2) + " dB)");
    }

    // --- Output gain & bypass ---------------------------------------------------------
    {
        EqualizerUltimateProcessor p;
        setParam (p, equ::ParamIDs::output, -6.0f);
        const auto g = measureGainDb (p, 1000.0);
        check (near (g, -6.0, 0.05), "Output gain -6 dB (" + juce::String (g, 3) + " dB)");

        setParam (p, equ::ParamIDs::gain (3), 12.0f);
        setParam (p, equ::ParamIDs::bypass, 1.0f);
        const auto bypassed = measureGainDb (p, 500.0);
        check (near (bypassed, 0.0, 1.0e-6), "Bypass passes audio unchanged (" + juce::String (bypassed, 6) + " dB)");
    }

    // --- Analytic response used by the UI matches the processor -----------------------
    {
        equ::BandParams bp;
        bp.type = equ::BandType::bell;
        bp.frequency = 3000.0f;
        bp.gainDb = -9.0f;
        bp.q = 2.0f;
        const auto set = equ::computeStages (bp, sampleRate);
        const auto db = juce::Decibels::gainToDecibels (equ::getMagnitude (set, 3000.0, sampleRate));
        check (near (db, -9.0, 0.01), "Display magnitude of -9 dB bell at centre (" + juce::String (db, 3) + " dB)");

        EqualizerUltimateProcessor p;
        setParam (p, equ::ParamIDs::frequency (5), 3000.0f);
        setParam (p, equ::ParamIDs::gain (5), -9.0f);
        setParam (p, equ::ParamIDs::q (5), 2.0f);
        const auto measured = measureGainDb (p, 4000.0);
        const auto predicted = juce::Decibels::gainToDecibels (equ::getMagnitude (set, 4000.0, sampleRate));
        check (near (measured, predicted, 0.1), "Processor matches displayed curve off-centre (measured "
                                               + juce::String (measured, 2) + ", display " + juce::String (predicted, 2) + ")");
    }

    // --- Parameter automation while playing stays finite & reaches target -----------------
    {
        EqualizerUltimateProcessor p;
        p.prepareToPlay (sampleRate, blockSize);
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        juce::Random rng (1234);
        bool finite = true;

        for (int block = 0; block < 400; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (ch, i, rng.nextFloat() * 0.5f - 0.25f);

            const auto band = block % equ::numBands;
            setParam (p, equ::ParamIDs::enabled (band), 1.0f);
            setParam (p, equ::ParamIDs::type (band), (float) (rng.nextInt (6)));
            setParam (p, equ::ParamIDs::frequency (band), 20.0f * std::pow (1000.0f, rng.nextFloat()));
            setParam (p, equ::ParamIDs::gain (band), rng.nextFloat() * 48.0f - 24.0f);
            setParam (p, equ::ParamIDs::q (band), 0.1f * std::pow (180.0f, rng.nextFloat()));
            setParam (p, equ::ParamIDs::slope (band), (float) rng.nextInt (4));

            p.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    finite = finite && std::isfinite (buffer.getSample (ch, i));
        }

        check (finite, "Random parameter automation produces finite output");
    }

    // --- Mono support -----------------------------------------------------------------
    {
        EqualizerUltimateProcessor p;
        juce::AudioProcessor::BusesLayout mono, surround;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        surround.inputBuses.add (juce::AudioChannelSet::create5point1());
        surround.outputBuses.add (juce::AudioChannelSet::create5point1());
        check (p.checkBusesLayoutSupported (mono), "Mono layout supported");
        check (! p.checkBusesLayoutSupported (surround), "5.1 layout rejected");
    }

    // --- State save / restore -----------------------------------------------------------
    {
        EqualizerUltimateProcessor a;
        setParam (a, equ::ParamIDs::frequency (2), 345.0f);
        setParam (a, equ::ParamIDs::gain (2), -7.5f);
        setParam (a, equ::ParamIDs::type (2), (float) equ::BandType::highShelf);
        setParam (a, equ::ParamIDs::enabled (7), 1.0f);

        juce::MemoryBlock state;
        a.getStateInformation (state);

        EqualizerUltimateProcessor b;
        b.setStateInformation (state.getData(), (int) state.getSize());

        const auto pa = a.getBandParams (2);
        const auto pb = b.getBandParams (2);
        check (near (pb.frequency, pa.frequency, 0.01) && near (pb.gainDb, pa.gainDb, 0.01) && pb.type == pa.type
               && b.getBandParams (7).enabled,
               "State save/restore round-trips");
    }

    // --- Editor can be created and destroyed -----------------------------------------------
    {
        EqualizerUltimateProcessor p;

        // A non-flat curve so the screenshot shows something
        setParam (p, equ::ParamIDs::enabled (0), 1.0f);
        setParam (p, equ::ParamIDs::frequency (0), 35.0f);
        setParam (p, equ::ParamIDs::type (1), (float) equ::BandType::lowShelf);
        setParam (p, equ::ParamIDs::frequency (1), 120.0f);
        setParam (p, equ::ParamIDs::gain (1), 4.0f);
        setParam (p, equ::ParamIDs::gain (2), -5.0f);
        setParam (p, equ::ParamIDs::frequency (2), 320.0f);
        setParam (p, equ::ParamIDs::q (2), 2.5f);
        setParam (p, equ::ParamIDs::gain (4), 3.0f);
        setParam (p, equ::ParamIDs::frequency (4), 1800.0f);
        setParam (p, equ::ParamIDs::type (5), (float) equ::BandType::notch);
        setParam (p, equ::ParamIDs::frequency (5), 4200.0f);
        setParam (p, equ::ParamIDs::q (5), 6.0f);
        setParam (p, equ::ParamIDs::type (6), (float) equ::BandType::highShelf);
        setParam (p, equ::ParamIDs::gain (6), 5.0f);

        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        check (editor != nullptr && editor->getWidth() > 0, "Editor constructs");

        // Render it offscreen as a smoke test of the paint code
        juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        juce::Graphics g (image);
        editor->paintEntireComponent (g, true);

        if (auto outPath = juce::SystemStats::getEnvironmentVariable ("EQU_SCREENSHOT", {}); outPath.isNotEmpty())
        {
            juce::File file (outPath);
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (image, stream);
        }

        check (true, "Editor paints offscreen");
    }

    std::cout << std::endl << (failures == 0 ? "All tests passed." : juce::String (failures) + " test(s) FAILED.") << std::endl;
    return failures == 0 ? 0 : 1;
}
