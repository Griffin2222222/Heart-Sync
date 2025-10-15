#include "PluginEditor.h"

HeartSyncEditor::HeartSyncEditor(HeartSyncProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // CRITICAL: setSize() must be FIRST (Ableton Live 12 requirement)
    setSize(800, 600);
    
    // Defer all complex initialization to async callback
    juce::MessageManager::callAsync([this]() {
        if (!isInitialized)
            initializeUI();
    });
    
    // Start timer for HR display updates (10 Hz)
    startTimer(100);
}

HeartSyncEditor::~HeartSyncEditor()
{
    stopTimer();
}

void HeartSyncEditor::initializeUI()
{
    // Scan Button
    scanButton.setButtonText("Scan for Devices");
    scanButton.setColour(juce::TextButton::buttonColourId, darkTeal);
    scanButton.setColour(juce::TextButton::textColourOffId, textPrimary);
    scanButton.onClick = [this]() { scanForDevices(); };
    addAndMakeVisible(scanButton);
    
    // Device Selector
    deviceSelector.setColour(juce::ComboBox::backgroundColourId, surfacePanel);
    deviceSelector.setColour(juce::ComboBox::textColourId, textPrimary);
    deviceSelector.setColour(juce::ComboBox::outlineColourId, quantumTeal);
    deviceSelector.setTextWhenNothingSelected("No devices found");
    addAndMakeVisible(deviceSelector);
    
    // Connect Button
    connectButton.setButtonText("Connect");
    connectButton.setColour(juce::TextButton::buttonColourId, quantumTeal);
    connectButton.setColour(juce::TextButton::textColourOffId, surfaceBase);
    connectButton.onClick = [this]() {
        int selectedId = deviceSelector.getSelectedId();
        if (selectedId > 0)
        {
            juce::String address = deviceSelector.getItemText(selectedId - 1);
            connectToDevice(address);
        }
    };
    connectButton.setEnabled(false); // Initially disabled until device selected
    addAndMakeVisible(connectButton);
    
    // Disconnect Button
    disconnectButton.setButtonText("Disconnect");
    disconnectButton.setColour(juce::TextButton::buttonColourId, medicalRed);
    disconnectButton.setColour(juce::TextButton::textColourOffId, surfaceBase);
    disconnectButton.onClick = [this]() {
        processorRef.getBLEClient().disconnectDevice();
    };
    disconnectButton.setVisible(false); // Hidden until connected
    addAndMakeVisible(disconnectButton);
    
    // Heart Rate Display Labels
    rawHRLabel.setText("Raw HR: -- BPM", juce::dontSendNotification);
    rawHRLabel.setColour(juce::Label::textColourId, medicalRed);
    rawHRLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    addAndMakeVisible(rawHRLabel);
    
    smoothedHRLabel.setText("Smoothed HR: -- BPM", juce::dontSendNotification);
    smoothedHRLabel.setColour(juce::Label::textColourId, quantumTeal);
    smoothedHRLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    addAndMakeVisible(smoothedHRLabel);
    
    wetDryLabel.setText("Wet/Dry: --%", juce::dontSendNotification);
    wetDryLabel.setColour(juce::Label::textColourId, medicalGold);
    wetDryLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    addAndMakeVisible(wetDryLabel);
    
    // Parameter Sliders
    hrOffsetSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    hrOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    hrOffsetSlider.setColour(juce::Slider::rotarySliderFillColourId, quantumTeal);
    hrOffsetSlider.setColour(juce::Slider::thumbColourId, quantumTeal);
    addAndMakeVisible(hrOffsetSlider);
    hrOffsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "hr_offset", hrOffsetSlider);
    
    smoothingSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    smoothingSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    smoothingSlider.setColour(juce::Slider::rotarySliderFillColourId, quantumTeal);
    smoothingSlider.setColour(juce::Slider::thumbColourId, quantumTeal);
    addAndMakeVisible(smoothingSlider);
    smoothingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "smoothing_factor", smoothingSlider);
    
    wetDryOffsetSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    wetDryOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    wetDryOffsetSlider.setColour(juce::Slider::rotarySliderFillColourId, quantumTeal);
    wetDryOffsetSlider.setColour(juce::Slider::thumbColourId, quantumTeal);
    addAndMakeVisible(wetDryOffsetSlider);
    wetDryOffsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.getAPVTS(), "wet_dry_offset", wetDryOffsetSlider);
    
    // Status Label
    statusLabel.setText("Ready to scan", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, textPrimary);
    statusLabel.setFont(juce::Font(14.0f));
    addAndMakeVisible(statusLabel);
    
    #ifdef HEARTSYNC_USE_BRIDGE
    // Permission Banner
    permissionBanner.setColour(juce::Label::backgroundColourId, medicalRed.withAlpha(0.2f));
    permissionBanner.setColour(juce::Label::textColourId, medicalRed);
    permissionBanner.setFont(juce::Font(14.0f, juce::Font::bold));
    permissionBanner.setJustificationType(juce::Justification::centred);
    permissionBanner.setText("", juce::dontSendNotification);
    addAndMakeVisible(permissionBanner);
    permissionBanner.setVisible(false);
    
    // Open Settings Button
    openSettingsButton.setButtonText("Open Bluetooth Settings");
    openSettingsButton.setColour(juce::TextButton::buttonColourId, medicalRed);
    openSettingsButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    openSettingsButton.onClick = [this]() {
        #if JUCE_MAC
        juce::URL("x-apple.systempreferences:com.apple.preferences.Bluetooth").launchInDefaultBrowser();
        #endif
    };
    addAndMakeVisible(openSettingsButton);
    openSettingsButton.setVisible(false);
    
    // Wire up permission callback
    processorRef.getBLEClient().onPermissionChanged = [this](const juce::String& state) {
        currentPermissionState = state;
        
        if (state == "unknown" || state == "requesting")
        {
            permissionBanner.setText("⏳ Checking Bluetooth permissions...", juce::dontSendNotification);
            permissionBanner.setColour(juce::Label::backgroundColourId, quantumTeal.withAlpha(0.2f));
            permissionBanner.setColour(juce::Label::textColourId, quantumTeal);
            permissionBanner.setVisible(true);
            openSettingsButton.setVisible(false);
            scanButton.setEnabled(false);
        }
        else if (state == "denied")
        {
            permissionBanner.setText("⚠ Bluetooth access denied. Please grant permission to use HeartSync.", juce::dontSendNotification);
            permissionBanner.setColour(juce::Label::backgroundColourId, medicalRed.withAlpha(0.2f));
            permissionBanner.setColour(juce::Label::textColourId, medicalRed);
            permissionBanner.setVisible(true);
            openSettingsButton.setVisible(true);
            scanButton.setEnabled(false);
        }
        else if (state == "authorized")
        {
            permissionBanner.setVisible(false);
            openSettingsButton.setVisible(false);
            scanButton.setEnabled(true);
            statusLabel.setText("✓ Bluetooth ready - click Scan to find devices", juce::dontSendNotification);
        }
        
        resized();
    };
    
    // Wire up connection state callbacks
    processorRef.getBLEClient().onConnected = [this](const juce::String& deviceId) {
        // Find device display name from the list
        auto devices = processorRef.getBLEClient().getDevicesSnapshot();
        juce::String displayName = deviceId; // Fallback to ID
        
        for (const auto& d : devices)
        {
            if (d.id == deviceId)
            {
                displayName = d.getDisplayName();
                break;
            }
        }
        
        // Update UI for connected state
        statusLabel.setText("✓ Connected to " + displayName, juce::dontSendNotification);
        connectButton.setVisible(false);
        disconnectButton.setVisible(true);
        scanButton.setEnabled(false);
        deviceSelector.setEnabled(false);
    };
    
    processorRef.getBLEClient().onDisconnected = [this](const juce::String& reason) {
        // Update UI for disconnected state
        statusLabel.setText("Disconnected: " + reason, juce::dontSendNotification);
        connectButton.setVisible(true);
        connectButton.setEnabled(deviceSelector.getNumItems() > 0);
        disconnectButton.setVisible(false);
        scanButton.setEnabled(true);
        deviceSelector.setEnabled(true);
    };
    
    #if JUCE_DEBUG
    // Debug Button (JUCE_DEBUG only) - Cycles through test workflow
    debugButton.setButtonText("⚙︎ Debug");
    debugButton.setColour(juce::TextButton::buttonColourId, juce::Colours::grey.withAlpha(0.3f));
    debugButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.7f));
    debugButton.onClick = [this]() {
        auto& client = processorRef.getBLEClient();
        
        switch (debugStep)
        {
            case 0:
                // Step 1: Set permission to authorized
                DBG("[DEBUG UI] Step 1: Inject authorized permission");
                client.__debugInjectPermission("authorized");
                debugButton.setButtonText("⚙︎ Debug [1/5]");
                break;
                
            case 1:
                // Step 2: Add fake device
                DBG("[DEBUG UI] Step 2: Inject fake device");
                client.__debugInjectDevice("AA:BB:CC:DD:EE:FF", "Polar H10 (Debug)", -60);
                debugButton.setButtonText("⚙︎ Debug [2/5]");
                break;
                
            case 2:
                // Step 3: Connect to device
                DBG("[DEBUG UI] Step 3: Inject connected event");
                client.__debugInjectConnected("AA:BB:CC:DD:EE:FF");
                debugButton.setButtonText("⚙︎ Debug [3/5]");
                break;
                
            case 3:
                // Step 4: Send HR data
                DBG("[DEBUG UI] Step 4: Inject HR data");
                client.__debugInjectHr(72);
                debugButton.setButtonText("⚙︎ Debug [4/5]");
                break;
                
            case 4:
                // Step 5: Disconnect
                DBG("[DEBUG UI] Step 5: Inject disconnected event");
                client.__debugInjectDisconnected("debug");
                debugButton.setButtonText("⚙︎ Debug [5/5]");
                break;
                
            case 5:
                // Reset
                DBG("[DEBUG UI] Reset debug workflow");
                debugButton.setButtonText("⚙︎ Debug");
                debugStep = -1; // Will be incremented to 0
                break;
        }
        
        debugStep++;
        if (debugStep > 5)
            debugStep = 0;
    };
    addAndMakeVisible(debugButton);
    #endif
    #endif
    
    isInitialized = true;
    resized();
}

