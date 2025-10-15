#include "PluginEditor.h"

HeartSyncEditor::HeartSyncEditor(HeartSyncProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // CRITICAL: setSize() must be FIRST (Ableton Live 12 requirement)
    setSize(1180, 740);
    
    // Apply custom look and feel (square, flat, no rounding)
    setLookAndFeel(&lnf);
    
    // Add metric tiles (matching Python's 3 vital value tiles)
    addAndMakeVisible(hrTile);
    addAndMakeVisible(smTile);
    addAndMakeVisible(wdTile);
    
    // Add waveform graph (matching Python's live graph)
    addAndMakeVisible(hrGraph);
    
    // Add BLE controls (matching Python's BLE bar)
    for (auto* b : { &scanBtn, &connectBtn, &lockBtn, &disconnectBtn })
        addAndMakeVisible(*b);
    
    addAndMakeVisible(deviceBox);
    addAndMakeVisible(statusDot);
    addAndMakeVisible(statusLabel);
    
    // Add device monitor terminal (matching Python's device terminal)
    addAndMakeVisible(deviceTerminal);
    
    // Configure status dot (connection indicator)
    statusDot.setColour(juce::Label::textColourId, juce::Colours::dimgrey);
    statusLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    deviceTerminal.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    deviceTerminal.setJustificationType(juce::Justification::centredLeft);
    
    // Configure BLE control button actions
    scanBtn.onClick = [this]() { scanForDevices(); };
    
    connectBtn.onClick = [this]() {
        if (deviceBox.getSelectedId() > 0)
        {
            auto deviceId = deviceBox.getItemText(deviceBox.getSelectedItemIndex());
            connectToDevice(deviceId);
        }
    };
    
    disconnectBtn.onClick = [this]() {
        #ifdef HEARTSYNC_USE_BRIDGE
        processorRef.getBLEClient().disconnectDevice();
        #endif
    };
    
    lockBtn.onClick = [this]() {
        deviceLocked = !deviceLocked;
        lockBtn.setButtonText(deviceLocked ? "UNLOCK" : "LOCK");
        deviceBox.setEnabled(!deviceLocked);
        connectBtn.setEnabled(!deviceLocked);
    };
    
    // Defer complex initialization and callback wiring
    juce::MessageManager::callAsync([this]() {
        buildLayout();
        wireClientCallbacks();
        isInitialized = true;
    });
    
    // Start timer for UI updates (40 Hz matching Python)
    startTimerHz(40);
}

HeartSyncEditor::~HeartSyncEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void HeartSyncEditor::buildLayout()
{
    resized();
}

