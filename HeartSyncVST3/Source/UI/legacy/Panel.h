#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace HeartSync
{

/**
 * @brief Reusable Panel Component
 * 
 * Provides a rounded rect container with:
 * - Title label
 * - Optional caption/subtitle
 * - Configurable stroke color (teal/amber/red variants)
 * - Body area for child components
 */
class Panel : public juce::Component
{
public:
    Panel(const juce::String& title = "",
          const juce::String& caption = "",
          const juce::Colour& strokeColour = Colors::panelStroke);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Set colors
    void setStrokeColour(const juce::Colour& colour);
    void setBodyColour(const juce::Colour& colour);
    
    // Set text
    void setTitle(const juce::String& title);
    void setCaption(const juce::String& caption);
    
    // Get body bounds for placing child components
    juce::Rectangle<int> getBodyBounds() const;
    
private:
    juce::String titleText;
    juce::String captionText;
    juce::Colour strokeColour;
    juce::Colour bodyColour;
    
    static constexpr int headerHeight = 40;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Panel)
};

} // namespace HeartSync
