#include "HeartSyncLookAndFeel.h"

namespace HeartSync
{

HeartSyncLookAndFeel::HeartSyncLookAndFeel()
{
    // Set default colors
    setColour(juce::ResizableWindow::backgroundColourId, Colors::surfaceBase);
    setColour(juce::TextButton::buttonColourId, Colors::surfacePanel);
    setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
    setColour(juce::TextButton::buttonOnColourId, Colors::accentTeal);
    setColour(juce::ComboBox::backgroundColourId, Colors::surfacePanel);
    setColour(juce::ComboBox::textColourId, Colors::textPrimary);
    setColour(juce::ComboBox::outlineColourId, Colors::panelStroke);
    setColour(juce::ComboBox::arrowColourId, Colors::accentTeal);
    setColour(juce::Label::textColourId, Colors::textPrimary);
    setColour(juce::Slider::textBoxTextColourId, Colors::textPrimary);
    setColour(juce::Slider::textBoxBackgroundColourId, Colors::surfacePanel);
    setColour(juce::Slider::textBoxOutlineColourId, Colors::panelStroke);
    setColour(juce::Slider::rotarySliderFillColourId, Colors::accentTeal);
    setColour(juce::Slider::rotarySliderOutlineColourId, Colors::textMuted);
    setColour(juce::Slider::thumbColourId, Colors::accentTeal);
}

void HeartSyncLookAndFeel::applyTo(juce::Component& component)
{
    component.setLookAndFeel(this);
    
    // Recursively apply to all children
    for (auto* child : component.getChildren())
    {
        if (child != nullptr)
            applyTo(*child);
    }
}

void HeartSyncLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto rx = centreX - radius;
    auto ry = centreY - radius;
    auto rw = radius * 2.0f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    
    auto ringBounds = juce::Rectangle<float>(rx, ry, rw, rw);
    auto trackColour = slider.findColour(juce::Slider::rotarySliderOutlineColourId);
    auto fillColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
    
    // Draw track (muted background arc)
    drawNeonRing(g, ringBounds, rotaryStartAngle, rotaryEndAngle, 3.0f, trackColour, true);
    
    // Draw filled arc (active portion)
    drawNeonRing(g, ringBounds, rotaryStartAngle, angle, 3.0f, fillColour, false);
    
    // Draw pointer line
    juce::Path pointer;
    auto pointerLength = radius * 0.5f;
    auto pointerThickness = 2.0f;
    pointer.addRectangle(-pointerThickness * 0.5f, -radius + 8.0f, pointerThickness, pointerLength);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(fillColour);
    g.fillPath(pointer);
}

void HeartSyncLookAndFeel::drawNeonRing(juce::Graphics& g, juce::Rectangle<float> bounds,
                                        float startAngle, float endAngle, float thickness,
                                        const juce::Colour& colour, bool drawTrack)
{
    juce::Path arc;
    arc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                     bounds.getWidth() / 2.0f, bounds.getHeight() / 2.0f,
                     0.0f, startAngle, endAngle, true);
    
    juce::PathStrokeType strokeType(thickness, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded);
    
    if (drawTrack)
    {
        // Muted track
        g.setColour(colour.withAlpha(0.3f));
        g.strokePath(arc, strokeType);
    }
    else
    {
        // Bright active arc with glow
        juce::PathStrokeType glowStroke(thickness + 2.0f, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded);
        g.setColour(colour.withAlpha(0.6f));
        g.strokePath(arc, glowStroke);
        
        g.setColour(colour);
        g.strokePath(arc, strokeType);
    }
}

void HeartSyncLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    auto baseColour = backgroundColour;
    
    if (shouldDrawButtonAsDown)
        baseColour = baseColour.brighter(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = baseColour.brighter(0.1f);
    
    // Fill with base color
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, Metrics::borderRadius);
    
    // Draw neon stroke
    auto strokeColour = button.getToggleState() ? Colors::accentTeal : Colors::panelStroke;
    if (!button.isEnabled())
        strokeColour = Colors::textMuted;
    
    g.setColour(strokeColour);
    g.drawRoundedRectangle(bounds, Metrics::borderRadius, Metrics::strokeWidth);
    
    // Subtle inner glow when enabled
    if (button.isEnabled() && shouldDrawButtonAsHighlighted)
    {
        g.setColour(strokeColour.withAlpha(0.1f));
        g.fillRoundedRectangle(bounds.reduced(Metrics::strokeWidth), Metrics::borderRadius - 1.0f);
    }
}

void HeartSyncLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                        bool isButtonDown, int buttonX, int buttonY,
                                        int buttonW, int buttonH, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(1.0f);
    
    // Background
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, Metrics::borderRadius);
    
    // Stroke
    auto strokeColour = box.findColour(juce::ComboBox::outlineColourId);
    if (!box.isEnabled())
        strokeColour = Colors::textMuted;
    
    g.setColour(strokeColour);
    g.drawRoundedRectangle(bounds, Metrics::borderRadius, Metrics::strokeWidth);
    
    // Arrow
    auto arrowBounds = juce::Rectangle<int>(buttonX, buttonY, buttonW, buttonH).toFloat();
    juce::Path arrow;
    arrow.addTriangle(arrowBounds.getCentreX() - 4.0f, arrowBounds.getCentreY() - 2.0f,
                     arrowBounds.getCentreX() + 4.0f, arrowBounds.getCentreY() - 2.0f,
                     arrowBounds.getCentreX(), arrowBounds.getCentreY() + 3.0f);
    
    g.setColour(box.findColour(juce::ComboBox::arrowColourId)
                   .withAlpha(box.isEnabled() ? 1.0f : 0.4f));
    g.fillPath(arrow);
}

void HeartSyncLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));
    
    if (!label.isBeingEdited())
    {
        auto alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font font(getLabelFont(label));
        
        g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
        g.setFont(font);
        
        auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
        
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                        juce::jmax(1, (int)(textArea.getHeight() / font.getHeight())),
                        label.getMinimumHorizontalScale());
        
        g.setColour(label.findColour(juce::Label::outlineColourId).withMultipliedAlpha(alpha));
    }
    else if (label.isEnabled())
    {
        g.setColour(label.findColour(juce::Label::outlineColourId));
    }
    
    g.drawRect(label.getLocalBounds());
}

void HeartSyncLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float minSliderPos, float maxSliderPos,
                                            const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    // Use default for now, can customize later if needed
    LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
}

} // namespace HeartSync
