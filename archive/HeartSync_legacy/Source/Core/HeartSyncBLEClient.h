
#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include <atomic>

/**
 * @brief UDS client for communicating with HeartSync Bridge
 * 
 * Connects to ~/Library/Application Support/HeartSync/bridge.sock
 * Uses length-prefixed JSON messages (4-byte big-endian + payload)
 * No CoreBluetooth dependencies - all BLE handled by external Bridge process
 */
class HeartSyncBLEClient : private juce::Thread
{
public:
    /** Device info from Bridge */
    struct DeviceInfo
    {
        juce::String id;
        int rssi;
        juce::String name;
        
        /** Get display name (name if available, else shortened ID) */
        juce::String getDisplayName() const
        {
            if (name.isNotEmpty() && name != "Unknown")
                return name;
            
            // Return shortened ID (e.g., "AA:BB:CC:DD:EE:FF" → "AA:BB:CC:DD:EE...")
            if (id.length() > 17)
                return id.substring(0, 17) + "...";
            
            return id;
        }
    };
    
    /** Callback types */
    using PermissionCallback = std::function<void(const juce::String& state)>;
    using DeviceFoundCallback = std::function<void(const DeviceInfo&)>;
    using HeartRateCallback = std::function<void(int bpm, double timestamp)>;
    using StatusCallback = std::function<void(const juce::String& status)>;
    using ErrorCallback = std::function<void(const juce::String& error)>;
    
    HeartSyncBLEClient();
    ~HeartSyncBLEClient() override;
    
    // --- Connection Management ---
    
    /** Connect to Bridge (non-blocking) */
    void connectToBridge();
    
    /** Disconnect from Bridge */
    void disconnect();
    
    /** Check if connected */
    bool isConnected() const { return connected.load(); }
    
    /** Launch Bridge app if not running */
    void launchBridge();
    
    // --- BLE Commands ---
    
    /** Start/stop scanning */
    void startScan(bool enable);
    
    /** Connect to device by ID */
    void connectToDevice(const juce::String& deviceId);
    
    /** Disconnect from current device */
    void disconnectDevice();
    
    /** Get thread-safe snapshot of device list */
    juce::Array<DeviceInfo> getDevicesSnapshot();
    
    /** Check if connected to a device */
    bool isDeviceConnected() const { return deviceConnected.load(); }
    
    /** Get current connected device ID (thread-safe) */
    juce::String getCurrentDeviceId() const
    {
        const juce::ScopedLock lock(deviceStateLock);
        return currentDeviceId;
    }
    
    // --- Callbacks ---
    
    std::function<void(const juce::String&)> onPermissionChanged;
    std::function<void(const DeviceInfo&)> onDeviceFound;
    std::function<void(int, double)> onHrData;
    std::function<void(float bpm, juce::Array<float> rr)> onHeartRate; // UI thread callback with BPM + RR intervals
    std::function<void(const juce::String&)> onConnected;        // Device connection established
    std::function<void(const juce::String&)> onDisconnected;    // Device disconnected (reason)
    std::function<void(const juce::String&)> onError;
    
    #if JUCE_DEBUG
    // --- Debug Injection Methods (JUCE_DEBUG only) ---
    
    /** Inject permission state for UI testing without hardware */
    void __debugInjectPermission(const juce::String& state);
    
    /** Inject fake device into cache for UI testing */
    void __debugInjectDevice(const juce::String& id, const juce::String& name, int rssi);
    
    /** Inject connected event for UI testing */
    void __debugInjectConnected(const juce::String& id);
    
    /** Inject disconnected event for UI testing */
    void __debugInjectDisconnected(const juce::String& reason);
    
    /** Inject heart rate data for UI testing */
    void __debugInjectHr(int bpm);
    #endif
    
private:
    void run() override;
    void sendCommand(const juce::var& command);
    void processMessage(const juce::var& message);
    bool connectToSocket();
    void attemptReconnect();
    void checkHeartbeat();
    
    int socketFd{-1};
    std::atomic<bool> connected{false};
    std::atomic<bool> shouldReconnect{false};
    
    // Device connection state
    std::atomic<bool> deviceConnected{false};
    juce::String currentDeviceId;
    juce::CriticalSection deviceStateLock;
    
    juce::CriticalSection deviceListLock;
    juce::Array<DeviceInfo> devices;
    
    juce::String currentPermissionState{"unknown"};
    double lastHeartbeatTime{0.0};
    
    int reconnectAttempts{0};
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncBLEClient)
};