void HeartSyncEditor::wireClientCallbacks()
{
    #ifdef HEARTSYNC_USE_BRIDGE
    auto& client = processorRef.getBLEClient();
    
    // Wire heart rate data to UI updates (matching Python's HR processing)
    client.onHeartRate = [this](float bpm, juce::Array<float> rr)
    {
        // Apply offset & smoothing exactly like Python
        const float incoming = bpm + hrOffset;
        const float alpha = 1.0f / (1.0f + juce::jmax(0.1f, smoothing));
        
        if (smoothedHR <= 0.0f) 
            smoothedHR = incoming;
        else 
            smoothedHR = alpha * incoming + (1.0f - alpha) * smoothedHR;
        
        // Update tiles with formatted values
        hrTile.setValueText(juce::String(juce::roundToInt(incoming)));
        smTile.setValueText(juce::String(juce::roundToInt(smoothedHR)));
        
        // Calculate wet/dry ratio (matching Python's baseline algorithm)
        const float source = useSmoothedForWetDry ? smoothedHR : incoming;
        float baseline = juce::jlimit(0.0f, 1.0f, (source - 40.0f) / 140.0f);
        float wetDry = 10.0f + baseline * 80.0f; // 10..90 baseline
        wetDry = juce::jlimit(1.0f, 100.0f, wetDry + wetDryOffset);
        wdTile.setValueText(juce::String(juce::roundToInt(wetDry)));
        
        // Push to waveform graph
        hrGraph.push(incoming);
    };
    
    // Wire device found callback
    client.onDeviceFound = [this](const HeartSyncBLEClient::DeviceInfo& device)
    {
        juce::MessageManager::callAsync([this, device]() {
            deviceBox.addItem(device.getDisplayName(), deviceBox.getNumItems() + 1);
        });
    };
    
    // Wire connection state callbacks
    client.onConnected = [this](const juce::String& deviceId)
    {
        juce::MessageManager::callAsync([this, deviceId]() {
            statusDot.setColour(juce::Label::textColourId, HSTheme::STATUS_CONNECTED);
            statusLabel.setText("Connected", juce::dontSendNotification);
            deviceTerminal.setText("Connected to: " + deviceId, juce::dontSendNotification);
        });
    };
    
    client.onDisconnected = [this](const juce::String& reason)
    {
        juce::MessageManager::callAsync([this, reason]() {
            statusDot.setColour(juce::Label::textColourId, juce::Colours::dimgrey);
            statusLabel.setText("Disconnected", juce::dontSendNotification);
            deviceTerminal.setText("Disconnected: " + reason, juce::dontSendNotification);
        });
    };
    
    client.onPermissionChanged = [this](const juce::String& state)
    {
        juce::MessageManager::callAsync([this, state]() {
            currentPermissionState = state;
            if (state == "authorized")
            {
                deviceTerminal.setText("Bluetooth authorized", juce::dontSendNotification);
            }
            else if (state == "denied")
            {
                deviceTerminal.setText("Bluetooth permission denied - check System Settings", juce::dontSendNotification);
            }
        });
    };
    
    #if JUCE_DEBUG
    // Debug button for UI testing
    debugButton.setButtonText("DEBUG");
    addAndMakeVisible(debugButton);
    debugButton.onClick = [this]() {
        auto& client = processorRef.getBLEClient();
        switch (debugStep++ % 4)
        {
            case 0:
                client.__debugInjectPermission("authorized");
                break;
            case 1:
                client.__debugInjectDevice("AA:BB:CC:DD:EE:FF", "Test HR Monitor", -60);
                break;
            case 2:
                client.__debugInjectConnected("AA:BB:CC:DD:EE:FF");
                break;
            case 3:
                for (int i = 0; i < 50; ++i)
                    client.__debugInjectHr(60 + (rand() % 40));
                break;
        }
    };
    #endif
    #endif
}

void HeartSyncEditor::paint(juce::Graphics& g)
{
    // Pure black background (matching Python)
    g.fillAll(HSTheme::SURFACE_BASE_START);
    
    auto r = getLocalBounds().toFloat();
    
    // Header bar (matching Python's top header)
    auto header = r.removeFromTop((float)HSTheme::headerH);
    g.setColour(HSTheme::SURFACE_PANEL_LIGHT);
    g.fillRect(header);
    
    // Header text with diamond symbol
    g.setColour(HSTheme::ACCENT_TEAL);
    g.setFont(HSTheme::heading());
    g.drawText("❖ HEART SYNC SYSTEM", header.reduced(HSTheme::grid / 2), juce::Justification::centredLeft);
    
    // Section headings with underlines (VALUES | CONTROLS | WAVEFORM)
    auto sectionRow = r.removeFromTop((float)HSTheme::grid * 2.0f);
    
    // VALUES heading
    auto col = sectionRow.removeFromLeft(200.0f);
    g.setColour(HSTheme::ACCENT_TEAL);
    g.setFont(HSTheme::label());
    g.drawText("VALUES", col, juce::Justification::centredLeft);
    g.setColour(HSTheme::ACCENT_TEAL_DARK);
    g.fillRect(col.removeFromBottom(2.0f));
    
    // CONTROLS heading
    col = sectionRow.removeFromLeft(200.0f);
    g.setColour(HSTheme::ACCENT_TEAL);
    g.drawText("CONTROLS", col, juce::Justification::centredLeft);
    g.setColour(HSTheme::ACCENT_TEAL_DARK);
    g.fillRect(col.removeFromBottom(2.0f));
    
    // WAVEFORM heading (expands to fill remaining width)
    col = sectionRow;
    g.setColour(HSTheme::ACCENT_TEAL);
    g.drawText("WAVEFORM", col, juce::Justification::centredLeft);
    g.setColour(HSTheme::ACCENT_TEAL_DARK);
    g.fillRect(col.removeFromBottom(2.0f));
}

