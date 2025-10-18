#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>

namespace
{
constexpr int headerHeight = 80;
constexpr int columnHeaderHeight = 36;
constexpr int metricRowHeight = 150;
constexpr int blePanelHeight = 100;
constexpr int devicePanelHeight = 96;
constexpr int oscPanelHeight = 80;
constexpr int consoleHeight = 160;
constexpr int valueColumnWidth = 200;
constexpr int controlsColumnWidth = 200;
constexpr int buttonWidth = 118;
}

HeartSyncVST3AudioProcessorEditor::HeartSyncVST3AudioProcessorEditor(HeartSyncVST3AudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);
    setResizable(false, false);
    setSize(1260, 1100);

    headerTitleLabel.setText("HEART SYNC SYSTEM", juce::dontSendNotification);
    headerTitleLabel.setFont(juce::Font("Rajdhani", 30.0f, juce::Font::bold));
    headerTitleLabel.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    headerTitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(headerTitleLabel);

    headerSubtitleLabel.setText("Adaptive Audio Bio Technology", juce::dontSendNotification);
    headerSubtitleLabel.setFont(HSTheme::label());
    headerSubtitleLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    headerSubtitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(headerSubtitleLabel);

    headerTimeLabel.setFont(HSTheme::mono(16.0f, true));
    headerTimeLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_PRIMARY);
    headerTimeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(headerTimeLabel);

    headerStatusLabel.setFont(HSTheme::mono(12.0f, true));
    headerStatusLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(headerStatusLabel);

    valuesHeading.setText("VALUES", juce::dontSendNotification);
    valuesHeading.setFont(HSTheme::heading());
    valuesHeading.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    valuesHeading.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(valuesHeading);

    controlsHeading.setText("CONTROLS", juce::dontSendNotification);
    controlsHeading.setFont(HSTheme::heading());
    controlsHeading.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    controlsHeading.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(controlsHeading);

    waveformHeading.setText("WAVEFORM", juce::dontSendNotification);
    waveformHeading.setFont(HSTheme::heading());
    waveformHeading.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    waveformHeading.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(waveformHeading);

    hrRow = std::make_unique<MetricRow>("HEART RATE", "BPM", HSTheme::VITAL_HEART_RATE,
                                        [this](juce::Component& host) { buildHROffsetControls(host); });
    addAndMakeVisible(*hrRow);

    smoothRow = std::make_unique<MetricRow>("SMOOTHED HR", "BPM", HSTheme::VITAL_SMOOTHED,
                                            [this](juce::Component& host) { buildSmoothControls(host); });
    addAndMakeVisible(*smoothRow);

    wetDryRow = std::make_unique<MetricRow>("WET/DRY RATIO", "%", HSTheme::VITAL_WET_DRY,
                                            [this](juce::Component& host) { buildWetDryControls(host); });
    addAndMakeVisible(*wetDryRow);

    blePanel = std::make_unique<RectPanel>(HSTheme::ACCENT_TEAL);
    addAndMakeVisible(*blePanel);

    statusIndicator.setText("*", juce::dontSendNotification);
    statusIndicator.setFont(HSTheme::mono(20.0f, true));
    statusIndicator.setColour(juce::Label::textColourId, HSTheme::STATUS_DISCONNECTED);
    statusIndicator.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusIndicator);

    bleTitleLabel.setText("BLUETOOTH LE CONNECTIVITY", juce::dontSendNotification);
    bleTitleLabel.setFont(HSTheme::label());
    bleTitleLabel.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    bleTitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bleTitleLabel);

    scanButton.setButtonText("SCAN");
    scanButton.addListener(this);
    addAndMakeVisible(scanButton);

    connectButton.setButtonText("CONNECT");
    connectButton.addListener(this);
    connectButton.setEnabled(false);
    addAndMakeVisible(connectButton);

    disconnectButton.setButtonText("DISCONNECT");
    disconnectButton.addListener(this);
    disconnectButton.setEnabled(false);
    addAndMakeVisible(disconnectButton);

    lockButton.setButtonText("LOCKED");
    lockButton.setClickingTogglesState(true);
    lockButton.addListener(this);
    addAndMakeVisible(lockButton);

    deviceCombo.addListener(this);
    addAndMakeVisible(deviceCombo);

    bleStatusLabel.setFont(HSTheme::label());
    bleStatusLabel.setColour(juce::Label::textColourId, HSTheme::STATUS_DISCONNECTED);
    bleStatusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bleStatusLabel);

    bleDeviceLabel.setFont(HSTheme::mono(12.0f, true));
    bleDeviceLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bleDeviceLabel);

    bleSignalLabel.setFont(HSTheme::mono(11.0f, true));
    bleSignalLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bleSignalLabel);

    bleLatencyLabel.setFont(HSTheme::mono(11.0f, true));
    bleLatencyLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bleLatencyLabel);

    blePacketsLabel.setFont(HSTheme::mono(11.0f, true));
    blePacketsLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(blePacketsLabel);

    deviceMonitorPanel = std::make_unique<RectPanel>(HSTheme::ACCENT_TEAL);
    addAndMakeVisible(*deviceMonitorPanel);

    deviceMonitorTitle.setText("DEVICE STATUS", juce::dontSendNotification);
    deviceMonitorTitle.setFont(HSTheme::label());
    deviceMonitorTitle.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    addAndMakeVisible(deviceMonitorTitle);

    deviceStatusLabel.setFont(HSTheme::mono(12.0f, false));
    deviceStatusLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_PRIMARY);
    deviceStatusLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(deviceStatusLabel);

    consoleView.setReadOnly(true);
    consoleView.setMultiLine(true);
    consoleView.setScrollbarsShown(true);
    consoleView.setFont(HSTheme::mono(12.0f, false));
    consoleView.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff001010));
    consoleView.setColour(juce::TextEditor::textColourId, HSTheme::TEXT_PRIMARY);
    consoleView.setColour(juce::TextEditor::outlineColourId, HSTheme::ACCENT_TEAL);
    addAndMakeVisible(consoleView);

    oscSectionLabel.setText("OSC OUTPUT", juce::dontSendNotification);
    oscSectionLabel.setFont(HSTheme::label());
    oscSectionLabel.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    addAndMakeVisible(oscSectionLabel);

    oscEnableToggle.setButtonText("Enable OSC");
    addAndMakeVisible(oscEnableToggle);

    oscHostLabel.setText("HOST", juce::dontSendNotification);
    oscHostLabel.setFont(HSTheme::mono(11.0f, true));
    oscHostLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    addAndMakeVisible(oscHostLabel);

    oscHostEditor.setInputRestrictions(0, "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:_");
    oscHostEditor.setFont(HSTheme::mono(12.0f, false));
    oscHostEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff000000));
    oscHostEditor.setColour(juce::TextEditor::textColourId, HSTheme::TEXT_PRIMARY);
    oscHostEditor.setColour(juce::TextEditor::outlineColourId, HSTheme::ACCENT_TEAL);
    oscHostEditor.setSelectAllWhenFocused(true);
    addAndMakeVisible(oscHostEditor);

    oscPortLabel.setText("PORT", juce::dontSendNotification);
    oscPortLabel.setFont(HSTheme::mono(11.0f, true));
    oscPortLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    addAndMakeVisible(oscPortLabel);

    oscPortSlider.setRange(1024.0, 65535.0, 1.0);
    oscPortSlider.setSliderStyle(juce::Slider::LinearBar);
    oscPortSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(oscPortSlider);

    oscStatusLabel.setFont(HSTheme::mono(11.0f, true));
    oscStatusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(oscStatusLabel);

    auto& vts = processor.getValueTreeState();
    lockAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(vts, HeartRateParams::UI_LOCKED, lockButton);
    oscEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(vts, HeartRateParams::OSC_ENABLED, oscEnableToggle);
    oscPortAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(vts, HeartRateParams::OSC_PORT, oscPortSlider);

    const juce::String initialHost = vts.state.getProperty(HeartRateParams::OSC_HOST, "127.0.0.1").toString();
    oscHostEditor.setText(initialHost, juce::dontSendNotification);
    oscHostEditor.onReturnKey = [this]() { oscHostEditor.giveAwayKeyboardFocus(); };
    oscHostEditor.onFocusLost = [this]() {
        auto text = oscHostEditor.getText().trim();
        if (text.isEmpty())
            text = "127.0.0.1";
        auto& state = processor.getValueTreeState().state;
        const auto current = state.getProperty(HeartRateParams::OSC_HOST, "127.0.0.1").toString();
        if (current != text)
            state.setProperty(HeartRateParams::OSC_HOST, text, nullptr);
    };

    startTimerHz(10);
    refreshDeviceList(true);
    syncParameterControls();
    refreshTelemetry();
    updateLockState();
    layoutPanels();
}

