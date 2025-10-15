#include "PluginEditor.h"

HeartSyncEditor::HeartSyncEditor(HeartSyncProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(1180, 740);
    setLookAndFeel(&lnf);

    // Create three stacked metric rows
    rowHR = std::make_unique<MetricRow>("HEART RATE", "BPM", HSTheme::VITAL_HEART_RATE,
        [this](juce::Component& host) { buildHROffsetControls(host); });
    addAndMakeVisible(*rowHR);
    rowHR->getGraph().setLineColour(HSTheme::VITAL_HEART_RATE);

    rowSmooth = std::make_unique<MetricRow>("SMOOTHED HR", "BPM", HSTheme::VITAL_SMOOTHED,
        [this](juce::Component& host) { buildSmoothControls(host); });
    addAndMakeVisible(*rowSmooth);
    rowSmooth->getGraph().setLineColour(HSTheme::VITAL_SMOOTHED);

    rowWetDry = std::make_unique<MetricRow>("WET/DRY RATIO", "", HSTheme::VITAL_WET_DRY,
        [this](juce::Component& host) { buildWetDryControls(host); });
    addAndMakeVisible(*rowWetDry);
    rowWetDry->getGraph().setLineColour(HSTheme::VITAL_WET_DRY);

    // Header (left title/subtitle, right clock/status) to mirror Python
    // Simplify header title to avoid special glyph rendering issues in some hosts
    headerTitleLeft.setText("HEART SYNC SYSTEM", juce::dontSendNotification);
    headerTitleLeft.setFont(juce::Font(20.0f, juce::Font::bold));
    headerTitleLeft.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    addAndMakeVisible(headerTitleLeft);

    headerSubtitleLeft.setText("Adaptive Audio Bio Technology", juce::dontSendNotification);
    headerSubtitleLeft.setFont(juce::Font(11.0f));
    headerSubtitleLeft.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    addAndMakeVisible(headerSubtitleLeft);

    headerClockRight.setText("", juce::dontSendNotification);
    headerClockRight.setFont(HSTheme::mono(14.0f, true));
    headerClockRight.setColour(juce::Label::textColourId, HSTheme::TEXT_PRIMARY);
    headerClockRight.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(headerClockRight);

    headerStatusRight.setText(juce::CharPointer_UTF8("◆ SYSTEM OPERATIONAL"), juce::dontSendNotification);
    headerStatusRight.setFont(juce::Font(11.0f, juce::Font::bold));
    headerStatusRight.setColour(juce::Label::textColourId, HSTheme::STATUS_CONNECTED);
    headerStatusRight.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(headerStatusRight);

    // BLE setup
    bleTitle.setText("BLUETOOTH LE CONNECTIVITY", juce::dontSendNotification);
    bleTitle.setFont(HSTheme::label());
    bleTitle.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    addAndMakeVisible(bleTitle);

    scanBtn.onClick = [this] { scanForDevices(); };
    addAndMakeVisible(scanBtn);

    connectBtn.onClick = [this] {
        if (deviceBox.getSelectedItemIndex() >= 0)
            connectToDevice(deviceBox.getText());
    };
    addAndMakeVisible(connectBtn);

    lockBtn.onClick = [this] {
        deviceLocked = !deviceLocked;
        lockBtn.setButtonText(deviceLocked ? "UNLOCK" : "LOCK");
    };
    addAndMakeVisible(lockBtn);

    disconnectBtn.onClick = [this] { processorRef.disconnectDevice(); };
    addAndMakeVisible(disconnectBtn);

    deviceBox.setTextWhenNoChoicesAvailable("No devices found");
    deviceBox.setTextWhenNothingSelected("Select device...");
    addAndMakeVisible(deviceBox);

    statusDot.setText(juce::CharPointer_UTF8("●"), juce::dontSendNotification);
    statusDot.setFont(juce::Font(18.0f));
    statusDot.setColour(juce::Label::textColourId, HSTheme::STATUS_DISCONNECTED);
    addAndMakeVisible(statusDot);

    statusLabel.setText("DISCONNECTED", juce::dontSendNotification);
    statusLabel.setFont(HSTheme::mono(12.0f, true));
    statusLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    addAndMakeVisible(statusLabel);

    terminalTitle.setText("DEVICE STATUS MONITOR", juce::dontSendNotification);
    terminalTitle.setFont(HSTheme::label());
    terminalTitle.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    addAndMakeVisible(terminalTitle);

    terminalLabel.setText("[ WAITING ]", juce::dontSendNotification);
    terminalLabel.setFont(HSTheme::mono(10.0f, false));
    terminalLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    addAndMakeVisible(terminalLabel);

    wireClientCallbacks();
    startTimer(100);
    isInitialized = true;
}

