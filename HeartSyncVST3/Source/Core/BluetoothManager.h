#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(HEARTSYNC_USE_WINRT) && HEARTSYNC_USE_WINRT
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>
#endif

struct BluetoothDevice
{
    std::string id;
    std::string identifier;
    std::string name;
    int signalStrength = 0;
    int rssi = 0;
    bool isConnected = false;
};

class BluetoothManager
{
public:
    using MeasurementCallback = std::function<void(float, const std::vector<float>&)>;

    BluetoothManager();
    ~BluetoothManager();

    void startScanning();
    void stopScanning();

    bool connectToDevice(const std::string& deviceId);
    void disconnectCurrentDevice();

    bool isScanning() const;
    bool isConnected() const;
    float getLatestHeartRate() const;

    std::vector<BluetoothDevice> getAvailableDevices() const;
    std::vector<BluetoothDevice> getDiscoveredDevices() const { return getAvailableDevices(); }
    BluetoothDevice getCurrentDevice() const;

    void setMeasurementCallback(MeasurementCallback callback);
    void setConnectionCallbacks(std::function<void(const BluetoothDevice&)> onConnected,
                                std::function<void()> onDisconnected,
                                std::function<void(const std::string&)> onError);

    std::string getLastError() const;

private:
    void dispatchMeasurement(float bpm, const std::vector<float>& rrIntervals);
    void dispatchConnected(const BluetoothDevice& device);
    void dispatchDisconnected();
    void dispatchError(const std::string& message);
    void addOrUpdateDevice(const BluetoothDevice& device);

#if JUCE_MAC
public:
    struct Impl;
private:
    std::unique_ptr<Impl> impl;
#else
    void scanForDevices();
#if defined(HEARTSYNC_USE_WINRT) && HEARTSYNC_USE_WINRT
    struct WindowsState;
    std::unique_ptr<WindowsState> windows;
    void connectToDeviceInternal(const winrt::Windows::Devices::Enumeration::DeviceInformation& deviceInfo);
    void subscribeToHeartRateNotifications(const winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic& characteristic);
    void processHeartRateData(const winrt::Windows::Storage::Streams::IBuffer& buffer);
    void updateDeviceList(const winrt::Windows::Devices::Enumeration::DeviceInformation& deviceInfo);
    void onDeviceRemoved(const winrt::Windows::Devices::Enumeration::DeviceWatcher& watcher,
                         const winrt::Windows::Devices::Enumeration::DeviceInformationUpdate& update);
#else
    void startHeartRateSimulation();
#endif

    std::thread scanningThread;
    std::condition_variable condition;
    std::atomic<bool> isRunning { false };
#endif

    std::atomic<float> latestHeartRate { 0.0f };
    mutable std::mutex devicesMutex;
    std::vector<BluetoothDevice> availableDevices;
    BluetoothDevice currentDevice;
    std::string lastError;

    std::atomic<bool> scanning { false };
    std::atomic<bool> connected { false };

    mutable std::mutex callbackMutex;
    MeasurementCallback measurementCallback;
    std::function<void(const BluetoothDevice&)> onConnectedCallback;
    std::function<void()> onDisconnectedCallback;
    std::function<void(const std::string&)> onErrorCallback;
};

