#include "PluginProcessor.h"
#include "PluginEditor.h"

HeartSyncProcessor::HeartSyncProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Set up BLE callbacks
    #ifdef HEARTSYNC_USE_BRIDGE
    bleClient.onHrData = [this](int bpm, double timestamp) {
        std::vector<float> rrIntervals; // TODO: Bridge should send RR intervals
        handleHeartRateUpdate(bpm, rrIntervals);
        bleConnected = true;
    };
    
    bleClient.onPermissionChanged = [this](const juce::String& state) {
        DBG("BLE Permission: " + state);
        if (state == "denied")
            bleConnected = false;
    };
    
    bleClient.onError = [this](const juce::String& error) {
        DBG("BLE Error: " + error);
        handleBLEDisconnect();
    };
    
    // Auto-connect to Bridge on construction
    bleClient.connectToBridge();
    #else
    bleManager.setHeartRateCallback([this](int hr, const std::vector<float>& rr) {
        handleHeartRateUpdate(hr, rr);
    });
    
    bleManager.setDisconnectCallback([this]() {
        handleBLEDisconnect();
    });
    #endif
}

HeartSyncProcessor::~HeartSyncProcessor()
{
    #ifdef HEARTSYNC_USE_BRIDGE
    bleClient.disconnect();
    #else
    bleManager.disconnect();
    #endif
}

juce::AudioProcessorValueTreeState::ParameterLayout HeartSyncProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Raw HR (40-180 BPM) - read-only, updated by BLE
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_RAW_HR,
        "Raw Heart Rate",
        juce::NormalisableRange<float>(40.0f, 180.0f, 1.0f),
        70.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value)) + " BPM"; }
    ));
    
    // Smoothed HR (40-180 BPM) - read-only, updated by processor
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_SMOOTHED_HR,
        "Smoothed Heart Rate",
        juce::NormalisableRange<float>(40.0f, 180.0f, 1.0f),
        70.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value)) + " BPM"; }
    ));
    
    // Wet/Dry Ratio (0-100%) - calculated from smoothed HR
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_WET_DRY_RATIO,
        "Wet/Dry Ratio",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
        50.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value)) + "%"; }
    ));
    
    // HR Offset (-100 to +100) - user-adjustable
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_HR_OFFSET,
        "HR Offset",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return (value >= 0 ? "+" : "") + juce::String(static_cast<int>(value));
        }
    ));
    
    // Smoothing Factor (0.01-10.0) - user-adjustable
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_SMOOTHING_FACTOR,
        "Smoothing Factor",
        juce::NormalisableRange<float>(0.01f, 10.0f, 0.01f, 0.3f), // Skewed toward lower values
        0.15f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 2); }
    ));
    
    // Wet/Dry Offset (-100 to +100) - user-adjustable
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        PARAM_WET_DRY_OFFSET,
        "Wet/Dry Offset",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            return (value >= 0 ? "+" : "") + juce::String(static_cast<int>(value));
        }
    ));
    
    return { params.begin(), params.end() };
}

void HeartSyncProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void HeartSyncProcessor::releaseResources()
{
}

void HeartSyncProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    // Clear output (pass-through audio unchanged)
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    
    // Update processor parameters from APVTS
    float hrOffset = apvts.getRawParameterValue(PARAM_HR_OFFSET)->load();
    float smoothingFactor = apvts.getRawParameterValue(PARAM_SMOOTHING_FACTOR)->load();
    float wetDryOffset = apvts.getRawParameterValue(PARAM_WET_DRY_OFFSET)->load();
    
    hrProcessor.setHROffset(hrOffset);
    hrProcessor.setSmoothingFactor(smoothingFactor);
    hrProcessor.setWetDryOffset(wetDryOffset);
    
    // Send MIDI CC for automation (CC 1-6 matching Python)
    // CC1: Raw HR (normalized 40-180 -> 0-127)
    float rawHRNorm = (hrProcessor.getRawHeartRate() - 40.0f) / (180.0f - 40.0f);
    sendMIDICC(midiMessages, 1, rawHRNorm);
    
    // CC2: Smoothed HR (normalized 40-180 -> 0-127)
    float smoothedHRNorm = (hrProcessor.getSmoothedHeartRate() - 40.0f) / (180.0f - 40.0f);
    sendMIDICC(midiMessages, 2, smoothedHRNorm);
    
    // CC3: Wet/Dry Ratio (normalized 0-100 -> 0-127)
    float wetDryNorm = hrProcessor.calculateWetDryRatio() / 100.0f;
    sendMIDICC(midiMessages, 3, wetDryNorm);
    
    // CC4-6: User parameters for automation
    float hrOffsetNorm = (hrOffset + 100.0f) / 200.0f;  // -100 to +100 -> 0-1
    sendMIDICC(midiMessages, 4, hrOffsetNorm);
    
    float smoothingNorm = (smoothingFactor - 0.01f) / (10.0f - 0.01f);  // 0.01-10.0 -> 0-1
    sendMIDICC(midiMessages, 5, smoothingNorm);
    
    float wetDryOffsetNorm = (wetDryOffset + 100.0f) / 200.0f;  // -100 to +100 -> 0-1
    sendMIDICC(midiMessages, 6, wetDryOffsetNorm);
}

void HeartSyncProcessor::handleHeartRateUpdate(int rawHR, const std::vector<float>& rrIntervals)
{
    juce::ignoreUnused(rrIntervals);
    
    // Process through HR processor
    float smoothedHR = hrProcessor.processHeartRate(rawHR);
    float wetDryRatio = hrProcessor.calculateWetDryRatio();
    
    // Update read-only parameters in APVTS
    auto* rawHRParam = apvts.getParameter(PARAM_RAW_HR);
    auto* smoothedHRParam = apvts.getParameter(PARAM_SMOOTHED_HR);
    auto* wetDryParam = apvts.getParameter(PARAM_WET_DRY_RATIO);
    
    if (rawHRParam)
        rawHRParam->setValueNotifyingHost(rawHRParam->convertTo0to1(hrProcessor.getRawHeartRate()));
    
    if (smoothedHRParam)
        smoothedHRParam->setValueNotifyingHost(smoothedHRParam->convertTo0to1(smoothedHR));
    
    if (wetDryParam)
        wetDryParam->setValueNotifyingHost(wetDryParam->convertTo0to1(wetDryRatio));
}

void HeartSyncProcessor::handleBLEDisconnect()
{
    bleConnected = false;
    hrProcessor.reset();
    
    DBG("BLE device disconnected");
}

void HeartSyncProcessor::sendMIDICC(juce::MidiBuffer& midiMessages, int ccNumber, float normalizedValue)
{
    // Convert normalized value (0-1) to MIDI CC value (0-127)
    int ccValue = static_cast<int>(juce::jlimit(0.0f, 127.0f, normalizedValue * 127.0f));
    
    // Send CC message on channel 1
    juce::MidiMessage cc = juce::MidiMessage::controllerEvent(1, ccNumber, ccValue);
    midiMessages.addEvent(cc, 0);
}

// --- Boilerplate ---

juce::AudioProcessorEditor* HeartSyncProcessor::createEditor()
{
    return new HeartSyncEditor(*this);
}

bool HeartSyncProcessor::hasEditor() const
{
    return true;
}

const juce::String HeartSyncProcessor::getName() const
{
    return "HeartSync";
}

bool HeartSyncProcessor::acceptsMidi() const
{
    return false;
}

bool HeartSyncProcessor::producesMidi() const
{
    return true;
}

bool HeartSyncProcessor::isMidiEffect() const
{
    return false;
}

double HeartSyncProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int HeartSyncProcessor::getNumPrograms()
{
    return 1;
}

int HeartSyncProcessor::getCurrentProgram()
{
    return 0;
}

void HeartSyncProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String HeartSyncProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void HeartSyncProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void HeartSyncProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void HeartSyncProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// --- Plugin Entry Point ---

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HeartSyncProcessor();
}
