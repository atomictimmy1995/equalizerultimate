#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EQBand.h"

namespace equ::theme
{
    inline const juce::Colour background  { 0xff15171c };
    inline const juce::Colour panel       { 0xff1e2128 };
    inline const juce::Colour panelLight  { 0xff2a2e37 };
    inline const juce::Colour grid        { 0xff2f343e };
    inline const juce::Colour gridText    { 0xff7c8494 };
    inline const juce::Colour text        { 0xffd9dee8 };
    inline const juce::Colour curve       { 0xfff2f4f8 };
    inline const juce::Colour accent      { 0xff4fc3f7 };

    inline juce::Colour bandColour (int band)
    {
        static const juce::Colour colours[numBands] {
            juce::Colour (0xffef5350), juce::Colour (0xffffa726), juce::Colour (0xffffee58),
            juce::Colour (0xff9ccc65), juce::Colour (0xff26c6da), juce::Colour (0xff42a5f5),
            juce::Colour (0xffab47bc), juce::Colour (0xffec407a)
        };
        return colours[juce::jlimit (0, numBands - 1, band)];
    }
}
