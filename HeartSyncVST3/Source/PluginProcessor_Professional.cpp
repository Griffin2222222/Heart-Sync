#include "PluginProcessor_Professional.h"
#include "PluginEditor.h"
#include <chrono>
#include <cmath>

//==============================================================================
// Parameter IDs (professional naming convention)
const juce::String HeartSyncVST3AudioProcessor::PARAM_RAW_HEART_RATE = "raw_heart_rate";
const juce::String HeartSyncVST3AudioProcessor::PARAM_SMOOTHED_HEART_RATE = "smoothed_heart_rate";
const juce::String HeartSyncVST3AudioProcessor::PARAM_WET_DRY_RATIO = "wet_dry_ratio";
const juce::String HeartSyncVST3AudioProcessor::PARAM_HEART_RATE_OFFSET = "heart_rate_offset";
const juce::String HeartSyncVST3AudioProcessor::PARAM_SMOOTHING_FACTOR = "smoothing_factor";
const juce::String HeartSyncVST3AudioProcessor::PARAM_WET_DRY_OFFSET = "wet_dry_offset";
const juce::String HeartSyncVST3AudioProcessor::PARAM_WET_DRY_INPUT_SOURCE = "wet_dry_input_source";
const juce::String HeartSyncVST3AudioProcessor::PARAM_TEMPO_SYNC_SOURCE = "tempo_sync_source";

//==============================================================================
HeartSyncVST3AudioProcessor::HeartSyncVST3AudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, "HeartSyncParameters", createParameterLayout()),
       historyWriteIndex(0),
       lastResetTime(std::chrono::steady_clock::now())
{
    // Initialize arrays to zero
    rawHeartRateHistory.fill(0.0f);
    smoothedHeartRateHistory.fill(0.0f);
    wetDryHistory.fill(50.0f); // Default to 50% wet/dry
    
    // Defer Bluetooth initialization to prevent constructor crashes
    bluetoothManager = nullptr;
    
    logSystemMessage("HeartSync Professional v2.0 - Enterprise Audio Processor Initialized");

#if JUCE_MAC
    initialiseBridgeClient();
#endif

    // Initialize Bluetooth on a timer to avoid constructor issues
    startTimer(1000); // Initialize after 1 second
}

HeartSyncVST3AudioProcessor::~HeartSyncVST3AudioProcessor()
{
    stopTimer(); // Stop deferred initialization timer
    bluetoothManager.reset();
#if JUCE_MAC
    bridgeClient.reset();
#endif
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HeartSyncVST3AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Biometric output parameters (for DAW automation)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_RAW_HEART_RATE, 
        "Raw Heart Rate",
        juce::NormalisableRange<float>(40.0f, 200.0f, 0.1f),
        70.0f,
        juce::String(),
        juce::AudioProcessorParameter::outputMeter));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_SMOOTHED_HEART_RATE,
        "Smoothed Heart Rate",
        juce::NormalisableRange<float>(40.0f, 200.0f, 0.1f),
        70.0f,
        juce::String(),
        juce::AudioProcessorParameter::outputMeter));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_WET_DRY_RATIO,
        "Wet/Dry Ratio",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        "%",
        juce::AudioProcessorParameter::outputMeter));

    // Control parameters (user adjustable)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_HEART_RATE_OFFSET,
        "Heart Rate Offset",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f),
        0.0f,
        "BPM"));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_SMOOTHING_FACTOR,
        "Smoothing Factor",
        juce::NormalisableRange<float>(0.01f, 2.0f, 0.01f, 0.3f),
        0.1f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_WET_DRY_OFFSET,
        "Wet/Dry Offset",
        juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f),
        0.0f,
        "%"));
    
    // Wet/Dry input source: 0=Raw HR, 1=Smoothed HR (default)
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        PARAM_WET_DRY_INPUT_SOURCE,
        "Wet/Dry Input Source",
        true, // Default to smoothed HR (true)
        ""));
    
    // Tempo sync source: 0=Off, 1=Raw HR, 2=Smoothed HR, 3=Wet/Dry Ratio
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        PARAM_TEMPO_SYNC_SOURCE,
        "Tempo Sync Source",
        juce::StringArray{"Off", "Raw Heart Rate", "Smoothed HR", "Wet/Dry Ratio"},
        0)); // Default to Off

    return { params.begin(), params.end() };
}

//==============================================================================
void HeartSyncVST3AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    
    // Initialize DSP chain
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());
    
    dspChain.prepare(spec);
    
    // Initialize parameter smoothing for sample-accurate automation
    heartRateOffsetSmoothed.reset(sampleRate, 0.05); // 50ms smoothing
    smoothingFactorSmoothed.reset(sampleRate, 0.05);
    wetDryOffsetSmoothed.reset(sampleRate, 0.05);
    
    // Reset performance metrics
    resetPerformanceMetrics();
    
    logSystemMessage("DSP chain prepared for " + juce::String(sampleRate, 1) + " Hz, " + 
                    juce::String(samplesPerBlock) + " samples");
}

