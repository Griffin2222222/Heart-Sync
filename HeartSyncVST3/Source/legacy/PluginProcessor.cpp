#include "PluginProcessor.h"
#include "PluginEditor.h"

// Parameter IDs
const juce::String HeartSyncVST3AudioProcessor::RAW_HR_PARAM_ID = "raw_hr";
const juce::String HeartSyncVST3AudioProcessor::SMOOTHED_HR_PARAM_ID = "smoothed_hr";
const juce::String HeartSyncVST3AudioProcessor::WET_DRY_PARAM_ID = "wet_dry_ratio";
const juce::String HeartSyncVST3AudioProcessor::HR_OFFSET_PARAM_ID = "hr_offset";
const juce::String HeartSyncVST3AudioProcessor::SMOOTHING_PARAM_ID = "smoothing_factor";
const juce::String HeartSyncVST3AudioProcessor::WET_DRY_OFFSET_PARAM_ID = "wet_dry_offset";

HeartSyncVST3AudioProcessor::HeartSyncVST3AudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Initialize Bluetooth LE Manager
    bluetoothManager = std::make_unique<BluetoothManager>();
    
    // Set up Bluetooth callbacks
    bluetoothManager->onHeartRateReceived = [this](float heartRate) {
        onHeartRateReceived(heartRate);
    };
    
    bluetoothManager->onConnectionStatusChanged = [this]() {
        onBluetoothConnectionChanged();
    };
    
    bluetoothManager->onDeviceDiscovered = [this]() {
        onBluetoothDeviceDiscovered();
    };
    
    bluetoothManager->onConsoleMessage = [this](const std::string& message) {
        onConsoleMessage(message);
    };
    
    onConsoleMessage("❖ HeartSync VST3 - Self-Contained Bluetooth LE Plugin Initialized");
}

HeartSyncVST3AudioProcessor::~HeartSyncVST3AudioProcessor()
{
    bluetoothManager.reset();
}

juce::AudioProcessorValueTreeState::ParameterLayout HeartSyncVST3AudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Heart rate parameters (read-only for display, but automatable for DAW)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        RAW_HR_PARAM_ID, "Raw Heart Rate", 40.0f, 180.0f, 70.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        SMOOTHED_HR_PARAM_ID, "Smoothed Heart Rate", 40.0f, 180.0f, 70.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        WET_DRY_PARAM_ID, "Wet/Dry Ratio", 0.0f, 100.0f, 50.0f));

    // Control parameters (user adjustable)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        HR_OFFSET_PARAM_ID, "HR Offset", -100.0f, 100.0f, 0.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        SMOOTHING_PARAM_ID, "Smoothing Factor", 0.01f, 10.0f, 0.1f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        WET_DRY_OFFSET_PARAM_ID, "Wet/Dry Offset", -100.0f, 100.0f, 0.0f));

    return { params.begin(), params.end() };
}

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
    return true; // We can produce MIDI for tempo sync
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
    juce::ignoreUnused(sampleRate, samplesPerBlock);
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
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Update VST3 parameters with current heart rate data from Bluetooth
    if (bluetoothManager) {
        auto rawHrParam = parameters.getParameter(RAW_HR_PARAM_ID);
        auto smoothedHrParam = parameters.getParameter(SMOOTHED_HR_PARAM_ID);
        auto wetDryParam = parameters.getParameter(WET_DRY_PARAM_ID);
        
        if (rawHrParam && smoothedHrParam && wetDryParam) {
            // Normalize values for VST3 parameters (0.0-1.0)
            float rawHr = bluetoothManager->getCurrentHeartRate();
            float smoothedHr = bluetoothManager->getSmoothedHeartRate();
            float wetDry = bluetoothManager->getWetDryRatio();
            
            float rawHrNorm = (rawHr - 40.0f) / 140.0f;
            float smoothedHrNorm = (smoothedHr - 40.0f) / 140.0f;
            float wetDryNorm = wetDry / 100.0f;
            
            rawHrParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, rawHrNorm));
            smoothedHrParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, smoothedHrNorm));
            wetDryParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, wetDryNorm));
        }
        
        // Update Bluetooth manager with current parameter values
        auto hrOffsetParam = parameters.getRawParameterValue(HR_OFFSET_PARAM_ID);
        auto smoothingParam = parameters.getRawParameterValue(SMOOTHING_PARAM_ID);
        auto wetDryOffsetParam = parameters.getRawParameterValue(WET_DRY_OFFSET_PARAM_ID);
        
        if (hrOffsetParam) bluetoothManager->setHeartRateOffset(*hrOffsetParam);
        if (smoothingParam) bluetoothManager->setSmoothingFactor(*smoothingParam);
        if (wetDryOffsetParam) bluetoothManager->setWetDryOffset(*wetDryOffsetParam);
    }

    // Process audio (pass-through for now - could add heart rate-based effects)
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Simple pass-through - could add heart rate-based modulation here
            channelData[sample] = channelData[sample];
        }
    }
}

bool HeartSyncVST3AudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* HeartSyncVST3AudioProcessor::createEditor()
{
    return new HeartSyncVST3Editor(*this);
}

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

// Bluetooth callback implementations
void HeartSyncVST3AudioProcessor::onHeartRateReceived(float heartRate)
{
    // Heart rate processing is handled in BluetoothManager
    // This callback can be used for additional processing if needed
    juce::ignoreUnused(heartRate);
}

void HeartSyncVST3AudioProcessor::onBluetoothConnectionChanged()
{
    if (onBluetoothStateChanged) {
        onBluetoothStateChanged();
    }
}

void HeartSyncVST3AudioProcessor::onBluetoothDeviceDiscovered()
{
    if (onDeviceListChanged) {
        onDeviceListChanged();
    }
}

void HeartSyncVST3AudioProcessor::onConsoleMessage(const std::string& message)
{
    latestConsoleMessage = message;
    if (onConsoleMessageReceived) {
        onConsoleMessageReceived(message);
    }
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HeartSyncVST3AudioProcessor();
}