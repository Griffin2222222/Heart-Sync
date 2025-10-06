#include "PluginProcessor.h"
#include "PluginEditor.h"

HeartSyncVST3AudioProcessorEditor::HeartSyncVST3AudioProcessorEditor(HeartSyncVST3AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(800, 600);
    
    // Add a big title to prove changes are working
    addAndMakeVisible(titleLabel);
    titleLabel.setText("*** ENHANCED HEARTSYNC MODULATOR ***", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    
    // Setup Bluetooth section
    addAndMakeVisible(bluetoothGroup);
    bluetoothGroup.setText("Bluetooth Heart Rate Monitor");
    bluetoothGroup.setTextLabelPosition(juce::Justification::centredTop);
    
    addAndMakeVisible(deviceList);
    deviceList.setTextWhenNoChoicesAvailable("No devices found");
    deviceList.setTextWhenNothingSelected("Select a heart rate monitor...");
    deviceList.addListener(this);
    
    addAndMakeVisible(scanButton);
    scanButton.setButtonText("Scan for Devices");
    scanButton.addListener(this);
    
    addAndMakeVisible(connectButton);
    connectButton.setButtonText("Connect");
    connectButton.addListener(this);
    connectButton.setEnabled(false);
    
    addAndMakeVisible(disconnectButton);
    disconnectButton.setButtonText("Disconnect");
    disconnectButton.addListener(this);
    disconnectButton.setEnabled(false);
    
    addAndMakeVisible(connectionStatusLabel);
    connectionStatusLabel.setText("Not connected", juce::dontSendNotification);
    connectionStatusLabel.setJustificationType(juce::Justification::centred);
    connectionStatusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    
    // Setup display section
    addAndMakeVisible(displayGroup);
    displayGroup.setText("Heart Rate Data");
    displayGroup.setTextLabelPosition(juce::Justification::centredTop);
    
    addAndMakeVisible(rawBpmLabel);
    rawBpmLabel.setText("Raw BPM:", juce::dontSendNotification);
    addAndMakeVisible(rawBpmValue);
    rawBpmValue.setText("--", juce::dontSendNotification);
    rawBpmValue.setFont(juce::Font(20.0f, juce::Font::bold));
    
    addAndMakeVisible(smoothedBpmLabel);
    smoothedBpmLabel.setText("Smoothed BPM:", juce::dontSendNotification);
    addAndMakeVisible(smoothedBpmValue);
    smoothedBpmValue.setText("--", juce::dontSendNotification);
    smoothedBpmValue.setFont(juce::Font(20.0f, juce::Font::bold));
    
    addAndMakeVisible(invertedBpmLabel);
    invertedBpmLabel.setText("Inverted Value:", juce::dontSendNotification);
    addAndMakeVisible(invertedBpmValue);
    invertedBpmValue.setText("--", juce::dontSendNotification);
    invertedBpmValue.setFont(juce::Font(20.0f, juce::Font::bold));
    
    // Setup parameter controls
    addAndMakeVisible(parameterGroup);
    parameterGroup.setText("Processing Controls");
    parameterGroup.setTextLabelPosition(juce::Justification::centredTop);
    
    addAndMakeVisible(smoothingSlider);
    smoothingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    smoothingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    smoothingSlider.setRange(0.0, 1.0, 0.01);
    smoothingSlider.setValue(0.5);
    
    addAndMakeVisible(smoothingLabel);
    smoothingLabel.setText("Smoothing Factor:", juce::dontSendNotification);
    smoothingLabel.attachToComponent(&smoothingSlider, true);
    
    // Attach to parameter
    smoothingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), HeartRateParams::SMOOTHING_FACTOR, smoothingSlider);
    
    // Setup OSC section
    addAndMakeVisible(oscGroup);
    oscGroup.setText("OSC Output");
    oscGroup.setTextLabelPosition(juce::Justification::centredTop);
    
    addAndMakeVisible(oscEnabledButton);
    oscEnabledButton.setButtonText("Enable OSC Output");
    
    addAndMakeVisible(oscPortSlider);
    oscPortSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    oscPortSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    oscPortSlider.setRange(1024, 65535, 1);
    oscPortSlider.setValue(8000);
    
    addAndMakeVisible(oscPortLabel);
    oscPortLabel.setText("OSC Port:", juce::dontSendNotification);
    oscPortLabel.attachToComponent(&oscPortSlider, true);
    
    addAndMakeVisible(oscStatusLabel);
    oscStatusLabel.setText("OSC Status: Ready", juce::dontSendNotification);
    oscStatusLabel.setJustificationType(juce::Justification::centred);
    
    // Attach OSC parameters
    oscEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), "oscEnabled", oscEnabledButton);
    oscPortAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "oscPort", oscPortSlider);
    
    // Start timer for UI updates (30 FPS)
    startTimer(33);
    
    // Initial device scan
    refreshDeviceList();
}

