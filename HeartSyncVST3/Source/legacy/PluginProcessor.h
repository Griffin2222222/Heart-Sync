#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/BluetoothManager.h"
#include <memory>
#include <deque>
#include <atomic>

class HeartSyncVST3AudioProcessor : public juce::AudioProcessor
{
public:
    HeartSyncVST3AudioProcessor();
    ~HeartSyncVST3AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   #endif

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

    // VST3 parameter access
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    
    // Heart rate data access for UI
    float getRawBpm() const { return bluetoothManager ? bluetoothManager->getCurrentHeartRate() : 0.0f; }
    float getSmoothedBpm() const { return bluetoothManager ? bluetoothManager->getSmoothedHeartRate() : 0.0f; }
    float getWetDryRatio() const { return bluetoothManager ? bluetoothManager->getWetDryRatio() : 50.0f; }
    
    // Heart rate history for waveform display
    std::deque<float> getRawHeartRateHistory() const { 
        return bluetoothManager ? bluetoothManager->getRawHeartRateHistory() : std::deque<float>(); 
    }
    std::deque<float> getSmoothedHeartRateHistory() const { 
        return bluetoothManager ? bluetoothManager->getSmoothedHeartRateHistory() : std::deque<float>(); 
    }
    std::deque<float> getWetDryHistory() const { 
        return bluetoothManager ? bluetoothManager->getWetDryHistory() : std::deque<float>(); 
    }
    
    // Bluetooth device management for UI
    bool isBluetoothConnected() const { return bluetoothManager ? bluetoothManager->isConnected() : false; }
    bool isBluetoothScanning() const { return bluetoothManager ? bluetoothManager->isScanning() : false; }
    std::string getDeviceName() const { return bluetoothManager ? bluetoothManager->getConnectedDeviceName() : "Not Connected"; }
    std::vector<BluetoothDevice> getDiscoveredDevices() const { 
        return bluetoothManager ? bluetoothManager->getDiscoveredDevices() : std::vector<BluetoothDevice>(); 
    }
    
    // Bluetooth controls for UI
    void startBluetoothScan() { if (bluetoothManager) bluetoothManager->startScanning(); }
    void stopBluetoothScan() { if (bluetoothManager) bluetoothManager->stopScanning(); }
    void connectToDevice(const std::string& deviceId) { if (bluetoothManager) bluetoothManager->connectToDevice(deviceId); }
    void disconnectDevice() { if (bluetoothManager) bluetoothManager->disconnectFromDevice(); }
    
    // Console message access for UI
    std::string getLatestConsoleMessage() const { return latestConsoleMessage; }
    
    // Callbacks for UI updates
    std::function<void()> onBluetoothStateChanged;
    std::function<void()> onDeviceListChanged;
    std::function<void(const std::string&)> onConsoleMessageReceived;

private:
    // VST3 parameter system
    juce::AudioProcessorValueTreeState parameters;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Parameter IDs
    static const juce::String RAW_HR_PARAM_ID;
    static const juce::String SMOOTHED_HR_PARAM_ID;
    static const juce::String WET_DRY_PARAM_ID;
    static const juce::String HR_OFFSET_PARAM_ID;
    static const juce::String SMOOTHING_PARAM_ID;
    static const juce::String WET_DRY_OFFSET_PARAM_ID;
    
    // Bluetooth LE Manager
    std::unique_ptr<BluetoothManager> bluetoothManager;
    
    // Console messaging
    std::string latestConsoleMessage;
    
    // Callbacks from Bluetooth manager
    void onHeartRateReceived(float heartRate);
    void onBluetoothConnectionChanged();
    void onBluetoothDeviceDiscovered();
    void onConsoleMessage(const std::string& message);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncVST3AudioProcessor)
};