HeartSyncVST3AudioProcessorEditor::~HeartSyncVST3AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void HeartSyncVST3AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(HSTheme::SURFACE_BASE_START);

    juce::Rectangle<int> headerArea(0, 0, getWidth(), headerHeight + HSTheme::grid * 2);
    g.setColour(HSTheme::SURFACE_PANEL_LIGHT);
    g.fillRect(headerArea);

    g.setColour(HSTheme::ACCENT_TEAL.darker(0.6f));
    g.drawLine(0.0f, static_cast<float>(headerHeight + HSTheme::grid * 2), static_cast<float>(getWidth()),
               static_cast<float>(headerHeight + HSTheme::grid * 2), 2.0f);
}

void HeartSyncVST3AudioProcessorEditor::resized()
{
    layoutPanels();
}

void HeartSyncVST3AudioProcessorEditor::timerCallback()
{
    refreshTelemetry();
    syncParameterControls();
    updateLockState();

    if (++deviceRefreshCounter >= 20)
    {
        refreshDeviceList();
        deviceRefreshCounter = 0;
    }
}

void HeartSyncVST3AudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &scanButton)
    {
        processor.beginDeviceScan();
        bleStatusLabel.setText("Scanning for devices...", juce::dontSendNotification);
        return;
    }

    if (button == &connectButton)
    {
        const int selectedId = deviceCombo.getSelectedId();
        if (selectedId > 0 && selectedId <= deviceIds.size())
        {
            const auto deviceId = deviceIds[selectedId - 1];
            processor.connectToDevice(deviceId.toStdString());
        }
        return;
    }

    if (button == &disconnectButton)
    {
        processor.disconnectFromDevice();
        return;
    }

    if (button == &lockButton)
    {
        updateLockState();
        return;
    }
}

