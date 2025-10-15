#include "PluginEditor_Professional.h"

//==============================================================================
// VitalSignsDisplay - Professional biometric data display
//==============================================================================

VitalSignsDisplay::VitalSignsDisplay(const juce::String& title, const juce::String& unit, juce::Colour color)
    : title(title), unit(unit), primaryColor(color)
{
}

void VitalSignsDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Professional panel background
    g.setColour(QuantumColors::SURFACE_PANEL_LIGHT);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
    
    // Quantum border
    g.setColour(primaryColor.withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.toFloat().reduced(1), 8.0f, 2.0f);
    
    // Title
    auto titleArea = bounds.removeFromTop(25);
    g.setColour(primaryColor);
    g.setFont(juce::Font("Helvetica", 11.0f, juce::Font::bold));
    g.drawText(title, titleArea, juce::Justification::centred);
    
    // Main value display
    auto valueArea = bounds.removeFromTop(40);
    g.setColour(QuantumColors::TEXT_PRIMARY);
    g.setFont(juce::Font("Helvetica", 22.0f, juce::Font::bold));
    
    juce::String valueText;
    if (currentValue > 0) {
        valueText = juce::String(currentValue, 1) + " " + unit;
    } else {
        valueText = "-- " + unit;
    }
    g.drawText(valueText, valueArea, juce::Justification::centred);
    
    // Status indicator
    auto statusArea = bounds;
    g.setColour(currentValue > 0 ? QuantumColors::STATUS_CONNECTED : QuantumColors::STATUS_DISCONNECTED);
    g.fillEllipse(statusArea.getCentreX() - 4, statusArea.getCentreY() - 4, 8, 8);
}

void VitalSignsDisplay::resized()
{
    // Layout handled in paint()
}

void VitalSignsDisplay::setValue(float value)
{
    if (std::abs(currentValue - value) > 0.01f) {
        currentValue = value;
        repaint();
        
        // Send to VST3 parameter system
        if (vstParameterCallback) {
            vstParameterCallback(value);
        }
    }
}

void VitalSignsDisplay::setVSTParameter(std::function<void(float)> callback)
{
    vstParameterCallback = callback;
}

void VitalSignsDisplay::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    // Click feedback for parameter mapping
    repaint();
}

//==============================================================================
// QuantumWaveform - Real-time data visualization
//==============================================================================

QuantumWaveform::QuantumWaveform(const juce::String& title, juce::Colour color)
    : title(title), waveColor(color)
{
    displayData.resize(300, 0.0f); // Match Python deque maxlen=300
}

void QuantumWaveform::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Professional panel background
    g.setColour(QuantumColors::SURFACE_PANEL_LIGHT);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    
    // Border
    g.setColour(waveColor.withAlpha(0.4f));
    g.drawRoundedRectangle(bounds.toFloat().reduced(1), 6.0f, 1.0f);
    
    // Title
    auto titleArea = bounds.removeFromTop(25);
    g.setColour(waveColor);
    g.setFont(juce::Font("Helvetica", 11.0f, juce::Font::bold));
    g.drawText(title, titleArea, juce::Justification::centred);
    
    // Waveform area
    auto waveArea = bounds.reduced(10);
    
    // Grid lines (subtle)
    g.setColour(QuantumColors::TEAL.withAlpha(0.1f));
    int gridLines = 5;
    for (int i = 0; i <= gridLines; ++i) {
        float y = waveArea.getY() + (waveArea.getHeight() * i / gridLines);
        g.drawHorizontalLine(y, waveArea.getX(), waveArea.getRight());
    }
    
    // Draw waveform
    const juce::ScopedLock lock(dataLock);
    if (displayData.size() > 1) {
        juce::Path wavePath;
        
        // Find data range for normalization
        auto [minIt, maxIt] = std::minmax_element(displayData.begin(), displayData.end());
        float minVal = *minIt;
        float maxVal = *maxIt;
        float range = maxVal - minVal;
        if (range < 0.01f) range = 1.0f; // Avoid division by zero
        
        // Build path
        bool firstPoint = true;
        for (size_t i = 0; i < displayData.size(); ++i) {
            float x = waveArea.getX() + (waveArea.getWidth() * i / float(displayData.size() - 1));
            float normalizedY = (displayData[i] - minVal) / range;
            float y = waveArea.getBottom() - (normalizedY * waveArea.getHeight());
            
            if (firstPoint) {
                wavePath.startNewSubPath(x, y);
                firstPoint = false;
            } else {
                wavePath.lineTo(x, y);
            }
        }
        
        // Draw waveform with glow effect
        g.setColour(waveColor.withAlpha(0.3f));
        g.strokePath(wavePath, juce::PathStrokeType(3.0f));
        g.setColour(waveColor);
        g.strokePath(wavePath, juce::PathStrokeType(1.5f));
    }
}