void HeartSyncVST3AudioProcessor::releaseResources()
{
    logSystemMessage("DSP resources released");
}

bool HeartSyncVST3AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Support mono and stereo configurations
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void HeartSyncVST3AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);
    
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Update biometric parameters from Bluetooth data
    updateBiometricParameters();
    
    // Update smoothed parameter values (sample-accurate)
    heartRateOffsetSmoothed.setTargetValue(*parameters.getRawParameterValue(PARAM_HEART_RATE_OFFSET));
    smoothingFactorSmoothed.setTargetValue(*parameters.getRawParameterValue(PARAM_SMOOTHING_FACTOR));
    wetDryOffsetSmoothed.setTargetValue(*parameters.getRawParameterValue(PARAM_WET_DRY_OFFSET));
    
    // Apply smoothed parameter changes to Bluetooth manager
    if (bluetoothManager)
    {
        bluetoothManager->setHeartRateOffset(heartRateOffsetSmoothed.getCurrentValue());
        bluetoothManager->setSmoothingFactor(smoothingFactorSmoothed.getCurrentValue());
        bluetoothManager->setWetDryOffset(wetDryOffsetSmoothed.getCurrentValue());
    }
    
    // Professional DSP processing
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    dspChain.process(context);
    
    // Advance smoothed values for next block
    auto numSamples = buffer.getNumSamples();
    heartRateOffsetSmoothed.skip(numSamples);
    smoothingFactorSmoothed.skip(numSamples);
    wetDryOffsetSmoothed.skip(numSamples);
    
    // Update performance metrics
    auto endTime = std::chrono::high_resolution_clock::now();
    auto processingTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    processBlockCount++;
    auto currentCount = processBlockCount.load();
    
    // Update average processing time
    auto currentAvg = averageProcessingTime.load();
    averageProcessingTime.store((currentAvg * (currentCount - 1) + processingTime) / currentCount);
    
    // Update peak processing time
    auto currentPeak = peakProcessingTime.load();
    if (processingTime > currentPeak)
        peakProcessingTime.store(processingTime);
}

void HeartSyncVST3AudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    
    // Still update biometric data when bypassed
    updateBiometricParameters();
    
    // Pass audio through unchanged
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
}

//==============================================================================
const juce::String HeartSyncVST3AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool HeartSyncVST3AudioProcessor::acceptsMidi() const
{
    return false;
}

bool HeartSyncVST3AudioProcessor::producesMidi() const
{
    return true; // Can produce MIDI for tempo sync
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

//==============================================================================
bool HeartSyncVST3AudioProcessor::hasEditor() const
{
    return true; // Show the full 3-row professional UI
}

juce::AudioProcessorEditor* HeartSyncVST3AudioProcessor::createEditor()
{
    return new HeartSyncEditor(*this);
}

//==============================================================================
void HeartSyncVST3AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void HeartSyncVST3AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Professional biometric data access
HeartSyncVST3AudioProcessor::BiometricData HeartSyncVST3AudioProcessor::getCurrentBiometricData() const
{
    const juce::SpinLock::ScopedLockType lock(biometricDataLock);
    return currentBiometricData;
}

std::vector<float> HeartSyncVST3AudioProcessor::getRawHeartRateHistory() const
{
    const juce::SpinLock::ScopedLockType lock(historyDataLock);
    const auto capacity = rawHeartRateHistory.size();
    const auto count = std::min(historyCount, capacity);

    std::vector<float> ordered;
    ordered.reserve(count);
    if (count == 0)
        return ordered;

    const size_t startIndex = (count == capacity) ? historyWriteIndex : 0;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t idx = (startIndex + i) % capacity;
        ordered.push_back(rawHeartRateHistory[idx]);
    }

    return ordered;
}

std::vector<float> HeartSyncVST3AudioProcessor::getSmoothedHeartRateHistory() const
{
    const juce::SpinLock::ScopedLockType lock(historyDataLock);
    const auto capacity = smoothedHeartRateHistory.size();
    const auto count = std::min(historyCount, capacity);

    std::vector<float> ordered;
    ordered.reserve(count);
    if (count == 0)
        return ordered;

    const size_t startIndex = (count == capacity) ? historyWriteIndex : 0;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t idx = (startIndex + i) % capacity;
        ordered.push_back(smoothedHeartRateHistory[idx]);
    }

    return ordered;
}

