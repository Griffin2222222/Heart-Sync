#include "Panel.h"

namespace HeartSync
{

Panel::Panel(const juce::String& title, const juce::String& caption, const juce::Colour& strokeColour)
    : titleText(title)
    , captionText(caption)
    , strokeColour(strokeColour)
    , bodyColour(Colors::surfacePanel)
{
}

void Panel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Fill panel background (pure black like Python app)
    g.setColour(Colors::surfaceBase);
    g.fillRect(bounds);
    
    // DOUBLE BORDER EFFECT (like Python's highlightbackground + highlightthickness)
    // Outer border (thick, 2px)
    g.setColour(strokeColour.darker(0.4f));  // Darker outer border
    g.drawRect(bounds, 2.0f);
    
    // Inner border (bright, 2px, inset by 4px)
    g.setColour(strokeColour);
    g.drawRect(bounds.reduced(4.0f), 2.0f);
    
    // Draw title at BOTTOM (like Python app)
    if (titleText.isNotEmpty())
    {
        auto textBounds = bounds.reduced(Metrics::padding, Metrics::padding);
        textBounds = textBounds.removeFromBottom(20);  // Bottom 20px for title
        
        g.setColour(Colors::textPrimary);
        g.setFont(Typography::getUIFont(Typography::sizeSmall, true));
        g.drawText(titleText, textBounds.toNearestInt(),
                  juce::Justification::centred, true);
    }
}

void Panel::resized()
{
    // Child components should use getBodyBounds()
}

void Panel::setStrokeColour(const juce::Colour& colour)
{
    strokeColour = colour;
    repaint();
}

void Panel::setBodyColour(const juce::Colour& colour)
{
    bodyColour = colour;
    repaint();
}

void Panel::setTitle(const juce::String& title)
{
    titleText = title;
    repaint();
}

void Panel::setCaption(const juce::String& caption)
{
    captionText = caption;
    repaint();
}

juce::Rectangle<int> Panel::getBodyBounds() const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(titleText.isNotEmpty() ? headerHeight : 0);
    return bounds.reduced(Metrics::padding);
}

} // namespace HeartSync
