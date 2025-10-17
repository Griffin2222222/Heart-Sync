#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Core/HeartRateParams.h"

#include <algorithm>

HeartSyncMetricView::HeartSyncMetricView(const juce::String& titleText,
                                         const juce::String& unitText,
                                         juce::Colour accentColour)
    : title(titleText), unit(unitText), accent(accentColour)
{
    valueLabel.setJustificationType(juce::Justification::centredLeft);
    valueLabel.setColour(juce::Label::textColourId, accent); 
    valueLabel.setFont(juce::Font(32.0f, juce::Font::bold));
    addAndMakeVisible(valueLabel);
}

void HeartSyncMetricView::setData(float value,
                                  const juce::Array<float>& timeHistory,
                                  const juce::Array<float>& valueHistory)
{
    currentValue = value;
    times = timeHistory;
    values = valueHistory;

    juce::String valueText;
    if (valueHistory.isEmpty())
    {
        valueText = "--";
    }
    else
    {
        valueText = juce::String(value, unit.isNotEmpty() ? 1 : 0);
        if (unit.isNotEmpty())
            valueText << " " << unit;
    }
    valueLabel.setText(valueText, juce::dontSendNotification);
    repaint();
}

void HeartSyncMetricView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto header = bounds.removeFromTop(28.0f);

    g.setColour(juce::Colour(0xFF001b1f));
    g.fillRoundedRectangle(header.expanded(0, 4.0f), 6.0f);
    g.setColour(accent);
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText(title, header.toNearestInt(), juce::Justification::centredLeft, true);

    auto waveformArea = bounds.reduced(6.0f);
    g.setColour(juce::Colour(0xFF001014));
    g.fillRoundedRectangle(waveformArea, 8.0f);
    g.setColour(accent.withAlpha(0.35f));
    g.drawRoundedRectangle(waveformArea, 8.0f, 1.5f);

    if (values.isEmpty() || times.isEmpty())
        return;

    float minValue = values.getFirst();
    float maxValue = values.getFirst();
    for (auto v : values)
    {
        minValue = juce::jmin(minValue, v);
        maxValue = juce::jmax(maxValue, v);
    }
    if (juce::approximatelyEqual(minValue, maxValue))
    {
        minValue -= 1.0f;
        maxValue += 1.0f;
    }

    float minTime = times.getFirst();
    float maxTime = times.getLast();
    if (juce::approximatelyEqual(minTime, maxTime))
        maxTime += 1.0f;

    juce::Path waveform;
    for (int i = 0; i < values.size(); ++i)
    {
        const float normT = juce::jmap(times.getUnchecked(i), minTime, maxTime, 0.0f, 1.0f);
        const float normV = juce::jmap(values.getUnchecked(i), maxValue, minValue, 0.0f, 1.0f);
        const float x = waveformArea.getX() + normT * waveformArea.getWidth();
        const float y = waveformArea.getY() + normV * waveformArea.getHeight();
        if (i == 0)
            waveform.startNewSubPath(x, y);
        else
            waveform.lineTo(x, y);
    }

    g.setColour(accent.withAlpha(0.85f));
    g.strokePath(waveform, juce::PathStrokeType(2.0f, juce::PathStrokeType::beveled, juce::PathStrokeType::rounded));
}

void HeartSyncMetricView::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(28);
    valueLabel.setBounds(header.removeFromRight(header.getWidth() / 2));
}