std::vector<float> HeartSyncVST3AudioProcessor::getWetDryHistory() const
{
    const juce::SpinLock::ScopedLockType lock(historyDataLock);
    const auto capacity = wetDryHistory.size();
    const auto count = std::min(historyCount, capacity);

    std::vector<float> ordered;
    ordered.reserve(count);
    if (count == 0)
        return ordered;

    const size_t startIndex = (count == capacity) ? historyWriteIndex : 0;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t idx = (startIndex + i) % capacity;
        ordered.push_back(wetDryHistory[idx]);
    }

    return ordered;
}

//==============================================================================
// Device management
bool HeartSyncVST3AudioProcessor::isBluetoothAvailable() const
{
#if JUCE_MAC
    if (bridgeClient && bridgeClient->isConnected())
        return bridgeAvailable.load();
#endif
    return bluetoothManager != nullptr;
}

bool HeartSyncVST3AudioProcessor::isDeviceConnected() const
{
#if JUCE_MAC
    if (bridgeClient && bridgeClient->isConnected())
        return bridgeDeviceConnected.load();
#endif
    return bluetoothManager ? bluetoothManager->isConnected() : false;
}

bool HeartSyncVST3AudioProcessor::isScanning() const
{
#if JUCE_MAC
    if (bridgeClient && bridgeClient->isConnected())
        return bridgeScanning.load();
#endif
    return bluetoothManager ? bluetoothManager->isScanning() : false;
}

bool HeartSyncVST3AudioProcessor::isBluetoothReady() const
{
#if JUCE_MAC
    if (bridgeClient && bridgeClient->isConnected())
        return bridgeReady.load();
#endif
    return bluetoothManager ? bluetoothManager->isReady() : false;
}

std::string HeartSyncVST3AudioProcessor::getConnectedDeviceName() const
{
#if JUCE_MAC
    if (bridgeClient && bridgeClient->isConnected())
    {
        const juce::ScopedLock lock(bridgeDevicesLock);
        for (const auto& device : bridgeDevices)
        {
            if (device.identifier == bridgeCurrentDeviceId.toStdString())
                return device.name;
        }
        return bridgeCurrentDeviceId.isNotEmpty() ? bridgeCurrentDeviceId.toStdString() : "Not Connected";
    }
#endif
    return bluetoothManager ? bluetoothManager->getConnectedDeviceName() : "Not Connected";
}

std::vector<HeartSyncVST3AudioProcessor::DeviceInfo> HeartSyncVST3AudioProcessor::getAvailableDevices() const
{
    std::vector<DeviceInfo> devices;
#if JUCE_MAC
    if (bridgeClient && bridgeClient->isConnected())
    {
        const juce::ScopedLock lock(bridgeDevicesLock);
        devices = bridgeDevices;
        return devices;
    }
#endif

    if (bluetoothManager)
    {
        auto bluetoothDevices = bluetoothManager->getDiscoveredDevices();
        devices.reserve(bluetoothDevices.size());
        
        for (const auto& btDevice : bluetoothDevices)
        {
            DeviceInfo device;
            device.name = btDevice.name;
            device.identifier = btDevice.identifier;
            device.signalStrength = btDevice.rssi;
            device.isConnected = btDevice.isConnected;
            device.lastSeen = std::chrono::steady_clock::now(); // Could be improved with actual timestamp from BT manager
            devices.push_back(device);
        }
    }
    
    return devices;
}

//==============================================================================
// Device control
juce::Result HeartSyncVST3AudioProcessor::startDeviceScan()
{
#if JUCE_MAC
    const bool nativeReady = bluetoothManager && bluetoothManager->isReady();
    if (bridgeClient)
    {
        if (bridgeClient->isConnected())
        {
            if (!bridgeReady.load())
            {
                logSystemMessage("Bridge not ready; waiting for permission state " + bridgePermissionState);
                return juce::Result::fail("Bridge not ready");
            }

            bridgeClient->startScan(true);
            bridgeScanning.store(true);
            logSystemMessage("Bridge scan requested");
            return juce::Result::ok();
        }

        if (!nativeReady)
        {
            logSystemMessage("HeartSync Bridge helper not connected; attempting reconnect");
            bridgeClient->launchBridge();
            bridgeClient->connectToBridge();
            return juce::Result::fail("Bridge not connected");
        }
    }
#endif

    if (!bluetoothManager)
        return juce::Result::fail("Bluetooth manager not initialized");
    if (!bluetoothManager->isReady())
        return juce::Result::fail("Bluetooth radio not ready");
    
    try
    {
        bluetoothManager->startScanning();
        logSystemMessage("Device scan started");
        return juce::Result::ok();
    }
    catch (const std::exception& e)
    {
        auto error = "Failed to start device scan: " + juce::String(e.what());
        logError(error);
        return juce::Result::fail(error);
    }
}

