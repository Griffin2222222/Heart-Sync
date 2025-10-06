#include "BluetoothManager.h"
#include <iostream>
#include <sstream>

#ifdef HEARTSYNC_USE_WINRT
using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation;
#endif

BluetoothManager::BluetoothManager()
{
#ifdef HEARTSYNC_USE_WINRT
    winrt::init_apartment();
#endif
    lastError.clear();
}

BluetoothManager::~BluetoothManager()
{
    stopScanning();
    disconnectCurrentDevice();
    if (scanningThread.joinable())
        scanningThread.join();
}

void BluetoothManager::startScanning()
{
    if (scanning.load()) return;
    
    scanning = true;
    isRunning = true;
    
#ifdef HEARTSYNC_USE_WINRT
    try {
        // Create device watcher for Heart Rate Service
        auto selector = BluetoothLEDevice::GetDeviceSelectorFromPairingState(false);
        deviceWatcher = DeviceInformation::CreateWatcher(selector);
        
        // Set up event handlers
        deviceWatcher.Added([this](DeviceWatcher const&, DeviceInformation const& deviceInfo) {
            updateDeviceList(deviceInfo);
        });
        
        deviceWatcher.Updated([this](DeviceWatcher const&, DeviceInformationUpdate const& update) {
            // Handle device updates
        });
        
        deviceWatcher.Removed([this](DeviceWatcher const& watcher, DeviceInformationUpdate const& update) {
            onDeviceRemoved(watcher, update);
        });
        
        deviceWatcher.Start();
        
        scanningThread = std::thread(&BluetoothManager::scanForDevices, this);
        
    } catch (const std::exception& e) {
        lastError = "Failed to start scanning: " + std::string(e.what());
        scanning = false;
    }
#else
    // MinGW simulation mode - add some fake devices
    availableDevices.clear();
    
    BluetoothDevice device1;
    device1.id = "sim_device_001";
    device1.name = "Heart Rate Monitor (Simulated)";
    device1.signalStrength = -45;
    device1.isConnected = false;
    
    BluetoothDevice device2;
    device2.id = "sim_device_002";
    device2.name = "Fitness Tracker (Simulated)";
    device2.signalStrength = -60;
    device2.isConnected = false;
    
    availableDevices.push_back(device1);
    availableDevices.push_back(device2);
    
    // Start simulation thread
    scanningThread = std::thread(&BluetoothManager::scanForDevices, this);
    
    std::cout << "Bluetooth scanning started (simulation mode)\n";
#endif
}

void BluetoothManager::stopScanning()
{
    scanning = false;
    isRunning = false;
    
#ifdef HEARTSYNC_USE_WINRT
    if (deviceWatcher) {
        deviceWatcher.Stop();
        deviceWatcher = nullptr;
    }
#else
    // MinGW simulation mode
    std::cout << "Bluetooth scanning stopped (simulation mode)\n";
#endif
    
    condition.notify_all();
    
    if (scanningThread.joinable()) {
        scanningThread.join();
    }
}

std::vector<BluetoothDevice> BluetoothManager::getAvailableDevices() const
{
    std::lock_guard<std::mutex> lock(devicesMutex);
    return availableDevices;
}

bool BluetoothManager::connectToDevice(const std::string& deviceId)
{
#ifdef HEARTSYNC_USE_WINRT
    try {
        auto deviceInfoAsync = DeviceInformation::CreateFromIdAsync(winrt::to_hstring(deviceId));
        auto deviceInfo = deviceInfoAsync.get();
        
        connectToDevice(deviceInfo);
        return true;
        
    } catch (const std::exception& e) {
        lastError = "Connection failed: " + std::string(e.what());
        return false;
    }
#else
    // MinGW simulation mode
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    for (auto& device : availableDevices) {
        if (device.id == deviceId) {
            device.isConnected = true;
            currentDevice = device;
            connected = true;
            
            // Start simulating heart rate data
            startHeartRateSimulation();
            
            std::cout << "Connected to simulated device: " << device.name << std::endl;
            return true;
        }
    }
    
    lastError = "Device not found: " + deviceId;
    return false;
#endif
}

