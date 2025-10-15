#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

/**
 * @brief Native CoreBluetooth wrapper for Heart Rate BLE communication
 * 
 * Provides asynchronous BLE scanning, connection, and heart rate monitoring
 * using CoreBluetooth framework. Thread-safe callbacks marshaled to JUCE message thread.
 */
class HeartSyncBLE
{
public:
    /** Structure representing a discovered BLE device */
    struct DeviceInfo
    {
        juce::String name;
        juce::String address;
        int rssi;
        
        DeviceInfo() : rssi(0) {}
        DeviceInfo(const juce::String& n, const juce::String& a, int r)
            : name(n), address(a), rssi(r) {}
    };
    
    /** Callback types for async events */
    using ScanCallback = std::function<void(const std::vector<DeviceInfo>&)>;
    using ConnectCallback = std::function<void(bool success, const juce::String& error)>;
    using HeartRateCallback = std::function<void(int heartRate, const std::vector<float>& rrIntervals)>;
    using DisconnectCallback = std::function<void()>;
    
    HeartSyncBLE();
    ~HeartSyncBLE();
    
    // --- BLE Operations ---
    
    /** Start scanning for BLE heart rate devices (async) */
    void startScan(ScanCallback callback);
    
    /** Stop ongoing scan */
    void stopScan();
    
    /** Connect to device by address (async) */
    void connectToDevice(const juce::String& deviceAddress, ConnectCallback callback);
    
    /** Disconnect from current device */
    void disconnect();
    
    /** Check if currently connected to a device */
    bool isConnected() const;
    
    /** Get currently connected device address */
    juce::String getConnectedDeviceAddress() const;
    
    // --- Callbacks ---
    
    /** Set callback for heart rate notifications */
    void setHeartRateCallback(HeartRateCallback callback);
    
    /** Set callback for unexpected disconnection */
    void setDisconnectCallback(DisconnectCallback callback);
    
private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncBLE)
};