void HeartSyncVST3AudioProcessor::stopDeviceScan()
{
#if JUCE_MAC
    if (bridgeClient && bridgeClient->isConnected())
    {
        bridgeClient->startScan(false);
        bridgeScanning.store(false);
        logSystemMessage("Bridge scan stopped");
        return;
    }
#endif

    if (bluetoothManager)
    {
        bluetoothManager->stopScanning();
        logSystemMessage("Device scan stopped");
    }
}

juce::Result HeartSyncVST3AudioProcessor::connectToDevice(const std::string& deviceIdentifier)
{
#if JUCE_MAC
    const bool nativeReady = bluetoothManager && bluetoothManager->isReady();
    if (bridgeClient)
    {
        if (bridgeClient->isConnected())
        {
            if (!bridgeReady.load())
            {
                logSystemMessage("Bridge not ready; cannot connect to device");
                return juce::Result::fail("Bridge not ready");
            }

            bridgeClient->connectToDevice(juce::String(deviceIdentifier));
            bridgeScanning.store(false);
            logSystemMessage("Bridge connecting to device: " + juce::String(deviceIdentifier));
            return juce::Result::ok();
        }

        if (!nativeReady)
        {
            logSystemMessage("HeartSync Bridge helper not connected; attempting reconnect before device connection");
            bridgeClient->launchBridge();
            bridgeClient->connectToBridge();
            return juce::Result::fail("Bridge not connected");
        }
    }
#endif

    if (!bluetoothManager)
        return juce::Result::fail("Bluetooth manager not initialized");
    
    try
    {
        bluetoothManager->connectToDevice(deviceIdentifier);
        logSystemMessage("Connecting to device: " + juce::String(deviceIdentifier));
        return juce::Result::ok();
    }
    catch (const std::exception& e)
    {
        auto error = "Failed to connect to device: " + juce::String(e.what());
        logError(error);
        return juce::Result::fail(error);
    }
}

void HeartSyncVST3AudioProcessor::disconnectDevice()
{
#if JUCE_MAC
    if (bridgeClient && bridgeClient->isConnected())
    {
        bridgeClient->disconnectDevice();
        bridgeDeviceConnected.store(false);
        logSystemMessage("Bridge disconnect requested");
        return;
    }
#endif

    if (bluetoothManager)
    {
        bluetoothManager->disconnectFromDevice();
        logSystemMessage("Device disconnected");
    }
}

bool HeartSyncVST3AudioProcessor::hasNativeBluetoothStack() const
{
    return bluetoothManager != nullptr;
}

bool HeartSyncVST3AudioProcessor::isNativeBluetoothReady() const
{
    return bluetoothManager ? bluetoothManager->isReady() : false;
}

#if JUCE_MAC
bool HeartSyncVST3AudioProcessor::isBridgeClientConfigured() const
{
    return bridgeClient != nullptr;
}

bool HeartSyncVST3AudioProcessor::isBridgeClientConnected() const
{
    return bridgeClient && bridgeClient->isConnected();
}

bool HeartSyncVST3AudioProcessor::isBridgeClientReady() const
{
    return bridgeClient && bridgeClient->isConnected() && bridgeReady.load();
}

juce::String HeartSyncVST3AudioProcessor::getBridgePermissionState() const
{
    return bridgePermissionState;
}

void HeartSyncVST3AudioProcessor::requestBridgeReconnect(bool relaunch)
{
    if (! bridgeClient)
        return;

    if (relaunch)
        bridgeClient->launchBridge();

    bridgeClient->connectToBridge();
}
#else
bool HeartSyncVST3AudioProcessor::isBridgeClientConfigured() const { return false; }
bool HeartSyncVST3AudioProcessor::isBridgeClientConnected() const { return false; }
bool HeartSyncVST3AudioProcessor::isBridgeClientReady() const { return false; }
juce::String HeartSyncVST3AudioProcessor::getBridgePermissionState() const { return {}; }
void HeartSyncVST3AudioProcessor::requestBridgeReconnect(bool) {}
#endif

//==============================================================================
// Performance metrics
HeartSyncVST3AudioProcessor::PerformanceMetrics HeartSyncVST3AudioProcessor::getPerformanceMetrics() const
{
    PerformanceMetrics metrics;
    metrics.averageProcessingTimeMs = averageProcessingTime.load();
    metrics.peakProcessingTimeMs = peakProcessingTime.load();
    metrics.totalProcessedBlocks = processBlockCount.load();
    
    // Calculate CPU usage percentage (approximation)
    if (getSampleRate() > 0 && getBlockSize() > 0)
    {
        double blockDurationMs = (getBlockSize() / getSampleRate()) * 1000.0;
        metrics.cpuUsagePercent = (metrics.averageProcessingTimeMs / blockDurationMs) * 100.0;
    }
    
    return metrics;
}

