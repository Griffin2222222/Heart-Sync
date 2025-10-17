#include "BluetoothManager.h"

#if JUCE_MAC

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

namespace
{
    static NSString* const kHeartRateServiceUUID = @"180D";
    static NSString* const kHeartRateMeasurementUUID = @"2A37";

    std::string toStdString(NSString* string)
    {
        return string != nil ? std::string([string UTF8String]) : std::string();
    }
}

@interface HeartSyncBLEDelegate : NSObject<CBCentralManagerDelegate, CBPeripheralDelegate>
{
@private
    BluetoothManager::Impl* impl;
}
- (instancetype)initWithImpl:(BluetoothManager::Impl*)impl;
@end

struct BluetoothManager::Impl
{
    explicit Impl(BluetoothManager& ownerIn)
        : owner(ownerIn)
    {
        @autoreleasepool
        {
            delegate = [[HeartSyncBLEDelegate alloc] initWithImpl:this];
            centralManager = [[CBCentralManager alloc] initWithDelegate:delegate queue:nil options:nil];
            peripherals = [[NSMutableDictionary alloc] init];
        }
    }

    ~Impl()
    {
        stopScanning();

        @autoreleasepool
        {
            if (centralManager != nil)
            {
                centralManager.delegate = nil;
                [centralManager stopScan];
            }

            if (connectedPeripheral != nil && centralManager != nil)
                [centralManager cancelPeripheralConnection:connectedPeripheral];

            delegate = nil;
            centralManager = nil;
            peripherals = nil;
            connectedPeripheral = nil;
        }
    }

    void startScanning()
    {
        if (! bluetoothReady)
        {
            owner.dispatchError("Bluetooth LE not available");
            return;
        }

        if (owner.scanning.exchange(true))
            return;

        @autoreleasepool
        {
            NSArray<CBUUID*>* services = @[ [CBUUID UUIDWithString:kHeartRateServiceUUID] ];
            [peripherals removeAllObjects];
            [centralManager scanForPeripheralsWithServices:services
                                                   options:@{ CBCentralManagerScanOptionAllowDuplicatesKey: @NO }];
        }
    }

    void stopScanning()
    {
        if (! owner.scanning.exchange(false))
            return;

        @autoreleasepool
        {
            if (centralManager != nil)
                [centralManager stopScan];
        }
    }

    bool connectToDevice(const std::string& deviceId)
    {
        if (deviceId.empty())
            return false;

        @autoreleasepool
        {
            NSString* identifier = [NSString stringWithUTF8String:deviceId.c_str()];
            NSUUID* uuid = [[NSUUID alloc] initWithUUIDString:identifier];
            if (uuid == nil)
            {
                owner.dispatchError("Invalid device identifier");
                return false;
            }

            CBPeripheral* peripheral = [peripherals objectForKey:uuid];
            if (peripheral == nil)
            {
                owner.dispatchError("Device not available for connection");
                return false;
            }

            connectedPeripheral = peripheral;
            [centralManager connectPeripheral:peripheral options:nil];
        }

        return true;
    }

    void disconnectCurrentDevice()
    {
        @autoreleasepool
        {
            if (connectedPeripheral != nil && centralManager != nil)
                [centralManager cancelPeripheralConnection:connectedPeripheral];
        }
    }

    void handleStateChange(CBCentralManager* manager)
    {
        bluetoothReady = (manager.state == CBManagerStatePoweredOn);
        if (! bluetoothReady)
        {
            owner.dispatchError("Bluetooth LE not powered on");
            owner.scanning.store(false);
            return;
        }
    }

    void handleDiscovery(CBPeripheral* peripheral,
                         NSDictionary<NSString*, id>* advertisementData,
                         NSNumber* RSSI)
    {
        if (peripheral == nil)
            return;

        @autoreleasepool
        {
            if (peripheral.identifier == nil)
                return;

            peripherals[peripheral.identifier] = peripheral;

            BluetoothDevice device;
            device.identifier = toStdString(peripheral.identifier.UUIDString);
            device.id = device.identifier;
            device.name = toStdString(peripheral.name ?: advertisementData[CBAdvertisementDataLocalNameKey]);
            device.signalStrength = RSSI != nil ? [RSSI intValue] : 0;
            device.rssi = device.signalStrength;

            owner.addOrUpdateDevice(device);
        }
    }

