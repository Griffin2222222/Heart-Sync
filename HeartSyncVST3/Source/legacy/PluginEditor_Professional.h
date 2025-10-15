#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor_Professional.h"
#include <vector>
#include <memory>

//==============================================================================
// HeartSync Professional - Python-to-VST3 Translation
// Universal Audio Quality Interface Architecture  
//==============================================================================

// Quantum color scheme (matching Python design tokens)
namespace QuantumColors
{
    const juce::Colour SURFACE_BASE_START = juce::Colour(0xFF000000);
    const juce::Colour SURFACE_PANEL_LIGHT = juce::Colour(0xFF001111);
    const juce::Colour TEXT_PRIMARY = juce::Colour(0xFFD6FFF5);
    const juce::Colour TEXT_SECONDARY = juce::Colour(0xFF00CCCC);
    const juce::Colour TEAL = juce::Colour(0xFF00F5D4);           // Quantum teal
    const juce::Colour VITAL_RED = juce::Colour(0xFFFF6B6B);      // Medical red
    const juce::Colour VITAL_GOLD = juce::Colour(0xFFFFD93D);     // Medical gold
    const juce::Colour STATUS_CONNECTED = juce::Colour(0xFF00FF88);
    const juce::Colour STATUS_DISCONNECTED = juce::Colour(0xFF666666);
}

//==============================================================================
// Professional UI Components (recreating Python interface)
//==============================================================================

class VitalSignsDisplay : public juce::Component
{
public:
    VitalSignsDisplay(const juce::String& title, const juce::String& unit, juce::Colour color);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setValue(float value);
    void setVSTParameter(std::function<void(float)> callback);
    void mouseDown(const juce::MouseEvent& event) override;
    
private:
    juce::String title;
    juce::String unit; 
    juce::Colour primaryColor;
    float currentValue = 0.0f;
    std::function<void(float)> vstParameterCallback;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VitalSignsDisplay)
};

class QuantumWaveform : public juce::Component
{
public:
    QuantumWaveform(const juce::String& title, juce::Colour color);
    void paint(juce::Graphics& g) override;
    void updateData(const std::vector<float>& data);
    
private:
    juce::String title;
    juce::Colour waveColor;
    std::vector<float> displayData;
    juce::CriticalSection dataLock;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuantumWaveform)
};

class QuantumParameterBox : public juce::Component
{
public:
    QuantumParameterBox(const juce::String& title, float minVal, float maxVal, float initialVal, const juce::String& unit);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    
    std::function<void(float)> onValueChanged;
    
private:
    juce::String title;
    juce::String unit;
    float minValue, maxValue, currentValue;
    float dragStartValue;
    juce::Point<int> dragStartPosition;
    
    juce::String getDisplayText() const;
    void setValue(float newValue);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuantumParameterBox)
};

class BluetoothDevicePanel : public juce::Component
{
public:
    BluetoothDevicePanel();
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    std::function<void()> onScanDevices;
    std::function<void(const juce::String&)> onConnectDevice;
    
private:
    std::unique_ptr<juce::TextButton> scanButton;
    std::unique_ptr<juce::TextButton> connectButton;
    std::unique_ptr<juce::Label> statusLabel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BluetoothDevicePanel)
};

class QuantumConsole : public juce::Component
{
public:
    QuantumConsole();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void addMessage(const juce::String& message);
    
private:
    std::unique_ptr<juce::TextEditor> consoleText;
    juce::StringArray messageBuffer;
    static const int MAX_MESSAGES = 100;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuantumConsole)
};

//==============================================================================
// Main Editor Class
//==============================================================================

class HeartSyncVST3Editor : public juce::AudioProcessorEditor,
                            public juce::Timer
{
public:
    HeartSyncVST3Editor(HeartSyncVST3AudioProcessor&);
    ~HeartSyncVST3Editor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    HeartSyncVST3AudioProcessor& audioProcessor;
    
    // Professional interface recreation
    void createProfessionalInterface();
    void updateDataBuffer(std::vector<float>& buffer, float newValue);
    void startBluetoothScan();
    void connectToBluetoothDevice(const juce::String& address);
    
    // Header components (matching Python layout)
    std::unique_ptr<juce::Label> titleLabel;
    std::unique_ptr<juce::Label> subtitleLabel;
    std::unique_ptr<juce::Label> timeLabel;
    std::unique_ptr<juce::Label> statusLabel;
    
    // Vital signs displays (matching Python)
    std::unique_ptr<VitalSignsDisplay> rawHrDisplay;
    std::unique_ptr<VitalSignsDisplay> smoothedHrDisplay;
    std::unique_ptr<VitalSignsDisplay> wetDryDisplay;
    
    // Real-time waveforms (matching Python plots)
    std::unique_ptr<QuantumWaveform> rawHrWaveform;
    std::unique_ptr<QuantumWaveform> smoothedHrWaveform;
    std::unique_ptr<QuantumWaveform> wetDryWaveform;
    
    // Parameter controls (TouchDesigner-style)
    std::unique_ptr<QuantumParameterBox> hrOffsetControl;
    std::unique_ptr<QuantumParameterBox> smoothingControl;
    std::unique_ptr<QuantumParameterBox> wetDryOffsetControl;
    
    // Bluetooth device panel
    std::unique_ptr<BluetoothDevicePanel> bluetoothPanel;
    
    // Professional console (matching Python terminal)
    std::unique_ptr<QuantumConsole> systemConsole;
    
    // Data buffers (matching Python deque maxlen=300)
    std::vector<float> hrDataBuffer;
    std::vector<float> smoothedHrDataBuffer;
    std::vector<float> wetDryDataBuffer;
    
    // Live state (matching Python variables)
    float currentHr = 0;
    float smoothedHr = 0;
    float wetDryRatio = 0;
    juce::String connectionStatus = "DISCONNECTED";
    
    // Parameters (matching Python defaults)
    float smoothingFactor = 0.1f;
    float hrOffset = 0.0f;
    float wetDryOffset = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncVST3Editor)
};