void HeartSyncVST3AudioProcessor::resetPerformanceMetrics()
{
    averageProcessingTime.store(0.0);
    peakProcessingTime.store(0.0);
    processBlockCount.store(0);
    lastResetTime = std::chrono::steady_clock::now();
}

//==============================================================================
// Internal processing methods
void HeartSyncVST3AudioProcessor::updateBiometricParameters()
{
#if JUCE_MAC
    if (bridgeClient && bridgeClient->isConnected() && bridgeDataValid.load())
    {
        // CRITICAL DATA FLOW (matching Python):
        // 1. Raw HR from device
        const float measuredHr = bridgeRawHeartRate.load();
        
        // Check if we actually have valid heart rate data
        if (measuredHr <= 0)
        {
            const juce::SpinLock::ScopedLockType lock(biometricDataLock);
            currentBiometricData.isDataValid = false;
            return;
        }
        
        // 2. Apply HR OFFSET → This is the displayed "HEART RATE (BPM)"
        const float hrOffset = parameters.getRawParameterValue(PARAM_HEART_RATE_OFFSET)->load();
        const float adjustedRawHr = measuredHr + hrOffset;
        
        // 3. Apply SMOOTHING to the offset-adjusted HR → This is the displayed "SMOOTHED HR (BPM)"
        const float smoothingFactor = juce::jlimit(0.01f, 1.0f,
            parameters.getRawParameterValue(PARAM_SMOOTHING_FACTOR)->load());
        
        if (!bridgeSmootherInitialised)
        {
            bridgeSmoothedValue = adjustedRawHr;
            bridgeSmootherInitialised = true;
        }
        
        const float alpha = smoothingFactor;
        bridgeSmoothedValue = bridgeSmoothedValue + alpha * (adjustedRawHr - bridgeSmoothedValue);
        const float smoothedHr = bridgeSmoothedValue;
        
        // 4. Calculate wet/dry based on difference, then apply WET/DRY OFFSET → This is displayed "WET/DRY RATIO"
        const float wetDryOffset = parameters.getRawParameterValue(PARAM_WET_DRY_OFFSET)->load();
        const bool useSmoothed = parameters.getRawParameterValue(PARAM_WET_DRY_INPUT_SOURCE)->load() > 0.5f;
        
        // Calculate difference based on input selector
        const float diff = useSmoothed 
            ? std::abs(adjustedRawHr - smoothedHr)  // Smoothed mode: difference between raw and smoothed
            : std::abs(smoothedHr - adjustedRawHr); // Raw mode: same but inverted for consistency
        
        const float wetDry = juce::jlimit(0.0f, 100.0f, 50.0f + diff * 2.0f + wetDryOffset);

        bridgeSmoothedHeartRate.store(smoothedHr);
        bridgeWetDryRatio.store(wetDry);

        {
            const juce::SpinLock::ScopedLockType lock(biometricDataLock);
            currentBiometricData.rawHeartRate = adjustedRawHr;        // HR + offset
            currentBiometricData.smoothedHeartRate = smoothedHr;       // smoothed(HR + offset)
            currentBiometricData.wetDryRatio = wetDry;                 // calculated + wet/dry offset
            currentBiometricData.isDataValid = (measuredHr > 0);
            currentBiometricData.timestamp = std::chrono::steady_clock::now();
        }

        // Update VST3 parameters for DAW automation
        if (auto* rawHrParam = parameters.getParameter(PARAM_RAW_HEART_RATE))
            rawHrParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, (adjustedRawHr - 40.0f) / 160.0f));
        if (auto* smoothParam = parameters.getParameter(PARAM_SMOOTHED_HEART_RATE))
            smoothParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, (smoothedHr - 40.0f) / 160.0f));
        if (auto* wetDryParam = parameters.getParameter(PARAM_WET_DRY_RATIO))
            wetDryParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, wetDry / 100.0f));

        addToHistory(adjustedRawHr, smoothedHr, wetDry);

        if (onBiometricDataUpdated)
            onBiometricDataUpdated();

        return;
    }
