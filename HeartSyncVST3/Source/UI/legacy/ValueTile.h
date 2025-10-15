#pragma once
#include "RectPanel.h"

class ValueTile : public RectPanel
{
public:
    ValueTile (juce::Colour border, juce::String title, juce::String unit)
      : RectPanel (border), title (std::move (title)), unit (std::move (unit)) {}

    void setValueText (juce::String s) { value = std::move (s); repaint(); }

    void paint (juce::Graphics& g) override
    {
        RectPanel::paint (g);
        auto r = getLocalBounds().reduced (HSTheme::grid);
        auto footer = r.removeFromBottom (HSTheme::grid * 2);

        // Big centered digits (matching Python's 64px monospaced font)
        g.setColour (findColour (juce::Label::textColourId, true));
        g.setFont (HSTheme::bigDigits());
        g.drawFittedText (value.isEmpty() ? "0" : value,
                          r.reduced (HSTheme::grid), juce::Justification::centred, 1);

        // Bottom title (matching Python: title at bottom with unit)
        g.setColour (HSTheme::TEXT_SECONDARY);
        g.setFont (HSTheme::caption());
        auto t = title + (unit.isNotEmpty() ? " [" + unit + "]" : juce::String());
        g.drawText (t, footer, juce::Justification::centred);
    }

private:
    juce::String title, unit, value;
};