void BluetoothManager::disconnectCurrentDevice()
{
    connected = false;
    
#ifdef HEARTSYNC_USE_WINRT
    if (heartRateCharacteristic) {
        try {
            heartRateCharacteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                GattClientCharacteristicConfigurationDescriptorValue::None).get();
        } catch (...) {}
        heartRateCharacteristic = nullptr;
    }
    
    if (bleDevice) {
        bleDevice.Close();
        bleDevice = nullptr;
    }
#else
    // MinGW simulation mode
    std::lock_guard<std::mutex> lock(devicesMutex);
    for (auto& device : availableDevices) {
        device.isConnected = false;
    }
    
    std::cout << "Disconnected from simulated device" << std::endl;
#endif
    
    currentDevice = BluetoothDevice{};
}

BluetoothDevice BluetoothManager::getCurrentDevice() const
{
    return currentDevice;
}

bool BluetoothManager::isConnected() const
{
    return connected.load();
}

float BluetoothManager::getLatestHeartRate() const
{
    return latestHeartRate.load();
}

void BluetoothManager::setHeartRateCallback(std::function<void(float)> callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    heartRateCallback = callback;
}

bool BluetoothManager::isScanning() const
{
    return scanning.load();
}

std::string BluetoothManager::getLastError() const
{
    return lastError;
}

void BluetoothManager::scanForDevices()
{
    while (isRunning.load()) {
        std::unique_lock<std::mutex> lock(devicesMutex);
        condition.wait_for(lock, std::chrono::seconds(1));
    }
}

#ifdef HEARTSYNC_USE_WINRT
void BluetoothManager::connectToDevice(const DeviceInformation& deviceInfo)
{
    try {
        auto bleDeviceAsync = BluetoothLEDevice::FromIdAsync(deviceInfo.Id());
        bleDevice = bleDeviceAsync.get();
        
        if (!bleDevice) {
            throw std::runtime_error("Failed to get BLE device");
        }
        
        auto servicesAsync = bleDevice.GetGattServicesAsync();
        auto servicesResult = servicesAsync.get();
        
        if (servicesResult.Status() != GattCommunicationStatus::Success) {
            throw std::runtime_error("Failed to get GATT services");
        }
        
        for (const auto& service : servicesResult.Services()) {
            // Check for Heart Rate Service (0x180D)
            if (service.Uuid() == BluetoothUuidHelper::FromShortId(0x180D)) {
                auto characteristicsAsync = service.GetCharacteristicsAsync();
                auto characteristicsResult = characteristicsAsync.get();
                
                if (characteristicsResult.Status() == GattCommunicationStatus::Success) {
                    for (const auto& characteristic : characteristicsResult.Characteristics()) {
                        // Check for Heart Rate Measurement characteristic (0x2A37)
                        if (characteristic.Uuid() == BluetoothUuidHelper::FromShortId(0x2A37)) {
                            subscribeToHeartRateNotifications(characteristic);
                            connected = true;
                            
                            currentDevice.id = winrt::to_string(deviceInfo.Id());
                            currentDevice.name = winrt::to_string(deviceInfo.Name());
                            currentDevice.isConnected = true;
                            
                            return;
                        }
                    }
                }
            }
        }
        
        throw std::runtime_error("Heart Rate Service not found");
        
    } catch (const std::exception& e) {
        lastError = "Connection error: " + std::string(e.what());
        disconnectCurrentDevice();
    }
}
#endif

