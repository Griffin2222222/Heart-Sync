#include "BluetoothManager.h"
#include <iostream>
#include <cmath>

// Only compile Core Bluetooth code on macOS
#ifdef __APPLE__
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

// Heart Rate Service and Characteristic UUIDs
static NSString* const HEART_RATE_SERVICE_UUID = @"180D";
static NSString* const HEART_RATE_MEASUREMENT_UUID = @"2A37";

@implementation HeartRateDelegate {
    BluetoothManager* cppManager;
}

- (instancetype)initWithManager:(BluetoothManager*)mgr {
    if (self = [super init]) {
        cppManager = mgr;
    }
    return self;
}

#pragma mark - CBCentralManagerDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    NSString* stateString = @"Unknown";
    bool isAvailable = false;
    
    switch (central.state) {
        case CBManagerStatePoweredOn:
            stateString = @"Bluetooth LE is powered on and ready";
            isAvailable = true;
            break;
        case CBManagerStatePoweredOff:
            stateString = @"Bluetooth LE is powered off";
            break;
        case CBManagerStateUnsupported:
            stateString = @"Bluetooth LE is not supported";
            break;
        case CBManagerStateUnauthorized:
            stateString = @"App is not authorized to use Bluetooth LE";
            break;
        case CBManagerStateResetting:
            stateString = @"Bluetooth LE is resetting";
            break;
        default:
            stateString = @"Bluetooth LE state unknown";
            break;
    }
    
    if (cppManager) {
        cppManager->logToConsole("❖ Bluetooth: " + std::string([stateString UTF8String]));
        cppManager->bluetoothStateDidUpdate(isAvailable);
    }
}

- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData RSSI:(NSNumber *)RSSI {
    
    NSString* deviceName = peripheral.name ?: @"Unknown Device";
    NSString* identifier = peripheral.identifier.UUIDString;
    int rssiValue = [RSSI intValue];
    
    // Filter for devices that advertise Heart Rate Service or have relevant names
    bool isHeartRateDevice = false;
    NSArray* serviceUUIDs = advertisementData[CBAdvertisementDataServiceUUIDsKey];
    for (CBUUID* serviceUUID in serviceUUIDs) {
        if ([serviceUUID.UUIDString.lowercaseString isEqualToString:HEART_RATE_SERVICE_UUID.lowercaseString]) {
            isHeartRateDevice = true;
            break;
        }
    }
    
    // Also check device name for common heart rate monitor patterns
    NSString* lowerName = deviceName.lowercaseString;
    if ([lowerName containsString:@"polar"] || 
        [lowerName containsString:@"heart"] ||
        [lowerName containsString:@"rate"] ||
        [lowerName containsString:@"hr"] ||
        [lowerName containsString:@"chest"] ||
        [lowerName containsString:@"band"]) {
        isHeartRateDevice = true;
    }
    
    if (isHeartRateDevice && cppManager) {
        std::string name([deviceName UTF8String]);
        std::string id([identifier UTF8String]);
        cppManager->didDiscoverPeripheral(name, id, rssiValue);
    }
}

- (void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral {
    if (cppManager) {
        std::string name([peripheral.name UTF8String] ?: "Unknown Device");
        cppManager->didConnectPeripheral(name);
        cppManager->logToConsole("✓ Connected to: " + name);
        
        // Set delegate and discover services
        peripheral.delegate = self;
        [peripheral discoverServices:@[[CBUUID UUIDWithString:HEART_RATE_SERVICE_UUID]]];
    }
}

- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    if (cppManager) {
        std::string message = "✗ Disconnected";
        if (error) {
            message += ": " + std::string([error.localizedDescription UTF8String]);
        }
        cppManager->logToConsole(message);
        cppManager->didDisconnectPeripheral();
    }
}

- (void)centralManager:(CBCentralManager *)central didFailToConnectPeripheral:(CBPeripheral *)peripheral error:(NSError *)error {
    if (cppManager) {
        std::string message = "✗ Failed to connect";
        if (error) {
            message += ": " + std::string([error.localizedDescription UTF8String]);
        }
        cppManager->logToConsole(message);
    }
}