HeartSyncVST3AudioProcessorEditor::HeartSyncVST3AudioProcessorEditor(HeartSyncVST3AudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      heartRateView("HEART RATE", "BPM", juce::Colour(0xFF00ffd5)),
      smoothedView("SMOOTHED HR", "BPM", juce::Colour(0xFF5bc0ff)),
      wetDryView("WET/DRY RATIO", "%", juce::Colour(0xFFffb347))
{
    // Note: setSize() moved to end of constructor to avoid calling resized() before components are ready
    
    addAndMakeVisible(titleLabel);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF00eaff));
    titleLabel.setFont(juce::Font("Rajdhani", 30.0f, juce::Font::bold));
    titleLabel.setText("❖ HEARTSYNC PROFESSIONAL", juce::dontSendNotification);

    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF6fdcff));
    subtitleLabel.setFont(juce::Font(16.0f));
    subtitleLabel.setText("Adaptive Audio Bio Technology", juce::dontSendNotification);

    addAndMakeVisible(timeLabel);
    timeLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    timeLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFcaffff));

    addAndMakeVisible(systemStatusLabel);
    systemStatusLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    systemStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lime);

    addAndMakeVisible(deviceCombo);
    deviceCombo.setTextWhenNoChoicesAvailable("No devices yet");
    deviceCombo.setTextWhenNothingSelected("Select a device");
    deviceCombo.addListener(this);

    addAndMakeVisible(scanButton);
    addAndMakeVisible(connectButton);
    addAndMakeVisible(disconnectButton);
    addAndMakeVisible(lockButton);

    connectButton.addListener(this);
    disconnectButton.addListener(this);
    scanButton.addListener(this);
    lockButton.addListener(this);
    lockButton.setClickingTogglesState(true);

    connectButton.setEnabled(false);
    disconnectButton.setEnabled(false);

    addAndMakeVisible(deviceInfoLabel);
    deviceInfoLabel.setFont(juce::Font(14.0f));
    deviceInfoLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(packetInfoLabel);
    packetInfoLabel.setFont(juce::Font(13.0f));
    packetInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF00ffc6));

    addAndMakeVisible(latencyInfoLabel);
    latencyInfoLabel.setFont(juce::Font(13.0f));
    latencyInfoLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFffc371));

    addAndMakeVisible(heartRateView);
    addAndMakeVisible(smoothedView);
    addAndMakeVisible(wetDryView);

    auto configureSlider = [](juce::Slider& slider, juce::Colour colour)
    {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        slider.setColour(juce::Slider::thumbColourId, colour);
        slider.setColour(juce::Slider::trackColourId, colour.withAlpha(0.6f));
    };

    configureSlider(hrOffsetSlider, juce::Colour(0xFF00ffaa));
    addAndMakeVisible(hrOffsetSlider);
    hrOffsetSlider.setName("HR Offset");

    configureSlider(smoothingSlider, juce::Colour(0xFF5ac9ff));
    smoothingSlider.setSkewFactor(0.4f);
    addAndMakeVisible(smoothingSlider);
    smoothingSlider.setName("Smoothing");

    configureSlider(wetDryOffsetSlider, juce::Colour(0xFFffb347));
    addAndMakeVisible(wetDryOffsetSlider);
    wetDryOffsetSlider.setName("Wet/Dry Offset");

    wetDrySourceCombo.addItem("Smoothed HR", 1);
    wetDrySourceCombo.addItem("Raw HR", 2);
    addAndMakeVisible(wetDrySourceCombo);

    addAndMakeVisible(smoothingMetricsLabel);
    smoothingMetricsLabel.setFont(juce::Font(13.0f));
    smoothingMetricsLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(consoleView);
    consoleView.setReadOnly(true);
    consoleView.setMultiLine(true);
    consoleView.setScrollbarsShown(true);
    consoleView.setFont(juce::Font(13.0f, juce::Font::plain));
    consoleView.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF00010a));
    consoleView.setColour(juce::TextEditor::textColourId, juce::Colour(0xFF00fff6));

    auto& vts = processor.getValueTreeState();
    hrOffsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        vts, HeartRateParams::HEART_RATE_OFFSET, hrOffsetSlider);
    smoothingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        vts, HeartRateParams::SMOOTHING_FACTOR, smoothingSlider);
    wetDryAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        vts, HeartRateParams::WET_DRY_OFFSET, wetDryOffsetSlider);
    wetDrySourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        vts, HeartRateParams::WET_DRY_SOURCE, wetDrySourceCombo);
    lockAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        vts, HeartRateParams::UI_LOCKED, lockButton);

    // Set size LAST to avoid calling resized() before all components are fully initialized
    setSize(1200, 860);

    startTimerHz(30);
    refreshDeviceList(true);
    updateLockState();
}

HeartSyncVST3AudioProcessorEditor::~HeartSyncVST3AudioProcessorEditor()
{
    stopTimer();
}

