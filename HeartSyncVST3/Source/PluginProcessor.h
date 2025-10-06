#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_osc/juce_osc.h>
#include "Core/BluetoothManager.h"
#include "Core/HeartRateProcessor.h"
#include "Core/OscSenderManager.h"

class HeartSyncVST3AudioProcessor : public juce::AudioProcessor,
                                   public BluetoothManager::Listener
{
public:
    HeartSyncVST3AudioProcessor();
    ~HeartSyncVST3AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter access
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    
    // Core component access
    BluetoothManager& getBluetoothManager() { return bluetoothManager; }
    HeartRateProcessor& getHeartRateProcessor() { return heartRateProcessor; }
    OscSenderManager& getOscSenderManager() { return oscSenderManager; }
    
    // Heart rate data access (for UI)
    float getRawBpm() const { return heartRateProcessor.getRawBpm(); }
    float getSmoothedBpm() const { return heartRateProcessor.getSmoothedBpm(); }
    float getInvertedBpm() const { return heartRateProcessor.getInvertedBpm(); }
    
    // BluetoothManager::Listener implementation
    void onHeartRateReceived(float bpm) override;
    void onDeviceConnected(const juce::String& deviceName) override;
    void onDeviceDisconnected() override;
    void onConnectionError(const juce::String& error) override;

private:
    // Parameters
    juce::AudioProcessorValueTreeState parameters;
    
    // Core components
    BluetoothManager bluetoothManager;
    HeartRateProcessor heartRateProcessor;
    OscSenderManager oscSenderManager;
    
    // Timing for OSC updates (30Hz)
    double timeSinceLastOsc = 0.0;
    const double oscUpdateInterval = 1.0 / 30.0; // 30Hz
    
    // Processing
    double currentSampleRate = 44100.0;
    
    // Parameter creation
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncVST3AudioProcessor)
};