#ifdef HEARTSYNC_USE_WINRT
void BluetoothManager::subscribeToHeartRateNotifications(const GattCharacteristic& characteristic)
{
    try {
        heartRateCharacteristic = characteristic;
        
        // Enable notifications
        auto status = characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::Notify).get();
        
        if (status != GattCommunicationStatus::Success) {
            throw std::runtime_error("Failed to enable notifications");
        }
        
        // Set up value changed handler
        characteristic.ValueChanged([this](GattCharacteristic const&, GattValueChangedEventArgs const& args) {
            processHeartRateData(args.CharacteristicValue());
        });
        
    } catch (const std::exception& e) {
        lastError = "Subscription error: " + std::string(e.what());
    }
}

void BluetoothManager::processHeartRateData(const IBuffer& buffer)
{
    try {
        auto reader = DataReader::FromBuffer(buffer);
        
        if (buffer.Length() < 2) return;
        
        uint8_t flags = reader.ReadByte();
        uint16_t heartRate;
        
        // Check if heart rate is in 16-bit format
        if (flags & 0x01) {
            heartRate = reader.ReadUInt16();
        } else {
            heartRate = reader.ReadByte();
        }
        
        latestHeartRate.store(static_cast<float>(heartRate));
        
        // Call callback if available
        std::lock_guard<std::mutex> lock(callbackMutex);
        if (heartRateCallback) {
            heartRateCallback(static_cast<float>(heartRate));
        }
        
    } catch (const std::exception& e) {
        lastError = "Data processing error: " + std::string(e.what());
    }
}

void BluetoothManager::updateDeviceList(const DeviceInformation& deviceInfo)
{
    // Only add devices that have Heart Rate Service
    auto properties = deviceInfo.Properties();
    if (properties.HasKey(L"System.Devices.Aep.Service.Gatt")) {
        auto gattServices = properties.Lookup(L"System.Devices.Aep.Service.Gatt");
        auto gattServiceString = winrt::unbox_value<winrt::hstring>(gattServices);
        
        // Check for Heart Rate Service UUID
        if (gattServiceString.find(L"0000180d") != std::wstring::npos) {
            std::lock_guard<std::mutex> lock(devicesMutex);
            
            BluetoothDevice device;
            device.id = winrt::to_string(deviceInfo.Id());
            device.name = winrt::to_string(deviceInfo.Name());
            if (device.name.empty()) {
                device.name = "Unknown Heart Rate Monitor";
            }
            
            // Check if device already exists
            auto it = std::find_if(availableDevices.begin(), availableDevices.end(),
                [&device](const BluetoothDevice& d) { return d.id == device.id; });
            
            if (it == availableDevices.end()) {
                availableDevices.push_back(device);
            }
        }
    }
}

void BluetoothManager::onDeviceRemoved(const DeviceWatcher& watcher, const DeviceInformationUpdate& update)
{
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    auto deviceId = winrt::to_string(update.Id());
    availableDevices.erase(
        std::remove_if(availableDevices.begin(), availableDevices.end(),
            [&deviceId](const BluetoothDevice& d) { return d.id == deviceId; }),
        availableDevices.end());
}
#else
// MinGW simulation methods
void BluetoothManager::startHeartRateSimulation()
{
    // Start generating simulated heart rate data
    std::thread([this]() {
        float baseHeartRate = 70.0f;
        auto startTime = std::chrono::steady_clock::now();
        
        while (connected.load()) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            
            // Generate realistic heart rate with slow variation
            float variation = std::sin(elapsed * 0.001f) * 15.0f;  // ±15 BPM variation
            float heartRate = baseHeartRate + variation + (rand() % 10 - 5); // Small random noise
            
            if (heartRate < 50.0f) heartRate = 50.0f;
            if (heartRate > 180.0f) heartRate = 180.0f;
            
            latestHeartRate.store(heartRate);
            
            // Call callback if available
            std::lock_guard<std::mutex> lock(callbackMutex);
            if (heartRateCallback) {
                heartRateCallback(heartRate);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10Hz updates
        }
    }).detach();
}
#endif