void HeartSyncVST3AudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient gradient(juce::Colour(0xFF050716), 0, 0,
                                  juce::Colour(0xFF02131f), 0, static_cast<float>(getHeight()), false);
    g.setGradientFill(gradient);
    g.fillAll();

    g.setColour(juce::Colour(0x3300ffff));
    g.drawRect(getLocalBounds());
}

void HeartSyncVST3AudioProcessorEditor::resized()
{
    layoutPanels();
}

void HeartSyncVST3AudioProcessorEditor::timerCallback()
{
    refreshTelemetry();
}

void HeartSyncVST3AudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &scanButton)
    {
        processor.beginDeviceScan();
        devicesDirty = true;
        refreshDeviceList(true);
    }
    else if (button == &connectButton)
    {
        const int selectedIndex = deviceCombo.getSelectedItemIndex();
        if (juce::isPositiveAndBelow(selectedIndex, deviceIds.size()))
        {
            const bool connected = processor.connectToDevice(deviceIds[selectedIndex].toStdString());
            disconnectButton.setEnabled(connected);
            connectButton.setEnabled(!connected && deviceCombo.getSelectedId() > 0);
            devicesDirty = true;
        }
    }
    else if (button == &disconnectButton)
    {
        processor.disconnectFromDevice();
        disconnectButton.setEnabled(false);
        connectButton.setEnabled(deviceCombo.getSelectedId() > 0);
        devicesDirty = true;
    }

    if (button == &lockButton)
        updateLockState();
}

void HeartSyncVST3AudioProcessorEditor::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &deviceCombo)
        connectButton.setEnabled(deviceCombo.getSelectedId() > 0);
}

void HeartSyncVST3AudioProcessorEditor::layoutPanels()
{
    auto bounds = getLocalBounds().reduced(18);

    auto header = bounds.removeFromTop(80);
    auto leftHeader = header.removeFromLeft(header.getWidth() * 0.6f);
    titleLabel.setBounds(leftHeader.removeFromTop(40));
    subtitleLabel.setBounds(leftHeader.removeFromTop(24));

    auto rightHeader = header;
    timeLabel.setBounds(rightHeader.removeFromTop(28));
    systemStatusLabel.setBounds(rightHeader.removeFromTop(24));

    auto devicePanel = bounds.removeFromTop(120);
    devicePanel.reduce(10, 4);

    auto deviceTop = devicePanel.removeFromTop(36);
    deviceCombo.setBounds(deviceTop.removeFromLeft(devicePanel.getWidth() * 0.5f));
    lockButton.setBounds(deviceTop.removeFromRight(120));

    auto buttonRow = devicePanel.removeFromTop(32);
    scanButton.setBounds(buttonRow.removeFromLeft(150));
    connectButton.setBounds(buttonRow.removeFromLeft(150));
    disconnectButton.setBounds(buttonRow.removeFromLeft(150));

    deviceInfoLabel.setBounds(devicePanel.removeFromTop(24));
    packetInfoLabel.setBounds(devicePanel.removeFromTop(20));
    latencyInfoLabel.setBounds(devicePanel.removeFromTop(20));

    auto metricsArea = bounds.removeFromTop(230);
    auto metricWidth = metricsArea.getWidth() / 3;
    heartRateView.setBounds(metricsArea.removeFromLeft(metricWidth).reduced(4));
    smoothedView.setBounds(metricsArea.removeFromLeft(metricWidth).reduced(4));
    wetDryView.setBounds(metricsArea.reduced(4));

    auto controlsArea = bounds.removeFromTop(260);
    auto sliderWidth = 160;
    hrOffsetSlider.setBounds(controlsArea.removeFromLeft(sliderWidth).reduced(20));
    smoothingSlider.setBounds(controlsArea.removeFromLeft(sliderWidth).reduced(20));
    wetDryOffsetSlider.setBounds(controlsArea.removeFromLeft(sliderWidth).reduced(20));

    auto sourceArea = controlsArea.removeFromTop(60);
    wetDrySourceCombo.setBounds(sourceArea.removeFromLeft(220).reduced(10));
    smoothingMetricsLabel.setBounds(sourceArea.reduced(10));

    consoleView.setBounds(bounds.reduced(4));
}

