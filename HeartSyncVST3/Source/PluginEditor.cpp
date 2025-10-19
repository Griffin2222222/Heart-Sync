#include "PluginEditor.h"
#include <unordered_set>

HeartSyncEditor::HeartSyncEditor(HeartSyncProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&lnf);
    setOpaque(true);

    // Header glyphs and labels (mirrors Python layout)
    headerSettingsIcon.setText(juce::CharPointer_UTF8("\xE2\x9A\x99"), juce::dontSendNotification);
    headerSettingsIcon.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    headerSettingsIcon.setFont(juce::Font(16.0f));
    headerSettingsIcon.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(headerSettingsIcon);

    headerGlyph.setText(juce::CharPointer_UTF8("◆"), juce::dontSendNotification);
    headerGlyph.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    headerGlyph.setFont(juce::Font(18.0f, juce::Font::bold));
    headerGlyph.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(headerGlyph);

    headerTitleLeft.setText("HEART SYNC SYSTEM", juce::dontSendNotification);
    headerTitleLeft.setFont(HSTheme::heading());
    headerTitleLeft.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    headerTitleLeft.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(headerTitleLeft);

    headerSubtitleLeft.setText("Adaptive Audio Bio Technology", juce::dontSendNotification);
    headerSubtitleLeft.setFont(HSTheme::caption());
    headerSubtitleLeft.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    headerSubtitleLeft.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(headerSubtitleLeft);

    headerClockRight.setText("", juce::dontSendNotification);
    headerClockRight.setFont(HSTheme::mono(13.0f, true));
    headerClockRight.setColour(juce::Label::textColourId, HSTheme::TEXT_PRIMARY);
    headerClockRight.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(headerClockRight);

    headerStatusRight.setText(juce::CharPointer_UTF8("◆ SYSTEM OPERATIONAL"), juce::dontSendNotification);
    headerStatusRight.setFont(HSTheme::mono(11.0f, true));
    headerStatusRight.setColour(juce::Label::textColourId, HSTheme::STATUS_CONNECTED);
    headerStatusRight.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(headerStatusRight);

    // BLE setup (buttons share the same styling)
    bleTitle.setText("BLUETOOTH LE CONNECTIVITY", juce::dontSendNotification);
    bleTitle.setFont(HSTheme::label());
    bleTitle.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    bleTitle.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bleTitle);

    auto configureButton = [](juce::TextButton& btn)
    {
        btn.setColour(juce::TextButton::buttonColourId, HSTheme::SURFACE_PANEL_LIGHT);
        btn.setColour(juce::TextButton::textColourOffId, HSTheme::ACCENT_TEAL);
        btn.setColour(juce::TextButton::textColourOnId, HSTheme::SURFACE_BASE_START);
    };

    configureButton(scanBtn);
    configureButton(connectBtn);
    configureButton(lockBtn);
    configureButton(disconnectBtn);

    // SCAN toggles scanning on/off
    scanBtn.setClickingTogglesState(true);
    scanBtn.onClick = [this] 
    { 
        if (scanBtn.getToggleState())
        {
            scanBtn.setButtonText("STOP");
            scanForDevices();
        }
        else
        {
            scanBtn.setButtonText("SCAN");
            processorRef.stopDeviceScan();
            appendTerminal("Scan stopped");
        }
    };
    addAndMakeVisible(scanBtn);

    connectBtn.onClick = [this] { connectSelectedDevice(); };
    addAndMakeVisible(connectBtn);

    lockBtn.setClickingTogglesState(true);
    lockBtn.setToggleState(true, juce::dontSendNotification);
    deviceLocked = true;
    lockBtn.onClick = [this] { deviceLocked = lockBtn.getToggleState(); syncBleControls(); };
    addAndMakeVisible(lockBtn);

    disconnectBtn.onClick = [this] {
        processorRef.disconnectDevice();
        appendTerminal("Disconnect requested");
        updateBluetoothStatus();
    };
    addAndMakeVisible(disconnectBtn);

    deviceLabel.setText("DEVICE:", juce::dontSendNotification);
    deviceLabel.setFont(HSTheme::mono(11.0f, true));
    deviceLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    deviceLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(deviceLabel);

    deviceBox.setJustificationType(juce::Justification::centredLeft);
    deviceBox.setTextWhenNoChoicesAvailable("No devices found");
    deviceBox.setTextWhenNothingSelected("Select device...");
    deviceBox.setColour(juce::ComboBox::backgroundColourId, HSTheme::SURFACE_PANEL_LIGHT);
    deviceBox.setColour(juce::ComboBox::textColourId, HSTheme::TEXT_PRIMARY);
    
    // Simple onChange - just sync controls, NO auto-connect
    deviceBox.onChange = [this] { syncBleControls(); };
    
    addAndMakeVisible(deviceBox);

    statusDot.setText(juce::CharPointer_UTF8("●"), juce::dontSendNotification);
    statusDot.setFont(juce::Font(14.0f));
    statusDot.setColour(juce::Label::textColourId, HSTheme::STATUS_DISCONNECTED);
    statusDot.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusDot);

    statusLabel.setText("DISCONNECTED", juce::dontSendNotification);
    statusLabel.setFont(HSTheme::mono(11.0f, true));
    statusLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    terminalTitle.setText("DEVICE STATUS MONITOR", juce::dontSendNotification);
    terminalTitle.setFont(HSTheme::label());
    terminalTitle.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    terminalTitle.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(terminalTitle);
    terminalOutput.setMultiLine(true);
    terminalOutput.setReadOnly(true);
    terminalOutput.setScrollbarsShown(true);
    terminalOutput.setReturnKeyStartsNewLine(false);
    terminalOutput.setCaretVisible(false);
    terminalOutput.setPopupMenuEnabled(false); // custom menu handles copy options
    terminalOutput.setFont(HSTheme::mono(10.0f, true));
    terminalOutput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff001818));
    terminalOutput.setColour(juce::TextEditor::textColourId, HSTheme::TEXT_SECONDARY);
    terminalOutput.setColour(juce::TextEditor::highlightColourId, HSTheme::ACCENT_TEAL.withAlpha(0.35f));
    terminalOutput.setColour(juce::TextEditor::highlightedTextColourId, HSTheme::SURFACE_BASE_START);
    terminalOutput.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    terminalOutput.setScrollBarThickness(10);
    terminalOutput.setText("[ WAITING ]  |  DEVICE: ---  |  ADDR: ---  |  BAT: --%  |  BPM: ---", false);
    terminalOutput.addMouseListener(this, false);
    addAndMakeVisible(terminalOutput);

    // Create three stacked metric rows
    rowHR = std::make_unique<MetricRow>("HEART RATE", "BPM", HSTheme::VITAL_HEART_RATE,
        [this](juce::Component& host) { buildHROffsetControls(host); });
    addAndMakeVisible(*rowHR);
    rowHR->getGraph().setLineColour(HSTheme::VITAL_HEART_RATE);
    rowHR->getGraph().setYAxisLabel("BPM");
    rowHR->getGraph().setFixedRange(40.0f, 200.0f);
    
    // Add tempo sync callback
    rowHR->onTempoSyncRequested = [this](bool enable)
    {
        if (enable)
        {
            processorRef.setTempoSyncSource(HeartSyncProcessor::TempoSyncSource::RawHeartRate);
            appendTerminal("Tempo sync: Raw Heart Rate -> Session Tempo");
        }
        else
        {
            processorRef.setTempoSyncSource(HeartSyncProcessor::TempoSyncSource::Off);
            appendTerminal("Tempo sync disabled");
        }
        
        // Force immediate UI update
        rowHR->setTempoSyncActive(enable);
        rowSmooth->setTempoSyncActive(false);
        rowWetDry->setTempoSyncActive(false);
        repaint();
    };

    rowSmooth = std::make_unique<MetricRow>("SMOOTHED HR", "BPM", HSTheme::VITAL_SMOOTHED,
        [this](juce::Component& host) { buildSmoothControls(host); });
    addAndMakeVisible(*rowSmooth);
    rowSmooth->getGraph().setLineColour(HSTheme::VITAL_SMOOTHED);
    rowSmooth->getGraph().setYAxisLabel("BPM");
    rowSmooth->getGraph().setFixedRange(40.0f, 200.0f);
    
    // Add tempo sync callback
    rowSmooth->onTempoSyncRequested = [this](bool enable)
    {
        if (enable)
        {
            processorRef.setTempoSyncSource(HeartSyncProcessor::TempoSyncSource::SmoothedHeartRate);
            appendTerminal("Tempo sync: Smoothed HR -> Session Tempo");
        }
        else
        {
            processorRef.setTempoSyncSource(HeartSyncProcessor::TempoSyncSource::Off);
            appendTerminal("Tempo sync disabled");
        }
        
        // Force immediate UI update
        rowHR->setTempoSyncActive(false);
        rowSmooth->setTempoSyncActive(enable);
        rowWetDry->setTempoSyncActive(false);
        repaint();
    };

    rowWetDry = std::make_unique<MetricRow>("WET/DRY RATIO", "", HSTheme::VITAL_WET_DRY,
        [this](juce::Component& host) { buildWetDryControls(host); });
    addAndMakeVisible(*rowWetDry);
    rowWetDry->getGraph().setLineColour(HSTheme::VITAL_WET_DRY);
    rowWetDry->getGraph().setYAxisLabel("%");
    rowWetDry->getGraph().setFixedRange(0.0f, 100.0f);
    
    // Add tempo sync callback
    rowWetDry->onTempoSyncRequested = [this](bool enable)
    {
        if (enable)
        {
            processorRef.setTempoSyncSource(HeartSyncProcessor::TempoSyncSource::WetDryRatio);
            appendTerminal("Tempo sync: Wet/Dry Ratio -> Session Tempo");
        }
        else
        {
            processorRef.setTempoSyncSource(HeartSyncProcessor::TempoSyncSource::Off);
            appendTerminal("Tempo sync disabled");
        }
        
        // Force immediate UI update
        rowHR->setTempoSyncActive(false);
        rowSmooth->setTempoSyncActive(false);
        rowWetDry->setTempoSyncActive(enable);
        repaint();
    };

    terminalLines.clear();
    updateTerminalLabel();

    wireClientCallbacks();
    refreshDeviceDropdown();
    updateBluetoothStatus();
    syncBleControls();
    isInitialized = true;
    
    // Do NOT auto-start scanning - user must manually click SCAN button
    
    setSize(1180, 740); // triggers resized once all children exist
    startTimer(100);
}