HeartSyncVST3AudioProcessorEditor::~HeartSyncVST3AudioProcessorEditor()
{
    stopTimer();
}

void HeartSyncVST3AudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background gradient
    juce::ColourGradient bg(juce::Colour(0xff1a1a2e), 0, 0,
                           juce::Colour(0xff16213e), getWidth(), getHeight(), false);
    g.setGradientFill(bg);
    g.fillAll();
    
    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("HeartSync Modulator", 10, 10, getWidth() - 20, 40, juce::Justification::centred);
    
    // Heart beat visualization
    juce::Rectangle<int> heartArea(getWidth() - 120, 60, 100, 100);
    paintHeartBeat(g, heartArea);
}

void HeartSyncVST3AudioProcessorEditor::paintHeartBeat(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (isConnected)
    {
        // Animate heart beat based on current BPM
        float bpm = audioProcessor.getSmoothedBpm();
        if (bpm > 0)
        {
            float beatsPerSecond = bpm / 60.0f;
            float phase = fmod(pulseAnimation * beatsPerSecond * 2.0f * juce::MathConstants<float>::pi, 
                              2.0f * juce::MathConstants<float>::pi);
            float pulse = (sin(phase) + 1.0f) * 0.5f; // 0.0 to 1.0
            
            // Draw pulsing heart
            float size = 30.0f + pulse * 20.0f;
            juce::Colour heartColour = juce::Colours::red.withAlpha(0.7f + pulse * 0.3f);
            
            g.setColour(heartColour);
            juce::Rectangle<float> heartRect(area.getCentreX() - size/2, area.getCentreY() - size/2, size, size);
            g.fillEllipse(heartRect);
            
            // Heart rate text
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(14.0f, juce::Font::bold));
            g.drawText(juce::String((int)bpm) + " BPM", area.getX(), area.getBottom() + 5, 
                      area.getWidth(), 20, juce::Justification::centred);
        }
    }
    else
    {
        // Static heart when not connected
        g.setColour(juce::Colours::grey);
        juce::Rectangle<float> heartRect(area.getCentreX() - 15, area.getCentreY() - 15, 30, 30);
        g.fillEllipse(heartRect);
        
        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font(12.0f));
        g.drawText("Not Connected", area.getX(), area.getBottom() + 5, 
                  area.getWidth(), 20, juce::Justification::centred);
    }
}

void HeartSyncVST3AudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    
    // Title area
    titleLabel.setBounds(area.removeFromTop(50));
    
    auto leftColumn = area.removeFromLeft(getWidth() / 2 - 10);
    auto rightColumn = area.removeFromLeft(getWidth() / 2 - 10);
    
    // Left column: Bluetooth and Display
    auto bluetoothArea = leftColumn.removeFromTop(180);
    bluetoothGroup.setBounds(bluetoothArea);
    bluetoothArea.removeFromTop(25); // Group header
    bluetoothArea.reduce(10, 5);
    
    deviceList.setBounds(bluetoothArea.removeFromTop(25));
    bluetoothArea.removeFromTop(5);
    
    auto buttonArea = bluetoothArea.removeFromTop(30);
    scanButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 3 - 5));
    buttonArea.removeFromLeft(5);
    connectButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2 - 2));
    buttonArea.removeFromLeft(5);
    disconnectButton.setBounds(buttonArea);
    
    bluetoothArea.removeFromTop(10);
    connectionStatusLabel.setBounds(bluetoothArea.removeFromTop(25));
    
    leftColumn.removeFromTop(10);
    auto displayArea = leftColumn.removeFromTop(200);
    displayGroup.setBounds(displayArea);
    displayArea.removeFromTop(25); // Group header
    displayArea.reduce(10, 5);
    
    auto row1 = displayArea.removeFromTop(30);
    rawBpmLabel.setBounds(row1.removeFromLeft(120));
    rawBpmValue.setBounds(row1);
    
    displayArea.removeFromTop(10);
    auto row2 = displayArea.removeFromTop(30);
    smoothedBpmLabel.setBounds(row2.removeFromLeft(120));
    smoothedBpmValue.setBounds(row2);
    
    displayArea.removeFromTop(10);
    auto row3 = displayArea.removeFromTop(30);
    invertedBpmLabel.setBounds(row3.removeFromLeft(120));
    invertedBpmValue.setBounds(row3);
    
    // Right column: Parameters and OSC
    auto paramArea = rightColumn.removeFromTop(120);
    parameterGroup.setBounds(paramArea);
    paramArea.removeFromTop(25); // Group header
    paramArea.reduce(10, 5);
    
    smoothingSlider.setBounds(paramArea.removeFromTop(30).removeFromRight(paramArea.getWidth() - 120));
    
    rightColumn.removeFromTop(10);
    auto oscArea = rightColumn.removeFromTop(150);
    oscGroup.setBounds(oscArea);
    oscArea.removeFromTop(25); // Group header
    oscArea.reduce(10, 5);
    
    oscEnabledButton.setBounds(oscArea.removeFromTop(30));
    oscArea.removeFromTop(10);
    oscPortSlider.setBounds(oscArea.removeFromTop(30).removeFromRight(oscArea.getWidth() - 80));
    oscArea.removeFromTop(10);
    oscStatusLabel.setBounds(oscArea.removeFromTop(25));
}

