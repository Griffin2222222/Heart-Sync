#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/HSTheme.h"
#include "UI/HSLookAndFeel.h"
#include "UI/ValueTile.h"
#include "UI/WaveGraph.h"

/**
 * @brief HeartSync Plugin Editor with 1:1 Python UI Parity
 * 
 * Replicates Python "Heart Sync System" UI EXACTLY:
 * - Square panels with double borders (tk.Frame style)
 * - Three metric tiles with big centered digits
 * - Bottom titles with units
 * - Waveform graph with grid
 * - BLE control bar
 * - Device monitor terminal
 * - Section headers with underlines
 */
class HeartSyncEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HeartSyncEditor(HeartSyncProcessor&);
    ~HeartSyncEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void buildLayout();
    void wireClientCallbacks();
    void scanForDevices();
    void connectToDevice(const juce::String& deviceAddress);
    
    HeartSyncProcessor& processorRef;
    
    // Custom Look and Feel (square, flat, no rounding)
    HSLookAndFeel lnf;
    
    // --- Metric Tiles (matching Python's 3 vital tiles) ---
    ValueTile hrTile   { HSTheme::VITAL_HEART_RATE, "HEART RATE", "BPM" };
    ValueTile smTile   { HSTheme::VITAL_SMOOTHED,   "SMOOTHED HR", "BPM" };
    ValueTile wdTile   { HSTheme::VITAL_WET_DRY,    "WET/DRY RATIO", "" };
    
    // --- Waveform (matching Python graph) ---
    WaveGraph hrGraph  { HSTheme::VITAL_SMOOTHED };
    
    // --- BLE Controls (matching Python BLE bar) ---
    juce::TextButton  scanBtn { "SCAN" };
    juce::TextButton  connectBtn { "CONNECT" };
    juce::TextButton  lockBtn { "LOCK" };
    juce::TextButton  disconnectBtn { "DISCONNECT" };
    juce::ComboBox    deviceBox;
    juce::Label       statusDot;
    juce::Label       statusLabel;
    
    // --- Device Monitor (matching Python terminal) ---
    juce::Label deviceTerminal;
    
    // --- State (matching Python heart rate processing) ---
    float hrOffset = 0.0f;
    float smoothing = 0.1f;
    float smoothedHR = 0.0f;
    float wetDryOffset = 0.0f;
    bool  useSmoothedForWetDry = true;
    
    #ifdef HEARTSYNC_USE_BRIDGE
    juce::String currentPermissionState{"unknown"};
    
    #if JUCE_DEBUG
    juce::TextButton debugButton;
    int debugStep{0};
    #endif
    #endif
    
    bool isInitialized = false;
    bool deviceLocked = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncEditor)
};
