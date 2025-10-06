#pragma once

// Conditional includes based on platform
#ifdef HEARTSYNC_USE_WINRT
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.Collections.h>
#endif

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <string>

struct BluetoothDevice
{
    std::string id;
    std::string name;
    bool isConnected = false;
    int signalStrength = 0;
};

class BluetoothManager
{
public:
    BluetoothManager();
    ~BluetoothManager();

    // Device scanning
    void startScanning();
    void stopScanning();
    
    // Device management
    std::vector<BluetoothDevice> getAvailableDevices() const;
    bool connectToDevice(const std::string& deviceId);
    void disconnectCurrentDevice();
    BluetoothDevice getCurrentDevice() const;
    bool isConnected() const;
    
    // Heart rate data
    float getLatestHeartRate() const;
    void setHeartRateCallback(std::function<void(float)> callback);
    
    // Status
    bool isScanning() const;
    std::string getLastError() const;

private:
    void scanForDevices();
#ifdef HEARTSYNC_USE_WINRT
    void connectToDevice(const winrt::Windows::Devices::Enumeration::DeviceInformation& deviceInfo);
    void subscribeToHeartRateNotifications(const winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic& characteristic);
    void processHeartRateData(const winrt::Windows::Storage::Streams::IBuffer& buffer);
    void updateDeviceList(const winrt::Windows::Devices::Enumeration::DeviceInformation& deviceInfo);
    void onDeviceRemoved(const winrt::Windows::Devices::Enumeration::DeviceWatcher& watcher, const winrt::Windows::Devices::Enumeration::DeviceInformationUpdate& update);
#else
    void startHeartRateSimulation();
#endif

    std::atomic<float> latestHeartRate { 0.0f };
    std::thread scanningThread;
    std::mutex devicesMutex;
    std::mutex callbackMutex;
    std::condition_variable condition;
    std::atomic<bool> isRunning { false };
    std::atomic<bool> scanning { false };
    std::atomic<bool> connected { false };
    
    std::function<void(float)> heartRateCallback;
    std::vector<BluetoothDevice> availableDevices;
    BluetoothDevice currentDevice;
    std::string lastError;
    
#ifdef HEARTSYNC_USE_WINRT
    winrt::Windows::Devices::Enumeration::DeviceWatcher deviceWatcher { nullptr };
    winrt::Windows::Devices::Bluetooth::BluetoothLEDevice bleDevice { nullptr };
    winrt::Windows::Devices::Bluetooth::GenericAttributeProfile::GattCharacteristic heartRateCharacteristic { nullptr };
#endif
};