#endif

    if (!bluetoothManager)
        return;
    
    // Native Bluetooth path - needs same data flow logic
    float measuredHr = bluetoothManager->getCurrentHeartRate();
    
    if (measuredHr <= 0)
    {
        const juce::SpinLock::ScopedLockType lock(biometricDataLock);
        currentBiometricData.isDataValid = false;
        return;
    }
    
    // Apply same data flow as bridge path
    const float hrOffset = parameters.getRawParameterValue(PARAM_HEART_RATE_OFFSET)->load();
    const float adjustedRawHr = measuredHr + hrOffset;
    
    const float smoothingFactor = juce::jlimit(0.01f, 1.0f,
        parameters.getRawParameterValue(PARAM_SMOOTHING_FACTOR)->load());
    
    // Update smoothing (maintain state in bluetoothManager)
    bluetoothManager->setSmoothingFactor(smoothingFactor);
    float smoothedHr = bluetoothManager->getSmoothedHeartRate();
    
    const float wetDryOffset = parameters.getRawParameterValue(PARAM_WET_DRY_OFFSET)->load();
    const bool useSmoothed = parameters.getRawParameterValue(PARAM_WET_DRY_INPUT_SOURCE)->load() > 0.5f;
    
    // Calculate difference based on input selector
    const float diff = useSmoothed 
        ? std::abs(adjustedRawHr - smoothedHr)  // Smoothed mode: difference between raw and smoothed
        : std::abs(smoothedHr - adjustedRawHr); // Raw mode: same but inverted for consistency
    
    const float wetDry = juce::jlimit(0.0f, 100.0f, 50.0f + diff * 2.0f + wetDryOffset);
    
    // Update thread-safe biometric data
    {
        const juce::SpinLock::ScopedLockType lock(biometricDataLock);
        currentBiometricData.rawHeartRate = adjustedRawHr;
        currentBiometricData.smoothedHeartRate = smoothedHr;
        currentBiometricData.wetDryRatio = wetDry;
        currentBiometricData.isDataValid = true;
        currentBiometricData.timestamp = std::chrono::steady_clock::now();
    }
    
    // Update VST3 parameters for DAW automation
    auto rawHrParam = parameters.getParameter(PARAM_RAW_HEART_RATE);
    auto smoothedHrParam = parameters.getParameter(PARAM_SMOOTHED_HEART_RATE);
    auto wetDryParam = parameters.getParameter(PARAM_WET_DRY_RATIO);
    
    if (rawHrParam && smoothedHrParam && wetDryParam)
    {
        // Convert to normalized range [0,1] for VST3
        float rawHrNorm = juce::jlimit(0.0f, 1.0f, (adjustedRawHr - 40.0f) / 160.0f);
        float smoothedHrNorm = juce::jlimit(0.0f, 1.0f, (smoothedHr - 40.0f) / 160.0f);
        float wetDryNorm = juce::jlimit(0.0f, 1.0f, wetDry / 100.0f);
        
        rawHrParam->setValueNotifyingHost(rawHrNorm);
        smoothedHrParam->setValueNotifyingHost(smoothedHrNorm);
        wetDryParam->setValueNotifyingHost(wetDryNorm);
    }
    
    // Update history arrays with offset-adjusted values
    addToHistory(adjustedRawHr, smoothedHr, wetDry);
    
    // Update tempo sync if enabled
    if (tempoSyncSource != TempoSyncSource::Off)
    {
        BiometricData syncData;
        syncData.rawHeartRate = adjustedRawHr;
        syncData.smoothedHeartRate = smoothedHr;
        syncData.wetDryRatio = wetDry;
        syncData.isDataValid = true;
        updateTempoSync(syncData);
    }
    
    // Notify UI of data update
    if (onBiometricDataUpdated)
        onBiometricDataUpdated();
}

//==============================================================================
// Tempo Sync Implementation
void HeartSyncVST3AudioProcessor::setTempoSyncSource(TempoSyncSource source)
{
    // Only one source can sync tempo at a time
    tempoSyncSource = source;
    
    // Update the parameter
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter(PARAM_TEMPO_SYNC_SOURCE)))
    {
        param->setValueNotifyingHost((float)static_cast<int>(source) / 3.0f);
    }
    
    if (source != TempoSyncSource::Off)
    {
        logSystemMessage("Tempo sync enabled: " + getTempoSyncSourceName());
    }
    else
    {
        logSystemMessage("Tempo sync disabled");
        currentSuggestedTempo = 120.0f; // Reset to default
    }
}

juce::String HeartSyncVST3AudioProcessor::getTempoSyncSourceName() const
{
    switch (tempoSyncSource)
    {
        case TempoSyncSource::RawHeartRate:     return "Raw Heart Rate";
        case TempoSyncSource::SmoothedHeartRate: return "Smoothed HR";
        case TempoSyncSource::WetDryRatio:      return "Wet/Dry Ratio";
        default:                                return "Off";
    }
}

