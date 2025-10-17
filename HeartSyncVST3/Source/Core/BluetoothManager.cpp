#include "BluetoothManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>

#if defined(HEARTSYNC_USE_WINRT) && HEARTSYNC_USE_WINRT
using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Storage::Streams;
#endif

namespace
{
    BluetoothDevice normaliseDevice(const BluetoothDevice& input)
    {
        BluetoothDevice device = input;

        if (device.id.empty() && ! device.identifier.empty())
            device.id = device.identifier;
        if (device.identifier.empty() && ! device.id.empty())
            device.identifier = device.id;
        if (device.name.empty())
            device.name = "Unknown Heart Rate Monitor";

        if (device.signalStrength == 0 && device.rssi != 0)
            device.signalStrength = device.rssi;
        if (device.rssi == 0 && device.signalStrength != 0)
            device.rssi = device.signalStrength;

        return device;
    }
}

BluetoothManager::BluetoothManager()
{
#if JUCE_MAC
    impl = std::make_unique<Impl>(*this);
#elif defined(HEARTSYNC_USE_WINRT) && HEARTSYNC_USE_WINRT
    winrt::init_apartment();
    windows = std::make_unique<WindowsState>();
#endif
}

BluetoothManager::~BluetoothManager()
{
    stopScanning();
    disconnectCurrentDevice();
#if ! JUCE_MAC
    if (scanningThread.joinable())
        scanningThread.join();
#endif
}

void BluetoothManager::startScanning()
{
#if JUCE_MAC
    if (impl)
        impl->startScanning();
    return;
#else
    if (scanning.exchange(true))
        return;

#if defined(HEARTSYNC_USE_WINRT) && HEARTSYNC_USE_WINRT
    try
    {
        auto selector = BluetoothLEDevice::GetDeviceSelectorFromPairingState(false);
        if (! windows)
            windows = std::make_unique<WindowsState>();

        windows->deviceWatcher = DeviceInformation::CreateWatcher(selector);

        windows->deviceWatcher.Added([this](DeviceWatcher const&, DeviceInformation const& info)
        {
            updateDeviceList(info);
        });

        windows->deviceWatcher.Removed([this](DeviceWatcher const& watcher, DeviceInformationUpdate const& update)
        {
            onDeviceRemoved(watcher, update);
        });

        windows->deviceWatcher.Start();
    }
    catch (const std::exception& e)
    {
        scanning.store(false);
        dispatchError("Failed to start scanning: " + std::string(e.what()));
        return;
    }
#else
    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        availableDevices.clear();
    }

    BluetoothDevice device1;
    device1.id = "sim_device_001";
    device1.name = "Heart Rate Monitor (Simulated)";
    device1.signalStrength = -45;
    addOrUpdateDevice(device1);

    BluetoothDevice device2;
    device2.id = "sim_device_002";
    device2.name = "Fitness Tracker (Simulated)";
    device2.signalStrength = -60;
    addOrUpdateDevice(device2);

    std::cout << "Bluetooth scanning started (simulation mode)\n";
#endif

    isRunning.store(true);
    scanningThread = std::thread(&BluetoothManager::scanForDevices, this);
#endif
}

void BluetoothManager::stopScanning()
{
#if JUCE_MAC
    if (impl)
        impl->stopScanning();
#else
    if (! scanning.exchange(false))
        return;

    isRunning.store(false);

#if defined(HEARTSYNC_USE_WINRT) && HEARTSYNC_USE_WINRT
    if (windows && windows->deviceWatcher)
    {
        try { windows->deviceWatcher.Stop(); }
        catch (...) {}
        windows->deviceWatcher = nullptr;
    }
#else
    std::cout << "Bluetooth scanning stopped (simulation mode)\n";
#endif

    condition.notify_all();
    if (scanningThread.joinable())
        scanningThread.join();
#endif
}

