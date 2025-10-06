#pragma once

#include <juce_osc/juce_osc.h>
#include <juce_core/juce_core.h>

class OscSenderManager
{
public:
    OscSenderManager();
    ~OscSenderManager();
    
    // Configuration
    bool initialize(const juce::String& targetHost, int targetPort);
    void setTargetAddress(const juce::String& host, int port);
    void stopSending();
    
    // Data transmission (sends all three streams simultaneously at 30Hz)
    void sendHeartRateData(float rawBpm, float smoothedBpm, float invertedBpm);
    
    // Legacy methods for compatibility
    void setOutputMode(const std::string& mode) { outputMode = mode; }
    void setIp(const std::string& ipAddress);
    void setPort(int portNumber);
    void setEnabled(bool isEnabled) { enabled = isEnabled; }
    void sendIfEnabled(float raw, float smoothed, float inverted);
    
    // Status
    bool isConnected() const { return oscSender != nullptr && isInitialized; }
    juce::String getTargetHost() const { return currentHost; }
    int getTargetPort() const { return currentPort; }

private:
    std::unique_ptr<juce::OSCSender> oscSender;
    bool isInitialized = false;
    juce::String currentHost;
    int currentPort = 0;
    
    // Legacy compatibility
    std::atomic<bool> enabled { false };
    std::string outputMode { "Smoothed" };
    
    // OSC address patterns
    static const juce::String OSC_RAW_BPM;
    static const juce::String OSC_SMOOTHED_BPM;
    static const juce::String OSC_INVERTED_BPM;
    static const juce::String OSC_HEARTRATE_BUNDLE;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscSenderManager)
};