HeartSyncEditor::~HeartSyncEditor()
{
    setLookAndFeel(nullptr);
}

void HeartSyncEditor::buildHROffsetControls(juce::Component& host)
{
    hrOffsetBox = std::make_unique<ParamBox>("HR OFFSET", HSTheme::VITAL_HEART_RATE, 
                                             "BPM", -100.0f, 100.0f, 1.0f);
    hrOffsetBox->setValue(0.0f);
    hrOffsetBox->onChange = [this](float v) { hrOffset = (int)v; };
    host.addAndMakeVisible(*hrOffsetBox);
    hrOffsetBox->setBounds(HSTheme::grid, HSTheme::grid, 120, 72);
}

void HeartSyncEditor::buildSmoothControls(juce::Component& host)
{
    smoothBox = std::make_unique<ParamBox>("SMOOTH", HSTheme::VITAL_SMOOTHED, 
                                           "x", 0.1f, 10.0f, 0.1f);
    smoothBox->setValue(0.1f);
    smoothBox->onChange = [this](float v) { smoothing = v; updateSmoothMetrics(); };
    host.addAndMakeVisible(*smoothBox);
    smoothBox->setBounds(HSTheme::grid, HSTheme::grid, 120, 72);

    smoothMetricsLabel.setFont(HSTheme::mono(9.0f, false));
    smoothMetricsLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    smoothMetricsLabel.setJustificationType(juce::Justification::topLeft);
    host.addAndMakeVisible(smoothMetricsLabel);
    smoothMetricsLabel.setBounds(HSTheme::grid, 88, 180, 60);
    updateSmoothMetrics();
}

void HeartSyncEditor::buildWetDryControls(juce::Component& host)
{
    wetDrySourceToggle = std::make_unique<ParamToggle>("SMOOTHED HR", "RAW HR");
    wetDrySourceToggle->setColours(juce::Colour(0xff004d44), HSTheme::VITAL_SMOOTHED,
                                   juce::Colour(0xff3a0000), HSTheme::VITAL_HEART_RATE,
                                   HSTheme::ACCENT_TEAL);
    wetDrySourceToggle->setState(true);
    wetDrySourceToggle->onChange = [this](bool on) { useSmoothedForWetDry = on; };
    host.addAndMakeVisible(*wetDrySourceToggle);
    wetDrySourceToggle->setBounds(HSTheme::grid, HSTheme::grid, 120, 48);

    wetDryBox = std::make_unique<ParamBox>("WET/DRY", HSTheme::VITAL_WET_DRY, 
                                           "%", -100.0f, 100.0f, 1.0f);
    wetDryBox->setValue(0.0f);
    wetDryBox->onChange = [this](float v) { wetDryOffset = (int)v; };
    host.addAndMakeVisible(*wetDryBox);
    wetDryBox->setBounds(HSTheme::grid, 52, 120, 72);
}

void HeartSyncEditor::updateSmoothMetrics()
{
    const float alpha = 1.0f / (1.0f + smoothing);
    const float halfLifeSamples = std::log(0.5f) / std::log(1.0f - alpha);
    const float halfLifeSeconds = halfLifeSamples * 0.025f;
    const int effectiveWindow = (int)std::round(halfLifeSamples * 5.0f);
    juce::String metrics = juce::String::formatted("α=%.3f\nT½=%.2fs\n≈%d samples",
                                                   alpha, halfLifeSeconds, effectiveWindow);
    smoothMetricsLabel.setText(metrics, juce::dontSendNotification);
}

void HeartSyncEditor::paint(juce::Graphics& g)
{
    g.fillAll(HSTheme::SURFACE_BASE_START);
    auto r = getLocalBounds().toFloat();

    // Header
    auto header = r.removeFromTop((float)HSTheme::headerH);
    g.setColour(HSTheme::SURFACE_PANEL_LIGHT);
    g.fillRect(header);
    // left/right header controls are Components; draw a tiny verification tag at far left
    g.setColour(HSTheme::ACCENT_TEAL_DARK);
    g.setFont(juce::Font(10.0f));
    g.drawText("3x3 STACK ACTIVE", header.removeFromLeft(140.0f).reduced(8, 4), juce::Justification::centredLeft);

    // Section headings
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

    g.setColour(HSTheme::ACCENT_TEAL);
    g.drawText("WAVEFORM", sectionRow, juce::Justification::centredLeft);
    g.setColour(HSTheme::ACCENT_TEAL_DARK);
    g.fillRect(sectionRow.removeFromBottom(2.0f));
}

