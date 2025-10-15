# 3-ROW STACKED LAYOUT IMPLEMENTATION GUIDE

## STATUS: Core UI Components Complete ✅

### COMPLETED:
1. ✅ Updated HSTheme.h - headerH=80, monoLarge() function
2. ✅ Created ParamBox.h - Interactive drag-to-adjust parameter controls
3. ✅ Updated WaveGraph.h - ECG grid with major/minor lines, LAST/PEAK/MIN display
4. ✅ Created MetricRow.h - Reusable row component with Value|Controls|Waveform layout

### NEXT STEPS (To Complete 3-Row Layout):

#### 1. Update PluginEditor.h

Replace current header with:

```cpp
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/HSTheme.h"
#include "UI/HSLookAndFeel.h"
#include "UI/MetricRow.h"
#include "UI/ParamBox.h"

class HeartSyncEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HeartSyncEditor(HeartSyncProcessor&);
    ~HeartSyncEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void wireClientCallbacks();
    void scanForDevices();
    void connectToDevice(const juce::String& deviceAddress);
    
    // Build control functions for each row
    void buildHROffsetControls(juce::Component& host);
    void buildSmoothControls(juce::Component& host);
    void buildWetDryControls(juce::Component& host);
    
    HeartSyncProcessor& processorRef;
    HSLookAndFeel lnf;
    
    // Three stacked metric rows
    std::unique_ptr<MetricRow> rowHR;
    std::unique_ptr<MetricRow> rowSmooth;
    std::unique_ptr<MetricRow> rowWetDry;
    
    // Parameter controls
    std::unique_ptr<ParamBox> hrOffsetBox;
    std::unique_ptr<ParamBox> smoothBox;
    std::unique_ptr<ParamBox> wetDryBox;
    juce::Label smoothMetricsLabel; // α=..., T½=...s, ≈... samples
    juce::TextButton wetDrySourceToggle; // SMOOTHED HR / RAW HR toggle
    
    // BLE controls
    juce::TextButton scanBtn{"SCAN"}, connectBtn{"CONNECT"},
                     lockBtn{"LOCK"}, disconnectBtn{"DISCONNECT"};
    juce::ComboBox deviceBox;
    juce::Label statusDot, statusLabel;
    juce::Label bleTitle; // "BLUETOOTH LE CONNECTIVITY"
    
    // Device terminal
    juce::Label terminalTitle; // "DEVICE STATUS MONITOR"
    juce::Label terminalLabel; // "[ WAITING ] DEVICE: --- ..."
    
    // State variables (matching Python)
    float currentHR = 0.0f;
    float smoothedHR = 0.0f;
    float smoothing = 0.1f;
    int hrOffset = 0;
    int wetDryOffset = 0;
    bool useSmoothedForWetDry = true;
    bool deviceLocked = false;
    
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
```

#### 2. Key PluginEditor.cpp Changes

**Constructor:**
```cpp
setSize(1180, 740);
setLookAndFeel(&lnf);

// Create three metric rows
rowHR = std::make_unique<MetricRow>("HEART RATE", "BPM", HSTheme::VITAL_HEART_RATE,
    [this](auto& host) { buildHROffsetControls(host); });
    
rowSmooth = std::make_unique<MetricRow>("SMOOTHED HR", "BPM", HSTheme::VITAL_SMOOTHED,
    [this](auto& host) { buildSmoothControls(host); });
    
rowWetDry = std::make_unique<MetricRow>("WET/DRY RATIO", "", HSTheme::VITAL_WET_DRY,
    [this](auto& host) { buildWetDryControls(host); });

addAndMakeVisible(*rowHR);
addAndMakeVisible(*rowSmooth);
addAndMakeVisible(*rowWetDry);

// Add BLE controls...
// Add device terminal...
```

**buildHROffsetControls:**
```cpp
void HeartSyncEditor::buildHROffsetControls(juce::Component& host)
{
    hrOffsetBox = std::make_unique<ParamBox>("HR OFFSET", HSTheme::VITAL_HEART_RATE, 
                                             "BPM", -100.0f, 100.0f, 1.0f);
    hrOffsetBox->setValue(0.0f);
    hrOffsetBox->onChange = [this](float v) {
        hrOffset = (int)v;
        // Re-process current HR
    };
    host.addAndMakeVisible(*hrOffsetBox);
    hrOffsetBox->setBounds(HSTheme::grid, HSTheme::grid, 120, 72);
}
```