void HeartSyncEditor::paint(juce::Graphics& g)
{
    // Background gradient
    g.fillAll(surfaceBase);
    
    // Header
    g.setColour(surfacePanel);
    g.fillRect(0, 0, getWidth(), 60);
    
    g.setColour(quantumTeal);
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("❖ HEART SYNC", 20, 10, 300, 40, juce::Justification::centredLeft);
    
    g.setColour(darkTeal);
    g.setFont(juce::Font(12.0f));
    g.drawText("Quantum Bio Audio Technology", 20, 35, 300, 20, juce::Justification::centredLeft);
    
    // Section dividers
    g.setColour(darkTeal.withAlpha(0.3f));
    g.drawLine(20.0f, 220.0f, static_cast<float>(getWidth() - 20), 220.0f, 2.0f);
    g.drawLine(20.0f, 380.0f, static_cast<float>(getWidth() - 20), 380.0f, 2.0f);
}

void HeartSyncEditor::resized()
{
    if (!isInitialized)
        return;
    
    auto bounds = getLocalBounds();
    
    #ifdef HEARTSYNC_USE_BRIDGE
    #if JUCE_DEBUG
    // Debug button (top-right corner, JUCE_DEBUG only)
    auto debugArea = bounds.removeFromTop(25).removeFromRight(100);
    debugArea.removeFromRight(10);
    debugButton.setBounds(debugArea);
    bounds.removeFromTop(5); // spacing
    #endif
    #endif
    
    // Header space
    bounds.removeFromTop(70);
    
    #ifdef HEARTSYNC_USE_BRIDGE
    // Permission banner (if visible)
    if (permissionBanner.isVisible())
    {
        auto bannerArea = bounds.removeFromTop(50);
        bannerArea.reduce(20, 5);
        permissionBanner.setBounds(bannerArea.removeFromTop(30));
        
        if (openSettingsButton.isVisible())
        {
            auto buttonArea = bannerArea.withSizeKeepingCentre(200, 25);
            openSettingsButton.setBounds(buttonArea);
        }
        
        bounds.removeFromTop(5); // spacing
    }
    #endif
    
    // BLE Connection Section
    auto bleSection = bounds.removeFromTop(140);
    bleSection.reduce(20, 10);
    
    auto bleRow1 = bleSection.removeFromTop(40);
    scanButton.setBounds(bleRow1.removeFromLeft(180));
    bleRow1.removeFromLeft(10);
    deviceSelector.setBounds(bleRow1);
    
    bleSection.removeFromTop(10);
    auto bleRow2 = bleSection.removeFromTop(40);
    connectButton.setBounds(bleRow2.removeFromLeft(180));
    bleRow2.removeFromLeft(10);
    disconnectButton.setBounds(bleRow2.removeFromLeft(180));
    
    bleSection.removeFromTop(10);
    statusLabel.setBounds(bleSection);
    
    bounds.removeFromTop(10);
    
    // Heart Rate Display Section
    auto hrSection = bounds.removeFromTop(150);
    hrSection.reduce(20, 10);
    
    auto hrCol1 = hrSection.removeFromLeft(getWidth() / 3);
    rawHRLabel.setBounds(hrCol1);
    
    auto hrCol2 = hrSection.removeFromLeft(getWidth() / 3);
    smoothedHRLabel.setBounds(hrCol2);
    
    wetDryLabel.setBounds(hrSection);
    
    bounds.removeFromTop(10);
    
    // Parameter Control Section
    auto paramSection = bounds;
    paramSection.reduce(20, 10);
    
    int sliderWidth = 100;
    int sliderSpacing = (paramSection.getWidth() - sliderWidth * 3) / 4;
    
    int xPos = sliderSpacing;
    hrOffsetSlider.setBounds(xPos, paramSection.getY(), sliderWidth, 120);
    
    xPos += sliderWidth + sliderSpacing;
    smoothingSlider.setBounds(xPos, paramSection.getY(), sliderWidth, 120);
    
    xPos += sliderWidth + sliderSpacing;
    wetDryOffsetSlider.setBounds(xPos, paramSection.getY(), sliderWidth, 120);
}

