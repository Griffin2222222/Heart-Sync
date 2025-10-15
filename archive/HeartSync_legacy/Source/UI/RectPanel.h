#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "HSTheme.h"

class RectPanel : public juce::Component
{
public:
    RectPanel (juce::Colour border) : borderColour (border) {}
    
    void paint (juce::Graphics& g) override
    {
        g.fillAll (HSTheme::SURFACE_BASE_START);
        auto r = getLocalBounds().toFloat();
        
        // Double border effect matching Python tk.Frame with highlightbackground
        g.setColour (borderColour);
        g.drawRect (r, HSTheme::stroke);
        
        g.setColour (borderColour.withAlpha (0.65f));
        g.drawRect (r.reduced (HSTheme::grid * 0.5f), HSTheme::inner);
    }
    
private:
    juce::Colour borderColour;
};