HeartSyncEditor::~HeartSyncEditor()
{
    processorRef.onDeviceListUpdated = nullptr;
    processorRef.onBluetoothStateChanged = nullptr;
    processorRef.onSystemMessage = nullptr;
    processorRef.onBiometricDataUpdated = nullptr;
    setLookAndFeel(nullptr);
}

void HeartSyncEditor::buildHROffsetControls(juce::Component& host)
{
    hrOffsetBox = std::make_unique<ParamBox>("HR OFFSET", HSTheme::VITAL_HEART_RATE, 
                                             "BPM", -100.0f, 100.0f, 1.0f, 0.0f);
    
    // Get the actual VST3 parameter
    auto* param = processorRef.getParameters().getParameter(HeartSyncProcessor::PARAM_HEART_RATE_OFFSET);
    if (param)
    {
        // Set initial value from parameter
        float paramValue = param->getValue() * 200.0f - 100.0f; // denormalize from [0,1] to [-100,100]
        hrOffsetBox->setValue(paramValue, false);
        
        // Connect onChange to update the parameter
        hrOffsetBox->onChange = [this, param](float v) 
        { 
            hrOffset = (int)v;
            // Normalize to [0,1] range for VST3
            float normalized = (v + 100.0f) / 200.0f;
            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalized));
        };
    }
    else
    {
        hrOffsetBox->setValue(0.0f, false);
        hrOffsetBox->onChange = [this](float v) { hrOffset = (int)v; };
    }
    
    host.addAndMakeVisible(*hrOffsetBox);

    if (auto* layoutHost = dynamic_cast<MetricRow::ControlsHost*>(&host))
    {
        layoutHost->onLayout = [box = hrOffsetBox.get()](const juce::Rectangle<int>& bounds)
        {
            if (box == nullptr)
                return;

            auto area = bounds.reduced(8, 6);
            auto boxBounds = juce::Rectangle<int>(160, 76);
            boxBounds.setCentre(area.getCentre());
            box->setBounds(boxBounds);
        };
    }
}