void HeartSyncVST3AudioProcessorEditor::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &deviceCombo)
    {
        const bool hasSelection = deviceCombo.getSelectedId() > 0;
        const bool locked = getParameterValue(HeartRateParams::UI_LOCKED) > 0.5f;
        const bool connected = processor.getTelemetrySnapshot().isConnected;
        connectButton.setEnabled(hasSelection && !locked && !connected);
    }
}

void HeartSyncVST3AudioProcessorEditor::layoutPanels()
{
    auto area = getLocalBounds().reduced(HSTheme::grid * 2, HSTheme::grid * 2);

    auto headerArea = area.removeFromTop(headerHeight);
    auto headerLeft = headerArea.removeFromLeft(static_cast<int>(headerArea.getWidth() * 0.6f));
    headerTitleLabel.setBounds(headerLeft.removeFromTop(36));
    headerSubtitleLabel.setBounds(headerLeft.removeFromTop(24));

    auto headerRight = headerArea.reduced(HSTheme::grid / 2, HSTheme::grid / 4);
    headerTimeLabel.setBounds(headerRight.removeFromTop(24));
    headerStatusLabel.setBounds(headerRight.removeFromTop(20));

    area.removeFromTop(HSTheme::grid);

    auto columnHeader = area.removeFromTop(columnHeaderHeight);
    auto valueHeadingBounds = columnHeader.removeFromLeft(valueColumnWidth);
    valuesHeading.setBounds(valueHeadingBounds.reduced(0, 4));

    columnHeader.removeFromLeft(HSTheme::grid);
    auto controlsHeadingBounds = columnHeader.removeFromLeft(controlsColumnWidth);
    controlsHeading.setBounds(controlsHeadingBounds);

    columnHeader.removeFromLeft(HSTheme::grid);
    waveformHeading.setBounds(columnHeader);

    auto placeRow = [this](MetricRow* row, juce::Rectangle<int>& bounds)
    {
        if (row == nullptr)
            return;
        auto rowBounds = bounds.removeFromTop(metricRowHeight);
        row->setBounds(rowBounds);
        bounds.removeFromTop(HSTheme::grid);
    };

    placeRow(hrRow.get(), area);
    placeRow(smoothRow.get(), area);
    placeRow(wetDryRow.get(), area);

    auto layoutParamBoxCentered = [](juce::Component* comp, const juce::Rectangle<int>& hostBounds, int width, int height, int centerYOffset)
    {
        if (comp == nullptr)
            return;
        juce::Rectangle<int> bounds(0, 0, width, height);
        bounds.setCentre({ hostBounds.getCentreX(), hostBounds.getCentreY() + centerYOffset });
        comp->setBounds(bounds);
    };

    if (hrControlsHost != nullptr && hrOffsetBox != nullptr)
    {
        auto hostBounds = hrControlsHost->getBounds().reduced(HSTheme::grid / 2);
        layoutParamBoxCentered(hrOffsetBox.get(), hostBounds, 140, 80, 0);
    }

    if (smoothingControlsHost != nullptr && smoothingBox != nullptr)
    {
        auto hostBounds = smoothingControlsHost->getBounds().reduced(HSTheme::grid / 2);
        layoutParamBoxCentered(smoothingBox.get(), hostBounds, 140, 80, -16);
        smoothingMetricsLabel.setBounds({ hostBounds.getX(), smoothingBox->getBounds().getBottom() + 6,
                                          hostBounds.getWidth(), 32 });
    }

    if (wetDryControlsHost != nullptr && wetDryOffsetBox != nullptr && wetDrySourceToggle != nullptr)
    {
        auto hostBounds = wetDryControlsHost->getBounds().reduced(HSTheme::grid / 2);
        layoutParamBoxCentered(wetDryOffsetBox.get(), hostBounds, 140, 80, -20);
        juce::Rectangle<int> toggleBounds(0, 0, 140, 50);
        toggleBounds.setCentre({ hostBounds.getCentreX(), wetDryOffsetBox->getBounds().getBottom() + 32 });
        wetDrySourceToggle->setBounds(toggleBounds);
    }

    area.removeFromTop(HSTheme::grid);

    auto bleBounds = area.removeFromTop(blePanelHeight);
    if (blePanel != nullptr)
        blePanel->setBounds(bleBounds);

    auto bleInner = bleBounds.reduced(HSTheme::grid);
    auto titleRow = bleInner.removeFromTop(26);
    statusIndicator.setBounds(titleRow.removeFromLeft(20));
    titleRow.removeFromLeft(HSTheme::grid / 2);
    lockButton.setBounds(titleRow.removeFromRight(buttonWidth));
    bleTitleLabel.setBounds(titleRow);

    bleInner.removeFromTop(HSTheme::grid / 2);
    auto deviceRow = bleInner.removeFromTop(32);
    scanButton.setBounds(deviceRow.removeFromLeft(buttonWidth));
    deviceRow.removeFromLeft(HSTheme::grid / 2);

    const int remainingWidth = deviceRow.getWidth() - (2 * buttonWidth + HSTheme::grid);
    deviceCombo.setBounds(deviceRow.removeFromLeft(std::max(remainingWidth, 160)));
    deviceRow.removeFromLeft(HSTheme::grid / 2);
    connectButton.setBounds(deviceRow.removeFromLeft(buttonWidth));
    deviceRow.removeFromLeft(HSTheme::grid / 2);
    disconnectButton.setBounds(deviceRow.removeFromLeft(buttonWidth));

    bleInner.removeFromTop(HSTheme::grid / 2);
    auto infoRow = bleInner.removeFromTop(24);
    bleStatusLabel.setBounds(infoRow.removeFromLeft(bleInner.getWidth() / 3));
    infoRow.removeFromLeft(HSTheme::grid / 2);
    bleDeviceLabel.setBounds(infoRow.removeFromLeft(bleInner.getWidth() / 3));
    infoRow.removeFromLeft(HSTheme::grid / 2);
    auto rightInfo = infoRow;
    bleSignalLabel.setBounds(rightInfo.removeFromLeft(110));
    bleLatencyLabel.setBounds(rightInfo.removeFromLeft(110));
    blePacketsLabel.setBounds(rightInfo);

    area.removeFromTop(HSTheme::grid);

    auto deviceBounds = area.removeFromTop(devicePanelHeight);
    if (deviceMonitorPanel != nullptr)
        deviceMonitorPanel->setBounds(deviceBounds);

    auto deviceInner = deviceBounds.reduced(HSTheme::grid);
    deviceMonitorTitle.setBounds(deviceInner.removeFromTop(24));
    deviceInner.removeFromTop(HSTheme::grid / 2);
    deviceStatusLabel.setBounds(deviceInner.removeFromTop(deviceInner.getHeight()));

    area.removeFromTop(HSTheme::grid);

    auto oscBounds = area.removeFromTop(oscPanelHeight);
    oscSectionLabel.setBounds(oscBounds.removeFromTop(24));

    auto oscControls = oscBounds.reduced(HSTheme::grid, HSTheme::grid / 2);
    oscEnableToggle.setBounds(oscControls.removeFromLeft(140));
    oscControls.removeFromLeft(HSTheme::grid);

    auto hostColumn = oscControls.removeFromLeft(240);
    oscHostLabel.setBounds(hostColumn.removeFromTop(18));
    oscHostEditor.setBounds(hostColumn.removeFromTop(30));

    oscControls.removeFromLeft(HSTheme::grid / 2);
    auto portColumn = oscControls.removeFromLeft(260);
    oscPortLabel.setBounds(portColumn.removeFromTop(18));
    oscPortSlider.setBounds(portColumn.removeFromTop(30));

    oscControls.removeFromLeft(HSTheme::grid / 2);
    oscStatusLabel.setBounds(oscControls);

    area.removeFromTop(HSTheme::grid);

    auto consoleBounds = area.removeFromTop(consoleHeight);
    consoleView.setBounds(consoleBounds);
}