void HeartSyncVST3AudioProcessorEditor::refreshTelemetry()
{
    auto newSnapshot = processor.getTelemetrySnapshot();
    snapshot = newSnapshot;

    timeLabel.setText(juce::Time::getCurrentTime().toString(true, true, true, true), juce::dontSendNotification);
    const bool connected = snapshot.isConnected;
    systemStatusLabel.setText(snapshot.statusText, juce::dontSendNotification);

    deviceInfoLabel.setText(snapshot.deviceName.isEmpty() ? "Device: --" : "Device: " + snapshot.deviceName,
                            juce::dontSendNotification);

    if (snapshot.packetsExpected > 0 || snapshot.packetsReceived > 0)
    {
        const float expected = static_cast<float>(std::max(1, snapshot.packetsExpected));
        const float percent = (static_cast<float>(snapshot.packetsReceived) / expected) * 100.0f;
        packetInfoLabel.setText(juce::String::formatted("Packets %d / %d (%.1f%%) | Signal %.0f%%",
                                                       snapshot.packetsReceived,
                                                       snapshot.packetsExpected,
                                                       percent,
                                                       snapshot.signalQuality),
                                juce::dontSendNotification);
    }
    else
    {
        packetInfoLabel.setText("Packets --", juce::dontSendNotification);
    }

    latencyInfoLabel.setText(connected
                                 ? "Latency " + juce::String(snapshot.lastLatencyMs, 1) + " ms"
                                 : "Latency --",
                             juce::dontSendNotification);

    heartRateView.setData(snapshot.heart.rawBpm, snapshot.history.time, snapshot.history.raw);
    smoothedView.setData(snapshot.heart.smoothedBpm, snapshot.history.time, snapshot.history.smoothed);
    wetDryView.setData(snapshot.heart.wetDry, snapshot.history.time, snapshot.history.wetDry);

    smoothingMetricsLabel.setText(juce::String::formatted("α=%.3f  T½=%.2fs  window≈%d samples",
                                                         snapshot.heart.alpha,
                                                         snapshot.heart.halfLifeSeconds,
                                                         snapshot.heart.effectiveWindowSamples),
                                  juce::dontSendNotification);

    if (snapshot.consoleLog != lastConsole)
    {
        juce::String joined;
        for (auto& line : snapshot.consoleLog)
            joined += line + "\n";
        consoleView.setText(joined, juce::dontSendNotification);
        consoleView.moveCaretToEnd();
        lastConsole = snapshot.consoleLog;
    }

    connectButton.setEnabled(!connected && deviceCombo.getSelectedId() > 0);
    disconnectButton.setEnabled(connected);
    refreshDeviceList();
    updateLockState();
}

void HeartSyncVST3AudioProcessorEditor::refreshDeviceList(bool force)
{
    if (!devicesDirty && !force)
        return;

    auto devices = processor.getDiscoveredDevices();
    deviceCombo.clear(juce::NotificationType::dontSendNotification);
    deviceIds.clear();
    int itemId = 1;
    for (const auto& device : devices)
    {
        juce::String label = device.name.empty() ? juce::String("Unknown device") : juce::String(device.name);
        juce::String idString = device.id.empty() ? juce::String("ID??") : juce::String(device.id);
        label += "  [" + idString + "]";
        deviceCombo.addItem(label, itemId);
        deviceIds.add(idString);
        ++itemId;
    }

    connectButton.setEnabled(!snapshot.isConnected && deviceCombo.getSelectedId() > 0);
    devicesDirty = false;
}

void HeartSyncVST3AudioProcessorEditor::updateLockState()
{
    if (lockButton.getToggleState())
    {
        lockButton.setButtonText("LOCKED");
        lockButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2a1500));
        lockButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFffb347));
        hrOffsetSlider.setEnabled(false);
        smoothingSlider.setEnabled(false);
        wetDryOffsetSlider.setEnabled(false);
        wetDrySourceCombo.setEnabled(false);
    }
    else
    {
        lockButton.setButtonText("UNLOCKED");
        lockButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF00331a));
        lockButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF7fffd4));
        hrOffsetSlider.setEnabled(true);
        smoothingSlider.setEnabled(true);
        wetDryOffsetSlider.setEnabled(true);
        wetDrySourceCombo.setEnabled(true);
    }
}