#pragma mark - CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
    if (error) {
        if (cppManager) {
            cppManager->logToConsole("✗ Service discovery error: " + std::string([error.localizedDescription UTF8String]));
        }
        return;
    }
    
    for (CBService* service in peripheral.services) {
        if ([service.UUID.UUIDString.lowercaseString isEqualToString:HEART_RATE_SERVICE_UUID.lowercaseString]) {
            if (cppManager) {
                cppManager->logToConsole("❖ Found Heart Rate Service");
            }
            [peripheral discoverCharacteristics:@[[CBUUID UUIDWithString:HEART_RATE_MEASUREMENT_UUID]] forService:service];
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error {
    if (error) {
        if (cppManager) {
            cppManager->logToConsole("✗ Characteristic discovery error: " + std::string([error.localizedDescription UTF8String]));
        }
        return;
    }
    
    for (CBCharacteristic* characteristic in service.characteristics) {
        if ([characteristic.UUID.UUIDString.lowercaseString isEqualToString:HEART_RATE_MEASUREMENT_UUID.lowercaseString]) {
            if (cppManager) {
                cppManager->logToConsole("❖ Found Heart Rate Measurement - subscribing to notifications");
            }
            [peripheral setNotifyValue:YES forCharacteristic:characteristic];
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    if (error) {
        if (cppManager) {
            cppManager->logToConsole("✗ Heart rate data error: " + std::string([error.localizedDescription UTF8String]));
        }
        return;
    }
    
    if ([characteristic.UUID.UUIDString.lowercaseString isEqualToString:HEART_RATE_MEASUREMENT_UUID.lowercaseString]) {
        NSData* data = characteristic.value;
        if (data.length >= 2) {
            const uint8_t* bytes = (const uint8_t*)data.bytes;
            
            // Parse heart rate measurement according to Bluetooth spec
            float heartRate = 0.0f;
            if (bytes[0] & 0x01) {
                // 16-bit heart rate value
                heartRate = (float)((bytes[2] << 8) | bytes[1]);
            } else {
                // 8-bit heart rate value
                heartRate = (float)bytes[1];
            }
            
            if (cppManager && heartRate > 0 && heartRate < 300) { // Sanity check
                cppManager->didReceiveHeartRateData(heartRate);
            }
        }
    }
}

@end

#endif // __APPLE__

// C++ Implementation
BluetoothManager::BluetoothManager()
    : centralManager(nullptr)
    , connectedPeripheral(nullptr)
    , discoveredPeripherals(nullptr)
    , delegate(nullptr)
    , scanning(false)
    , connected(false)
    , currentHeartRate(0.0f)
    , smoothedHeartRate(0.0f)
    , wetDryRatio(50.0f)
    , heartRateOffset(0.0f)
    , smoothingFactor(0.1f)
    , wetDryOffset(0.0f)
{
    initializeBluetooth();
}

BluetoothManager::~BluetoothManager()
{
    cleanupBluetooth();
}

void BluetoothManager::initializeBluetooth()
{
    // Temporarily disable Bluetooth initialization to prevent crashes
    // TODO: Implement proper deferred initialization
    logToConsole("❖ HeartSync Bluetooth LE Manager - Deferred initialization");
    return;
    
#ifdef __APPLE__
    @autoreleasepool {
        delegate = [[HeartRateDelegate alloc] initWithManager:this];
        centralManager = [[CBCentralManager alloc] initWithDelegate:delegate queue:nil];
        discoveredPeripherals = [[NSMutableArray alloc] init];
        logToConsole("❖ HeartSync Bluetooth LE Manager Initialized");
    }
#else
    logToConsole("❖ Bluetooth LE not supported on this platform");
#endif
}

void BluetoothManager::cleanupBluetooth()
{
#ifdef __APPLE__
    @autoreleasepool {
        if (centralManager) {
            [centralManager stopScan];
            centralManager = nil;
        }
        if (discoveredPeripherals) {
            [discoveredPeripherals removeAllObjects];
            discoveredPeripherals = nil;
        }
        delegate = nil;
        connectedPeripheral = nil;
    }
#endif
}

void BluetoothManager::startScanning()
{
    if (scanning.load()) return;
    
#ifdef __APPLE__
    @autoreleasepool {
        if (centralManager && centralManager.state == CBManagerStatePoweredOn) {
            scanning = true;
            [discoveredPeripherals removeAllObjects];
            
            std::lock_guard<std::mutex> lock(devicesMutex);
            discoveredDevices.clear();
            
            // Scan for devices advertising Heart Rate Service
            NSArray* serviceUUIDs = @[[CBUUID UUIDWithString:HEART_RATE_SERVICE_UUID]];
            [centralManager scanForPeripheralsWithServices:nil // nil to find all devices
                                                    options:@{CBCentralManagerScanOptionAllowDuplicatesKey: @NO}];
            
            logToConsole("❖ Scanning for Heart Rate monitors...");
            if (onDeviceDiscovered) onDeviceDiscovered();
        } else {
            logToConsole("✗ Bluetooth not ready for scanning");
        }
    }
#endif
}

void BluetoothManager::stopScanning()
{
    if (!scanning.load()) return;
    
#ifdef __APPLE__
    @autoreleasepool {
        scanning = false;
        if (centralManager) {
            [centralManager stopScan];
            logToConsole("❖ Stopped scanning");
        }
    }
#endif
}

void BluetoothManager::connectToDevice(const std::string& deviceIdentifier)
{
#ifdef __APPLE__
    @autoreleasepool {
        if (!centralManager || centralManager.state != CBManagerStatePoweredOn) {
            logToConsole("✗ Bluetooth not ready for connection");
            return;
        }
        
        NSString* targetID = [NSString stringWithUTF8String:deviceIdentifier.c_str()];
        NSUUID* uuid = [[NSUUID alloc] initWithUUIDString:targetID];
        
        if (uuid) {
            NSArray* peripherals = [centralManager retrievePeripheralsWithIdentifiers:@[uuid]];
            if (peripherals.count > 0) {
                CBPeripheral* peripheral = peripherals[0];
                connectedPeripheral = peripheral;
                [centralManager connectPeripheral:peripheral options:nil];
                logToConsole("❖ Connecting to " + std::string([peripheral.name UTF8String] ?: "Unknown Device"));
            } else {
                logToConsole("✗ Device not found for connection");
            }
        }
    }
#endif
}

void BluetoothManager::disconnectFromDevice()
{
#ifdef __APPLE__
    @autoreleasepool {
        if (connectedPeripheral && centralManager) {
            [centralManager cancelPeripheralConnection:connectedPeripheral];
            connectedPeripheral = nil;
        }
    }
#endif
}

std::vector<BluetoothDevice> BluetoothManager::getDiscoveredDevices() const
{
    std::lock_guard<std::mutex> lock(devicesMutex);
    return discoveredDevices;
}

std::deque<float> BluetoothManager::getRawHeartRateHistory() const
{
    std::lock_guard<std::mutex> lock(historyMutex);
    return rawHeartRateHistory;
}

std::deque<float> BluetoothManager::getSmoothedHeartRateHistory() const
{
    std::lock_guard<std::mutex> lock(historyMutex);
    return smoothedHistory;
}

std::deque<float> BluetoothManager::getWetDryHistory() const
{
    std::lock_guard<std::mutex> lock(historyMutex);
    return wetDryHistory;
}

void BluetoothManager::setHeartRateOffset(float offset)
{
    heartRateOffset = offset;
}

void BluetoothManager::setSmoothingFactor(float factor)
{
    smoothingFactor = juce::jlimit(0.01f, 10.0f, factor);
}

void BluetoothManager::setWetDryOffset(float offset)
{
    wetDryOffset = offset;
}

void BluetoothManager::processHeartRateData(float rawHeartRate)
{
    // Apply offset
    float adjustedHeartRate = rawHeartRate + heartRateOffset.load();
    currentHeartRate = adjustedHeartRate;
    
    // Add to raw history
    addToHistory(rawHeartRateHistory, adjustedHeartRate);
    
    // Update smoothed heart rate
    updateSmoothedHeartRate();
    
    // Update wet/dry ratio
    updateWetDryRatio();
    
    // Trigger callbacks
    if (onHeartRateReceived) {
        onHeartRateReceived(adjustedHeartRate);
    }
}

void BluetoothManager::updateSmoothedHeartRate()
{
    float current = currentHeartRate.load();
    float smoothed = smoothedHeartRate.load();
    float factor = smoothingFactor.load();
    
    // Exponential moving average
    float newSmoothed = smoothed + (factor * (current - smoothed));
    smoothedHeartRate = newSmoothed;
    
    addToHistory(smoothedHistory, newSmoothed);
}

void BluetoothManager::updateWetDryRatio()
{
    std::lock_guard<std::mutex> lock(historyMutex);
    
    if (rawHeartRateHistory.size() < 10) {
        wetDryRatio = 50.0f + wetDryOffset.load(); // Default to 50% when insufficient data
        return;
    }
    
    // Calculate variance over recent history
    float sum = 0.0f;
    size_t count = std::min(rawHeartRateHistory.size(), static_cast<size_t>(30));
    for (size_t i = rawHeartRateHistory.size() - count; i < rawHeartRateHistory.size(); ++i) {
        sum += rawHeartRateHistory[i];
    }
    float mean = sum / count;
    
    float variance = 0.0f;
    for (size_t i = rawHeartRateHistory.size() - count; i < rawHeartRateHistory.size(); ++i) {
        float diff = rawHeartRateHistory[i] - mean;
        variance += diff * diff;
    }
    variance /= count;
    
    // Convert variance to wet/dry ratio (higher variance = more "wet"/active)
    float ratio = juce::jlimit(0.0f, 100.0f, (variance / 10.0f) * 100.0f);
    ratio += wetDryOffset.load();
    wetDryRatio = juce::jlimit(0.0f, 100.0f, ratio);
    
    addToHistory(wetDryHistory, wetDryRatio.load());
}

void BluetoothManager::addToHistory(std::deque<float>& history, float value)
{
    std::lock_guard<std::mutex> lock(historyMutex);
    history.push_back(value);
    if (history.size() > MAX_HISTORY_SIZE) {
        history.pop_front();
    }
}

void BluetoothManager::logToConsole(const std::string& message)
{
    if (onConsoleMessage) {
        onConsoleMessage(message);
    }
}

// Methods called by Objective-C delegate
void BluetoothManager::didDiscoverPeripheral(const std::string& name, const std::string& identifier, int rssi)
{
    std::lock_guard<std::mutex> lock(devicesMutex);
    
    // Check if device already exists
    for (auto& device : discoveredDevices) {
        if (device.identifier == identifier) {
            device.rssi = rssi; // Update RSSI
            return;
        }
    }
    
    // Add new device
    discoveredDevices.emplace_back(name, identifier, rssi);
    logToConsole("📡 Found: " + name + " (RSSI: " + std::to_string(rssi) + ")");
    
    if (onDeviceDiscovered) {
        onDeviceDiscovered();
    }
}

void BluetoothManager::didConnectPeripheral(const std::string& name)
{
    connected = true;
    connectedDeviceName = name;
    
    if (onConnectionStatusChanged) {
        onConnectionStatusChanged();
    }
}

void BluetoothManager::didDisconnectPeripheral()
{
    connected = false;
    connectedDeviceName.clear();
    
    if (onConnectionStatusChanged) {
        onConnectionStatusChanged();
    }
}

void BluetoothManager::didReceiveHeartRateData(float heartRate)
{
    processHeartRateData(heartRate);
    logToConsole("💓 Heart Rate: " + std::to_string((int)heartRate) + " BPM");
}

void BluetoothManager::bluetoothStateDidUpdate(bool isAvailable)
{
    if (!isAvailable) {
        scanning = false;
        connected = false;
        connectedDeviceName.clear();
        
        if (onConnectionStatusChanged) {
            onConnectionStatusChanged();
        }
    }
}