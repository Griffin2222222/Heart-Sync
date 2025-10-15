#pragma once

#include <JuceHeader.h>
#include "Theme.h"

namespace HeartSync
{

/**
 * @brief Metric Panel matching Python HeartSyncProfessional structure
 * 
 * 3-column layout: VALUE | CONTROLS | WAVEFORM
 * - Value: Large centered number with title at bottom
 * - Controls: Parameter knobs/sliders
 * - Waveform: Graph/visualization area
 */
class MetricPanel : public juce::Component
{
public:
    MetricPanel(const juce::String& title, const juce::Colour& borderColour);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Set the large display value
    void setValue(const juce::String& value);
    
    // Get bounds for each section
    juce::Rectangle<int> getValueBounds() const;
    juce::Rectangle<int> getControlBounds() const;
    juce::Rectangle<int> getWaveformBounds() const;
    
private:
    juce::String titleText;
    juce::Colour borderColour;
    juce::Label valueLabel;
    juce::Label titleLabel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetricPanel)
};

} // namespace HeartSync
