#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

class HeartSyncMetricView : public juce::Component
{
public:
    HeartSyncMetricView(const juce::String& titleText,
                        const juce::String& unitText,
                        juce::Colour accentColour);

    void setData(float value,
                 const juce::Array<float>& timeHistory,
                 const juce::Array<float>& valueHistory);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::String title;
    juce::String unit;
    juce::Colour accent;
    float currentValue = 0.0f;
    juce::Array<float> times;
    juce::Array<float> values;
    juce::Label valueLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncMetricView)
};

class HeartSyncVST3AudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer,
                                          private juce::Button::Listener,
                                          private juce::ComboBox::Listener
{
public:
    explicit HeartSyncVST3AudioProcessorEditor(HeartSyncVST3AudioProcessor&);
    ~HeartSyncVST3AudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    void layoutPanels();
    void refreshTelemetry();
    void refreshDeviceList(bool force = false);
    void updateLockState();

    HeartSyncVST3AudioProcessor& processor;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label timeLabel;
    juce::Label systemStatusLabel;

    juce::ComboBox deviceCombo;
    juce::TextButton scanButton{ "SCAN" };
    juce::TextButton connectButton{ "CONNECT" };
    juce::TextButton disconnectButton{ "DISCONNECT" };
    juce::TextButton lockButton{ "LOCKED" };

    juce::Label deviceInfoLabel;
    juce::Label packetInfoLabel;
    juce::Label latencyInfoLabel;

    HeartSyncMetricView heartRateView;
    HeartSyncMetricView smoothedView;
    HeartSyncMetricView wetDryView;

    juce::Slider hrOffsetSlider;
    juce::Slider smoothingSlider;
    juce::Slider wetDryOffsetSlider;
    juce::ComboBox wetDrySourceCombo;

    juce::Label smoothingMetricsLabel;

    juce::TextEditor consoleView;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hrOffsetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smoothingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetDryAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> wetDrySourceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lockAttachment;

    HeartSyncVST3AudioProcessor::TelemetrySnapshot snapshot;
    juce::StringArray lastConsole;
    juce::StringArray deviceIds;
    bool devicesDirty = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncVST3AudioProcessorEditor)
};
