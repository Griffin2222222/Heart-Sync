#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "Core/HeartRateParams.h"
#include "UI/HSTheme.h"
#include "UI/HSLookAndFeel.h"
#include "UI/MetricRow.h"
#include "UI/ParamBox.h"
#include "UI/ParamToggle.h"
#include "UI/RectPanel.h"

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
    void buildHROffsetControls(juce::Component& host);
    void buildSmoothControls(juce::Component& host);
    void buildWetDryControls(juce::Component& host);
    void updateSmoothingMetricsLabel(float smoothingFactor);
    void syncParameterControls();
    float getParameterValue(const juce::String& paramId) const;
    void setParameterValue(const juce::String& paramId, float newValue);
    int getChoiceParameterIndex(const juce::String& paramId) const;
    void setChoiceParameterIndex(const juce::String& paramId, int newIndex);

    HeartSyncVST3AudioProcessor& processor;
    HSLookAndFeel lookAndFeel;

    // Header
    juce::Label headerTitleLabel;
    juce::Label headerSubtitleLabel;
    juce::Label headerTimeLabel;
    juce::Label headerStatusLabel;
    juce::Label valuesHeading;
    juce::Label controlsHeading;
    juce::Label waveformHeading;

    // Metric rows
    std::unique_ptr<MetricRow> hrRow;
    std::unique_ptr<MetricRow> smoothRow;
    std::unique_ptr<MetricRow> wetDryRow;

    std::unique_ptr<ParamBox> hrOffsetBox;
    std::unique_ptr<ParamBox> smoothingBox;
    std::unique_ptr<ParamBox> wetDryOffsetBox;
    std::unique_ptr<ParamToggle> wetDrySourceToggle;
    juce::Label smoothingMetricsLabel;

    // BLE and device panels
    juce::ComboBox deviceCombo;
    juce::TextButton scanButton{ "SCAN" };
    juce::TextButton connectButton{ "CONNECT" };
    juce::TextButton disconnectButton{ "DISCONNECT" };
    juce::TextButton lockButton{ "LOCKED" };
    std::unique_ptr<RectPanel> blePanel;
    juce::Label bleTitleLabel;
    juce::Label bleStatusLabel;
    juce::Label bleSignalLabel;
    juce::Label bleLatencyLabel;
    juce::Label blePacketsLabel;
    juce::Label bleDeviceLabel;
    juce::Label statusIndicator;

    std::unique_ptr<RectPanel> deviceMonitorPanel;
    juce::Label deviceMonitorTitle;
    juce::Label deviceStatusLabel;
    juce::TextEditor consoleView;
    juce::Label oscSectionLabel;
    juce::ToggleButton oscEnableToggle { "Enable" };
    juce::Label oscHostLabel;
    juce::TextEditor oscHostEditor;
    juce::Label oscPortLabel;
    juce::Slider oscPortSlider;
    juce::Label oscStatusLabel;

    HeartSyncVST3AudioProcessor::TelemetrySnapshot snapshot;
    juce::StringArray lastConsole;
    juce::StringArray deviceIds;
    bool updatingParameterControls = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lockAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> oscEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> oscPortAttachment;

    HeartSyncVST3AudioProcessor::TelemetrySnapshot lastSnapshot;
    juce::Component* hrControlsHost = nullptr;
    juce::Component* smoothingControlsHost = nullptr;
    juce::Component* wetDryControlsHost = nullptr;
    int deviceRefreshCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncVST3AudioProcessorEditor)
};