void HeartSyncEditor::buildSmoothControls(juce::Component& host)
{
    smoothBox = std::make_unique<ParamBox>("SMOOTH", HSTheme::VITAL_SMOOTHED, 
                                           "x", 0.01f, 1.0f, 0.01f, 0.1f);
    
    // Get the actual VST3 parameter
    auto* param = processorRef.getParameters().getParameter(HeartSyncProcessor::PARAM_SMOOTHING_FACTOR);
    if (param)
    {
        // Set initial value from parameter (already in [0.01, 1.0] range)
        float paramValue = param->getValue();
        smoothBox->setValue(paramValue, false);
        
        // Connect onChange to update the parameter
        smoothBox->onChange = [this, param](float v) 
        { 
            smoothing = v;
            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, v));
            updateSmoothMetrics();
        };
    }
    else
    {
        smoothBox->setValue(0.1f, false);
        smoothBox->onChange = [this](float v) { smoothing = v; updateSmoothMetrics(); };
    }
    
    host.addAndMakeVisible(*smoothBox);

    smoothMetricsLabel.setFont(HSTheme::mono(9.0f, false));
    smoothMetricsLabel.setColour(juce::Label::textColourId, HSTheme::TEXT_SECONDARY);
    smoothMetricsLabel.setJustificationType(juce::Justification::topLeft);
    host.addAndMakeVisible(smoothMetricsLabel);
    updateSmoothMetrics();

    if (auto* layoutHost = dynamic_cast<MetricRow::ControlsHost*>(&host))
    {
        layoutHost->onLayout = [box = smoothBox.get(), metrics = &smoothMetricsLabel](const juce::Rectangle<int>& bounds)
        {
            if (box == nullptr)
                return;

            auto area = bounds.reduced(8, 6);
            auto top = area.removeFromTop(90);

            auto boxBounds = juce::Rectangle<int>(160, 76);
            boxBounds.setCentre(top.getCentre());
            box->setBounds(boxBounds);

            if (metrics != nullptr)
            {
                auto metricsArea = area.reduced(4);
                metrics->setBounds(metricsArea.removeFromBottom(48));
            }
        };
    }
}