void HeartSyncEditor::resized()
{
    auto r = getLocalBounds();
    // Header layout
    auto header = r.removeFromTop(HSTheme::headerH);
    auto left = header.removeFromLeft(header.proportionOfWidth(0.6f)).reduced(HSTheme::grid);
    auto right = header.reduced(HSTheme::grid);
    auto t = left.removeFromTop(36);
    headerTitleLeft.setBounds(t);
    headerSubtitleLeft.setBounds(left.removeFromTop(20));
    headerClockRight.setBounds(right.removeFromTop(28));
    headerStatusRight.setBounds(right.removeFromTop(20));

    // Section headings row under header
    r.removeFromTop(40);

    // Reserve bottom areas first to ensure rows always have equal space above
    const int bleBarHeight = 96;
    const int terminalHeight = 72;
    auto terminal = r.removeFromBottom(terminalHeight).reduced(HSTheme::grid);
    auto bleBar   = r.removeFromBottom(bleBarHeight).reduced(HSTheme::grid);

    // Now r contains only the area for the 3 stacked metric rows
    const int minRowH = 110;
    int rowHeight = juce::jmax(minRowH, r.getHeight() / 3);
    // If rounding leaves a few pixels, give them to the last row
    int leftover = r.getHeight() - (rowHeight * 3);

    auto row1 = r.removeFromTop(rowHeight);
    auto row2 = r.removeFromTop(rowHeight);
    auto row3 = r; // whatever remains
    if (leftover > 0)
        row3 = row3.withHeight(row3.getHeight() + leftover);

    rowHR->setBounds(row1);
    rowSmooth->setBounds(row2);
    rowWetDry->setBounds(row3);

    // BLE bar content (match Python: SCAN | CONNECT | LOCK | DISCONNECT | device dropdown on right)
    bleTitle.setBounds(bleBar.removeFromTop(24));
    auto bleControls = bleBar.removeFromTop(40);
    scanBtn.setBounds(bleControls.removeFromLeft(90).reduced(4));
    bleControls.removeFromLeft(HSTheme::grid);
    connectBtn.setBounds(bleControls.removeFromLeft(110).reduced(4));
    bleControls.removeFromLeft(HSTheme::grid);
    lockBtn.setBounds(bleControls.removeFromLeft(90).reduced(4));
    bleControls.removeFromLeft(HSTheme::grid);
    disconnectBtn.setBounds(bleControls.removeFromLeft(130).reduced(4));
    bleControls.removeFromLeft(HSTheme::grid);
    // Device dropdown anchored to the remaining right side
    deviceBox.setBounds(bleControls);

    auto bleStatus = bleBar.removeFromTop(24);
    statusDot.setBounds(bleStatus.removeFromLeft(24));
    statusLabel.setBounds(bleStatus.removeFromLeft(240));

    // Terminal panel under BLE
    terminalTitle.setBounds(terminal.removeFromTop(20));
    terminalLabel.setFont(HSTheme::mono(10.0f, true));
    terminalLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff001a1a));
    terminalLabel.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    terminalLabel.setJustificationType(juce::Justification::centredLeft);
    terminalLabel.setBounds(terminal);
}

void HeartSyncEditor::timerCallback()
{
    headerClockRight.setText(juce::Time::getCurrentTime().toString(true, true), juce::dontSendNotification);
    auto bioData = processorRef.getCurrentBiometricData();
    if (bioData.isDataValid)
    {
        rowHR->setValueText(juce::String(juce::roundToInt(bioData.rawHeartRate)));
        rowSmooth->setValueText(juce::String(juce::roundToInt(bioData.smoothedHeartRate)));
        rowWetDry->setValueText(juce::String(juce::roundToInt(bioData.wetDryRatio)));
        rowHR->getGraph().push(bioData.rawHeartRate);
        rowSmooth->getGraph().push(bioData.smoothedHeartRate);
        rowWetDry->getGraph().push(bioData.wetDryRatio);
    }
    repaint();
}

void HeartSyncEditor::wireClientCallbacks() {}
void HeartSyncEditor::scanForDevices() { processorRef.startDeviceScan(); }
void HeartSyncEditor::connectToDevice(const juce::String& deviceAddress) 
{ 
    processorRef.connectToDevice(deviceAddress.toStdString());
}
