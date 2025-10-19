#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <atomic>

/**
 * @brief UDS client for communicating with the HeartSync Bridge helper.
 *
 * Connects to ~/Library/Application Support/HeartSync/bridge.sock using a
 * length-prefixed JSON protocol. The helper owns all CoreBluetooth access so
 * the plugin can run inside sandboxed hosts without additional entitlements.
 */
class HeartSyncBLEClient : private juce::Thread
{
public:
    struct DeviceInfo
    {
        juce::String id;
        int rssi;
        juce::String name;
        juce::StringArray services;

        juce::String getDisplayName() const
        {
            auto trimmedName = name.trim();
            if (trimmedName.isNotEmpty() && !trimmedName.equalsIgnoreCase("Unknown"))
                return trimmedName;

            const auto shortId = getShortIdentifier();

            for (auto service : services)
            {
                service = service.trim();
                if (service.equalsIgnoreCase("180D"))
                    return "Heart Rate Monitor • " + shortId;
            }

            return shortId.isNotEmpty() ? "BLE Device • " + shortId : juce::String("BLE Device");
        }

        juce::String getShortIdentifier() const
        {
            auto trimmedId = id.trim();
            if (trimmedId.isEmpty())
                return {};

            auto lastDash = trimmedId.lastIndexOfChar('-');
            if (lastDash >= 0 && lastDash < trimmedId.length() - 1)
                trimmedId = trimmedId.substring(lastDash + 1);

            if (trimmedId.length() > 6)
                trimmedId = trimmedId.substring(trimmedId.length() - 6);

            return trimmedId.toUpperCase();
        }
    };

    using PermissionCallback = std::function<void(const juce::String& state)>;
    using DeviceFoundCallback = std::function<void(const DeviceInfo&)>;
    using HeartRateCallback = std::function<void(float bpm, juce::Array<float> rr)>;
    using StatusCallback = std::function<void(const juce::String& status)>;
    using ErrorCallback = std::function<void(const juce::String& error)>;

    HeartSyncBLEClient();
    ~HeartSyncBLEClient() override;

    void connectToBridge();
    void disconnect();
    bool isConnected() const { return connected.load(); }
    void launchBridge();
    void resetReconnectAttempts() { reconnectAttempts = 0; }

    void startScan(bool enable);
    void connectToDevice(const juce::String& deviceId);
    void disconnectDevice();
    juce::Array<DeviceInfo> getDevicesSnapshot();
    bool isDeviceConnected() const { return deviceConnected.load(); }
    juce::String getCurrentDeviceId() const;

    std::function<void(const juce::String&)> onPermissionChanged;
    std::function<void(const DeviceInfo&)> onDeviceFound;
    std::function<void(float, juce::Array<float>)> onHeartRate;
    std::function<void(const juce::String&)> onConnected;
    std::function<void(const juce::String&)> onDisconnected;
    std::function<void(const juce::String&)> onError;
    std::function<void()> onBridgeConnected;
    std::function<void()> onBridgeDisconnected;
    std::function<void(const juce::String&)> onLog;

#if JUCE_DEBUG
    void __debugInjectPermission(const juce::String& state);
    void __debugInjectDevice(const juce::String& id, const juce::String& name, int rssi);
    void __debugInjectConnected(const juce::String& id);
    void __debugInjectDisconnected(const juce::String& reason);
    void __debugInjectHr(int bpm);
#endif

private:
    void run() override;
    void sendCommand(const juce::var& command);
    void processMessage(const juce::var& message);
    bool connectToSocket();
    void attemptReconnect();
    void checkHeartbeat();
    void dispatchLog(const juce::String& message);

    int socketFd{-1};
    std::atomic<bool> connected{false};
    std::atomic<bool> shouldReconnect{false};

    std::atomic<bool> deviceConnected{false};
    juce::String currentDeviceId;
    juce::CriticalSection deviceStateLock;

    juce::CriticalSection deviceListLock;
    juce::Array<DeviceInfo> devices;

    juce::String currentPermissionState{"unknown"};
    double lastHeartbeatTime{0.0};

    int reconnectAttempts{0};
    int lastLoggedFailureAttempt{-1};
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeartSyncBLEClient)
};