void HeartSyncEditor::buildWetDryControls(juce::Component& host)
{
    wetDrySourceToggle = std::make_unique<ParamToggle>("SMOOTHED HR", "RAW HR");
    wetDrySourceToggle->setColours(juce::Colour(0xff004d44), HSTheme::VITAL_SMOOTHED,
                                   juce::Colour(0xff3a0000), HSTheme::VITAL_HEART_RATE,
                                   HSTheme::ACCENT_TEAL);
    
    // Get the actual VST3 parameter for input source
    auto* sourceParam = processorRef.getParameters().getParameter(HeartSyncProcessor::PARAM_WET_DRY_INPUT_SOURCE);
    if (sourceParam)
    {
        // Set initial state from parameter (true = smoothed)
        bool useSmoothed = sourceParam->getValue() > 0.5f;
        wetDrySourceToggle->setState(useSmoothed);
        
        // Connect onChange to update the parameter
        wetDrySourceToggle->onChange = [this, sourceParam](bool on) 
        { 
            useSmoothedForWetDry = on;
            sourceParam->setValueNotifyingHost(on ? 1.0f : 0.0f);
        };
    }
    else
    {
        wetDrySourceToggle->setState(true);
        wetDrySourceToggle->onChange = [this](bool on) { useSmoothedForWetDry = on; };
    }
    
    host.addAndMakeVisible(*wetDrySourceToggle);

    wetDryBox = std::make_unique<ParamBox>("WET/DRY OFFSET", HSTheme::VITAL_WET_DRY, 
                                           "%", -100.0f, 100.0f, 1.0f, 0.0f);
    
    // Get the actual VST3 parameter
    auto* param = processorRef.getParameters().getParameter(HeartSyncProcessor::PARAM_WET_DRY_OFFSET);
    if (param)
    {
        // Set initial value from parameter
        float paramValue = param->getValue() * 200.0f - 100.0f; // denormalize from [0,1] to [-100,100]
        wetDryBox->setValue(paramValue, false);
        
        // Connect onChange to update the parameter
        wetDryBox->onChange = [this, param](float v) 
        { 
            wetDryOffset = (int)v;
            // Normalize to [0,1] range for VST3
            float normalized = (v + 100.0f) / 200.0f;
            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalized));
        };
    }
    else
    {
        wetDryBox->setValue(0.0f, false);
        wetDryBox->onChange = [this](float v) { wetDryOffset = (int)v; };
    }
    
    host.addAndMakeVisible(*wetDryBox);

    if (auto* layoutHost = dynamic_cast<MetricRow::ControlsHost*>(&host))
    {
        layoutHost->onLayout = [toggle = wetDrySourceToggle.get(), box = wetDryBox.get()]
                               (const juce::Rectangle<int>& bounds)
        {
            auto area = bounds.reduced(10, 8);
            constexpr int toggleHeight = 48;
            constexpr int boxHeight = 76;
            constexpr int boxWidth = 160;
            constexpr int spacing = 24; // Increased spacing between toggle and box

            if (toggle != nullptr)
            {
                auto toggleArea = area.removeFromTop(toggleHeight);
                toggle->setBounds(toggleArea.withSizeKeepingCentre(boxWidth, toggleHeight));
            }

            area.removeFromTop(spacing); // More space between toggle and offset box

            if (box != nullptr)
            {
                auto boxArea = area.removeFromTop(boxHeight);
                box->setBounds(boxArea.withSizeKeepingCentre(boxWidth, boxHeight));
            }
        };
    }
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

    const int width = getWidth();

    juce::Rectangle<int> headerArea(0, 0, width, HSTheme::headerH);
    g.setColour(HSTheme::SURFACE_PANEL_LIGHT);
    g.fillRect(headerArea);

    g.setColour(HSTheme::ACCENT_TEAL.withAlpha(0.35f));
    g.fillRect(0, headerArea.getBottom() - 2, width, 2);

    const int headingBandHeight = 40;
    juce::Rectangle<int> headingBand(0, headerArea.getBottom(), width, headingBandHeight);
    g.setColour(HSTheme::ACCENT_TEAL.withAlpha(0.12f));
    g.fillRect(0, headingBand.getY(), width, 1);
    g.fillRect(0, headingBand.getBottom() - 2, width, 2);

    auto textRow = juce::Rectangle<float>(HSTheme::grid,
                                          (float)headingBand.getY() + HSTheme::grid * 0.5f,
                                          (float)width - 2.0f * HSTheme::grid,
                                          22.0f);

    g.setColour(HSTheme::ACCENT_TEAL);
    g.setFont(HSTheme::label());

    auto valueColumn = textRow.removeFromLeft(200.0f);
    g.drawText("VALUES", valueColumn, juce::Justification::centredLeft);
    g.drawLine(valueColumn.getX(), valueColumn.getBottom(), valueColumn.getRight(), valueColumn.getBottom(), 2.0f);

    textRow.removeFromLeft((float)HSTheme::grid);

    auto controlColumn = textRow.removeFromLeft(200.0f);
    g.drawText("CONTROLS", controlColumn, juce::Justification::centredLeft);
    g.drawLine(controlColumn.getX(), controlColumn.getBottom(), controlColumn.getRight(), controlColumn.getBottom(), 2.0f);

    g.drawText("WAVEFORM", textRow, juce::Justification::centredLeft);
    g.drawLine(textRow.getX(), textRow.getBottom(), textRow.getRight(), textRow.getBottom(), 2.0f);
}

