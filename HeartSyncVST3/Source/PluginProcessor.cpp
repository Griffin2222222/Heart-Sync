#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/HeartRateParams.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int midiControllerChannel = 1;
    constexpr int midiControllerNumber = 74;
    constexpr float midiMinBpm = 40.0f;
    constexpr float midiMaxBpm = 180.0f;
}

HeartSyncVST3AudioProcessor::HeartSyncVST3AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "HeartSyncParameters", createParameterLayout())
{
    heartRateProcessor.attachToValueTree(parameters);
    handleParameterChanges();

    parameters.state.setProperty(HeartRateParams::OSC_HOST, "127.0.0.1", nullptr);

    bluetoothManager.setMeasurementCallback([this](float bpm, const std::vector<float>& rrIntervals)
    {
        heartRateProcessor.pushNewMeasurement(bpm, rrIntervals);

        const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        if (packetsReceived.load() == 0)
            sessionStartSeconds.store(now);

        packetsReceived.fetch_add(1);
        const double start = sessionStartSeconds.load();
        if (start > 0.0)
        {
            const int expected = static_cast<int>((now - start) * 10.0) + 1; // assume 10Hz HR packets
            const int currentExpected = packetsExpected.load();
            if (expected > currentExpected)
                packetsExpected.store(expected);
        }

        lastPacketTimeSeconds.store(now);
        pendingOscFrame.store(true);
    });

    bluetoothManager.setConnectionCallbacks(
        [this](const BluetoothDevice& device)
        {
            const juce::SpinLock::ScopedLockType lock(deviceLock);
            currentDeviceName = device.name.empty() ? juce::String("Unknown Device") : juce::String(device.name);
            appendConsoleLog("Connected to " + currentDeviceName, "BLE");
        },
        [this]()
        {
            const juce::SpinLock::ScopedLockType lock(deviceLock);
            appendConsoleLog("Bluetooth device disconnected", "BLE");
            currentDeviceName.clear();
            packetsReceived.store(0);
            packetsExpected.store(0);
            sessionStartSeconds.store(0.0);
            lastPacketTimeSeconds.store(0.0);
        },
        [this](const std::string& error)
        {
            appendConsoleLog("Bluetooth error: " + juce::String(error), "ERR");
        });

    startTimerHz(oscTicksPerSecond);
    appendConsoleLog("HeartSync processor initialised", "INIT");
}

HeartSyncVST3AudioProcessor::~HeartSyncVST3AudioProcessor()
{
    stopTimer();
    bluetoothManager.stopScanning();
    bluetoothManager.disconnectCurrentDevice();
    oscSenderManager.stopSending();
}

const juce::String HeartSyncVST3AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool HeartSyncVST3AudioProcessor::acceptsMidi() const
{
    return true;
}

bool HeartSyncVST3AudioProcessor::producesMidi() const
{
    return true;
}

bool HeartSyncVST3AudioProcessor::isMidiEffect() const
{
    return false;
}

double HeartSyncVST3AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int HeartSyncVST3AudioProcessor::getNumPrograms()
{
    return 1;
}

int HeartSyncVST3AudioProcessor::getCurrentProgram()
{
    return 0;
}

void HeartSyncVST3AudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String HeartSyncVST3AudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return "Default";
}

void HeartSyncVST3AudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void HeartSyncVST3AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    samplesUntilOsc = 0;
}

void HeartSyncVST3AudioProcessor::releaseResources()
{
}

bool HeartSyncVST3AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void HeartSyncVST3AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    handleParameterChanges();
    heartRateProcessor.process();

    const float smoothed = heartRateProcessor.getSmoothedBpm();
    const float normalised = juce::jlimit(0.0f, 1.0f, (smoothed - midiMinBpm) / (midiMaxBpm - midiMinBpm));
    const int midiValue = static_cast<int>(std::round(normalised * 127.0f));
    midiMessages.addEvent(juce::MidiMessage::controllerEvent(midiControllerChannel,
                                                             midiControllerNumber,
                                                             midiValue),
                          0);

    const double samplesPerOscTick = currentSampleRate / static_cast<double>(oscTicksPerSecond);
    samplesUntilOsc += buffer.getNumSamples();

    if (samplesUntilOsc >= samplesPerOscTick)
    {
        samplesUntilOsc -= static_cast<int>(samplesPerOscTick);
        pendingOscFrame.store(true);
    }
}

bool HeartSyncVST3AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* HeartSyncVST3AudioProcessor::createEditor()
{
    return new HeartSyncVST3AudioProcessorEditor(*this);
}

void HeartSyncVST3AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void HeartSyncVST3AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));

    if (! parameters.state.hasProperty(HeartRateParams::OSC_HOST))
        parameters.state.setProperty(HeartRateParams::OSC_HOST, "127.0.0.1", nullptr);

    handleParameterChanges();
    updateOscConfiguration();
}

HeartRateProcessor::SmoothingMetrics HeartSyncVST3AudioProcessor::getSmoothingMetrics() const
{
    return heartRateProcessor.getSmoothingMetrics();
}

