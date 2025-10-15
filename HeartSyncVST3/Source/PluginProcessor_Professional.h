#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "Core/BluetoothManager.h"
#include <memory>
#include <atomic>
#include <array>
#include <chrono>

//==============================================================================
/**
    HeartSync Professional Audio Processor
    
    Enterprise-grade VST3 plugin for heart rate reactive audio processing.
    Implements Universal Audio quality standards for professional DAW integration.
    
    Features:
    - Sample-accurate parameter automation
    - Professional DSP processing chain
    - Robust Bluetooth LE heart rate monitoring
    - Real-time biometric data visualization
    - Enterprise error handling and logging
    
    @author Quantum Systems Audio
    @version 2.0 Professional
*/
class HeartSyncVST3AudioProcessor : public juce::AudioProcessor,
                                    public juce::Timer
{
public:
    //==============================================================================
    // Plugin lifecycle
    HeartSyncVST3AudioProcessor();
    ~HeartSyncVST3AudioProcessor() override;

    //==============================================================================
    // Audio processing
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   #endif

    //==============================================================================
    // Editor management
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    // Plugin information
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    // Program management
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    // State management
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter system access
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    const juce::AudioProcessorValueTreeState& getParameters() const { return parameters; }
    
    //==============================================================================
    // Biometric data access (thread-safe)
    struct BiometricData
    {
        float rawHeartRate{0.0f};
        float smoothedHeartRate{0.0f};
        float wetDryRatio{50.0f};
        float heartRateVariability{0.0f};
        bool isDataValid{false};
        std::chrono::steady_clock::time_point timestamp;
    };
    
    BiometricData getCurrentBiometricData() const;
    std::array<float, 200> getRawHeartRateHistory() const;
    std::array<float, 200> getSmoothedHeartRateHistory() const;
    std::array<float, 200> getWetDryHistory() const;
    
    //==============================================================================
    // Bluetooth device management
    struct DeviceInfo
    {
        std::string name;
        std::string identifier;
        int signalStrength{0};
        bool isConnected{false};
        std::chrono::steady_clock::time_point lastSeen;
    };
    
    bool isBluetoothAvailable() const;
    bool isDeviceConnected() const;
    bool isScanning() const;
    std::string getConnectedDeviceName() const;
    std::vector<DeviceInfo> getAvailableDevices() const;
    
    //==============================================================================
    // Device control methods
    juce::Result startDeviceScan();
    void stopDeviceScan();
    juce::Result connectToDevice(const std::string& deviceIdentifier);
    void disconnectDevice();
    
    //==============================================================================
    // Performance metrics
    struct PerformanceMetrics
    {
        double averageProcessingTimeMs{0.0};
        double peakProcessingTimeMs{0.0};
        size_t totalProcessedBlocks{0};
        double cpuUsagePercent{0.0};
    };
    
    PerformanceMetrics getPerformanceMetrics() const;
    void resetPerformanceMetrics();
    
    //==============================================================================
    // UI callback registration
    std::function<void()> onBiometricDataUpdated;
    std::function<void()> onBluetoothStateChanged;
    std::function<void()> onDeviceListUpdated;
    std::function<void(const juce::String&)> onSystemMessage;

    //==============================================================================
    // Timer callback for deferred initialization
    void timerCallback() override;

private:
    //==============================================================================
    // Parameter system
    juce::AudioProcessorValueTreeState parameters;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Parameter IDs (professional naming convention)
    static const juce::String PARAM_RAW_HEART_RATE;
    static const juce::String PARAM_SMOOTHED_HEART_RATE;
    static const juce::String PARAM_WET_DRY_RATIO;
    static const juce::String PARAM_HEART_RATE_OFFSET;
    static const juce::String PARAM_SMOOTHING_FACTOR;
    static const juce::String PARAM_WET_DRY_OFFSET;
    
    //==============================================================================
    // DSP processing chain
    juce::dsp::ProcessorChain<
        juce::dsp::Gain<float>,
        juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>
    > dspChain;
    
    //==============================================================================
    // Bluetooth LE manager
    std::unique_ptr<BluetoothManager> bluetoothManager;
    
    //==============================================================================
    // Thread-safe data storage
    mutable juce::SpinLock biometricDataLock;
    BiometricData currentBiometricData;
    
    mutable juce::SpinLock historyDataLock;
    std::array<float, 200> rawHeartRateHistory;
    std::array<float, 200> smoothedHeartRateHistory;
    std::array<float, 200> wetDryHistory;
    size_t historyWriteIndex{0};
    
    //==============================================================================
    // Sample-accurate parameter smoothing
    juce::SmoothedValue<float> heartRateOffsetSmoothed;
    juce::SmoothedValue<float> smoothingFactorSmoothed;
    juce::SmoothedValue<float> wetDryOffsetSmoothed;
    
    //==============================================================================
    // Performance monitoring
    mutable std::atomic<double> averageProcessingTime{0.0};
    mutable std::atomic<double> peakProcessingTime{0.0};
    mutable std::atomic<size_t> processBlockCount{0};
    mutable std::chrono::steady_clock::time_point lastResetTime;
    
    //==============================================================================
    // Professional error handling
    mutable juce::CriticalSection errorLogLock;
    std::vector<std::pair<std::chrono::steady_clock::time_point, juce::String>> errorLog;
    static const size_t MAX_ERROR_LOG_SIZE = 100;
    
    //==============================================================================
    // Internal processing methods
    void updateBiometricParameters();
    void addToHistory(float rawHr, float smoothedHr, float wetDry);
    void logError(const juce::String& error) const;
    void logSystemMessage(const juce::String& message) const;
    
    //==============================================================================
    // Bluetooth event handlers
    void handleHeartRateData(float heartRate);
    void handleBluetoothStateChange();
    void handleDeviceDiscovery();
    void handleSystemMessage(const std::string& message);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncVST3AudioProcessor)
};