void HeartSyncEditor::resized()
{
    if (!isInitialized)
        return;

    auto r = getLocalBounds();
    // Header layout
    auto header = r.removeFromTop(HSTheme::headerH);
    auto headerInner = header.reduced(HSTheme::grid, HSTheme::grid / 2);

    auto rightBlock = headerInner.removeFromRight(260);
    auto gearArea = rightBlock.removeFromRight(28);
    headerSettingsIcon.setBounds(gearArea.withSizeKeepingCentre(22, 22));
    headerClockRight.setBounds(rightBlock.removeFromTop(26));
    headerStatusRight.setBounds(rightBlock);

    auto leftBlock = headerInner;
    auto titleRow = leftBlock.removeFromTop(32);
    headerGlyph.setBounds(titleRow.removeFromLeft(24).withSizeKeepingCentre(18, 18));
    headerTitleLeft.setBounds(titleRow);
    headerSubtitleLeft.setBounds(leftBlock.removeFromTop(18));

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
    auto bleControls = bleBar.removeFromTop(44);

    const int buttonHeight = 34;
    const int buttonWidth = 110;
    auto layoutButton = [buttonHeight, buttonWidth](juce::Component& comp, juce::Rectangle<int>& area)
    {
        auto slot = area.removeFromLeft(buttonWidth);
        comp.setBounds(slot.withSizeKeepingCentre(buttonWidth, buttonHeight).reduced(2, 0));
    };

    layoutButton(scanBtn, bleControls);
    bleControls.removeFromLeft(HSTheme::grid / 2);
    layoutButton(connectBtn, bleControls);
    bleControls.removeFromLeft(HSTheme::grid / 2);
    layoutButton(lockBtn, bleControls);
    bleControls.removeFromLeft(HSTheme::grid / 2);
    layoutButton(disconnectBtn, bleControls);
    bleControls.removeFromLeft(HSTheme::grid);

    auto deviceArea = bleControls;
    auto labelArea = deviceArea.removeFromLeft(70);
    deviceLabel.setBounds(labelArea.withSizeKeepingCentre(labelArea.getWidth(), buttonHeight).reduced(2, 0));
    deviceBox.setBounds(deviceArea.withSizeKeepingCentre(deviceArea.getWidth(), buttonHeight).reduced(2));

    auto bleStatus = bleBar.removeFromTop(24);
    statusDot.setBounds(bleStatus.removeFromLeft(24).withSizeKeepingCentre(14, 14));
    statusLabel.setBounds(bleStatus.removeFromLeft(220).withSizeKeepingCentre(200, 18));

    // Terminal panel under BLE
    terminalTitle.setBounds(terminal.removeFromTop(20));
    terminalOutput.setBounds(terminal);
}

void HeartSyncEditor::timerCallback()
{
    headerClockRight.setText(juce::Time::getCurrentTime().toString(true, true), juce::dontSendNotification);
    updateBluetoothStatus();

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
    else
    {
        // No valid data - show dashes
        rowHR->setValueText("--");
        rowSmooth->setValueText("--");
        rowWetDry->setValueText("--");
    }
    
    // Update tempo sync indicators
    auto syncSource = processorRef.getTempoSyncSource();
    rowHR->setTempoSyncActive(syncSource == HeartSyncProcessor::TempoSyncSource::RawHeartRate);
    rowSmooth->setTempoSyncActive(syncSource == HeartSyncProcessor::TempoSyncSource::SmoothedHeartRate);
    rowWetDry->setTempoSyncActive(syncSource == HeartSyncProcessor::TempoSyncSource::WetDryRatio);
    
    // Update header status to show tempo sync info
    if (syncSource != HeartSyncProcessor::TempoSyncSource::Off)
    {
        float suggestedTempo = processorRef.getCurrentSuggestedTempo();
        juce::String tempoText = juce::String::formatted("♩ TEMPO: %.1f BPM (%s)", 
            suggestedTempo, 
            processorRef.getTempoSyncSourceName().toRawUTF8());
        headerStatusRight.setText(tempoText, juce::dontSendNotification);
        headerStatusRight.setColour(juce::Label::textColourId, HSTheme::ACCENT_TEAL);
    }
    else if (processorRef.isDeviceConnected())
    {
        headerStatusRight.setText(juce::CharPointer_UTF8("◆ CONNECTED"), juce::dontSendNotification);
        headerStatusRight.setColour(juce::Label::textColourId, HSTheme::STATUS_CONNECTED);
    }
    else
    {
        headerStatusRight.setText(juce::CharPointer_UTF8("◆ SYSTEM OPERATIONAL"), juce::dontSendNotification);
        headerStatusRight.setColour(juce::Label::textColourId, HSTheme::STATUS_CONNECTED);
    }
    
    repaint();
}

void HeartSyncEditor::mouseDown(const juce::MouseEvent& event)
{
    if (event.eventComponent == &terminalOutput && event.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.addItem("Copy Selected", [this]() { terminalOutput.copy(); });
        menu.addItem("Copy All", [this]() { juce::SystemClipboard::copyTextToClipboard(terminalOutput.getText()); });

        menu.showMenuAsync(juce::PopupMenu::Options{}
                               .withTargetComponent(&terminalOutput)
                               .withTargetScreenArea({ event.getScreenPosition(), { 1, 1 } }));
        return;
    }

    juce::AudioProcessorEditor::mouseDown(event);
}

void HeartSyncEditor::wireClientCallbacks()
{
    auto safe = juce::Component::SafePointer<HeartSyncEditor>(this);

    processorRef.onDeviceListUpdated = [safe]()
    {
        if (safe != nullptr)
        {
            juce::MessageManager::callAsync([safe]()
            {
                if (auto* editor = safe.getComponent())
                    editor->refreshDeviceDropdown();
            });
        }
    };

    processorRef.onBluetoothStateChanged = [safe]()
    {
        if (safe != nullptr)
        {
            juce::MessageManager::callAsync([safe]()
            {
                if (auto* editor = safe.getComponent())
                    editor->updateBluetoothStatus();
            });
        }
    };

    processorRef.onSystemMessage = [safe](const juce::String& message)
    {
        if (safe != nullptr)
        {
            juce::MessageManager::callAsync([safe, message]()
            {
                if (auto* editor = safe.getComponent())
                    editor->appendTerminal(message);
            });
        }
    };
}