void HeartSyncVST3AudioProcessor::getHistory(juce::Array<float>& raw,
                                             juce::Array<float>& smoothed,
                                             juce::Array<float>& wetDry) const
{
    heartRateProcessor.getHistoryCopy(raw, smoothed, wetDry);
}

HeartSyncVST3AudioProcessor::Telemetry HeartSyncVST3AudioProcessor::getTelemetry() const
{
    Telemetry telemetry;
    {
        const juce::SpinLock::ScopedLockType lock(deviceLock);
        telemetry.deviceName = currentDeviceName;
        telemetry.isConnected = currentDeviceName.isNotEmpty();
    }
    telemetry.packetsReceived = packetsReceived.load();
    telemetry.packetsExpected = packetsExpected.load();
    telemetry.lastPacketTimeSeconds = lastPacketTimeSeconds.load();
    return telemetry;
}

juce::StringArray HeartSyncVST3AudioProcessor::getConsoleLog() const
{
    juce::StringArray copy;
    const juce::SpinLock::ScopedLockType lock(logLock);
    copy = consoleLog;
    return copy;
}

void HeartSyncVST3AudioProcessor::appendConsoleLog(const juce::String& message, const juce::String& tag)
{
    juce::String formatted = "[" + juce::Time::getCurrentTime().toString(true, true) + "] ";
    if (tag.isNotEmpty())
        formatted += tag + " | ";
    formatted += message;

    const juce::SpinLock::ScopedLockType lock(logLock);
    consoleLog.add(formatted);
    while (consoleLog.size() > maxConsoleEntries)
        consoleLog.remove(0);
}

HeartSyncVST3AudioProcessor::TelemetrySnapshot HeartSyncVST3AudioProcessor::getTelemetrySnapshot() const
{
    TelemetrySnapshot snapshot;

    const auto telemetry = getTelemetry();
    snapshot.deviceName = telemetry.deviceName;
    snapshot.isConnected = telemetry.isConnected;
    snapshot.packetsReceived = telemetry.packetsReceived;
    snapshot.packetsExpected = telemetry.packetsExpected;
    snapshot.lastLatencyMs = telemetry.lastPacketTimeSeconds > 0.0
                                 ? static_cast<float>((juce::Time::getMillisecondCounterHiRes() / 1000.0
                                                       - telemetry.lastPacketTimeSeconds) * 1000.0)
                                 : 0.0f;

    snapshot.signalQuality = snapshot.packetsExpected > 0
                                 ? juce::jlimit(0.0f, 100.0f,
                                                (static_cast<float>(snapshot.packetsReceived)
                                                 / static_cast<float>(snapshot.packetsExpected)) * 100.0f)
                                 : 0.0f;

    if (telemetry.isConnected)
        snapshot.statusText = "Connected";
    else if (bluetoothManager.isScanning())
        snapshot.statusText = "Scanning...";
    else
        snapshot.statusText = "Idle";

    snapshot.heart.rawBpm = heartRateProcessor.getRawBpm();
    snapshot.heart.smoothedBpm = heartRateProcessor.getSmoothedBpm();
    snapshot.heart.wetDry = heartRateProcessor.getWetDryValue();
    snapshot.heart.invertedBpm = heartRateProcessor.getInvertedBpm();

    const auto metrics = heartRateProcessor.getSmoothingMetrics();
    snapshot.heart.alpha = metrics.alpha;
    snapshot.heart.halfLifeSeconds = metrics.halfLifeSeconds;
    snapshot.heart.effectiveWindowSamples = metrics.effectiveWindowSamples;

    juce::Array<float> raw;
    juce::Array<float> smoothed;
    juce::Array<float> wetDry;
    heartRateProcessor.getHistoryCopy(raw, smoothed, wetDry);

    snapshot.history.raw = raw;
    snapshot.history.smoothed = smoothed;
    snapshot.history.wetDry = wetDry;

    const int historySize = raw.size();
    const float dt = HeartRateProcessor::historyTimeDeltaSeconds;
    snapshot.history.time.clearQuick();
    snapshot.history.time.ensureStorageAllocated(historySize);
    for (int i = 0; i < historySize; ++i)
    {
        const float t = static_cast<float>(i - historySize + 1) * dt;
        snapshot.history.time.add(t);
    }

    snapshot.consoleLog = getConsoleLog();

    if (snapshot.isConnected)
        snapshot.statusText = "Connected • " + juce::String(snapshot.heart.smoothedBpm, 1) + " BPM";

    return snapshot;
}

void HeartSyncVST3AudioProcessor::timerCallback()
{
    handleParameterChanges();
    heartRateProcessor.process();
    updateOscConfiguration();
    sendOscIfNeeded();
}

