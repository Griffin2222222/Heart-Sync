#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor_Professional.h"

<<<<<<< HEAD
// HeartSync Professional Color Scheme (matching Python version exactly)
namespace HeartSyncColors
{
    const juce::Colour QUANTUM_TEAL = juce::Colour(0xFF00F5D4);
    const juce::Colour QUANTUM_TEAL_DARK = juce::Colour(0xFF00D4AA);
    const juce::Colour VITAL_RED = juce::Colour(0xFFFF6B6B);
    const juce::Colour MEDICAL_GOLD = juce::Colour(0xFFFFD93D);
    const juce::Colour SURFACE_BLACK = juce::Colour(0xFF000000);
    const juce::Colour SURFACE_PANEL = juce::Colour(0xFF001111);
    const juce::Colour TEXT_PRIMARY = juce::Colour(0xFFD6FFF5);    // Matching Python
    const juce::Colour TEXT_SECONDARY = juce::Colour(0xFF00CCCC);  // Matching Python
    const juce::Colour STATUS_CONNECTED = juce::Colour(0xFF00FF88);
    const juce::Colour STATUS_DISCONNECTED = juce::Colour(0xFF666666);
}

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor_Professional.h"
#include "UI/HSTheme.h"
#include "UI/HSLookAndFeel.h"
#include "UI/MetricRow.h"
#include "UI/ParamBox.h"
#include "UI/ParamToggle.h"

// Use the Professional processor type
using HeartSyncProcessor = HeartSyncVST3AudioProcessor;

class HeartSyncEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HeartSyncEditor(HeartSyncProcessor&);
    ~HeartSyncEditor() override;
=======
class HeartSyncMetricView : public juce::Component
{
public:
    HeartSyncMetricView(const juce::String& titleText, const juce::String& unitText, juce::Colour accentColour);
    void setData(float value, const juce::Array<float>& timeHistory, const juce::Array<float>& valueHistory);
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
};

class HeartSyncVST3AudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer,
                                          private juce::Button::Listener,
                                          private juce::ComboBox::Listener
{
public:
    #pragma once

    #include <juce_audio_processors/juce_audio_processors.h>
    #include "PluginProcessor.h"

    class HeartSyncMetricView : public juce::Component
    {
    public:
        HeartSyncMetricView(const juce::String& titleText, const juce::String& unitText, juce::Colour accentColour);
        void setData(float value, const juce::Array<float>& timeHistory, const juce::Array<float>& valueHistory);
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
        juce::TextButton scanButton{"SCAN"};
        juce::TextButton connectButton{"CONNECT"};
        juce::TextButton disconnectButton{"DISCONNECT"};
        juce::TextButton lockButton{"LOCKED"};

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
    juce::Label titleLabel;