bool BluetoothManager::connectToDevice(const std::string& deviceId)
{
#if JUCE_MAC
    return impl ? impl->connectToDevice(deviceId) : false;
#else
#if defined(HEARTSYNC_USE_WINRT) && HEARTSYNC_USE_WINRT
    try
    {
        auto infoAsync = DeviceInformation::CreateFromIdAsync(winrt::to_hstring(deviceId));
        auto info = infoAsync.get();
        connectToDeviceInternal(info);
        return true;
    }
    catch (const std::exception& e)
    {
        dispatchError("Connection failed: " + std::string(e.what()));
        return false;
    }
#else
    BluetoothDevice target;
    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        auto it = std::find_if(availableDevices.begin(), availableDevices.end(),
                               [&deviceId](const BluetoothDevice& device)
                               {
                                   return device.id == deviceId || device.identifier == deviceId;
                               });
        if (it == availableDevices.end())
        {
            dispatchError("Device not found: " + deviceId);
            return false;
        }

        target = *it;
    }

    target.isConnected = true;
    addOrUpdateDevice(target);
    dispatchConnected(target);
    startHeartRateSimulation();
    std::cout << "Connected to simulated device: " << target.name << '\n';
    return true;
#endif
#endif
}

void BluetoothManager::disconnectCurrentDevice()
{
#if JUCE_MAC
    if (impl)
        impl->disconnectCurrentDevice();
#else
#if defined(HEARTSYNC_USE_WINRT) && HEARTSYNC_USE_WINRT
    if (windows)
    {
        if (windows->heartRateCharacteristic)
        {
            try
            {
                windows->heartRateCharacteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
                    GattClientCharacteristicConfigurationDescriptorValue::None).get();
            }
            catch (...) {}
            windows->heartRateCharacteristic = nullptr;
        }

        if (windows->bleDevice)
        {
            windows->bleDevice.Close();
            windows->bleDevice = nullptr;
        }
    }
#endif
    dispatchDisconnected();
#endif
}

bool BluetoothManager::isScanning() const
{
    return scanning.load();
}

bool BluetoothManager::isConnected() const
{
    return connected.load();
}

float BluetoothManager::getLatestHeartRate() const
{
    return latestHeartRate.load();
}

std::vector<BluetoothDevice> BluetoothManager::getAvailableDevices() const
{
    std::lock_guard<std::mutex> lock(devicesMutex);
    return availableDevices;
}

BluetoothDevice BluetoothManager::getCurrentDevice() const
{
    std::lock_guard<std::mutex> lock(devicesMutex);
    return currentDevice;
}

void BluetoothManager::setMeasurementCallback(MeasurementCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    measurementCallback = std::move(callback);
}

void BluetoothManager::setConnectionCallbacks(std::function<void(const BluetoothDevice&)> onConnectedCallbackIn,
                                              std::function<void()> onDisconnectedCallbackIn,
                                              std::function<void(const std::string&)> onErrorCallbackIn)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    onConnectedCallback = std::move(onConnectedCallbackIn);
    onDisconnectedCallback = std::move(onDisconnectedCallbackIn);
    onErrorCallback = std::move(onErrorCallbackIn);
}

std::string BluetoothManager::getLastError() const
{
    std::lock_guard<std::mutex> lock(devicesMutex);
    return lastError;
}

void BluetoothManager::dispatchMeasurement(float bpm, const std::vector<float>& rrIntervals)
{
    latestHeartRate.store(bpm);

    std::lock_guard<std::mutex> lock(callbackMutex);
    if (measurementCallback)
        measurementCallback(bpm, rrIntervals);
}

void BluetoothManager::dispatchConnected(const BluetoothDevice& device)
{
    BluetoothDevice normalised = normaliseDevice(device);
    normalised.isConnected = true;

    addOrUpdateDevice(normalised);

    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        currentDevice = normalised;
        connected.store(true);
        lastError.clear();
    }

    std::lock_guard<std::mutex> lock(callbackMutex);
    if (onConnectedCallback)
        onConnectedCallback(normalised);
}