void HeartSyncVST3AudioProcessor::updateTempoSync(const BiometricData& data)
{
    if (tempoSyncSource == TempoSyncSource::Off || !data.isDataValid)
        return;
    
    float sourceValue = 0.0f;
    
    switch (tempoSyncSource)
    {
        case TempoSyncSource::RawHeartRate:
            sourceValue = data.rawHeartRate;
            break;
        case TempoSyncSource::SmoothedHeartRate:
            sourceValue = data.smoothedHeartRate;
            break;
        case TempoSyncSource::WetDryRatio:
            sourceValue = data.wetDryRatio;
            break;
        default:
            return;
    }
    
    float targetTempo = mapValueToTempo(sourceValue, tempoSyncSource);
    
    // Smooth tempo changes to avoid jarring jumps (30% smoothing)
    const float smoothing = 0.3f;
    currentSuggestedTempo += (targetTempo - currentSuggestedTempo) * smoothing;
    currentSuggestedTempo = juce::jlimit(60.0f, 200.0f, currentSuggestedTempo);
}

float HeartSyncVST3AudioProcessor::mapValueToTempo(float value, TempoSyncSource source) const
{
    float targetTempo = 120.0f; // Default fallback
    
    switch (source)
    {
        case TempoSyncSource::RawHeartRate:
        case TempoSyncSource::SmoothedHeartRate:
            // Direct mapping: HR value becomes tempo
            // User's heart rate directly drives DAW tempo
            // Example: HR 60 → 60 BPM, HR 120 → 120 BPM, HR 180 → 180 BPM
            targetTempo = value; // Direct 1:1 mapping by default
            break;
            
        case TempoSyncSource::WetDryRatio:
            // Map Wet/Dry (0-100%) to tempo range
            // Example: 0% → 80 BPM, 50% → 130 BPM, 100% → 180 BPM
            targetTempo = 80.0f + (value / 100.0f) * 100.0f;
            break;
            
        default:
            break;
    }
    
    return juce::jlimit(60.0f, 200.0f, targetTempo);
}

void HeartSyncVST3AudioProcessor::addToHistory(float rawHr, float smoothedHr, float wetDry)
{
    const juce::SpinLock::ScopedLockType lock(historyDataLock);
    
    rawHeartRateHistory[historyWriteIndex] = rawHr;
    smoothedHeartRateHistory[historyWriteIndex] = smoothedHr;
    wetDryHistory[historyWriteIndex] = wetDry;
    
    historyWriteIndex = (historyWriteIndex + 1) % rawHeartRateHistory.size();
    if (historyCount < rawHeartRateHistory.size())
        ++historyCount;
}

void HeartSyncVST3AudioProcessor::logError(const juce::String& error) const
{
    // Note: Cannot modify mutable members from const method
    // This is a design limitation - error logging should be non-const
    // For now, just log to debug output
    DBG("HeartSync Error: " << error);
    
    // TODO: Implement thread-safe error logging without const issues
}

void HeartSyncVST3AudioProcessor::logSystemMessage(const juce::String& message) const
{
    DBG("HeartSync: " << message);
    
    if (onSystemMessage)
        onSystemMessage(message);
}

//==============================================================================
// Bluetooth event handlers
void HeartSyncVST3AudioProcessor::handleHeartRateData(float heartRate)
{
    juce::ignoreUnused(heartRate);
    // Heart rate processing is handled in updateBiometricParameters()
}

void HeartSyncVST3AudioProcessor::handleBluetoothStateChange()
{
    if (onBluetoothStateChanged)
        onBluetoothStateChanged();
}

void HeartSyncVST3AudioProcessor::handleDeviceDiscovery()
{
    if (onDeviceListUpdated)
        onDeviceListUpdated();
}

void HeartSyncVST3AudioProcessor::handleSystemMessage(const std::string& message)
{
    logSystemMessage(juce::String(message));
}