void QuantumWaveform::updateData(const std::vector<float>& data)
{
    const juce::ScopedLock lock(dataLock);
    displayData = data;
    repaint();
}

//==============================================================================
// QuantumParameterBox - TouchDesigner-style parameter control
//==============================================================================

QuantumParameterBox::QuantumParameterBox(const juce::String& title, float minVal, float maxVal, float initialVal, const juce::String& unit)
    : title(title), unit(unit), minValue(minVal), maxValue(maxVal), currentValue(initialVal)
{
}

void QuantumParameterBox::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Professional control background
    g.setColour(QuantumColors::SURFACE_PANEL_LIGHT);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    
    // Border
    g.setColour(QuantumColors::TEAL.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.toFloat().reduced(1), 6.0f, 1.5f);
    
    // Title
    auto titleArea = bounds.removeFromTop(20);
    g.setColour(QuantumColors::TEAL);
    g.setFont(juce::Font("Helvetica", 11.0f, juce::Font::bold));
    g.drawText(title, titleArea, juce::Justification::centred);
    
    // Value display
    auto valueArea = bounds.removeFromTop(30);
    g.setColour(QuantumColors::TEXT_PRIMARY);
    g.setFont(juce::Font("Courier", 14.0f, juce::Font::bold));
    g.drawText(getDisplayText(), valueArea, juce::Justification::centred);
    
    // Parameter bar
    auto barArea = bounds.reduced(8);
    float normalizedValue = (currentValue - minValue) / (maxValue - minValue);
    
    // Background bar
    g.setColour(QuantumColors::SURFACE_BASE_START);
    g.fillRoundedRectangle(barArea.toFloat(), 3.0f);
    
    // Value bar
    auto fillArea = barArea;
    fillArea.setWidth(barArea.getWidth() * normalizedValue);
    g.setColour(QuantumColors::TEAL);
    g.fillRoundedRectangle(fillArea.toFloat(), 3.0f);
}

void QuantumParameterBox::resized()
{
    // Layout handled in paint()
}

void QuantumParameterBox::mouseDown(const juce::MouseEvent& event)
{
    dragStartValue = currentValue;
    dragStartPosition = event.getPosition();
}

void QuantumParameterBox::mouseDrag(const juce::MouseEvent& event)
{
    float dragDelta = event.getPosition().x - dragStartPosition.x;
    float sensitivity = (maxValue - minValue) / 200.0f; // 200 pixels for full range
    float newValue = dragStartValue + (dragDelta * sensitivity);
    setValue(newValue);
}

void QuantumParameterBox::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused(event);
    float wheelSensitivity = (maxValue - minValue) / 50.0f;
    float newValue = currentValue + (wheel.deltaY * wheelSensitivity);
    setValue(newValue);
}

juce::String QuantumParameterBox::getDisplayText() const
{
    if (maxValue - minValue > 10.0f) {
        return juce::String(int(currentValue)) + unit;
    } else {
        return juce::String(currentValue, 2) + unit;
    }
}

void QuantumParameterBox::setValue(float newValue)
{
    newValue = juce::jlimit(minValue, maxValue, newValue);
    if (std::abs(currentValue - newValue) > 0.001f) {
        currentValue = newValue;
        repaint();
        
        if (onValueChanged) {
            onValueChanged(currentValue);
        }
    }
}

//==============================================================================
// BluetoothDevicePanel - Professional device management
//==============================================================================