void HeartSyncEditor::timerCallback()
{
    if (!isInitialized)
        return;
    
    // Update heart rate displays
    float rawHR = processorRef.getRawHeartRate();
    float smoothedHR = processorRef.getSmoothedHeartRate();
    float wetDry = processorRef.getWetDryRatio();
    
    if (rawHR > 0)
    {
        rawHRLabel.setText("Raw HR: " + juce::String(static_cast<int>(rawHR)) + " BPM",
                          juce::dontSendNotification);
        smoothedHRLabel.setText("Smoothed HR: " + juce::String(static_cast<int>(smoothedHR)) + " BPM",
                               juce::dontSendNotification);
        wetDryLabel.setText("Wet/Dry: " + juce::String(static_cast<int>(wetDry)) + "%",
                           juce::dontSendNotification);
    }
    
    // Update connection status
    if (processorRef.isBLEConnected())
    {
        statusLabel.setText("✓ Connected - Receiving heart rate data", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, quantumTeal);
    }
    else
    {
        statusLabel.setText("Not connected", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, textPrimary);
    }
}

void HeartSyncEditor::scanForDevices()
{
    statusLabel.setText("Scanning for BLE devices...", juce::dontSendNotification);
    deviceSelector.clear();
    
    #ifdef HEARTSYNC_USE_BRIDGE
    // Set up callback to receive each device
    processorRef.getBLEClient().onDeviceFound = [this](const HeartSyncBLEClient::DeviceInfo& device) {
        // Update device list on message thread (already called async by client)
        deviceSelector.clear();
        
        // Get full device list
        auto devices = processorRef.getBLEClient().getDevicesSnapshot();
        
        int itemId = 1;
        for (const auto& d : devices)
        {
            // Use getDisplayName() for friendly display
            juce::String displayName = d.getDisplayName() + " (RSSI: " + juce::String(d.rssi) + ")";
            deviceSelector.addItem(displayName, itemId++);
        }
        
        if (devices.isEmpty())
        {
            statusLabel.setText("No devices found - make sure device is advertising", juce::dontSendNotification);
        }
        else
        {
            statusLabel.setText("Found " + juce::String(devices.size()) + " device(s)", juce::dontSendNotification);
        }
        
        // Enable connect button if devices found and not already connected
        connectButton.setEnabled(!devices.isEmpty() && !processorRef.getBLEClient().isDeviceConnected());
    };
    
    // Start scan
    processorRef.getBLEClient().startScan(true);
    
    // Stop scan after 5 seconds
    juce::Timer::callAfterDelay(5000, [this]() {
        processorRef.getBLEClient().startScan(false);
    });
    #else
    processorRef.getBLEManager().startScan([this](const std::vector<HeartSyncBLE::DeviceInfo>& devices) {
        // Update device list on message thread
        deviceSelector.clear();
        
        int itemId = 1;
        for (const auto& device : devices)
        {
            juce::String displayName = device.name + " (" + device.address.substring(0, 8) + "...)";
            deviceSelector.addItem(displayName, itemId++);
            
            // Store full address in item text for later retrieval
            deviceSelector.setItemEnabled(itemId - 1, true);
        }
        
        if (devices.empty())
        {
            statusLabel.setText("No devices found - make sure device is advertising", juce::dontSendNotification);
        }
        else
        {
            statusLabel.setText("Found " + juce::String(devices.size()) + " device(s)", juce::dontSendNotification);
        }
    });
    
    // Stop scan after 5 seconds
    juce::Timer::callAfterDelay(5000, [this]() {
        processorRef.getBLEManager().stopScan();
    });
    #endif
}