    void handleConnect(CBPeripheral* peripheral)
    {
        if (peripheral == nil)
            return;

        connectedPeripheral = peripheral;
        connectedPeripheral.delegate = delegate;
        owner.scanning.store(false);

        BluetoothDevice device;
        device.identifier = toStdString(peripheral.identifier.UUIDString);
        device.id = device.identifier;
        device.name = toStdString(peripheral.name);
        device.isConnected = true;

        owner.addOrUpdateDevice(device);
        owner.dispatchConnected(device);

        @autoreleasepool
        {
            NSArray<CBUUID*>* services = @[ [CBUUID UUIDWithString:kHeartRateServiceUUID] ];
            [peripheral discoverServices:services];
        }
    }

    void handleConnectionFailure(CBPeripheral* peripheral, NSError* error)
    {
        std::string message = "Failed to connect";
        if (error != nil)
            message += ": " + toStdString(error.localizedDescription);
        owner.dispatchError(message);
        owner.dispatchDisconnected();
    }

    void handleDisconnect(CBPeripheral* peripheral, NSError* error)
    {
        std::string message;
        if (error != nil)
            message = "Disconnected: " + toStdString(error.localizedDescription);
        if (! message.empty())
            owner.dispatchError(message);

        owner.dispatchDisconnected();
        connectedPeripheral = nil;
    }

    void handleServicesDiscovered(CBPeripheral* peripheral, NSError* error)
    {
        if (error != nil)
        {
            owner.dispatchError("Service discovery error: " + toStdString(error.localizedDescription));
            return;
        }

        for (CBService* service in peripheral.services)
        {
            if (![service.UUID.UUIDString isEqualToString:kHeartRateServiceUUID])
                continue;

            NSArray<CBUUID*>* characteristics = @[ [CBUUID UUIDWithString:kHeartRateMeasurementUUID] ];
            [peripheral discoverCharacteristics:characteristics forService:service];
        }
    }

    void handleCharacteristicsDiscovered(CBPeripheral* peripheral,
                                         CBService* service,
                                         NSError* error)
    {
        if (error != nil)
        {
            owner.dispatchError("Characteristic discovery error: " + toStdString(error.localizedDescription));
            return;
        }

        for (CBCharacteristic* characteristic in service.characteristics)
        {
            if (![characteristic.UUID.UUIDString isEqualToString:kHeartRateMeasurementUUID])
                continue;

            [peripheral setNotifyValue:YES forCharacteristic:characteristic];
        }
    }

    void handleCharacteristicUpdate(CBPeripheral* peripheral,
                                    CBCharacteristic* characteristic,
                                    NSError* error)
    {
        if (error != nil)
        {
            owner.dispatchError("Heart rate data error: " + toStdString(error.localizedDescription));
            return;
        }

        NSData* data = characteristic.value;
        if (data.length < 2)
            return;

        const uint8_t* bytes = static_cast<const uint8_t*>(data.bytes);
        const uint8_t flags = bytes[0];

        uint16_t heartRateValue = 0;
        if (flags & 0x01)
            heartRateValue = static_cast<uint16_t>(bytes[1] | (bytes[2] << 8));
        else
            heartRateValue = bytes[1];

        std::vector<float> rrIntervals;
        if (flags & 0x10)
        {
            const uint8_t* rrData = bytes + ((flags & 0x01) ? 3 : 2);
            const NSUInteger rrLength = data.length - (rrData - bytes);
            for (NSUInteger index = 0; index + 1 < rrLength; index += 2)
            {
                uint16_t rrRaw = static_cast<uint16_t>(rrData[index] | (rrData[index + 1] << 8));
                float rrMs = static_cast<float>(rrRaw) / 1024.0f * 1000.0f;
                rrIntervals.push_back(rrMs);
            }
        }

        owner.dispatchMeasurement(static_cast<float>(heartRateValue), rrIntervals);
    }

    BluetoothManager& owner;
    CBCentralManager* centralManager = nil;
    HeartSyncBLEDelegate* delegate = nil;
    NSMutableDictionary<NSUUID*, CBPeripheral*>* peripherals = nil;
    CBPeripheral* connectedPeripheral = nil;
    bool bluetoothReady = false;
};

@implementation HeartSyncBLEDelegate

