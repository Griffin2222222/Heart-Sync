#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "HSTheme.h"

/**
 * ParamToggle
 * - Matches Python's input source toggle sized like ParamBox value box
 * - Click to toggle between two labels with per-state colours
 * - Ridge-like double border and mono bold text, centred
 */
class ParamToggle : public juce::Component
{
public:
    ParamToggle(const juce::String& onText, const juce::String& offText)
        : textOn(onText), textOff(offText)
    {
        setSize(120, 48);
        setInterceptsMouseClicks(true, true);
    }

    void setState(bool on)
    {
        isOn = on;
        repaint();
        if (onChange) onChange(isOn);
    }

    bool getState() const { return isOn; }

    void setColours(juce::Colour onBg, juce::Colour onFg,
                    juce::Colour offBg, juce::Colour offFg,
                    juce::Colour border)
    {
        onBG = onBg; onFG = onFg; offBG = offBg; offFG = offFg; borderColour = border;
        repaint();
    }

    std::function<void(bool)> onChange;

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        // Background
        g.setColour(HSTheme::SURFACE_BASE_START);
        g.fillRect(r);

        // Inner box area (like ParamBox value area)
        auto box = r.reduced(0);

        // Outer darker border
        g.setColour(borderColour.darker(0.6f));
        g.drawRect(box, 1.0f);
        // Inner bright border
        g.setColour(borderColour);
        g.drawRect(box.reduced(2), 1.0f);

        // Fill with state colour
        g.setColour(isOn ? onBG : offBG);
        g.fillRect(box.reduced(3));

        // Text
        g.setFont(HSTheme::mono(13.0f, true));
        g.setColour(isOn ? onFG : offFG);
        g.drawText(isOn ? textOn : textOff, box.reduced(4), juce::Justification::centred);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown()) return; // only on release
        isOn = !isOn;
        repaint();
        if (onChange) onChange(isOn);
    }

private:
    bool isOn { true };
    juce::String textOn, textOff;
    juce::Colour onBG { juce::Colour(0xff004d44) }, onFG { HSTheme::VITAL_SMOOTHED };
    juce::Colour offBG { juce::Colour(0xff8b0000) }, offFG { HSTheme::VITAL_HEART_RATE };
    juce::Colour borderColour { HSTheme::ACCENT_TEAL };
};
