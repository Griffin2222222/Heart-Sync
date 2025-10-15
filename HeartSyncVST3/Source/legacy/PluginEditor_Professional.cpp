#include "PluginEditor.h"
#include <ctime>

//==============================================================================
// HeartSync Professional - Direct Python-to-VST3 Translation
// This recreates the exact Python HeartSync interface with JUCE/VST3
//==============================================================================

HeartSyncVST3Editor::HeartSyncVST3Editor(HeartSyncVST3AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Match Python geometry: 1600x1000
    setSize(1600, 1000);
    setResizable(true, true);
    
    // Initialize data buffers (matching Python deque maxlen=300)
    hrDataBuffer.resize(300, 0.0f);
    smoothedHrDataBuffer.resize(300, 0.0f);
    wetDryDataBuffer.resize(300, 0.0f);
    
    // Live values (matching Python state)
    currentHr = 0;
    smoothedHr = 0;
    wetDryRatio = 0;
    connectionStatus = "DISCONNECTED";
    
    // Parameters (matching Python defaults)
    smoothingFactor = 0.1f;
    hrOffset = 0.0f;
    wetDryOffset = 0.0f;
    
    createProfessionalInterface();
    
    // Start timer for real-time updates (matching Python refresh rate)
    startTimer(33); // ~30 FPS like Python GUI
}

HeartSyncVST3Editor::~HeartSyncVST3Editor()
{
    stopTimer();
}

void HeartSyncVST3Editor::createProfessionalInterface()
{
    // ========== HEADER SECTION (matching Python title bar) ==========
    titleLabel = std::make_unique<juce::Label>("title", juce::CharPointer_UTF8("❖ HEART SYNC SYSTEM"));
    titleLabel->setFont(juce::Font("Helvetica", 20.0f, juce::Font::bold));
    titleLabel->setColour(juce::Label::textColourId, QuantumColors::TEAL);
    titleLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(*titleLabel);
    
    subtitleLabel = std::make_unique<juce::Label>("subtitle", "Next-Generation Scientific Monitoring");
    subtitleLabel->setFont(juce::Font("Helvetica", 13.0f, juce::Font::plain));
    subtitleLabel->setColour(juce::Label::textColourId, QuantumColors::TEXT_SECONDARY);
    subtitleLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(*subtitleLabel);
    
    // System status and time (top right)
    statusLabel = std::make_unique<juce::Label>("status", juce::CharPointer_UTF8("◆ SYSTEM OPERATIONAL"));
    statusLabel->setFont(juce::Font("Helvetica", 12.0f, juce::Font::bold));
    statusLabel->setColour(juce::Label::textColourId, QuantumColors::STATUS_CONNECTED);
    statusLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*statusLabel);
    
    timeLabel = std::make_unique<juce::Label>("time", "");
    timeLabel->setFont(juce::Font("Helvetica", 16.0f, juce::Font::bold));
    timeLabel->setColour(juce::Label::textColourId, QuantumColors::TEXT_PRIMARY);
    timeLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*timeLabel);
    
    // ========== VITAL SIGNS DISPLAY (matching Python layout) ==========
    // Raw Heart Rate Display
    rawHrDisplay = std::make_unique<VitalSignsDisplay>("HEART RATE", "BPM", QuantumColors::VITAL_RED);
    rawHrDisplay->setVSTParameter([this](float value) {
        // Map to VST3 parameter for DAW automation
        audioProcessor.setParameterNotifyingHost("raw_hr", (value - 40.0f) / 140.0f);
    });
    addAndMakeVisible(*rawHrDisplay);
    
    // Smoothed Heart Rate Display  
    smoothedHrDisplay = std::make_unique<VitalSignsDisplay>("SMOOTHED HR", "BPM", QuantumColors::TEAL);
    smoothedHrDisplay->setVSTParameter([this](float value) {
        audioProcessor.setParameterNotifyingHost("smoothed_hr", (value - 40.0f) / 140.0f);
    });
    addAndMakeVisible(*smoothedHrDisplay);
    
    // Wet/Dry Ratio Display
    wetDryDisplay = std::make_unique<VitalSignsDisplay>("WET/DRY RATIO", "%", QuantumColors::VITAL_GOLD);
    wetDryDisplay->setVSTParameter([this](float value) {
        audioProcessor.setParameterNotifyingHost("wet_dry_ratio", value / 100.0f);
    });
    addAndMakeVisible(*wetDryDisplay);
    
    // ========== REAL-TIME WAVEFORMS (matching Python plots) ==========
    rawHrWaveform = std::make_unique<QuantumWaveform>("Raw HR Waveform", QuantumColors::VITAL_RED);
    addAndMakeVisible(*rawHrWaveform);
    
    smoothedHrWaveform = std::make_unique<QuantumWaveform>("Smoothed HR", QuantumColors::TEAL);
    addAndMakeVisible(*smoothedHrWaveform);
    
    wetDryWaveform = std::make_unique<QuantumWaveform>("Wet/Dry Ratio", QuantumColors::VITAL_GOLD);
    addAndMakeVisible(*wetDryWaveform);
    
    // ========== PARAMETER CONTROLS (TouchDesigner-style) ==========
    hrOffsetControl = std::make_unique<QuantumParameterBox>("HR OFFSET", -100, 100, 0, "BPM");
    hrOffsetControl->onValueChanged = [this](float value) {
        hrOffset = value;
        audioProcessor.setParameterNotifyingHost("hr_offset", (value + 100.0f) / 200.0f);
    };
    addAndMakeVisible(*hrOffsetControl);
    
    smoothingControl = std::make_unique<QuantumParameterBox>("SMOOTHING", 0.01f, 10.0f, 0.1f, "");
    smoothingControl->onValueChanged = [this](float value) {
        smoothingFactor = value;
        audioProcessor.setParameterNotifyingHost("smoothing_factor", value / 10.0f);
    };
    addAndMakeVisible(*smoothingControl);
    
    wetDryOffsetControl = std::make_unique<QuantumParameterBox>("WET/DRY OFFSET", -100, 100, 0, "%");
    wetDryOffsetControl->onValueChanged = [this](float value) {
        wetDryOffset = value;
        audioProcessor.setParameterNotifyingHost("wet_dry_offset", (value + 100.0f) / 200.0f);
    };
    addAndMakeVisible(*wetDryOffsetControl);
    
    // ========== BLUETOOTH DEVICE PANEL ==========
    bluetoothPanel = std::make_unique<BluetoothDevicePanel>();
    bluetoothPanel->onScanDevices = [this]() {
        startBluetoothScan();
    };
    bluetoothPanel->onConnectDevice = [this](const juce::String& address) {
        connectToBluetoothDevice(address);
    };
    addAndMakeVisible(*bluetoothPanel);
    
    // ========== PROFESSIONAL CONSOLE (matching Python terminal) ==========
    systemConsole = std::make_unique<QuantumConsole>();
    systemConsole->addMessage("❖ HeartSync Professional VST3 by Conscious Audio");
    systemConsole->addMessage("❖ Next-Generation Scientific Monitoring");
    systemConsole->addMessage("❖ VST3 Parameters exposed for DAW automation");
    addAndMakeVisible(*systemConsole);
}