juce::AudioProcessorValueTreeState::ParameterLayout HeartSyncVST3AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        HeartRateParams::SMOOTHING_FACTOR,
        "Smoothing",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.4f),
        0.1f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        HeartRateParams::HEART_RATE_OFFSET,
        "HR Offset",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        HeartRateParams::WET_DRY_OFFSET,
        "Wet/Dry Offset",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        HeartRateParams::WET_DRY_SOURCE,
        "Wet/Dry Source",
        juce::StringArray{ "Smoothed HR", "Raw HR" },
        0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        HeartRateParams::OSC_ENABLED,
        "OSC Enabled",
        false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        HeartRateParams::OSC_PORT,
        "OSC Port",
        juce::NormalisableRange<float>(1024.0f, 65535.0f, 1.0f),
        8000.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        HeartRateParams::UI_LOCKED,
        "UI Locked",
        true));

    return { params.begin(), params.end() };
}

void HeartSyncVST3AudioProcessor::handleParameterChanges()
{
    if (auto* smoothing = parameters.getRawParameterValue(HeartRateParams::SMOOTHING_FACTOR))
        heartRateProcessor.setSmoothingFactor(smoothing->load());
    if (auto* hrOffset = parameters.getRawParameterValue(HeartRateParams::HEART_RATE_OFFSET))
        heartRateProcessor.setHeartRateOffset(hrOffset->load());
    if (auto* wetDryOffset = parameters.getRawParameterValue(HeartRateParams::WET_DRY_OFFSET))
        heartRateProcessor.setWetDryOffset(wetDryOffset->load());
    if (auto* wetDrySource = parameters.getRawParameterValue(HeartRateParams::WET_DRY_SOURCE))
    {
        heartRateProcessor.setWetDrySource(wetDrySource->load() > 0.5f
                                               ? HeartRateProcessor::WetDrySource::Raw
                                               : HeartRateProcessor::WetDrySource::Smoothed);
    }
}

void HeartSyncVST3AudioProcessor::updateOscConfiguration()
{
    const bool shouldBeEnabled = parameters.getRawParameterValue(HeartRateParams::OSC_ENABLED)->load() >= 0.5f;
    if (!shouldBeEnabled)
    {
        if (oscConnected)
        {
            oscSenderManager.stopSending();
            oscConnected = false;
        }
        return;
    }

    const int desiredPort = static_cast<int>(parameters.getRawParameterValue(HeartRateParams::OSC_PORT)->load());
    const auto desiredHostVar = parameters.state.getProperty(HeartRateParams::OSC_HOST, "127.0.0.1");
    const juce::String desiredHost = desiredHostVar.toString();

    if (!oscConnected || desiredPort != currentOscPort || desiredHost != currentOscHost)
    {
        oscSenderManager.stopSending();
        if (oscSenderManager.initialize(desiredHost, desiredPort))
        {
            oscConnected = true;
            currentOscHost = desiredHost;
            currentOscPort = desiredPort;
            appendConsoleLog("OSC connected to " + desiredHost + ":" + juce::String(desiredPort), "OSC");
        }
        else
        {
            oscConnected = false;
            appendConsoleLog("OSC connection failed at " + desiredHost + ":" + juce::String(desiredPort), "OSC");
        }
    }
}

void HeartSyncVST3AudioProcessor::sendOscIfNeeded()
{
    if (!oscConnected)
        return;

    if (!pendingOscFrame.exchange(false))
        return;

    oscSenderManager.sendHeartRateData(heartRateProcessor.getRawBpm(),
                                       heartRateProcessor.getSmoothedBpm(),
                                       heartRateProcessor.getWetDryValue());
}

juce::String HeartSyncVST3AudioProcessor::getConnectedDeviceName() const
{
    const juce::SpinLock::ScopedLockType lock(deviceLock);
    return currentDeviceName;
}

void HeartSyncVST3AudioProcessor::beginDeviceScan()
{
    bluetoothManager.stopScanning();
    bluetoothManager.startScanning();
    appendConsoleLog("Scanning for Bluetooth devices...", "BLE");
}

std::vector<BluetoothDevice> HeartSyncVST3AudioProcessor::getDiscoveredDevices() const
{
    return bluetoothManager.getAvailableDevices();
}

bool HeartSyncVST3AudioProcessor::connectToDevice(const std::string& deviceId)
{
    bluetoothManager.stopScanning();
    packetsReceived.store(0);
    packetsExpected.store(0);
    sessionStartSeconds.store(0.0);
    lastPacketTimeSeconds.store(0.0);

    appendConsoleLog("Connecting to device " + juce::String(deviceId), "BLE");
    const bool success = bluetoothManager.connectToDevice(deviceId);
    if (!success)
        appendConsoleLog("Connection failed: " + juce::String(bluetoothManager.getLastError()), "ERR");
    return success;
}

void HeartSyncVST3AudioProcessor::disconnectFromDevice()
{
    bluetoothManager.disconnectCurrentDevice();
    packetsReceived.store(0);
    packetsExpected.store(0);
    sessionStartSeconds.store(0.0);
    lastPacketTimeSeconds.store(0.0);
    appendConsoleLog("Bluetooth device disconnected", "BLE");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HeartSyncVST3AudioProcessor();
}
