#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor_Professional.h"
#include <vector>

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

class HeartSyncEditor : public juce::AudioProcessorEditor, 
                        private juce::Timer
{
public:
    explicit HeartSyncEditor(HeartSyncProcessor&);
    ~HeartSyncEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& event) override;
    
    void wireClientCallbacks();
    void scanForDevices();
    void connectToDevice(const juce::String& deviceAddress);
    void connectSelectedDevice();
    
    // Build control functions for each row
    void buildHROffsetControls(juce::Component& host);
    void buildSmoothControls(juce::Component& host);
    void buildWetDryControls(juce::Component& host);
    void updateSmoothMetrics();
    void refreshDeviceDropdown();
    void updateBiometricDisplay();
    void appendTerminal(const juce::String& message);
    void updateTerminalLabel();
    void updateBluetoothStatus();
    void syncBleControls();
    void setStatusIndicator(juce::Colour colour, const juce::String& text);
    juce::String buildDeviceLabel(const HeartSyncProcessor::DeviceInfo& device) const;
    juce::String buildDeviceDetail(const HeartSyncProcessor::DeviceInfo& device) const;
    juce::String shortenIdentifier(const std::string& identifier) const;

    HeartSyncProcessor& processorRef;
    HSLookAndFeel lnf;

    // Three stacked metric rows
    std::unique_ptr<MetricRow> rowHR;
    std::unique_ptr<MetricRow> rowSmooth;
    std::unique_ptr<MetricRow> rowWetDry;

    // Parameter controls (owned by control build functions)
    std::unique_ptr<ParamBox> hrOffsetBox;
    std::unique_ptr<ParamBox> smoothBox;
    std::unique_ptr<ParamBox> wetDryBox;
    juce::Label smoothMetricsLabel; // α=..., T½=...s, ≈... samples
    std::unique_ptr<ParamToggle> wetDrySourceToggle; // SMOOTHED HR / RAW HR toggle

    // BLE controls in a panel
    juce::TextButton scanBtn{"SCAN"}, connectBtn{"CONNECT"},
                     lockBtn{"LOCK"}, disconnectBtn{"DISCONNECT"};
    juce::ComboBox deviceBox;
    juce::Label deviceLabel;
    juce::Label statusDot, statusLabel;
    juce::Label bleTitle;

    // Device terminal row
    juce::Label terminalTitle;
    juce::TextEditor terminalOutput;
    juce::StringArray terminalLines;

    // Header labels (Python parity)
    juce::Label headerSettingsIcon;   // gear glyph
    juce::Label headerGlyph;          // diamond glyph
    juce::Label headerTitleLeft;      // ❖ HEART SYNC SYSTEM
    juce::Label headerSubtitleLeft;   // Adaptive Audio Bio Technology
    juce::Label headerClockRight;     // YYYY-MM-DD HH:MM:SS
    juce::Label headerStatusRight;    // ◆ SYSTEM OPERATIONAL

    // State variables (matching Python)
    float currentHR = 0.0f;
    float smoothedHR = 0.0f;
    float smoothing = 0.1f;
    int hrOffset = 0;
    int wetDryOffset = 0;
    bool useSmoothedForWetDry = true;
    bool deviceLocked = false;
    bool statusWasConnected = false;
    bool statusWasScanning = false;
    bool statusWasReady = false;
    bool pendingScanRequest = false;
    juce::String statusLastDeviceName;
    std::vector<HeartSyncProcessor::DeviceInfo> availableDevices;
    size_t knownDeviceCount = 0;
    bool bridgeWasConnected = false;
    bool bridgeWasReady = false;
    bool bridgeHintShown = false;
    juce::String lastBridgePermission;
    
    // UX improvement: auto-connect on device selection
    juce::String pendingDeviceSelection;
    juce::uint32 lastDeviceSelectionTime = 0;
    bool isAutoConnecting = false;

#ifdef HEARTSYNC_USE_BRIDGE
    juce::String currentPermissionState{"unknown"};
#if JUCE_DEBUG
    juce::TextButton debugButton;
    int debugStep{0};
#endif
#endif

    bool isInitialized = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncEditor)
};