void HeartSyncVST3Editor::paint(juce::Graphics& g)
{
    // Professional gradient background (matching Python)
    juce::ColourGradient gradient(
        QuantumColors::SURFACE_BASE_START, 0, 0,
        QuantumColors::SURFACE_PANEL_LIGHT, 0, getHeight(),
        false
    );
    g.setGradientFill(gradient);
    g.fillAll();
    
    // Quantum grid overlay (subtle)
    g.setColour(QuantumColors::TEAL.withAlpha(0.05f));
    int gridSize = 20;
    for (int x = 0; x < getWidth(); x += gridSize) {
        g.drawVerticalLine(x, 0, getHeight());
    }
    for (int y = 0; y < getHeight(); y += gridSize) {
        g.drawHorizontalLine(y, 0, getWidth());
    }
    
    // Professional border
    g.setColour(QuantumColors::TEAL.withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 2);
}

void HeartSyncVST3Editor::resized()
{
    auto bounds = getLocalBounds();
    auto margin = 10;
    bounds.reduce(margin, margin);
    
    // Header section (top 60px)
    auto headerArea = bounds.removeFromTop(60);
    auto titleArea = headerArea.removeFromLeft(headerArea.getWidth() / 2);
    titleLabel->setBounds(titleArea.removeFromTop(30));
    subtitleLabel->setBounds(titleArea);
    
    auto statusArea = headerArea;
    timeLabel->setBounds(statusArea.removeFromTop(30));
    statusLabel->setBounds(statusArea);
    
    bounds.removeFromTop(20); // Spacing
    
    // Main content area - 3 columns like Python
    auto leftColumn = bounds.removeFromLeft(400);   // Vital signs + controls
    auto centerColumn = bounds.removeFromLeft(800); // Waveforms  
    auto rightColumn = bounds;                       // Bluetooth + console
    
    // Left column: Vital displays (top) + parameters (bottom)
    auto vitalArea = leftColumn.removeFromTop(300);
    rawHrDisplay->setBounds(vitalArea.removeFromTop(90));
    vitalArea.removeFromTop(10);
    smoothedHrDisplay->setBounds(vitalArea.removeFromTop(90));
    vitalArea.removeFromTop(10);
    wetDryDisplay->setBounds(vitalArea.removeFromTop(90));
    
    leftColumn.removeFromTop(20);
    hrOffsetControl->setBounds(leftColumn.removeFromTop(80));
    leftColumn.removeFromTop(10);
    smoothingControl->setBounds(leftColumn.removeFromTop(80));
    leftColumn.removeFromTop(10);
    wetDryOffsetControl->setBounds(leftColumn.removeFromTop(80));
    
    // Center column: Waveforms
    auto waveformHeight = centerColumn.getHeight() / 3 - 10;
    rawHrWaveform->setBounds(centerColumn.removeFromTop(waveformHeight));
    centerColumn.removeFromTop(10);
    smoothedHrWaveform->setBounds(centerColumn.removeFromTop(waveformHeight));
    centerColumn.removeFromTop(10);
    wetDryWaveform->setBounds(centerColumn);
    
    // Right column: Bluetooth (top) + console (bottom)
    bluetoothPanel->setBounds(rightColumn.removeFromTop(250));
    rightColumn.removeFromTop(20);
    systemConsole->setBounds(rightColumn);
}