void BluetoothManager::dispatchDisconnected()
{
    BluetoothDevice previous;
    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        previous = currentDevice;
        connected.store(false);

        if (! previous.id.empty())
        {
            for (auto& device : availableDevices)
            {
                if (device.id == previous.id || device.identifier == previous.id)
                {
                    device.isConnected = false;
                    break;
                }
            }
        }

        currentDevice = {};
    }

    std::lock_guard<std::mutex> lock(callbackMutex);
    if (onDisconnectedCallback)
        onDisconnectedCallback();
}

void BluetoothManager::dispatchError(const std::string& message)
{
    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        lastError = message;
    }

    std::lock_guard<std::mutex> lock(callbackMutex);
    if (onErrorCallback)
        onErrorCallback(message);
}

void BluetoothManager::addOrUpdateDevice(const BluetoothDevice& device)
{
    BluetoothDevice normalised = normaliseDevice(device);

    std::lock_guard<std::mutex> lock(devicesMutex);
    auto it = std::find_if(availableDevices.begin(), availableDevices.end(),
                           [&normalised](const BluetoothDevice& existing)
                           {
                               return existing.id == normalised.id
                                   || existing.identifier == normalised.id
                                   || existing.id == normalised.identifier;
                           });

    if (it == availableDevices.end())
    {
        availableDevices.push_back(normalised);
    }
    else
    {
        normalised.isConnected = normalised.isConnected || it->isConnected;
        *it = normalised;
    }
}

#if JUCE_MAC

void BluetoothManager::scanForDevices() {}

#else

void BluetoothManager::scanForDevices()
{
    while (isRunning.load())
        std::this_thread::sleep_for(std::chrono::seconds(1));
}

#if defined(HEARTSYNC_USE_WINRT) && HEARTSYNC_USE_WINRT

struct BluetoothManager::WindowsState
{
    DeviceWatcher deviceWatcher { nullptr };
    GattCharacteristic heartRateCharacteristic { nullptr };
    BluetoothLEDevice bleDevice { nullptr };
};

void BluetoothManager::connectToDeviceInternal(const DeviceInformation& deviceInfo)
{
    try
    {
        auto bleDeviceAsync = BluetoothLEDevice::FromIdAsync(deviceInfo.Id());
        windows->bleDevice = bleDeviceAsync.get();

        if (! windows->bleDevice)
            throw std::runtime_error("Failed to obtain Bluetooth LE device");

        auto servicesAsync = windows->bleDevice.GetGattServicesAsync();
        auto servicesResult = servicesAsync.get();
        if (servicesResult.Status() != GattCommunicationStatus::Success)
            throw std::runtime_error("Failed to enumerate GATT services");

        for (const auto& service : servicesResult.Services())
        {
            if (service.Uuid() != BluetoothUuidHelper::FromShortId(0x180D))
                continue;

            auto characteristicsAsync = service.GetCharacteristicsAsync();
            auto characteristicsResult = characteristicsAsync.get();
            if (characteristicsResult.Status() != GattCommunicationStatus::Success)
                continue;

            for (const auto& characteristic : characteristicsResult.Characteristics())
            {
                if (characteristic.Uuid() == BluetoothUuidHelper::FromShortId(0x2A37))
                {
                    subscribeToHeartRateNotifications(characteristic);

                    BluetoothDevice device;
                    device.id = winrt::to_string(deviceInfo.Id());
                    device.name = winrt::to_string(deviceInfo.Name());
                    device.isConnected = true;
                    addOrUpdateDevice(device);
                    dispatchConnected(device);
                    return;
                }
            }
        }

        throw std::runtime_error("Heart Rate Service not available on device");
    }
    catch (const std::exception& e)
    {
        dispatchError("Connection error: " + std::string(e.what()));
        disconnectCurrentDevice();
    }
}

