#include "PluginEditor.h"
#include <ctime>

HeartSyncVST3Editor::HeartSyncVST3Editor(HeartSyncVST3AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Minimal safe constructor for debugging
    setSize(400, 300);
    
    // Create a simple label - most basic UI component
    try {
        titleLabel = std::make_unique<juce::Label>("title", "HeartSync by Conscious Audio");
        titleLabel->setFont(juce::Font("Arial", 16.0f, juce::Font::bold));
        titleLabel->setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*titleLabel);
        
        // Start a timer for basic updates
        startTimer(1000);
    }
    catch (const std::exception& e) {
        DBG("Error in constructor: " << e.what());
    }
}

HeartSyncVST3Editor::~HeartSyncVST3Editor()
{
    stopTimer();
}

void HeartSyncVST3Editor::paint(juce::Graphics& g)
{
    // Simple gradient background
    g.fillAll(juce::Colour(0xFF001111));
    
    // Simple border
    g.setColour(juce::Colour(0xFF00F5D4));
    g.drawRect(getLocalBounds(), 2);
}

void HeartSyncVST3Editor::resized()
{
    if (titleLabel)
        titleLabel->setBounds(10, 10, getWidth() - 20, 30);
}

void HeartSyncVST3Editor::timerCallback()
{
    // Basic timer - just to test it works
    repaint();
}

// Minimal implementations for the custom components
HeartSyncVST3Editor::ValueDisplayComponent::ValueDisplayComponent(const juce::String& title, const juce::String& unit, juce::Colour color)
    : title(title), unit(unit), color(color)
{
}

void HeartSyncVST3Editor::ValueDisplayComponent::paint(juce::Graphics& g)
{
    g.fillAll(color.withAlpha(0.1f));
    g.setColour(color);
    g.drawRect(getLocalBounds());
    g.drawText(title, getLocalBounds(), juce::Justification::centred);
}

void HeartSyncVST3Editor::ValueDisplayComponent::resized()
{
}

void HeartSyncVST3Editor::ValueDisplayComponent::setValue(float value)
{
    currentValue = value;
    repaint();
}

void HeartSyncVST3Editor::ValueDisplayComponent::setClickCallback(std::function<void()> callback)
{
    clickCallback = callback;
}

void HeartSyncVST3Editor::ValueDisplayComponent::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    if (clickCallback)
        clickCallback();
}

void HeartSyncVST3Editor::ValueDisplayComponent::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    isHovered = true;
    repaint();
}

void HeartSyncVST3Editor::ValueDisplayComponent::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    isHovered = false;
    repaint();
}

HeartSyncVST3Editor::WaveformComponent::WaveformComponent(juce::Colour color)
    : waveformColor(color)
{
}

void HeartSyncVST3Editor::WaveformComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(waveformColor);
    g.drawRect(getLocalBounds());
}

void HeartSyncVST3Editor::WaveformComponent::updateData(const std::deque<float>& data)
{
    juce::ignoreUnused(data);
    // Minimal implementation
}

HeartSyncVST3Editor::MedicalButton::MedicalButton(const juce::String& text, juce::Colour color)
    : juce::TextButton(text), buttonColor(color)
{
}

void HeartSyncVST3Editor::MedicalButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    
    g.fillAll(buttonColor.withAlpha(0.2f));
    g.setColour(buttonColor);
    g.drawRect(getLocalBounds());
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred);
}