#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <vector>
#include <string>
#include <deque>
#include <atomic>

#ifdef __OBJC__
@class CBCentralManager;
@class CBPeripheral;
@class NSMutableArray;
@class HeartRateDelegate;
#else
class CBCentralManager;
class CBPeripheral;
class NSMutableArray;
class HeartRateDelegate;
#endif

struct BluetoothDevice
{
    std::string name;
    std::string identifier;
    int rssi;
    bool isConnected;
    
    BluetoothDevice(const std::string& n = "", const std::string& id = "", int r = 0)
        : name(n), identifier(id), rssi(r), isConnected(false) {}
};

class BluetoothManager
{
public:
    BluetoothManager();
    ~BluetoothManager();
    
    // Bluetooth scanning and connection
    void startScanning();
    void stopScanning();
    void connectToDevice(const std::string& deviceIdentifier);
    void disconnectFromDevice();
    
    // State queries
    bool isScanning() const { return scanning.load(); }
    bool isConnected() const { return connected.load(); }
    std::string getConnectedDeviceName() const { return connectedDeviceName; }
    std::vector<BluetoothDevice> getDiscoveredDevices() const;
    
<<<<<<< HEAD
    // Heart rate data with history for UI
    float getCurrentHeartRate() const { return currentHeartRate.load(); }
    float getSmoothedHeartRate() const { return smoothedHeartRate.load(); }
    float getWetDryRatio() const { return wetDryRatio.load(); }
    std::deque<float> getRawHeartRateHistory() const;
    std::deque<float> getSmoothedHeartRateHistory() const;
    std::deque<float> getWetDryHistory() const;
    
    // Callbacks for UI updates
    std::function<void()> onDeviceDiscovered;
    std::function<void()> onConnectionStatusChanged;
    std::function<void(float)> onHeartRateReceived;
    
    // Heart rate processing parameters (for VST3 automation)
    void setHeartRateOffset(float offset);
    void setSmoothingFactor(float factor);
    void setWetDryOffset(float offset);
    
    // Console logging for UI
    std::function<void(const std::string&)> onConsoleMessage;
=======
    // Heart rate data
    float getLatestHeartRate() const;
    using MeasurementCallback = std::function<void(float, const std::vector<float>&)>;
    void setMeasurementCallback(MeasurementCallback callback);
    void setConnectionCallbacks(std::function<void(const BluetoothDevice&)> onConnected,
                                std::function<void()> onDisconnected,
                                std::function<void(const std::string&)> onError);
>>>>>>> e2e13cb (Fix JUCE build configuration and parameter handling)
    
private:
<<<<<<< HEAD
    // Core Bluetooth objects (macOS)
    CBCentralManager* centralManager;
    CBPeripheral* connectedPeripheral;
    NSMutableArray* discoveredPeripherals;
    HeartRateDelegate* delegate;
    
    // State variables
    std::atomic<bool> scanning;
    std::atomic<bool> connected;
    std::string connectedDeviceName;
    mutable std::mutex devicesMutex;
    std::vector<BluetoothDevice> discoveredDevices;
=======
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
    mutable std::mutex devicesMutex;
    mutable std::mutex callbackMutex;
    std::condition_variable condition;
    std::atomic<bool> isRunning { false };
    std::atomic<bool> scanning { false };
    std::atomic<bool> connected { false };
    
    MeasurementCallback measurementCallback;
    std::function<void(const BluetoothDevice&)> onConnectedCallback;
    std::function<void()> onDisconnectedCallback;
    std::function<void(const std::string&)> onErrorCallback;
    std::vector<BluetoothDevice> availableDevices;
    BluetoothDevice currentDevice;
    std::string lastError;
>>>>>>> e2e13cb (Fix JUCE build configuration and parameter handling)
    
    // Heart rate processing
    std::atomic<float> currentHeartRate;
    std::atomic<float> smoothedHeartRate;
    std::atomic<float> wetDryRatio;
    std::atomic<float> heartRateOffset;
    std::atomic<float> smoothingFactor;
    std::atomic<float> wetDryOffset;
    
    // Heart rate history for smoothing, wet/dry calculation, and UI display
    mutable std::mutex historyMutex;
    std::deque<float> rawHeartRateHistory;
    std::deque<float> smoothedHistory;
    std::deque<float> wetDryHistory;
    static const size_t MAX_HISTORY_SIZE = 200; // 200 samples for waveform display
    
    // Internal methods
    void processHeartRateData(float rawHeartRate);
    void updateSmoothedHeartRate();
    void updateWetDryRatio();
    void addToHistory(std::deque<float>& history, float value);
    
    // Platform-specific initialization
    void initializeBluetooth();
    void cleanupBluetooth();
    
public:
    // Methods called by Objective-C delegate
    void didDiscoverPeripheral(const std::string& name, const std::string& identifier, int rssi);
    void didConnectPeripheral(const std::string& name);
    void didDisconnectPeripheral();
    void didReceiveHeartRateData(float heartRate);
    void bluetoothStateDidUpdate(bool isAvailable);
    void logToConsole(const std::string& message);
};

// Forward declare Objective-C classes for compilation
#ifdef __OBJC__
#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

@interface HeartRateDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
@property (nonatomic, assign) BluetoothManager* manager;
- (instancetype)initWithManager:(BluetoothManager*)mgr;
@end
#endif