BluetoothDevicePanel::BluetoothDevicePanel()
{
    // Status label
    statusLabel = std::make_unique<juce::Label>("status", "BLUETOOTH LE CONNECTIVITY");
    statusLabel->setFont(juce::Font("Helvetica", 12.0f, juce::Font::bold));
    statusLabel->setColour(juce::Label::textColourId, QuantumColors::TEAL);
    statusLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*statusLabel);
    
    // Scan button
    scanButton = std::make_unique<juce::TextButton>("SCAN DEVICES");
    scanButton->setColour(juce::TextButton::buttonColourId, QuantumColors::TEAL.withAlpha(0.2f));
    scanButton->setColour(juce::TextButton::textColourOffId, QuantumColors::TEAL);
    scanButton->onClick = [this]() {
        if (onScanDevices) onScanDevices();
    };
    addAndMakeVisible(*scanButton);
    
    // Connect button
    connectButton = std::make_unique<juce::TextButton>("CONNECT");
    connectButton->setColour(juce::TextButton::buttonColourId, QuantumColors::STATUS_CONNECTED.withAlpha(0.2f));
    connectButton->setColour(juce::TextButton::textColourOffId, QuantumColors::STATUS_CONNECTED);
    connectButton->onClick = [this]() {
        if (onConnectDevice) onConnectDevice(""); // Connect to first available
    };
    addAndMakeVisible(*connectButton);
}

void BluetoothDevicePanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Professional panel background
    g.setColour(QuantumColors::SURFACE_PANEL_LIGHT);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
    
    // Border
    g.setColour(QuantumColors::TEAL.withAlpha(0.4f));
    g.drawRoundedRectangle(bounds.toFloat().reduced(1), 8.0f, 2.0f);
}

void BluetoothDevicePanel::resized()
{
    auto bounds = getLocalBounds().reduced(15);
    
    statusLabel->setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(10);
    
    scanButton->setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);
    connectButton->setBounds(bounds.removeFromTop(40));
}

//==============================================================================
// QuantumConsole - Professional system console
//==============================================================================

QuantumConsole::QuantumConsole()
{
    consoleText = std::make_unique<juce::TextEditor>();
    consoleText->setMultiLine(true);
    consoleText->setReadOnly(true);
    consoleText->setScrollbarsShown(true);
    consoleText->setCaretVisible(false);
    consoleText->setColour(juce::TextEditor::backgroundColourId, QuantumColors::SURFACE_BASE_START);
    consoleText->setColour(juce::TextEditor::textColourId, QuantumColors::TEXT_PRIMARY);
    consoleText->setColour(juce::TextEditor::outlineColourId, QuantumColors::TEAL.withAlpha(0.3f));
    consoleText->setFont(juce::Font("Courier", 10.0f, juce::Font::plain));
    addAndMakeVisible(*consoleText);
}

void QuantumConsole::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Console panel background
    g.setColour(QuantumColors::SURFACE_PANEL_LIGHT);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    
    // Border
    g.setColour(QuantumColors::TEAL.withAlpha(0.4f));
    g.drawRoundedRectangle(bounds.toFloat().reduced(1), 6.0f, 1.5f);
    
    // Title
    auto titleArea = bounds.removeFromTop(25);
    g.setColour(QuantumColors::TEAL);
    g.setFont(juce::Font("Helvetica", 11.0f, juce::Font::bold));
    g.drawText("SYSTEM CONSOLE", titleArea, juce::Justification::centred);
}

void QuantumConsole::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    bounds.removeFromTop(25); // Title space
    consoleText->setBounds(bounds);
}

void QuantumConsole::addMessage(const juce::String& message)
{
    // Add timestamp
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    char timeStr[10];
    std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tm);
    
    juce::String timestampedMessage = "[" + juce::String(timeStr) + "] " + message;
    
    messageBuffer.add(timestampedMessage);
    
    // Limit buffer size
    if (messageBuffer.size() > MAX_MESSAGES) {
        messageBuffer.remove(0);
    }
    
    // Update console text
    consoleText->setText(messageBuffer.joinIntoString("\n"));
    consoleText->moveCaretToEnd();
}