void HeartSyncVST3AudioProcessorEditor::refreshTelemetry()
{
    auto snapshot = processor.getTelemetrySnapshot();
    lastSnapshot = snapshot;

    headerTimeLabel.setText(juce::Time::getCurrentTime().toString(true, true), juce::dontSendNotification);
    headerStatusLabel.setText(snapshot.statusText, juce::dontSendNotification);

    juce::Colour statusColour = HSTheme::STATUS_DISCONNECTED;
    if (snapshot.isConnected)
        statusColour = HSTheme::STATUS_CONNECTED;
    else if (processor.getBluetoothManager().isScanning())
        statusColour = HSTheme::STATUS_SCANNING;

    headerStatusLabel.setColour(juce::Label::textColourId, statusColour);
    statusIndicator.setColour(juce::Label::textColourId, statusColour);

    const auto formatBpm = [](float value)
    {
        return value > 0.0f ? juce::String(value, 1) + " BPM" : juce::String("-- BPM");
    };

    if (hrRow != nullptr)
    {
        hrRow->setValueText(formatBpm(snapshot.heart.rawBpm));
        hrRow->getGraph().push(snapshot.heart.rawBpm);
    }

    if (smoothRow != nullptr)
    {
        smoothRow->setValueText(formatBpm(snapshot.heart.smoothedBpm));
        smoothRow->getGraph().push(snapshot.heart.smoothedBpm);
    }

    if (wetDryRow != nullptr)
    {
        const auto wetDryText = snapshot.heart.wetDry > 0.0f ? juce::String(snapshot.heart.wetDry, 1) + " %"
                                                             : juce::String("-- %");
        wetDryRow->setValueText(wetDryText);
        wetDryRow->getGraph().push(snapshot.heart.wetDry);
    }

    const auto deviceName = snapshot.deviceName.isNotEmpty() ? snapshot.deviceName : juce::String("No device connected");
    bleStatusLabel.setText(snapshot.statusText, juce::dontSendNotification);
    bleStatusLabel.setColour(juce::Label::textColourId, statusColour);
    bleDeviceLabel.setText("Device: " + deviceName, juce::dontSendNotification);

    bleSignalLabel.setText(juce::String::formatted("Signal: %.0f%%", snapshot.signalQuality), juce::dontSendNotification);
    if (snapshot.signalQuality >= 80.0f)
        bleSignalLabel.setColour(juce::Label::textColourId, HSTheme::STATUS_CONNECTED);
    else if (snapshot.signalQuality >= 40.0f)
        bleSignalLabel.setColour(juce::Label::textColourId, HSTheme::STATUS_SCANNING);
    else
        bleSignalLabel.setColour(juce::Label::textColourId, HSTheme::STATUS_DISCONNECTED);

    bleLatencyLabel.setText(juce::String::formatted("Latency: %.0f ms", snapshot.lastLatencyMs), juce::dontSendNotification);
    blePacketsLabel.setText(juce::String::formatted("Packets: %d/%d", snapshot.packetsReceived, snapshot.packetsExpected),
                            juce::dontSendNotification);

    deviceStatusLabel.setText(juce::String::formatted("Connected: %s\nSignal Quality: %.0f%%\nLatency: %.0f ms",
                                                      snapshot.isConnected ? "Yes" : "No",
                                                      snapshot.signalQuality,
                                                      snapshot.lastLatencyMs),
                              juce::dontSendNotification);

    const bool locked = getParameterValue(HeartRateParams::UI_LOCKED) > 0.5f;
    connectButton.setEnabled(!locked && !snapshot.isConnected && deviceCombo.getSelectedId() > 0);
    disconnectButton.setEnabled(snapshot.isConnected);

    const bool oscEnabled = getParameterValue(HeartRateParams::OSC_ENABLED) > 0.5f;
    oscStatusLabel.setText(oscEnabled ? "OSC READY" : "OSC DISABLED", juce::dontSendNotification);
    oscStatusLabel.setColour(juce::Label::textColourId,
                             oscEnabled ? HSTheme::STATUS_CONNECTED : HSTheme::STATUS_DISCONNECTED);

    const auto hostProperty = processor.getValueTreeState().state.getProperty(HeartRateParams::OSC_HOST, "127.0.0.1").toString();
    if (!oscHostEditor.hasKeyboardFocus(true) && oscHostEditor.getText() != hostProperty)
        oscHostEditor.setText(hostProperty, juce::dontSendNotification);

    if (snapshot.consoleLog != lastConsole)
    {
        lastConsole = snapshot.consoleLog;
        juce::String joined = lastConsole.joinIntoString("\n");
        const bool atBottom = consoleView.getCaretPosition() == consoleView.getText().length();
        consoleView.setText(joined, juce::dontSendNotification);
        if (atBottom)
            consoleView.moveCaretToEnd();
    }
}

