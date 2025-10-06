#include "OscSenderManager.h"

// Static OSC address patterns
const juce::String OscSenderManager::OSC_RAW_BPM = "/HeartSync/Raw";
const juce::String OscSenderManager::OSC_SMOOTHED_BPM = "/HeartSync/Smoothed";
const juce::String OscSenderManager::OSC_INVERTED_BPM = "/HeartSync/Inverted";
const juce::String OscSenderManager::OSC_HEARTRATE_BUNDLE = "/HeartSync/Bundle";

OscSenderManager::OscSenderManager()
    : oscSender(std::make_unique<juce::OSCSender>())
{
}

OscSenderManager::~OscSenderManager()
{
    stopSending();
}

bool OscSenderManager::initialize(const juce::String& targetHost, int targetPort)
{
    stopSending();
    
    currentHost = targetHost;
    currentPort = targetPort;
    
    if (oscSender->connect(targetHost, targetPort))
    {
        isInitialized = true;
        DBG("OSC sender connected to " << targetHost << ":" << targetPort);
        return true;
    }
    else
    {
        isInitialized = false;
        DBG("Failed to connect OSC sender to " << targetHost << ":" << targetPort);
        return false;
    }
}

void OscSenderManager::setTargetAddress(const juce::String& host, int port)
{
    if (host != currentHost || port != currentPort)
    {
        initialize(host, port);
    }
}

void OscSenderManager::stopSending()
{
    if (oscSender && isInitialized)
    {
        oscSender->disconnect();
        isInitialized = false;
        DBG("OSC sender disconnected");
    }
}

void OscSenderManager::sendHeartRateData(float rawBpm, float smoothedBpm, float invertedBpm)
{
    if (!isInitialized || !oscSender)
        return;
    
    // Send all three data streams simultaneously for maximum flexibility
    // This allows the receiving application to choose which stream to use
    
    // Individual messages
    oscSender->send(juce::OSCAddressPattern(OSC_RAW_BPM), rawBpm);
    oscSender->send(juce::OSCAddressPattern(OSC_SMOOTHED_BPM), smoothedBpm);
    oscSender->send(juce::OSCAddressPattern(OSC_INVERTED_BPM), invertedBpm);
    
    // Bundle message for efficiency (all three values in one message)
    juce::OSCBundle bundle;
    bundle.addElement(juce::OSCMessage(juce::OSCAddressPattern(OSC_RAW_BPM), rawBpm));
    bundle.addElement(juce::OSCMessage(juce::OSCAddressPattern(OSC_SMOOTHED_BPM), smoothedBpm));
    bundle.addElement(juce::OSCMessage(juce::OSCAddressPattern(OSC_INVERTED_BPM), invertedBpm));
    
    oscSender->send(bundle);
}

// Legacy compatibility methods
void OscSenderManager::setIp(const std::string& ipAddress)
{
    setTargetAddress(juce::String(ipAddress), currentPort);
}

void OscSenderManager::setPort(int portNumber)
{
    setTargetAddress(currentHost, portNumber);
}

void OscSenderManager::sendIfEnabled(float raw, float smoothed, float inverted)
{
    if (enabled)
    {
        sendHeartRateData(raw, smoothed, inverted);
    }
}