void HeartSyncVST3AudioProcessorEditor::timerCallback()
{
    // Update pulse animation
    pulseAnimation += 0.033f; // 30 FPS
    
    // Update heart rate displays
    float rawBpm = audioProcessor.getRawBpm();
    float smoothedBpm = audioProcessor.getSmoothedBpm();
    float invertedBpm = audioProcessor.getInvertedBpm();
    
    rawBpmValue.setText(rawBpm > 0 ? juce::String((int)rawBpm) : "--", juce::dontSendNotification);
    smoothedBpmValue.setText(smoothedBpm > 0 ? juce::String(smoothedBpm, 1) : "--", juce::dontSendNotification);
    invertedBpmValue.setText(invertedBpm > 0 ? juce::String(invertedBpm, 2) : "--", juce::dontSendNotification);
    
    // Update connection status
    updateConnectionStatus();
    updateOscStatus();
    
    // Repaint for animation
    repaint();
}

void HeartSyncVST3AudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &scanButton)
    {
        refreshDeviceList();
    }
    else if (button == &connectButton)
    {
        int selectedIndex = deviceList.getSelectedItemIndex();
        if (selectedIndex >= 0)
        {
            // Get device list from Bluetooth manager and connect
            auto devices = audioProcessor.getBluetoothManager().getAvailableDevices();
            if (selectedIndex < devices.size())
            {
                audioProcessor.getBluetoothManager().connectToDevice(devices[selectedIndex]);
                connectButton.setEnabled(false);
                disconnectButton.setEnabled(true);
            }
        }
    }
    else if (button == &disconnectButton)
    {
        audioProcessor.getBluetoothManager().disconnect();
        connectButton.setEnabled(true);
        disconnectButton.setEnabled(false);
    }
}

void HeartSyncVST3AudioProcessorEditor::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &deviceList)
    {
        connectButton.setEnabled(deviceList.getSelectedItemIndex() >= 0);
    }
}

void HeartSyncVST3AudioProcessorEditor::refreshDeviceList()
{
    deviceList.clear();
    scanButton.setEnabled(false);
    scanButton.setButtonText("Scanning...");
    
    // Trigger device scan
    audioProcessor.getBluetoothManager().startDeviceScan();
    
    // Simulate scan completion (in real implementation, this would be async)
    juce::Timer::callAfterDelay(2000, [this]()
    {
        auto devices = audioProcessor.getBluetoothManager().getAvailableDevices();
        for (int i = 0; i < devices.size(); ++i)
        {
            deviceList.addItem(devices[i], i + 1);
        }
        
        if (devices.isEmpty())
        {
            deviceList.addItem("No heart rate monitors found", 1);
        }
        
        scanButton.setEnabled(true);
        scanButton.setButtonText("Scan for Devices");
    });
}

void HeartSyncVST3AudioProcessorEditor::updateConnectionStatus()
{
    bool connected = audioProcessor.getBluetoothManager().isConnected();
    if (connected != isConnected)
    {
        isConnected = connected;
        if (isConnected)
        {
            connectionStatusLabel.setText("Connected: " + audioProcessor.getBluetoothManager().getConnectedDeviceName(), 
                                        juce::dontSendNotification);
            connectionStatusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
        }
        else
        {
            connectionStatusLabel.setText("Not connected", juce::dontSendNotification);
            connectionStatusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        }
    }
}

void HeartSyncVST3AudioProcessorEditor::updateOscStatus()
{
    bool oscEnabled = oscEnabledButton.getToggleState();
    if (oscEnabled)
    {
        int port = (int)oscPortSlider.getValue();
        oscStatusLabel.setText("OSC sending to port " + juce::String(port), juce::dontSendNotification);
        oscStatusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
    }
    else
    {
        oscStatusLabel.setText("OSC disabled", juce::dontSendNotification);
        oscStatusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    }
}