void HeartSyncVST3AudioProcessorEditor::refreshDeviceList(bool force)
{
    auto devices = processor.getDiscoveredDevices();

    juce::StringArray newIds;
    juce::StringArray newLabels;
    newIds.ensureStorageAllocated(static_cast<int>(devices.size()));
    newLabels.ensureStorageAllocated(static_cast<int>(devices.size()));

    juce::String previouslySelected;
    const int selectedId = deviceCombo.getSelectedId();
    if (selectedId > 0 && selectedId <= deviceIds.size())
        previouslySelected = deviceIds[selectedId - 1];

    int connectedIndex = -1;
    for (int i = 0; i < static_cast<int>(devices.size()); ++i)
    {
        const auto& device = devices[static_cast<size_t>(i)];
        const juce::String id(device.id.c_str());
        juce::String name(device.name.empty() ? "Unknown" : device.name.c_str());
        if (device.signalStrength != 0)
            name << "  (RSSI " << device.signalStrength << ")";
        newIds.add(id);
        newLabels.add(name);
        if (device.isConnected)
            connectedIndex = i;
    }

    if (!force && newIds == deviceIds)
        return;

    deviceCombo.clear(juce::NotificationType::dontSendNotification);
    deviceIds = newIds;
    for (int i = 0; i < newLabels.size(); ++i)
        deviceCombo.addItem(newLabels[i], i + 1);

    if (connectedIndex >= 0)
    {
        deviceCombo.setSelectedId(connectedIndex + 1, juce::dontSendNotification);
    }
    else if (previouslySelected.isNotEmpty())
    {
        const int index = deviceIds.indexOf(previouslySelected);
        if (index >= 0)
            deviceCombo.setSelectedId(index + 1, juce::dontSendNotification);
    }

    comboBoxChanged(&deviceCombo);
}

