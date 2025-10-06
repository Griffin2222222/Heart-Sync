#include "PluginProcessor.h"
#include "PluginEditor.h"

HeartSyncVST3AudioProcessor::HeartSyncVST3AudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Initialize heart rate history
    for (int i = 0; i < 16; ++i)
        heartRateHistory[i] = 70.0f;
    
    // Set up OSC receiver for heart rate data
    oscReceiver = std::make_unique<juce::OSCReceiver>();
    oscReceiver->addListener(this);
    
    if (oscReceiver->connect(8000))
    {
        DBG("OSC receiver connected on port 8000");
    }
    
    // Initialize the three automation outputs
    updateAutomationOutputs();
}

HeartSyncVST3AudioProcessor::~HeartSyncVST3AudioProcessor()
{
    if (oscReceiver)
        oscReceiver->disconnect();
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
    this->sampleRate = sampleRate;
    juce::ignoreUnused(samplesPerBlock);
}

void HeartSyncVST3AudioProcessor::releaseResources()
{
    // Release any resources
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

    // Clear unused channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Update heart rate smoothing and automation outputs
    updateHeartRateSmoothing();
    updateAutomationOutputs();
    
    // Generate MIDI CC for normalized heart rate
    float normalizedValue = normalizedHeartRate.load();
    if (normalizedValue > 0.0f)
    {
        int midiValue = static_cast<int>(normalizedValue * 127.0f);
        auto ccMessage = juce::MidiMessage::controllerEvent(1, 74, midiValue); // CC 74
        midiMessages.addEvent(ccMessage, 0);
    }
    
    // Audio passthrough (no processing)
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
    juce::ignoreUnused(destData);
}

void HeartSyncVST3AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

void HeartSyncVST3AudioProcessor::setHeartRate(float bpm)
{
    rawHeartRate = juce::jlimit(40.0f, 200.0f, bpm);
}

void HeartSyncVST3AudioProcessor::oscMessageReceived(const juce::OSCMessage& message)
{
    if (message.getAddressPattern().toString() == "/hr" && message.size() > 0)
    {
        if (message[0].isFloat32())
        {
            float newHeartRate = message[0].getFloat32();
            setHeartRate(newHeartRate);
        }
        else if (message[0].isInt32())
        {
            float newHeartRate = static_cast<float>(message[0].getInt32());
            setHeartRate(newHeartRate);
        }
    }
}

void HeartSyncVST3AudioProcessor::updateHeartRateSmoothing()
{
    // Add current heart rate to history
    float currentRaw = rawHeartRate.load();
    heartRateHistory[historyIndex] = currentRaw;
    historyIndex = (historyIndex + 1) % 16;
    
    if (!historyFilled && historyIndex == 0)
        historyFilled = true;
    
    // Apply smoothing if enabled
    float smoothing = smoothingAmount.load();
    
    if (smoothing < 0.01f)
    {
        // No smoothing - automation outputs use raw value
        return;
    }
    else
    {
        // Calculate smoothed heart rate
        float sum = 0.0f;
        int count = historyFilled ? 16 : historyIndex;
        
        for (int i = 0; i < count; ++i)
        {
            sum += heartRateHistory[i];
        }
        
        float average = count > 0 ? sum / count : currentRaw;
        
        // Apply exponential smoothing to raw heart rate
        float alpha = 1.0f - smoothing;
        float smoothedValue = alpha * currentRaw + (1.0f - alpha) * average;
        
        // Update raw heart rate with smoothed value
        rawHeartRate.store(smoothedValue);
    }
}

void HeartSyncVST3AudioProcessor::updateAutomationOutputs()
{
    float rawHR = rawHeartRate.load();
    
    // Clamp to valid range
    rawHR = juce::jlimit(40.0f, 200.0f, rawHR);
    
    // 1. Raw Heart Rate (40-200 BPM) - already available as rawHeartRate
    
    // 2. Normalized Heart Rate (0.0-1.0)
    float normalized = (rawHR - 40.0f) / (200.0f - 40.0f);
    normalized = juce::jlimit(0.0f, 1.0f, normalized);
    normalizedHeartRate.store(normalized);
    
    // 3. Inverted Heart Rate (1.0-0.0)
    float inverted = 1.0f - normalized;
    invertedHeartRate.store(inverted);
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HeartSyncVST3AudioProcessor();
}