**buildSmoothControls:**
```cpp
void HeartSyncEditor::buildSmoothControls(juce::Component& host)
{
    smoothBox = std::make_unique<ParamBox>("SMOOTH", HSTheme::VITAL_SMOOTHED, 
                                           "x", 0.1f, 10.0f, 0.1f);
    smoothBox->setValue(0.1f);
    smoothBox->onChange = [this](float v) {
        smoothing = v;
        updateSmoothMetrics();
    };
    host.addAndMakeVisible(*smoothBox);
    smoothBox->setBounds(HSTheme::grid, HSTheme::grid, 120, 72);
    
    // Smoothing metrics label below
    smoothMetricsLabel.setFont(HSTheme::mono(9.0f, false));
    smoothMetricsLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    host.addAndMakeVisible(smoothMetricsLabel);
    smoothMetricsLabel.setBounds(HSTheme::grid, 88, 180, 60);
    updateSmoothMetrics();
}

void HeartSyncEditor::updateSmoothMetrics()
{
    const float alpha = 1.0f / (1.0f + smoothing);
    const float halfLifeSamples = std::log(0.5f) / std::log(1.0f - alpha);
    const float halfLifeSeconds = halfLifeSamples * 0.025f; // 40Hz = 0.025s per sample
    const int effectiveWindow = (int)std::round(halfLifeSamples * 5.0f);
    
    juce::String metrics = juce::String::formatted("α=%.3f\nT½=%.2fs\n≈%d samples", 
                                                   alpha, halfLifeSeconds, effectiveWindow);
    smoothMetricsLabel.setText(metrics, juce::dontSendNotification);
}
```

**buildWetDryControls:**
```cpp
void HeartSyncEditor::buildWetDryControls(juce::Component& host)
{
    // Input source toggle button
    wetDrySourceToggle.setButtonText("SMOOTHED HR");
    wetDrySourceToggle.setColour(juce::TextButton::buttonColourId, HSTheme::VITAL_SMOOTHED);
    wetDrySourceToggle.onClick = [this]() {
        useSmoothedForWetDry = !useSmoothedForWetDry;
        if (useSmoothedForWetDry) {
            wetDrySourceToggle.setButtonText("SMOOTHED HR");
            wetDrySourceToggle.setColour(juce::TextButton::buttonColourId, HSTheme::VITAL_SMOOTHED);
        } else {
            wetDrySourceToggle.setButtonText("RAW HR");
            wetDrySourceToggle.setColour(juce::TextButton::buttonColourId, HSTheme::VITAL_HEART_RATE.darker(0.3f));
        }
    };
    host.addAndMakeVisible(wetDrySourceToggle);
    wetDrySourceToggle.setBounds(HSTheme::grid, HSTheme::grid, 120, 32);
    
    // Wet/Dry offset box below
    wetDryBox = std::make_unique<ParamBox>("WET/DRY", HSTheme::VITAL_WET_DRY, 
                                           "%", -100.0f, 100.0f, 1.0f);
    wetDryBox->setValue(0.0f);
    wetDryBox->onChange = [this](float v) {
        wetDryOffset = (int)v;
        // Re-calculate wet/dry
    };
    host.addAndMakeVisible(*wetDryBox);
    wetDryBox->setBounds(HSTheme::grid, 52, 120, 72);
}
```

**paint():**
```cpp
// Pure black background
g.fillAll(HSTheme::SURFACE_BASE_START);

auto r = getLocalBounds().toFloat();

// Header bar (80px)
auto header = r.removeFromTop((float)HSTheme::headerH);
g.setColour(HSTheme::SURFACE_PANEL_LIGHT);
g.fillRect(header);

g.setColour(HSTheme::ACCENT_TEAL);
g.setFont(HSTheme::heading());
g.drawText("❖ HEART SYNC SYSTEM", header.reduced(HSTheme::grid), juce::Justification::centredLeft);

g.setFont(HSTheme::caption());
g.drawText("Adaptive Audio Bio Technology", header.reduced(HSTheme::grid), juce::Justification::centredRight);

// Section headings row (40px)
auto sectionRow = r.removeFromTop(40.0f);
auto col = sectionRow.removeFromLeft(200.0f);
g.setColour(HSTheme::ACCENT_TEAL);
g.setFont(HSTheme::label());
g.drawText("VALUES", col, juce::Justification::centredLeft);
g.setColour(HSTheme::ACCENT_TEAL_DARK);
g.fillRect(col.removeFromBottom(2.0f));

col = sectionRow.removeFromLeft(200.0f);
g.setColour(HSTheme::ACCENT_TEAL);
g.drawText("CONTROLS", col, juce::Justification::centredLeft);
g.setColour(HSTheme::ACCENT_TEAL_DARK);
g.fillRect(col.removeFromBottom(2.0f));

col = sectionRow;
g.setColour(HSTheme::ACCENT_TEAL);
g.drawText("WAVEFORM", col, juce::Justification::centredLeft);
g.setColour(HSTheme::ACCENT_TEAL_DARK);
g.fillRect(col.removeFromBottom(2.0f));
```

