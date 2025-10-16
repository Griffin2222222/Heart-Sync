#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <string>
#include <vector>
#include "Core/BluetoothManager.h"
#include "Core/HeartRateProcessor.h"
#include "Core/OscSenderManager.h"

class HeartSyncVST3AudioProcessor : public juce::AudioProcessor,
                                    private juce::Timer
{
public:
    struct TelemetrySnapshot
    {
        juce::String statusText;
        juce::String deviceName;
        bool isConnected = false;
        float signalQuality = 0.0f;
        float lastLatencyMs = 0.0f;
        int packetsReceived = 0;
        int packetsExpected = 0;

        struct Heart
        {
            float rawBpm = 0.0f;
            float smoothedBpm = 0.0f;
            float wetDry = 0.0f;
            float invertedBpm = 0.0f;
            float alpha = 0.0f;
            float halfLifeSeconds = 0.0f;
            int effectiveWindowSamples = 0;
        } heart;

        struct History
        {
            juce::Array<float> time;
            juce::Array<float> raw;
            juce::Array<float> smoothed;
            juce::Array<float> wetDry;
        } history;

        juce::StringArray consoleLog;
    };

    struct Telemetry
    {
        bool isConnected = false;
        juce::String deviceName;
        int packetsReceived = 0;
        int packetsExpected = 0;
        double lastPacketTimeSeconds = 0.0;
    };

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

    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }

    HeartRateProcessor& getHeartRateProcessor() { return heartRateProcessor; }
    BluetoothManager& getBluetoothManager() { return bluetoothManager; }
    OscSenderManager& getOscSenderManager() { return oscSenderManager; }

    TelemetrySnapshot getTelemetrySnapshot() const;
    HeartRateProcessor::SmoothingMetrics getSmoothingMetrics() const;
    void getHistory(juce::Array<float>& raw, juce::Array<float>& smoothed, juce::Array<float>& wetDry) const;
    Telemetry getTelemetry() const;
    juce::StringArray getConsoleLog() const;
    void appendConsoleLog(const juce::String& message, const juce::String& tag = juce::String());

    juce::String getConnectedDeviceName() const;

    void beginDeviceScan();
    std::vector<BluetoothDevice> getDiscoveredDevices() const;
    bool connectToDevice(const std::string& deviceId);
    void disconnectFromDevice();

private:
    void timerCallback() override;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleParameterChanges();
    void updateOscConfiguration();
    void sendOscIfNeeded();

    juce::AudioProcessorValueTreeState parameters;

    HeartRateProcessor heartRateProcessor;
    BluetoothManager bluetoothManager;
    OscSenderManager oscSenderManager;

    std::atomic<int> packetsReceived { 0 };
    std::atomic<int> packetsExpected { 0 };
    std::atomic<double> lastPacketTimeSeconds { 0.0 };
    std::atomic<double> sessionStartSeconds { 0.0 };
    std::atomic<bool> pendingOscFrame { false };

    juce::String currentDeviceName;
    mutable juce::SpinLock deviceLock;

    juce::String currentOscHost { "127.0.0.1" };
    int currentOscPort = 8000;
    bool oscConnected = false;

    double currentSampleRate = 44100.0;
    int samplesUntilOsc = 0;
    static constexpr int oscTicksPerSecond = 30;

    juce::StringArray consoleLog;
    mutable juce::SpinLock logLock;
    static constexpr int maxConsoleEntries = 400;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncVST3AudioProcessor)
};