void HeartSyncEditor::resized()
{
    auto r = getLocalBounds().reduced(HSTheme::grid);
    
    // Skip header area
    r.removeFromTop(HSTheme::headerH);
    
    // Skip section header row
    r.removeFromTop(HSTheme::grid * 2);
    
    // Metric row (three tiles + waveform)
    auto rowH = juce::jmin(200, r.getHeight() / 3);
    auto row1 = r.removeFromTop(rowH);
    
    // Column widths (matching Python Grid.COL_* constants)
    auto cWValues   = 200;
    auto cWControls = 200;
    
    // Layout three tiles horizontally
    auto colA = row1.removeFromLeft(cWValues);
    colA.removeFromRight(HSTheme::grid);
    hrTile.setBounds(colA);
    
    auto colB = row1.removeFromLeft(cWControls);
    colB.removeFromRight(HSTheme::grid);
    smTile.setBounds(colB);
    
    auto colC = row1.removeFromLeft(cWValues);
    colC.removeFromRight(HSTheme::grid);
    wdTile.setBounds(colC);
    
    // Waveform spans remaining width
    hrGraph.setBounds(row1);
    
    // Controls band (for future slider/knob placement)
    auto controlsBand = r.removeFromTop(HSTheme::grid * 8);
    
    // BLE controls band (matching Python's BLE bar)
    auto bleBand = r.removeFromTop(HSTheme::grid * 6);
    
    // Device monitor terminal (matching Python's device terminal)
    auto monitorBand = r;
    
    // Layout BLE controls horizontally
    {
        auto x = bleBand.removeFromLeft(80);
        scanBtn.setBounds(x.reduced(2));
        
        x = bleBand.removeFromLeft(100);
        connectBtn.setBounds(x.reduced(2));
        
        x = bleBand.removeFromLeft(80);
        lockBtn.setBounds(x.reduced(2));
        
        x = bleBand.removeFromLeft(120);
        disconnectBtn.setBounds(x.reduced(2));
        
        deviceBox.setBounds(bleBand.removeFromLeft(juce::jmax(220, bleBand.getWidth() / 3)).reduced(2));
        statusDot.setBounds(bleBand.removeFromLeft(24).reduced(2));
        statusLabel.setBounds(bleBand.reduced(2));
    }
    
    // Device terminal at bottom
    deviceTerminal.setBounds(monitorBand.reduced(HSTheme::grid));
    
    #if JUCE_DEBUG && defined(HEARTSYNC_USE_BRIDGE)
    // Debug button in top-right corner
    debugButton.setBounds(getWidth() - 80, 5, 70, 30);
    #endif
}

void HeartSyncEditor::timerCallback()
{
    // Placeholder for any UI animations or clock updates
}

void HeartSyncEditor::scanForDevices()
{
    #ifdef HEARTSYNC_USE_BRIDGE
    auto& client = processorRef.getBLEClient();
    
    // Clear existing devices
    deviceBox.clear();
    
    // Update status
    statusDot.setColour(juce::Label::textColourId, HSTheme::VITAL_WET_DRY);
    statusLabel.setText("Scanning...", juce::dontSendNotification);
    deviceTerminal.setText("Scanning for heart rate monitors...", juce::dontSendNotification);
    
    // Start scan
    client.startScan(true);
    
    // Stop scan after 10 seconds
    juce::Timer::callAfterDelay(10000, [this, &client]() {
        client.startScan(false);
        statusDot.setColour(juce::Label::textColourId, juce::Colours::dimgrey);
        statusLabel.setText("Scan complete", juce::dontSendNotification);
        deviceTerminal.setText("Scan complete - found " + juce::String(deviceBox.getNumItems()) + " devices", juce::dontSendNotification);
    });
    #endif
}

void HeartSyncEditor::connectToDevice(const juce::String& deviceAddress)
{
    #ifdef HEARTSYNC_USE_BRIDGE
    auto& client = processorRef.getBLEClient();
    
    // Update status
    statusDot.setColour(juce::Label::textColourId, HSTheme::VITAL_WET_DRY);
    statusLabel.setText("Connecting...", juce::dontSendNotification);
    deviceTerminal.setText("Connecting to: " + deviceAddress, juce::dontSendNotification);
    
    // Connect to device
    client.connectToDevice(deviceAddress);
    #endif
}