**resized():**
```cpp
auto r = getLocalBounds();

// Skip header (80px) and section headings (40px)
r.removeFromTop(HSTheme::headerH);
r.removeFromTop(40);

// Calculate remaining space
const int bleBarHeight = 96;
const int terminalHeight = 72;
const int rowsAvailable = r.getHeight() - bleBarHeight - terminalHeight;
const int rowHeight = rowsAvailable / 3;

// Three metric rows
rowHR->setBounds(r.removeFromTop(rowHeight));
rowSmooth->setBounds(r.removeFromTop(rowHeight));
rowWetDry->setBounds(r.removeFromTop(rowHeight));

// BLE bar
auto bleBar = r.removeFromTop(bleBarHeight).reduced(HSTheme::grid);
// Layout scanBtn, connectBtn, lockBtn, disconnectBtn, deviceBox, statusDot, statusLabel...

// Device terminal
auto terminal = r.removeFromTop(terminalHeight).reduced(HSTheme::grid);
// Layout terminalTitle, terminalLabel...
```

**HR Data Callback:**
```cpp
client.onHeartRate = [this](float bpm, juce::Array<float> rr)
{
    // Apply offset
    const float incoming = bpm + hrOffset;
    
    // Alpha smoothing
    const float alpha = 1.0f / (1.0f + juce::jmax(0.1f, smoothing));
    if (smoothedHR <= 0.0f)
        smoothedHR = incoming;
    else
        smoothedHR = alpha * incoming + (1.0f - alpha) * smoothedHR;
    
    // Update value labels
    rowHR->setValueText(juce::String(juce::roundToInt(incoming)));
    rowSmooth->setValueText(juce::String(juce::roundToInt(smoothedHR)));
    
    // Wet/dry baseline calculation (10..90 range)
    const float src = useSmoothedForWetDry ? smoothedHR : incoming;
    float baseline = juce::jlimit(0.0f, 1.0f, (src - 40.0f) / 140.0f);
    float wetDry = juce::jlimit(1.0f, 100.0f, 10.0f + baseline * 80.0f + (float)wetDryOffset);
    rowWetDry->setValueText(juce::String(juce::roundToInt(wetDry)));
    
    // Push to graphs
    rowHR->getGraph().push(incoming);
    rowSmooth->getGraph().push(smoothedHR);
    rowWetDry->getGraph().push(wetDry);
};
```

### BUILD INSTRUCTIONS:

```bash
cd /Users/gmc/Documents/GitHub/Heart-Sync/HeartSync/build
cmake --build . --config Release
```

### TESTING CHECKLIST:

- [ ] Three stacked rows visible
- [ ] Each row has Value (left), Controls (middle), Waveform (right)
- [ ] Double borders on all panels
- [ ] Titles at bottom: "HEART RATE [BPM]", "SMOOTHED HR [BPM]", "WET/DRY RATIO"
- [ ] HR OFFSET control shows "0 BPM"
- [ ] SMOOTH control shows "0.1x"
- [ ] Smoothing metrics display: α=..., T½=...s, ≈... samples
- [ ] WET/DRY source toggle switches between "SMOOTHED HR" and "RAW HR"
- [ ] WET/DRY control shows "0%"
- [ ] ECG grid visible in waveforms
- [ ] LAST/PEAK/MIN display in waveform headers
- [ ] BLE bar under rows
- [ ] Device terminal at bottom
- [ ] Drag ParamBox values up/down works
- [ ] Double-click ParamBox opens text editor
- [ ] Live HR updates all three rows correctly

### KNOWN WORKING COMPONENTS:
- ✅ HSTheme.h - All colors and fonts defined
- ✅ HSLookAndFeel.h - Square flat buttons
- ✅ RectPanel.h - Double borders
- ✅ ParamBox.h - Interactive parameter controls
- ✅ WaveGraph.h - ECG grid with stats
- ✅ MetricRow.h - 3-column row layout
- ✅ HeartSyncBLEClient - onHeartRate callback exists

### FILES TO IMPLEMENT:
1. Update PluginEditor.h with new structure (see above)
2. Rewrite PluginEditor.cpp with 3-row layout (see above)
3. No CMakeLists.txt changes needed (all header-only)

The implementation is straightforward - just follow the code snippets above to wire everything together!
