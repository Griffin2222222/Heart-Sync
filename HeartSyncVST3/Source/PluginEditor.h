#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class HeartSyncVST3AudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::Timer,
                                          public juce::Button::Listener,
                                          public juce::ComboBox::Listener
{
public:
    HeartSyncVST3AudioProcessorEditor(HeartSyncVST3AudioProcessor&);
    ~HeartSyncVST3AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    
    void timerCallback() override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

private:
    HeartSyncVST3AudioProcessor& audioProcessor;
    
    // Title
    juce::Label titleLabel;
    
    // Bluetooth section
    juce::GroupComponent bluetoothGroup;
    juce::ComboBox deviceList;
    juce::TextButton scanButton;
    juce::TextButton connectButton;
    juce::TextButton disconnectButton;
    juce::Label connectionStatusLabel;
    
    // Heart rate display section
    juce::GroupComponent displayGroup;
    juce::Label rawBpmLabel;
    juce::Label smoothedBpmLabel;
    juce::Label invertedBpmLabel;
    juce::Label rawBpmValue;
    juce::Label smoothedBpmValue;
    juce::Label invertedBpmValue;
    
    // Parameter controls section
    juce::GroupComponent parameterGroup;
    juce::Slider smoothingSlider;
    juce::Label smoothingLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smoothingAttachment;
    
    // OSC section
    juce::GroupComponent oscGroup;
    juce::ToggleButton oscEnabledButton;
    juce::Slider oscPortSlider;
    juce::Label oscPortLabel;
    juce::Label oscStatusLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> oscEnabledAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> oscPortAttachment;
    
    // Visual elements
    float pulseAnimation = 0.0f;
    bool isConnected = false;
    
    // Helper methods
    void refreshDeviceList();
    void updateConnectionStatus();
    void updateOscStatus();
    void paintHeartBeat(juce::Graphics& g, juce::Rectangle<int> area);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncVST3AudioProcessorEditor)
};