void HeartSyncEditor::scanForDevices()
{
    const bool bridgeConfigured = processorRef.isBridgeClientConfigured();
    const bool bridgeConnected = processorRef.isBridgeClientConnected();
    const bool bridgeReady = processorRef.isBridgeClientReady();
    const bool nativeReady = processorRef.isNativeBluetoothReady();
    const bool available = processorRef.isBluetoothAvailable();
    const bool ready = processorRef.isBluetoothReady();

    if (!available)
    {
#if JUCE_MAC
        if (bridgeConfigured && !bridgeConnected && !nativeReady)
        {
            appendTerminal("Waiting for HeartSync Bridge helper; attempting to launch helper...");
            processorRef.requestBridgeReconnect(true);
        }
        else
        {
            appendTerminal("Bluetooth subsystem still initializing");
        }
#else
        appendTerminal("Bluetooth subsystem still initializing");
#endif
        pendingScanRequest = true;
        syncBleControls();
        return;
    }

    if (!ready)
    {
#if JUCE_MAC
        if (bridgeConfigured && bridgeConnected && !bridgeReady)
        {
            appendTerminal("Bridge helper awaiting permission; retrying shortly");
        }
        else if (bridgeConfigured && !bridgeConnected && !nativeReady)
        {
            appendTerminal("HeartSync Bridge helper not yet ready; will retry automatically");
            processorRef.requestBridgeReconnect();
        }
        else
        {
            appendTerminal("Bluetooth radio not ready; will retry when available");
        }
#else
        appendTerminal("Bluetooth radio not ready; will retry when available");
#endif
        pendingScanRequest = true;
        syncBleControls();
        return;
    }

    pendingScanRequest = false;

    if (processorRef.isScanning())
    {
        processorRef.stopDeviceScan();
        appendTerminal("Scan stopped");
    }
    else
    {
        auto result = processorRef.startDeviceScan();
        if (result.failed())
        {
            appendTerminal("Scan failed: " + result.getErrorMessage());
        }
        else
        {
            appendTerminal("Scanning for devices...");
            availableDevices.clear();
            knownDeviceCount = 0;
            deviceBox.clear(juce::dontSendNotification);
        }
    }

    updateBluetoothStatus();
    syncBleControls();
}

void HeartSyncEditor::connectToDevice(const juce::String& deviceAddress)
{
    processorRef.connectToDevice(deviceAddress.toStdString());
}

void HeartSyncEditor::connectSelectedDevice()
{
    auto selectedId = deviceBox.getSelectedId();
    if (selectedId <= 0 || selectedId > (int)availableDevices.size())
    {
        appendTerminal("Select a device before connecting");
        return;
    }

    const auto& device = availableDevices[(size_t)(selectedId - 1)];
    
    appendTerminal("DEBUG: Selected device index " + juce::String(selectedId - 1) + 
                   ", id: '" + juce::String(device.identifier) + 
                   "', name: '" + juce::String(device.name) + "'");
    
    auto result = processorRef.connectToDevice(device.identifier);
    if (result.failed())
    {
        appendTerminal("Connection failed: " + result.getErrorMessage());
    }
    else
    {
        juce::String name = device.name.empty() ? juce::String(device.identifier) : juce::String(device.name);
        appendTerminal("Connecting to " + name);
    }

    updateBluetoothStatus();
}

