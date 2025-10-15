#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace HeartSync
{

/**
 * @brief Custom LookAndFeel for HeartSync
 * 
 * Implements the quantum teal medical aesthetic:
 * - Rounded rect borders with neon strokes
 * - Ring-style knobs with thin arc indicators
 * - Dark panels with subtle glow effects
 */
class HeartSyncLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HeartSyncLookAndFeel();
    
    // Apply to a component and all children
    void applyTo(juce::Component& component);
    
    // Override drawing methods
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                         juce::Slider& slider) override;
    
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                            const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;
    
    void drawComboBox(juce::Graphics& g, int width, int height,
                     bool isButtonDown, int buttonX, int buttonY,
                     int buttonW, int buttonH, juce::ComboBox& box) override;
    
    void drawLabel(juce::Graphics& g, juce::Label& label) override;
    
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float minSliderPos, float maxSliderPos,
                         const juce::Slider::SliderStyle style, juce::Slider& slider) override;

private:
    void drawNeonRing(juce::Graphics& g, juce::Rectangle<float> bounds,
                     float startAngle, float endAngle, float thickness,
                     const juce::Colour& colour, bool drawTrack = true);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncLookAndFeel)
};

} // namespace HeartSync