void HeartSyncVST3AudioProcessorEditor::updateLockState()
{
    const bool locked = getParameterValue(HeartRateParams::UI_LOCKED) > 0.5f;
    lockButton.setButtonText(locked ? "LOCKED" : "UNLOCKED");

    const auto setEnabledState = [locked](juce::Component* component)
    {
        if (component != nullptr)
            component->setEnabled(!locked);
    };

    setEnabledState(hrOffsetBox.get());
    setEnabledState(smoothingBox.get());
    setEnabledState(wetDryOffsetBox.get());
    setEnabledState(wetDrySourceToggle.get());
    setEnabledState(&deviceCombo);
    setEnabledState(&scanButton);
    setEnabledState(&oscHostEditor);
    setEnabledState(&oscPortSlider);
    setEnabledState(&oscEnableToggle);

    if (locked)
        connectButton.setEnabled(false);
}

void HeartSyncVST3AudioProcessorEditor::buildHROffsetControls(juce::Component& host)
{
    hrControlsHost = &host;
    hrOffsetBox = std::make_unique<ParamBox>("OFFSET", HSTheme::VITAL_HEART_RATE, "BPM", -100.0f, 100.0f, 1.0f);
    hrOffsetBox->onChange = [this](float value)
    {
        if (updatingParameterControls)
            return;
        setParameterValue(HeartRateParams::HEART_RATE_OFFSET, value);
    };
    host.addAndMakeVisible(*hrOffsetBox);
}

