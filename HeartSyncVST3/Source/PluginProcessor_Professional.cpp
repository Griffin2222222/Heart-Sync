#include "PluginProcessor_Professional.h"
#include "PluginEditor.h"
#include <chrono>

//==============================================================================
// Parameter IDs (professional naming convention)
const juce::String HeartSyncVST3AudioProcessor::PARAM_RAW_HEART_RATE = "raw_heart_rate";
const juce::String HeartSyncVST3AudioProcessor::PARAM_SMOOTHED_HEART_RATE = "smoothed_heart_rate";
const juce::String HeartSyncVST3AudioProcessor::PARAM_WET_DRY_RATIO = "wet_dry_ratio";
const juce::String HeartSyncVST3AudioProcessor::PARAM_HEART_RATE_OFFSET = "heart_rate_offset";
const juce::String HeartSyncVST3AudioProcessor::PARAM_SMOOTHING_FACTOR = "smoothing_factor";
const juce::String HeartSyncVST3AudioProcessor::PARAM_WET_DRY_OFFSET = "wet_dry_offset";

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
    
    // Initialize Bluetooth on a timer to avoid constructor issues
    startTimer(1000); // Initialize after 1 second
}

HeartSyncVST3AudioProcessor::~HeartSyncVST3AudioProcessor()
{
    stopTimer(); // Stop deferred initialization timer
    bluetoothManager.reset();
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

std::array<float, 200> HeartSyncVST3AudioProcessor::getRawHeartRateHistory() const
{
    const juce::SpinLock::ScopedLockType lock(historyDataLock);
    return rawHeartRateHistory;
}

std::array<float, 200> HeartSyncVST3AudioProcessor::getSmoothedHeartRateHistory() const
{
    const juce::SpinLock::ScopedLockType lock(historyDataLock);
    return smoothedHeartRateHistory;
}

std::array<float, 200> HeartSyncVST3AudioProcessor::getWetDryHistory() const
{
    const juce::SpinLock::ScopedLockType lock(historyDataLock);
    return wetDryHistory;
}

//==============================================================================
// Device management
bool HeartSyncVST3AudioProcessor::isBluetoothAvailable() const
{
    return bluetoothManager != nullptr;
}

bool HeartSyncVST3AudioProcessor::isDeviceConnected() const
{
    return bluetoothManager ? bluetoothManager->isConnected() : false;
}

bool HeartSyncVST3AudioProcessor::isScanning() const
{
    return bluetoothManager ? bluetoothManager->isScanning() : false;
}

std::string HeartSyncVST3AudioProcessor::getConnectedDeviceName() const
{
    return bluetoothManager ? bluetoothManager->getConnectedDeviceName() : "Not Connected";
}

std::vector<HeartSyncVST3AudioProcessor::DeviceInfo> HeartSyncVST3AudioProcessor::getAvailableDevices() const
{
    std::vector<DeviceInfo> devices;
    
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
    if (!bluetoothManager)
        return juce::Result::fail("Bluetooth manager not initialized");
    
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
    if (bluetoothManager)
    {
        bluetoothManager->stopScanning();
        logSystemMessage("Device scan stopped");
    }
}

juce::Result HeartSyncVST3AudioProcessor::connectToDevice(const std::string& deviceIdentifier)
{
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
    if (bluetoothManager)
    {
        bluetoothManager->disconnectFromDevice();
        logSystemMessage("Device disconnected");
    }
}

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
    if (!bluetoothManager)
        return;
    
    // Get current biometric data
    float rawHr = bluetoothManager->getCurrentHeartRate();
    float smoothedHr = bluetoothManager->getSmoothedHeartRate();
    float wetDry = bluetoothManager->getWetDryRatio();
    
    // Update thread-safe biometric data
    {
        const juce::SpinLock::ScopedLockType lock(biometricDataLock);
        currentBiometricData.rawHeartRate = rawHr;
        currentBiometricData.smoothedHeartRate = smoothedHr;
        currentBiometricData.wetDryRatio = wetDry;
        currentBiometricData.isDataValid = (rawHr > 0);
        currentBiometricData.timestamp = std::chrono::steady_clock::now();
    }
    
    // Update VST3 parameters for DAW automation
    auto rawHrParam = parameters.getParameter(PARAM_RAW_HEART_RATE);
    auto smoothedHrParam = parameters.getParameter(PARAM_SMOOTHED_HEART_RATE);
    auto wetDryParam = parameters.getParameter(PARAM_WET_DRY_RATIO);
    
    if (rawHrParam && smoothedHrParam && wetDryParam)
    {
        // Convert to normalized range [0,1] for VST3
        float rawHrNorm = juce::jlimit(0.0f, 1.0f, (rawHr - 40.0f) / 160.0f);
        float smoothedHrNorm = juce::jlimit(0.0f, 1.0f, (smoothedHr - 40.0f) / 160.0f);
        float wetDryNorm = juce::jlimit(0.0f, 1.0f, wetDry / 100.0f);
        
        rawHrParam->setValueNotifyingHost(rawHrNorm);
        smoothedHrParam->setValueNotifyingHost(smoothedHrNorm);
        wetDryParam->setValueNotifyingHost(wetDryNorm);
    }
    
    // Update history arrays
    addToHistory(rawHr, smoothedHr, wetDry);
    
    // Notify UI of data update
    if (onBiometricDataUpdated)
        onBiometricDataUpdated();
}

void HeartSyncVST3AudioProcessor::addToHistory(float rawHr, float smoothedHr, float wetDry)
{
    const juce::SpinLock::ScopedLockType lock(historyDataLock);
    
    rawHeartRateHistory[historyWriteIndex] = rawHr;
    smoothedHeartRateHistory[historyWriteIndex] = smoothedHr;
    wetDryHistory[historyWriteIndex] = wetDry;
    
    historyWriteIndex = (historyWriteIndex + 1) % rawHeartRateHistory.size();
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