#if JUCE_MAC
void HeartSyncVST3AudioProcessor::initialiseBridgeClient()
{
    logSystemMessage("Initializing HeartSync Bridge helper interface");
    bridgeClient = std::make_unique<HeartSyncBLEClient>();

    bridgeClient->onLog = [this](const juce::String& message) {
        logSystemMessage("Bridge: " + message);
    };

    bridgeClient->onBridgeConnected = [this]() {
        bridgeAvailable.store(true);
        bridgeReady.store(true);
        logSystemMessage("Bridge helper connected");
        if (onBluetoothStateChanged)
            onBluetoothStateChanged();
    };

    bridgeClient->onBridgeDisconnected = [this]() {
        bridgeAvailable.store(false);
        bridgeReady.store(false);
        bridgeScanning.store(false);
        bridgeDeviceConnected.store(false);
        bridgeDataValid.store(false);
        bridgeSmootherInitialised = false;
        bridgeCurrentDeviceId.clear();

        {
            const juce::ScopedLock lock(bridgeDevicesLock);
            bridgeDevices.clear();
        }

        logSystemMessage("Bridge helper disconnected");
        if (onBluetoothStateChanged)
            onBluetoothStateChanged();
    };

    bridgeClient->onPermissionChanged = [this](const juce::String& state) {
        bridgePermissionState = state;

        auto lower = state.toLowerCase();
        const bool denied = lower == "denied" || lower == "restricted" || lower == "unauthorized";
        bridgeReady.store(!denied);

        logSystemMessage("Bridge permission state: " + state);
        if (onBluetoothStateChanged)
            onBluetoothStateChanged();
    };

    bridgeClient->onDeviceFound = [this](const HeartSyncBLEClient::DeviceInfo& info) {
        logSystemMessage("Processor: Received device from bridge - id: '" + info.id + "', name: '" + info.name + "'");
        
        DeviceInfo device;
        device.identifier = info.id.toStdString();
        device.name = info.getDisplayName().toStdString();
        device.signalStrength = info.rssi;
        device.isConnected = (info.id == bridgeCurrentDeviceId);
        device.lastSeen = std::chrono::steady_clock::now();
        device.services.clear();
        for (const auto& service : info.services)
            device.services.push_back(service.toStdString());

        {
            const juce::ScopedLock lock(bridgeDevicesLock);
            bool updated = false;
            for (auto& existing : bridgeDevices)
            {
                if (existing.identifier == device.identifier)
                {
                    existing = device;
                    updated = true;
                    break;
                }
            }

            if (!updated)
                bridgeDevices.push_back(device);
        }

        logSystemMessage("Processor: Device list now has " + juce::String(bridgeDevices.size()) + " devices");

        if (onDeviceListUpdated)
            onDeviceListUpdated();
    };

    bridgeClient->onHeartRate = [this](float bpm, juce::Array<float> rr) {
        juce::ignoreUnused(rr);
        updateBridgeBiometrics(bpm);
    };

    bridgeClient->onConnected = [this](const juce::String& deviceId) {
        bridgeDeviceConnected.store(true);
        bridgeCurrentDeviceId = deviceId;
        bridgeScanning.store(false);
        bridgeDataValid.store(false);

        {
            const juce::ScopedLock lock(bridgeDevicesLock);
            for (auto& device : bridgeDevices)
                device.isConnected = (device.identifier == deviceId.toStdString());
        }

        logSystemMessage("Bridge connected to device: " + deviceId);
        if (onDeviceListUpdated)
            onDeviceListUpdated();
        if (onBluetoothStateChanged)
            onBluetoothStateChanged();
    };

    bridgeClient->onDisconnected = [this](const juce::String& reason) {
        bridgeDeviceConnected.store(false);
        bridgeDataValid.store(false);
        bridgeCurrentDeviceId.clear();
        bridgeSmootherInitialised = false;

        {
            const juce::ScopedLock lock(bridgeDevicesLock);
            for (auto& device : bridgeDevices)
                device.isConnected = false;
        }

        logSystemMessage("Bridge disconnected: " + reason);
        if (onDeviceListUpdated)
            onDeviceListUpdated();
        if (onBluetoothStateChanged)
            onBluetoothStateChanged();
    };

    bridgeClient->onError = [this](const juce::String& error) {
        logSystemMessage("Bridge error: " + error);
        logError("Bridge error: " + error);
    };

    bridgeClient->launchBridge();
    bridgeClient->connectToBridge();
}

void HeartSyncVST3AudioProcessor::updateBridgeBiometrics(float bpm)
{
    bridgeRawHeartRate.store(bpm);
    bridgeDataValid.store(bpm > 0.0f);
}
#endif
#if ! JUCE_MAC
void HeartSyncVST3AudioProcessor::initialiseBridgeClient() {}
void HeartSyncVST3AudioProcessor::updateBridgeBiometrics(float) {}
#endif

//==============================================================================
// Timer callback for deferred Bluetooth initialization
void HeartSyncVST3AudioProcessor::timerCallback()
{
    stopTimer(); // Only run once
    
    // Initialize Bluetooth LE Manager with professional error handling
    try 
    {
        bluetoothManager = std::make_unique<BluetoothManager>();
        
        // Set up Bluetooth callbacks with professional error handling
        bluetoothManager->onHeartRateReceived = [this](float heartRate) {
            handleHeartRateData(heartRate);
        };
        
        bluetoothManager->onConnectionStatusChanged = [this]() {
            handleBluetoothStateChange();
        };
        
        bluetoothManager->onDeviceDiscovered = [this]() {
            handleDeviceDiscovery();
        };
        
        bluetoothManager->onConsoleMessage = [this](const std::string& message) {
            handleSystemMessage(message);
        };
        
        logSystemMessage("Bluetooth LE Manager initialized successfully");
    }
    catch (const std::exception& e)
    {
        logError("Failed to initialize Bluetooth manager: " + juce::String(e.what()));
    }
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HeartSyncVST3AudioProcessor();
}