#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "Core/BluetoothManager.h"
#include "Core/HeartSyncBLEClient.h"
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
    std::vector<float> getRawHeartRateHistory() const;
    std::vector<float> getSmoothedHeartRateHistory() const;
    std::vector<float> getWetDryHistory() const;
    
    //==============================================================================
    // Bluetooth device management
    struct DeviceInfo
    {
        std::string name;
        std::string identifier;
        int signalStrength{0};
        bool isConnected{false};
        std::chrono::steady_clock::time_point lastSeen;
        std::vector<std::string> services;
    };
    
    bool isBluetoothAvailable() const;
    bool isDeviceConnected() const;
    bool isScanning() const;
    bool isBluetoothReady() const;
    std::string getConnectedDeviceName() const;
    std::vector<DeviceInfo> getAvailableDevices() const;
    
    //==============================================================================
    // Device control methods
    juce::Result startDeviceScan();
    void stopDeviceScan();
    juce::Result connectToDevice(const std::string& deviceIdentifier);
    void disconnectDevice();
    bool hasNativeBluetoothStack() const;
    bool isNativeBluetoothReady() const;
    bool isBridgeClientConfigured() const;
    bool isBridgeClientConnected() const;
    bool isBridgeClientReady() const;
    juce::String getBridgePermissionState() const;
    void requestBridgeReconnect(bool relaunch = false);
    
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
    // Tempo Sync Control
    enum class TempoSyncSource
    {
        Off = 0,
        RawHeartRate = 1,
        SmoothedHeartRate = 2,
        WetDryRatio = 3
    };
    
    void setTempoSyncSource(TempoSyncSource source);
    TempoSyncSource getTempoSyncSource() const { return tempoSyncSource; }
    juce::String getTempoSyncSourceName() const;
    float getCurrentSuggestedTempo() const { return currentSuggestedTempo; }

    //==============================================================================
    // Parameter IDs (public for UI binding)
    static const juce::String PARAM_RAW_HEART_RATE;
    static const juce::String PARAM_SMOOTHED_HEART_RATE;
    static const juce::String PARAM_WET_DRY_RATIO;
    static const juce::String PARAM_HEART_RATE_OFFSET;
    static const juce::String PARAM_SMOOTHING_FACTOR;
    static const juce::String PARAM_WET_DRY_OFFSET;
    static const juce::String PARAM_WET_DRY_INPUT_SOURCE; // true=Smoothed, false=Raw
    static const juce::String PARAM_TEMPO_SYNC_SOURCE;    // 0=Off, 1=Raw, 2=Smooth, 3=WetDry

    //==============================================================================
    // Timer callback for deferred initialization
    void timerCallback() override;

private:
    //==============================================================================
    // Parameter system
    juce::AudioProcessorValueTreeState parameters;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    //==============================================================================
    // DSP processing chain
    juce::dsp::ProcessorChain<
        juce::dsp::Gain<float>,
        juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>
    > dspChain;
    
    //==============================================================================
    // Bluetooth LE manager
    std::unique_ptr<BluetoothManager> bluetoothManager;
    std::unique_ptr<HeartSyncBLEClient> bridgeClient;
    
    //==============================================================================
    // Thread-safe data storage
    mutable juce::SpinLock biometricDataLock;
    BiometricData currentBiometricData;
    
    mutable juce::SpinLock historyDataLock;
    std::array<float, 200> rawHeartRateHistory;
    std::array<float, 200> smoothedHeartRateHistory;
    std::array<float, 200> wetDryHistory;
    size_t historyWriteIndex{0};
    size_t historyCount{0};

    // Bridge state (macOS helper)
    std::atomic<bool> bridgeAvailable{false};
    std::atomic<bool> bridgeReady{false};
    std::atomic<bool> bridgeScanning{false};
    std::atomic<bool> bridgeDeviceConnected{false};
    std::atomic<float> bridgeRawHeartRate{0.0f};
    std::atomic<float> bridgeSmoothedHeartRate{0.0f};
    std::atomic<float> bridgeWetDryRatio{0.0f};
    std::atomic<bool> bridgeDataValid{false};
    juce::String bridgePermissionState{"unknown"};
    juce::String bridgeCurrentDeviceId;
    mutable juce::CriticalSection bridgeDevicesLock;
    std::vector<DeviceInfo> bridgeDevices;
    float bridgeSmoothedValue{0.0f};
    float bridgeWetDryValue{50.0f};
    bool bridgeSmootherInitialised{false};
    
    //==============================================================================
    // Sample-accurate parameter smoothing
    juce::SmoothedValue<float> heartRateOffsetSmoothed;
    juce::SmoothedValue<float> smoothingFactorSmoothed;
    juce::SmoothedValue<float> wetDryOffsetSmoothed;
    
    //==============================================================================
    // Tempo sync state
    TempoSyncSource tempoSyncSource{TempoSyncSource::Off};
    float currentSuggestedTempo{120.0f};
    float tempoMultiplier{1.0f};
    float tempoOffset{0.0f};
    
    void updateTempoSync(const BiometricData& data);
    float mapValueToTempo(float value, TempoSyncSource source) const;
    
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
    void initialiseBridgeClient();
    void updateBridgeBiometrics(float bpm);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncVST3AudioProcessor)
};