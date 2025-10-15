#include "MetricPanel.h"

namespace HeartSync
{

MetricPanel::MetricPanel(const juce::String& title, const juce::Colour& borderColour)
    : titleText(title)
    , borderColour(borderColour)
{
    // Large value display (centered, monospaced)
    valueLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 64.0f, juce::Font::bold));
    valueLabel.setColour(juce::Label::textColourId, borderColour);
    valueLabel.setJustificationType(juce::Justification::centred);
    valueLabel.setText("0", juce::dontSendNotification);
    addAndMakeVisible(valueLabel);
    
    // Title at bottom (small, centered)
    titleLabel.setFont(Typography::getUIFont(Typography::sizeSmall));
    titleLabel.setColour(juce::Label::textColourId, Colors::textPrimary);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setText(title, juce::dontSendNotification);
    addAndMakeVisible(titleLabel);
}

void MetricPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Fill background
    g.setColour(Colors::surfaceBase);
    g.fillRect(bounds);
    
    // DOUBLE BORDER (match Python tk.Frame highlightbackground effect)
    // Outer darker border
    g.setColour(borderColour.darker(0.4f));
    g.drawRect(bounds, 2.0f);
    
    // Inner bright border (inset)
    g.setColour(borderColour);
    g.drawRect(bounds.reduced(4.0f), 2.0f);
    
    // Draw vertical dividers for 3-column layout
    auto b = bounds.reduced(6.0f);
    float col1Width = 200.0f;  // VALUE column
    float col2Width = 200.0f;  // CONTROLS column
    // Remaining width is WAVEFORM column
    
    g.setColour(borderColour.withAlpha(0.3f));
    g.drawLine(b.getX() + col1Width, b.getY(), 
               b.getX() + col1Width, b.getBottom(), 1.0f);
    g.drawLine(b.getX() + col1Width + col2Width, b.getY(),
               b.getX() + col1Width + col2Width, b.getBottom(), 1.0f);
}

void MetricPanel::resized()
{
    auto bounds = getLocalBounds().reduced(12);
    
    // VALUE column (left 200px)
    auto valueColumn = bounds.removeFromLeft(200);
    
    // Center the value vertically, title at bottom
    auto titleArea = valueColumn.removeFromBottom(24);
    titleLabel.setBounds(titleArea);
    
    valueLabel.setBounds(valueColumn);
}

void MetricPanel::setValue(const juce::String& value)
{
    valueLabel.setText(value, juce::dontSendNotification);
}

juce::Rectangle<int> MetricPanel::getValueBounds() const
{
    return getLocalBounds().reduced(12).removeFromLeft(200);
}

juce::Rectangle<int> MetricPanel::getControlBounds() const
{
    auto bounds = getLocalBounds().reduced(12);
    bounds.removeFromLeft(200);  // Skip value column
    return bounds.removeFromLeft(200);
}

juce::Rectangle<int> MetricPanel::getWaveformBounds() const
{
    auto bounds = getLocalBounds().reduced(12);
    bounds.removeFromLeft(400);  // Skip value + control columns
    return bounds;
}

} // namespace HeartSync