- (instancetype)initWithImpl:(BluetoothManager::Impl*)implIn
{
    if (self = [super init])
        impl = implIn;
    return self;
}

- (void)centralManagerDidUpdateState:(CBCentralManager*)central
{
    if (impl)
        impl->handleStateChange(central);
}

- (void)centralManager:(CBCentralManager*)central
  didDiscoverPeripheral:(CBPeripheral*)peripheral
      advertisementData:(NSDictionary<NSString*, id>*)advertisementData
                   RSSI:(NSNumber*)RSSI
{
    if (impl)
        impl->handleDiscovery(peripheral, advertisementData, RSSI);
}

- (void)centralManager:(CBCentralManager*)central didConnectPeripheral:(CBPeripheral*)peripheral
{
    if (impl)
        impl->handleConnect(peripheral);
}

- (void)centralManager:(CBCentralManager*)central didFailToConnectPeripheral:(CBPeripheral*)peripheral error:(NSError*)error
{
    if (impl)
        impl->handleConnectionFailure(peripheral, error);
}

- (void)centralManager:(CBCentralManager*)central didDisconnectPeripheral:(CBPeripheral*)peripheral error:(NSError*)error
{
    if (impl)
        impl->handleDisconnect(peripheral, error);
}

- (void)peripheral:(CBPeripheral*)peripheral didDiscoverServices:(NSError*)error
{
    if (impl)
        impl->handleServicesDiscovered(peripheral, error);
}

- (void)peripheral:(CBPeripheral*)peripheral
didDiscoverCharacteristicsForService:(CBService*)service
             error:(NSError*)error
{
    if (impl)
        impl->handleCharacteristicsDiscovered(peripheral, service, error);
}

- (void)peripheral:(CBPeripheral*)peripheral
 didUpdateValueForCharacteristic:(CBCharacteristic*)characteristic
             error:(NSError*)error
{
    if (impl)
        impl->handleCharacteristicUpdate(peripheral, characteristic, error);
}

@end

// BluetoothManager public method implementations (forward to Impl)
BluetoothManager::BluetoothManager()
    : scanning(false), connected(false), latestHeartRate(0.0f)
{
    impl = std::make_unique<Impl>(*this);
}

BluetoothManager::~BluetoothManager() = default;

void BluetoothManager::startScanning()
{
    if (impl)
        impl->startScanning();
}

void BluetoothManager::stopScanning()
{
    if (impl)
        impl->stopScanning();
}

bool BluetoothManager::connectToDevice(const std::string& deviceId)
{
    return impl ? impl->connectToDevice(deviceId) : false;
}

void BluetoothManager::disconnectCurrentDevice()
{
    if (impl)
        impl->disconnectCurrentDevice();
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

void BluetoothManager::setConnectionCallbacks(
    std::function<void(const BluetoothDevice&)> onConnected,
    std::function<void()> onDisconnected,
    std::function<void(const std::string&)> onError)
{
    std::lock_guard<std::mutex> lock(callbackMutex);
    onConnectedCallback = std::move(onConnected);
    onDisconnectedCallback = std::move(onDisconnected);
    onErrorCallback = std::move(onError);
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
    connected.store(true);
    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        currentDevice = device;
    }
    std::lock_guard<std::mutex> lockCb(callbackMutex);
    if (onConnectedCallback)
        onConnectedCallback(device);
}

void BluetoothManager::dispatchDisconnected()
{
    connected.store(false);
    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        currentDevice = BluetoothDevice();
    }
    std::lock_guard<std::mutex> lockCb(callbackMutex);
    if (onDisconnectedCallback)
        onDisconnectedCallback();
}

void BluetoothManager::dispatchError(const std::string& message)
{
    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        lastError = message;
    }
    std::lock_guard<std::mutex> lockCb(callbackMutex);
    if (onErrorCallback)
        onErrorCallback(message);
}

void BluetoothManager::addOrUpdateDevice(const BluetoothDevice& device)
{
    std::lock_guard<std::mutex> lock(devicesMutex);
    auto it = std::find_if(availableDevices.begin(), availableDevices.end(),
                           [&](const BluetoothDevice& d) { return d.identifier == device.identifier; });
    if (it != availableDevices.end())
        *it = device;
    else
        availableDevices.push_back(device);
}

#else

void bluetoothManagerNativeFileIsUnused() {}

#endif