void HeartSyncVST3AudioProcessorEditor::buildSmoothControls(juce::Component& host)
{
    smoothingControlsHost = &host;
    smoothingBox = std::make_unique<ParamBox>("SMOOTH", HSTheme::VITAL_SMOOTHED, "x", 0.1f, 10.0f, 0.1f);
    smoothingBox->onChange = [this](float value)
    {
        if (updatingParameterControls)
            return;
        setParameterValue(HeartRateParams::SMOOTHING_FACTOR, value);
        updateSmoothingMetricsLabel(value);
    };
    host.addAndMakeVisible(*smoothingBox);

    smoothingMetricsLabel.setFont(HSTheme::mono(11.0f, false));
    smoothingMetricsLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    smoothingMetricsLabel.setJustificationType(juce::Justification::centred);
    host.addAndMakeVisible(smoothingMetricsLabel);
}

void HeartSyncVST3AudioProcessorEditor::buildWetDryControls(juce::Component& host)
{
    wetDryControlsHost = &host;
    wetDryOffsetBox = std::make_unique<ParamBox>("OFFSET", HSTheme::VITAL_WET_DRY, "%", -100.0f, 100.0f, 1.0f);
    wetDryOffsetBox->onChange = [this](float value)
    {
        if (updatingParameterControls)
            return;
        setParameterValue(HeartRateParams::WET_DRY_OFFSET, value);
    };
    host.addAndMakeVisible(*wetDryOffsetBox);

    wetDrySourceToggle = std::make_unique<ParamToggle>("SMOOTHED", "RAW");
    wetDrySourceToggle->setColours(HSTheme::VITAL_SMOOTHED, HSTheme::SURFACE_BASE_START,
                                   juce::Colour(0xff1a1a1a), HSTheme::VITAL_HEART_RATE,
                                   HSTheme::ACCENT_TEAL);
    wetDrySourceToggle->onChange = [this](bool on)
    {
        if (updatingParameterControls)
            return;
        setChoiceParameterIndex(HeartRateParams::WET_DRY_SOURCE, on ? 0 : 1);
    };
    host.addAndMakeVisible(*wetDrySourceToggle);
}