void HeartSyncVST3Editor::timerCallback()
{
    // Update time display (matching Python)
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    char timeStr[100];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d  %H:%M:%S", &tm);
    timeLabel->setText(timeStr, juce::dontSendNotification);
    
    // Get current biometric data from processor
    auto biometricData = audioProcessor.getCurrentBiometricData();
    
    if (biometricData.isDataValid) {
        currentHr = biometricData.rawHeartRate;
        smoothedHr = biometricData.smoothedHeartRate;
        wetDryRatio = biometricData.wetDryRatio;
        
        // Update displays
        rawHrDisplay->setValue(currentHr);
        smoothedHrDisplay->setValue(smoothedHr);
        wetDryDisplay->setValue(wetDryRatio);
        
        // Update waveform data buffers (like Python deque)
        updateDataBuffer(hrDataBuffer, currentHr);
        updateDataBuffer(smoothedHrDataBuffer, smoothedHr);
        updateDataBuffer(wetDryDataBuffer, wetDryRatio);
        
        // Update waveform displays
        rawHrWaveform->updateData(hrDataBuffer);
        smoothedHrWaveform->updateData(smoothedHrDataBuffer);
        wetDryWaveform->updateData(wetDryDataBuffer);
        
        connectionStatus = "CONNECTED";
    } else {
        connectionStatus = "DISCONNECTED";
    }
    
    // Update connection status
    if (connectionStatus == "CONNECTED") {
        statusLabel->setText(juce::CharPointer_UTF8("◆ BIOMETRIC DATA ACTIVE"), juce::dontSendNotification);
        statusLabel->setColour(juce::Label::textColourId, QuantumColors::STATUS_CONNECTED);
    } else {
        statusLabel->setText(juce::CharPointer_UTF8("◆ AWAITING CONNECTION"), juce::dontSendNotification);
        statusLabel->setColour(juce::Label::textColourId, QuantumColors::TEXT_SECONDARY);
    }
    
    repaint();
}

void HeartSyncVST3Editor::updateDataBuffer(std::vector<float>& buffer, float newValue)
{
    // Implement Python deque behavior - shift left and add new value
    for (size_t i = 0; i < buffer.size() - 1; ++i) {
        buffer[i] = buffer[i + 1];
    }
    buffer.back() = newValue;
}

void HeartSyncVST3Editor::startBluetoothScan()
{
    systemConsole->addMessage("❖ Scanning for Bluetooth LE heart rate devices...");
    // Delegate to processor's Bluetooth manager
    audioProcessor.startDeviceScan();
}

void HeartSyncVST3Editor::connectToBluetoothDevice(const juce::String& address)
{
    systemConsole->addMessage("❖ Connecting to device: " + address);
    // Delegate to processor's Bluetooth manager  
    auto result = audioProcessor.connectToDevice(address.toStdString());
    if (result.wasOk()) {
        systemConsole->addMessage("❖ Connection initiated successfully");
    } else {
        systemConsole->addMessage("❖ Connection failed: " + result.getErrorMessage());
    }
}