void HeartSyncEditor::refreshDeviceDropdown()
{
    auto devices = processorRef.getAvailableDevices();
    
    // FILTER: Only show devices advertising Heart Rate service (180D) like Python version
    std::vector<HeartSyncProcessor::DeviceInfo> filteredDevices;
    for (const auto& device : devices)
    {
        bool hasHRService = false;
        for (const auto& service : device.services)
        {
            juce::String svc = juce::String(service).trim();
            if (svc.equalsIgnoreCase("180D") || svc.equalsIgnoreCase("0000180D-0000-1000-8000-00805F9B34FB"))
            {
                hasHRService = true;
                break;
            }
        }
        
        if (hasHRService)
            filteredDevices.push_back(device);
    }
    
    availableDevices = filteredDevices;

    auto previousId = deviceBox.getSelectedId();
    deviceBox.clear(juce::dontSendNotification);

    int itemId = 1;
    int connectedId = 0;
    std::set<std::string> namesSeen;

    for (const auto& device : availableDevices)
    {
        // Build smart display name
        juce::String displayName;
        juce::String deviceName = juce::String(device.name).trim();
        
        // Use actual device name if available and meaningful
        if (deviceName.isNotEmpty() && !deviceName.equalsIgnoreCase("Unknown"))
        {
            displayName = deviceName;
        }
        else
        {
            // Fallback to service-based naming
            bool isHeartRateMonitor = false;
            for (const auto& service : device.services)
            {
                if (juce::String(service).equalsIgnoreCase("180D"))
                {
                    isHeartRateMonitor = true;
                    break;
                }
            }
            
            // Get short identifier from UUID
            juce::String shortId = juce::String(device.identifier);
            auto lastDash = shortId.lastIndexOfChar('-');
            if (lastDash >= 0 && lastDash < shortId.length() - 1)
                shortId = shortId.substring(lastDash + 1);
            if (shortId.length() > 5)
                shortId = shortId.substring(shortId.length() - 5);
            shortId = shortId.toUpperCase();
            
            displayName = isHeartRateMonitor 
                ? ("HR Monitor • " + shortId)
                : ("BLE Device • " + shortId);
        }

        // Append signal strength
        if (device.signalStrength != 0)
            displayName += juce::String(" (") + juce::String(device.signalStrength) + " dBm)";

        deviceBox.addItem(displayName, itemId);
        if (device.isConnected)
            connectedId = itemId;

        if (!namesSeen.count(device.identifier))
        {
            namesSeen.insert(device.identifier);
        }

        ++itemId;
    }

    if (connectedId > 0)
        deviceBox.setSelectedId(connectedId, juce::dontSendNotification);
    else if (previousId > 0 && previousId <= deviceBox.getNumItems())
        deviceBox.setSelectedId(previousId, juce::dontSendNotification);
    else if (deviceBox.getNumItems() > 0)
        deviceBox.setSelectedId(1, juce::dontSendNotification);

    if (availableDevices.size() > knownDeviceCount)
    {
        auto newCount = availableDevices.size() - knownDeviceCount;
        if (!availableDevices.empty())
        {
            const auto& latest = availableDevices.back();
            
            // Build smart display name for terminal
            juce::String latestLabel;
            juce::String deviceName = juce::String(latest.name).trim();
            
            if (deviceName.isNotEmpty() && !deviceName.equalsIgnoreCase("Unknown"))
            {
                latestLabel = deviceName;
            }
            else
            {
                bool isHeartRateMonitor = false;
                for (const auto& service : latest.services)
                {
                    if (juce::String(service).equalsIgnoreCase("180D"))
                    {
                        isHeartRateMonitor = true;
                        break;
                    }
                }
                
                juce::String shortId = juce::String(latest.identifier);
                auto lastDash = shortId.lastIndexOfChar('-');
                if (lastDash >= 0 && lastDash < shortId.length() - 1)
                    shortId = shortId.substring(lastDash + 1);
                if (shortId.length() > 5)
                    shortId = shortId.substring(shortId.length() - 5);
                shortId = shortId.toUpperCase();
                
                latestLabel = isHeartRateMonitor 
                    ? ("HR Monitor • " + shortId)
                    : ("BLE Device • " + shortId);
            }
            
            appendTerminal("Discovered " + juce::String(newCount) + (newCount == 1 ? " device: " : " devices (latest): ") + latestLabel);
        }
        else
        {
            appendTerminal("Discovered " + juce::String(newCount) + (newCount == 1 ? " device" : " devices"));
        }
    }
    else if (availableDevices.empty() && knownDeviceCount != 0)
    {
        appendTerminal("Devices cleared");
    }

    knownDeviceCount = availableDevices.size();
    syncBleControls();
}

void HeartSyncEditor::appendTerminal(const juce::String& message)
{
    auto timestamp = juce::Time::getCurrentTime().formatted("%H:%M:%S");
    terminalLines.add("[" + timestamp + "] " + message);
    while (terminalLines.size() > 200)
        terminalLines.remove(0);

    updateTerminalLabel();
}

void HeartSyncEditor::updateTerminalLabel()
{
    if (terminalLines.isEmpty())
    {
        terminalOutput.setText("[ WAITING ]  |  DEVICE: ---  |  ADDR: ---  |  BAT: --%  |  BPM: ---",
                               false);
        return;
    }

    juce::String text;
    for (int i = 0; i < terminalLines.size(); ++i)
    {
        if (i > 0)
            text += '\n';
        text += terminalLines[i];
    }

    terminalOutput.setText(text, false);
    terminalOutput.setCaretPosition(terminalOutput.getText().length());
}

juce::String HeartSyncEditor::buildDeviceLabel(const HeartSyncProcessor::DeviceInfo& device) const
{
    juce::String deviceName = juce::String(device.name).trim();
    
    if (deviceName.isNotEmpty() && !deviceName.equalsIgnoreCase("Unknown"))
    {
        return deviceName;
    }
    
    // Fallback to service-based naming
    bool isHeartRateMonitor = false;
    for (const auto& service : device.services)
    {
        if (juce::String(service).equalsIgnoreCase("180D"))
        {
            isHeartRateMonitor = true;
            break;
        }
    }
    
    juce::String shortId = shortenIdentifier(device.identifier);
    return isHeartRateMonitor 
        ? ("HR Monitor • " + shortId)
        : ("BLE Device • " + shortId);
}

juce::String HeartSyncEditor::buildDeviceDetail(const HeartSyncProcessor::DeviceInfo& device) const
{
    return buildDeviceLabel(device) + juce::String(" (") + juce::String(device.signalStrength) + " dBm)";
}