void HeartSyncVST3AudioProcessorEditor::updateSmoothingMetricsLabel(float /*smoothingFactor*/)
{
    const auto metrics = processor.getSmoothingMetrics();
    smoothingMetricsLabel.setText(juce::String::formatted("alpha=%.3f  T1/2=%.2fs  window~= %d",
                                                         metrics.alpha,
                                                         metrics.halfLifeSeconds,
                                                         metrics.effectiveWindowSamples),
                                  juce::dontSendNotification);
}

void HeartSyncVST3AudioProcessorEditor::syncParameterControls()
{
    updatingParameterControls = true;

    if (hrOffsetBox != nullptr)
        hrOffsetBox->setValue(getParameterValue(HeartRateParams::HEART_RATE_OFFSET));

    if (smoothingBox != nullptr)
    {
        const float smoothing = getParameterValue(HeartRateParams::SMOOTHING_FACTOR);
        smoothingBox->setValue(smoothing);
        updateSmoothingMetricsLabel(smoothing);
    }

    if (wetDryOffsetBox != nullptr)
        wetDryOffsetBox->setValue(getParameterValue(HeartRateParams::WET_DRY_OFFSET));

    if (wetDrySourceToggle != nullptr)
    {
        const int index = getChoiceParameterIndex(HeartRateParams::WET_DRY_SOURCE);
        wetDrySourceToggle->setState(index == 0);
    }

    updatingParameterControls = false;
}

float HeartSyncVST3AudioProcessorEditor::getParameterValue(const juce::String& paramId) const
{
    if (auto* param = processor.getValueTreeState().getParameter(paramId))
        return param->convertFrom0to1(param->getValue());
    return 0.0f;
}

void HeartSyncVST3AudioProcessorEditor::setParameterValue(const juce::String& paramId, float newValue)
{
    if (auto* param = processor.getValueTreeState().getParameter(paramId))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(newValue));
        param->endChangeGesture();
    }
}

int HeartSyncVST3AudioProcessorEditor::getChoiceParameterIndex(const juce::String& paramId) const
{
    if (auto* param = processor.getValueTreeState().getParameter(paramId))
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(param))
            return choice->getIndex();
    return 0;
}

void HeartSyncVST3AudioProcessorEditor::setChoiceParameterIndex(const juce::String& paramId, int newIndex)
{
    if (auto* param = processor.getValueTreeState().getParameter(paramId))
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(param))
        {
            newIndex = juce::jlimit(0, choice->choices.size() - 1, newIndex);
            choice->beginChangeGesture();
            choice->setValueNotifyingHost(choice->convertTo0to1(static_cast<float>(newIndex)));
            choice->endChangeGesture();
        }
}