void HeartSyncEditor::connectToDevice(const juce::String& deviceAddress)
{
    statusLabel.setText("Connecting to device...", juce::dontSendNotification);
    
    #ifdef HEARTSYNC_USE_BRIDGE
    // Extract device ID from display string (stored as "ID (RSSI: -65)")
    int rssiIdx = deviceAddress.indexOf(" (RSSI:");
    
    if (rssiIdx < 0)
    {
        statusLabel.setText("Invalid device selection", juce::dontSendNotification);
        return;
    }
    
    juce::String deviceId = deviceAddress.substring(0, rssiIdx);
    
    // Connect and wait for result
    processorRef.getBLEClient().connectToDevice(deviceId);
    
    // Wait briefly and check connection status
    juce::Timer::callAfterDelay(2000, [this]() {
        if (processorRef.isBLEConnected())
        {
            statusLabel.setText("✓ Connected successfully!", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, quantumTeal);
        }
        else
        {
            statusLabel.setText("Connection failed - check device", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, medicalRed);
        }
    });
    #else
    // Extract UUID from display string (stored as "Name (UUID...)")
    int startIdx = deviceAddress.indexOf("(");
    int endIdx = deviceAddress.indexOf("...)");
    
    if (startIdx < 0 || endIdx < 0)
    {
        statusLabel.setText("Invalid device selection", juce::dontSendNotification);
        return;
    }
    
    juce::String uuid = deviceAddress.substring(startIdx + 1, endIdx);
    
    processorRef.getBLEManager().connectToDevice(uuid, [this](bool success, const juce::String& error) {
        if (success)
        {
            statusLabel.setText("✓ Connected successfully!", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, quantumTeal);
        }
        else
        {
            statusLabel.setText("Connection failed: " + error, juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, medicalRed);
        }
    });
    #endif
}