juce::String HeartSyncEditor::shortenIdentifier(const std::string& identifier) const
{
    juce::String shortId = juce::String(identifier);
    auto lastDash = shortId.lastIndexOfChar('-');
    if (lastDash >= 0 && lastDash < shortId.length() - 1)
        shortId = shortId.substring(lastDash + 1);
    if (shortId.length() > 5)
        shortId = shortId.substring(shortId.length() - 5);
    return shortId.toUpperCase();
}

void HeartSyncEditor::updateBluetoothStatus()
{
    const bool bridgeConfigured = processorRef.isBridgeClientConfigured();
    const bool bridgeConnected = processorRef.isBridgeClientConnected();
    const bool bridgeReady = processorRef.isBridgeClientReady();
    const juce::String bridgePermission = processorRef.getBridgePermissionState();
    const bool nativeAvailable = processorRef.hasNativeBluetoothStack();
    const bool nativeReady = processorRef.isNativeBluetoothReady();
    const bool scanning = processorRef.isScanning();
    const bool connected = processorRef.isDeviceConnected();

    const bool available = bridgeConnected ? true : nativeAvailable;
    const bool ready = bridgeConnected ? bridgeReady : nativeReady;

    juce::String deviceName = juce::String(processorRef.getConnectedDeviceName());

    if (connected && deviceName.isEmpty())
        deviceName = "Device";

    if (ready && !statusWasReady)
        appendTerminal("Bluetooth radio ready");
    else if (!ready && statusWasReady)
        appendTerminal("Bluetooth radio powered down");

    if (connected && (!statusWasConnected || deviceName != statusLastDeviceName))
    {
        appendTerminal("Connected to " + deviceName);
    }
    else if (!connected && statusWasConnected)
    {
        appendTerminal("Device disconnected");
    }

    statusWasConnected = connected;
    statusWasScanning = scanning;
    statusWasReady = ready;
    statusLastDeviceName = deviceName;

    if (!bridgeConnected)
    {
        bridgeWasConnected = false;
        bridgeWasReady = false;
        lastBridgePermission.clear();
        if (!nativeReady && bridgeConfigured && !bridgeHintShown)
        {
            appendTerminal("Launch HeartSync Bridge.app (~/Applications or /Applications) to enable Bluetooth inside this host");
            bridgeHintShown = true;
        }
    }
    else
    {
        bridgeHintShown = false;
        if (!bridgeWasConnected)
            bridgeWasConnected = true;

        if (bridgePermission != lastBridgePermission && bridgePermission.isNotEmpty())
        {
            lastBridgePermission = bridgePermission;
            appendTerminal("Bridge permission state: " + bridgePermission);
        }

        bridgeWasReady = bridgeReady;
    }

    if (!available)
        setStatusIndicator(HSTheme::STATUS_DISCONNECTED, "INITIALIZING...");
    else if (bridgeConnected && !bridgeReady)
        setStatusIndicator(HSTheme::STATUS_CONNECTING, "WAITING FOR BRIDGE...");
    else if (!ready)
        setStatusIndicator(HSTheme::STATUS_CONNECTING, bridgeConfigured ? "WAITING FOR BRIDGE..." : "POWERING ON...");
    else if (scanning)
        setStatusIndicator(HSTheme::STATUS_SCANNING, "SCANNING...");
    else if (connected)
        setStatusIndicator(HSTheme::STATUS_CONNECTED, "CONNECTED: " + deviceName);
    else
        setStatusIndicator(HSTheme::STATUS_DISCONNECTED, "DISCONNECTED");

    if (ready && pendingScanRequest && !scanning)
    {
        pendingScanRequest = false;
        scanForDevices();
        return;
    }

    syncBleControls();
}

void HeartSyncEditor::syncBleControls()
{
    const bool bridgeConnected = processorRef.isBridgeClientConnected();
    const bool bridgeReady = processorRef.isBridgeClientReady();
    const bool nativeAvailable = processorRef.hasNativeBluetoothStack();
    const bool nativeReady = processorRef.isNativeBluetoothReady();
    const bool scanning = processorRef.isScanning();
    const bool connected = processorRef.isDeviceConnected();
    const bool hasSelection = deviceBox.getSelectedId() > 0 && deviceBox.getSelectedId() <= (int)availableDevices.size();

    const bool ready = bridgeConnected ? bridgeReady : nativeReady;
    const bool available = bridgeConnected ? true : nativeAvailable;

    // Sync scan button state with actual scanning state
    scanBtn.setEnabled(true);
    scanBtn.setToggleState(scanning, juce::dontSendNotification);
    
    if (!available)
        scanBtn.setButtonText("SCAN");
    else if (!ready && !scanning)
        scanBtn.setButtonText("WAIT");
    else
        scanBtn.setButtonText(scanning ? "STOP" : "SCAN");

    connectBtn.setEnabled(ready && !connected && hasSelection);
    disconnectBtn.setEnabled(available && connected);

    deviceBox.setEnabled(ready);
}

void HeartSyncEditor::setStatusIndicator(juce::Colour colour, const juce::String& text)
{
    statusDot.setColour(juce::Label::textColourId, colour);
    statusLabel.setColour(juce::Label::textColourId, colour);
    statusLabel.setText(text, juce::dontSendNotification);
    statusDot.repaint();
    statusLabel.repaint();
}