void BluetoothManager::subscribeToHeartRateNotifications(const GattCharacteristic& characteristic)
{
    try
    {
        windows->heartRateCharacteristic = characteristic;
        auto status = characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::Notify).get();

        if (status != GattCommunicationStatus::Success)
            throw std::runtime_error("Failed to enable notifications");

        characteristic.ValueChanged([this](GattCharacteristic const&, GattValueChangedEventArgs const& args)
        {
            processHeartRateData(args.CharacteristicValue());
        });
    }
    catch (const std::exception& e)
    {
        dispatchError("Subscription error: " + std::string(e.what()));
    }
}

void BluetoothManager::processHeartRateData(const IBuffer& buffer)
{
    try
    {
        auto reader = DataReader::FromBuffer(buffer);
        if (buffer.Length() < 2)
            return;

        const auto flags = reader.ReadByte();
        uint16_t heartRate = 0;
        if (flags & 0x01)
            heartRate = reader.ReadUInt16();
        else
            heartRate = reader.ReadByte();

        std::vector<float> rrIntervals;
        if (flags & 0x10)
        {
            while (reader.UnconsumedBufferLength() >= 2)
            {
                const auto rrValue = reader.ReadUInt16();
                const float rrMs = static_cast<float>(rrValue) / 1024.0f * 1000.0f;
                rrIntervals.push_back(rrMs);
            }
        }

        dispatchMeasurement(static_cast<float>(heartRate), rrIntervals);
    }
    catch (const std::exception& e)
    {
        dispatchError("Data processing error: " + std::string(e.what()));
    }
}

void BluetoothManager::updateDeviceList(const DeviceInformation& deviceInfo)
{
    auto properties = deviceInfo.Properties();
    if (! properties.HasKey(L"System.Devices.Aep.Service.Gatt"))
        return;

    auto gattServices = properties.Lookup(L"System.Devices.Aep.Service.Gatt");
    auto gattServiceString = winrt::unbox_value<winrt::hstring>(gattServices);
    if (gattServiceString.find(L"0000180d") == std::wstring::npos)
        return;

    BluetoothDevice device;
    device.id = winrt::to_string(deviceInfo.Id());
    device.name = winrt::to_string(deviceInfo.Name());
    device.signalStrength = 0;
    addOrUpdateDevice(device);
}

void BluetoothManager::onDeviceRemoved(const DeviceWatcher&, const DeviceInformationUpdate& update)
{
    const std::string deviceId = winrt::to_string(update.Id());
    std::lock_guard<std::mutex> lock(devicesMutex);
    availableDevices.erase(
        std::remove_if(availableDevices.begin(), availableDevices.end(),
                       [&deviceId](const BluetoothDevice& device)
                       {
                           return device.id == deviceId || device.identifier == deviceId;
                       }),
        availableDevices.end());
}

#else

void BluetoothManager::startHeartRateSimulation()
{
    std::thread([this]()
    {
        std::mt19937 rng(static_cast<unsigned int>(std::random_device{}()));
        std::normal_distribution<float> noise(0.0f, 2.0f);

        const float baseHeartRate = 72.0f;
        const auto startTime = std::chrono::steady_clock::now();

        while (connected.load())
        {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            const float slowVariation = std::sin(static_cast<float>(elapsedMs) * 0.001f) * 10.0f;
            float heartRate = baseHeartRate + slowVariation + noise(rng);
            heartRate = std::clamp(heartRate, 50.0f, 180.0f);

            std::vector<float> rrIntervals;
            const float rrMs = 60000.0f / heartRate;
            rrIntervals.push_back(rrMs);

            dispatchMeasurement(heartRate, rrIntervals);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }).detach();
}

#endif // defined(HEARTSYNC_USE_WINRT) && HEARTSYNC_USE_WINRT

#endif // JUCE_MAC
