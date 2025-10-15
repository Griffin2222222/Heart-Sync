#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "HSTheme.h"

struct HSLookAndFeel : juce::LookAndFeel_V4
{
    HSLookAndFeel()
    {
        setColour (juce::Label::textColourId, HSTheme::TEXT_PRIMARY);
        setColour (juce::PopupMenu::textColourId, HSTheme::TEXT_PRIMARY);
        setColour (juce::ComboBox::outlineColourId, HSTheme::ACCENT_TEAL);
        setColour (juce::ComboBox::backgroundColourId, HSTheme::SURFACE_BASE_START);
        setColour (juce::TextButton::buttonColourId, HSTheme::SURFACE_BASE_START);
        setColour (juce::TextButton::textColourOnId, HSTheme::ACCENT_TEAL);
        setColour (juce::TextButton::textColourOffId, HSTheme::ACCENT_TEAL_DARK);
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        if (std::abs (f.getHeight() - HSTheme::bigDigits().getHeight()) < 0.1f)
            return juce::Typeface::createSystemTypefaceFor (nullptr, 0); // system mono is fine
        return LookAndFeel_V4::getTypefaceForFont (f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                               const juce::Colour& /*bg*/, bool /*over*/, bool /*down*/) override
    {
        auto r = b.getLocalBounds().toFloat();
        g.setColour (HSTheme::SURFACE_BASE_START);
        g.fillRect (r);

        auto border = b.isEnabled() ? HSTheme::ACCENT_TEAL
                                    : HSTheme::ACCENT_TEAL_DARK.withAlpha (0.35f);
        g.setColour (border); g.drawRect (r, HSTheme::stroke);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool /*down*/,
                       int /*x*/, int /*y*/, int /*bx*/, int /*by*/, juce::ComboBox& c) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (HSTheme::SURFACE_BASE_START); g.fillRect (r);
        g.setColour (HSTheme::ACCENT_TEAL); g.drawRect (r, HSTheme::stroke);

        // caret
        juce::Path p; p.startNewSubPath (w - 18.0f,  h/2.0f - 4.0f);
        p.lineTo          (w - 10.0f,  h/2.0f - 4.0f);
        p.lineTo          (w - 14.0f,  h/2.0f + 4.0f);
        p.closeSubPath();
        g.setColour (HSTheme::ACCENT_TEAL); g.fillPath (p);
        c.setColour (juce::ComboBox::textColourId, HSTheme::TEXT_PRIMARY);
    }
};
