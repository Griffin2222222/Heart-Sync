// CRITICAL: Import system frameworks BEFORE JUCE to prevent name collisions
// Foundation.h must come before any JUCE includes to establish Objective-C types
#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

// Now include JUCE
#import "HeartSyncBLE.h"

// --- Objective-C CoreBluetooth Delegate ---
@interface HeartSyncBLEDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
{
@public
    CBCentralManager* centralManager;
    CBPeripheral* connectedPeripheral;
    NSMutableArray<CBPeripheral*>* discoveredPeripherals;
    NSMutableDictionary<NSUUID*, NSNumber*>* rssiCache;
    
    HeartSyncBLE::ScanCallback scanCallback;
    HeartSyncBLE::ConnectCallback connectCallback;
    HeartSyncBLE::HeartRateCallback heartRateCallback;
    HeartSyncBLE::DisconnectCallback disconnectCallback;
    
    BOOL isScanning;
    BOOL isConnected;
    NSString* targetDeviceUUID;
}

- (instancetype)init;
- (void)startScan;
- (void)stopScan;
- (void)connectToDeviceWithUUID:(NSString*)uuid;
- (void)disconnect;
- (std::vector<HeartSyncBLE::DeviceInfo>)getDiscoveredDevices;

@end

@implementation HeartSyncBLEDelegate

// BLE Service and Characteristic UUIDs (Heart Rate Service)
static CBUUID* HR_SERVICE_UUID;
static CBUUID* HR_MEASUREMENT_CHAR_UUID;

+ (void)initialize
{
    if (self == [HeartSyncBLEDelegate class])
    {
        HR_SERVICE_UUID = [CBUUID UUIDWithString:@"180D"];
        HR_MEASUREMENT_CHAR_UUID = [CBUUID UUIDWithString:@"2A37"];
    }
}

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        discoveredPeripherals = [[NSMutableArray alloc] init];
        rssiCache = [[NSMutableDictionary alloc] init];
        isScanning = NO;
        isConnected = NO;
        connectedPeripheral = nil;
        targetDeviceUUID = nil;
        
        // Initialize central manager on main dispatch queue
        dispatch_async(dispatch_get_main_queue(), ^{
            self->centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
        });
    }
    return self;
}

- (void)dealloc
{
    [self disconnect];
    [centralManager stopScan];
}

- (void)startScan
{
    if (!centralManager || centralManager.state != CBManagerStatePoweredOn)
    {
        NSLog(@"[HeartSyncBLE] Central manager not ready, state: %ld", (long)centralManager.state);
        return;
    }
    
    [discoveredPeripherals removeAllObjects];
    [rssiCache removeAllObjects];
    isScanning = YES;
    
    // Scan for heart rate service
    NSDictionary* options = @{CBCentralManagerScanOptionAllowDuplicatesKey: @YES};
    [centralManager scanForPeripheralsWithServices:@[HR_SERVICE_UUID] options:options];
    
    NSLog(@"[HeartSyncBLE] Started scanning for HR devices");
}

- (void)stopScan
{
    if (isScanning)
    {
        [centralManager stopScan];
        isScanning = NO;
        NSLog(@"[HeartSyncBLE] Stopped scanning");
    }
}

- (void)connectToDeviceWithUUID:(NSString*)uuid
{
    targetDeviceUUID = uuid;
    
    // Find peripheral in discovered list
    CBPeripheral* target = nil;
    for (CBPeripheral* p in discoveredPeripherals)
    {
        if ([p.identifier.UUIDString isEqualToString:uuid])
        {
            target = p;
            break;
        }
    }
    
    if (!target)
    {
        NSLog(@"[HeartSyncBLE] Device not found in discovered list: %@", uuid);
        if (connectCallback)
        {
            juce::String error = "Device not found";
            juce::MessageManager::callAsync([cb = connectCallback, error]() {
                cb(false, error);
            });
        }
        return;
    }
    
    NSLog(@"[HeartSyncBLE] Connecting to device: %@", target.name);
    [centralManager connectPeripheral:target options:nil];
}

- (void)disconnect
{
    if (connectedPeripheral)
    {
        [centralManager cancelPeripheralConnection:connectedPeripheral];
        connectedPeripheral = nil;
        isConnected = NO;
        targetDeviceUUID = nil;
        NSLog(@"[HeartSyncBLE] Disconnected");
    }
}

- (std::vector<HeartSyncBLE::DeviceInfo>)getDiscoveredDevices
{
    std::vector<HeartSyncBLE::DeviceInfo> devices;
    
    for (CBPeripheral* p in discoveredPeripherals)
    {
        juce::String name = p.name ? [p.name UTF8String] : "Unknown Device";
        juce::String address = [p.identifier.UUIDString UTF8String];
        
        int rssi = 0;
        NSNumber* cachedRSSI = rssiCache[p.identifier];
        if (cachedRSSI)
            rssi = [cachedRSSI intValue];
        
        devices.emplace_back(name, address, rssi);
    }
    
    return devices;
}

// --- CBCentralManagerDelegate ---

- (void)centralManagerDidUpdateState:(CBCentralManager*)central
{
    NSLog(@"[HeartSyncBLE] Central manager state: %ld", (long)central.state);
    
    if (central.state == CBManagerStatePoweredOn)
    {
        NSLog(@"[HeartSyncBLE] Bluetooth powered on and ready");
    }
    else if (central.state == CBManagerStatePoweredOff)
    {
        NSLog(@"[HeartSyncBLE] Bluetooth powered off");
    }
    else if (central.state == CBManagerStateUnsupported)
    {
        NSLog(@"[HeartSyncBLE] Bluetooth not supported on this device");
    }
}

- (void)centralManager:(CBCentralManager*)central
 didDiscoverPeripheral:(CBPeripheral*)peripheral
     advertisementData:(NSDictionary<NSString*, id>*)advertisementData
                  RSSI:(NSNumber*)RSSI
{
    // Cache RSSI
    rssiCache[peripheral.identifier] = RSSI;
    
    // Add to discovered list if not already present
    BOOL found = NO;
    for (CBPeripheral* p in discoveredPeripherals)
    {
        if ([p.identifier isEqual:peripheral.identifier])
        {
            found = YES;
            break;
        }
    }
    
    if (!found)
    {
        [discoveredPeripherals addObject:peripheral];
        NSLog(@"[HeartSyncBLE] Discovered: %@ (%@) RSSI: %@", 
              peripheral.name, peripheral.identifier.UUIDString, RSSI);
    }
    
    // Notify scan callback
    if (scanCallback)
    {
        auto devices = [self getDiscoveredDevices];
        juce::MessageManager::callAsync([cb = scanCallback, devices]() {
            cb(devices);
        });
    }
}

- (void)centralManager:(CBCentralManager*)central
  didConnectPeripheral:(CBPeripheral*)peripheral
{
    NSLog(@"[HeartSyncBLE] Connected to: %@", peripheral.name);
    
    connectedPeripheral = peripheral;
    connectedPeripheral.delegate = self;
    isConnected = YES;
    
    [self stopScan];
    
    // Discover heart rate service
    [connectedPeripheral discoverServices:@[HR_SERVICE_UUID]];
}

- (void)centralManager:(CBCentralManager*)central
didFailToConnectPeripheral:(CBPeripheral*)peripheral
                 error:(NSError*)error
{
    NSLog(@"[HeartSyncBLE] Failed to connect: %@", error.localizedDescription);
    
    if (connectCallback)
    {
        juce::String errorMsg = error ? [error.localizedDescription UTF8String] : "Unknown error";
        juce::MessageManager::callAsync([cb = connectCallback, errorMsg]() {
            cb(false, errorMsg);
        });
    }
}

- (void)centralManager:(CBCentralManager*)central
didDisconnectPeripheral:(CBPeripheral*)peripheral
                 error:(NSError*)error
{
    NSLog(@"[HeartSyncBLE] Disconnected from: %@ (error: %@)", peripheral.name, error);
    
    isConnected = NO;
    connectedPeripheral = nil;
    
    if (disconnectCallback)
    {
        juce::MessageManager::callAsync([cb = disconnectCallback]() {
            cb();
        });
    }
}

// --- CBPeripheralDelegate ---

- (void)peripheral:(CBPeripheral*)peripheral
didDiscoverServices:(NSError*)error
{
    if (error)
    {
        NSLog(@"[HeartSyncBLE] Service discovery error: %@", error);
        return;
    }
    
    for (CBService* service in peripheral.services)
    {
        if ([service.UUID isEqual:HR_SERVICE_UUID])
        {
            NSLog(@"[HeartSyncBLE] Found HR service, discovering characteristics");
            [peripheral discoverCharacteristics:@[HR_MEASUREMENT_CHAR_UUID] forService:service];
        }
    }
}

- (void)peripheral:(CBPeripheral*)peripheral
didDiscoverCharacteristicsForService:(CBService*)service
             error:(NSError*)error
{
    if (error)
    {
        NSLog(@"[HeartSyncBLE] Characteristic discovery error: %@", error);
        return;
    }
    
    for (CBCharacteristic* characteristic in service.characteristics)
    {
        if ([characteristic.UUID isEqual:HR_MEASUREMENT_CHAR_UUID])
        {
            NSLog(@"[HeartSyncBLE] Found HR measurement characteristic, enabling notifications");
            [peripheral setNotifyValue:YES forCharacteristic:characteristic];
            
            // Notify successful connection
            if (connectCallback)
            {
                juce::MessageManager::callAsync([cb = connectCallback]() {
                    cb(true, juce::String());
                });
            }
        }
    }
}

- (void)peripheral:(CBPeripheral*)peripheral
didUpdateValueForCharacteristic:(CBCharacteristic*)characteristic
             error:(NSError*)error
{
    if (error)
    {
        NSLog(@"[HeartSyncBLE] Characteristic update error: %@", error);
        return;
    }
    
    if ([characteristic.UUID isEqual:HR_MEASUREMENT_CHAR_UUID])
    {
        NSData* data = characteristic.value;
        if (data && data.length > 0)
        {
            [self parseHeartRateMeasurement:data];
        }
    }
}

- (void)parseHeartRateMeasurement:(NSData*)data
{
    const uint8_t* bytes = (const uint8_t*)data.bytes;
    NSUInteger length = data.length;
    
    if (length < 2)
        return;
    
    uint8_t flags = bytes[0];
    BOOL is16Bit = (flags & 0x01) != 0;
    BOOL hasEnergyExpended = (flags & 0x08) != 0;
    BOOL hasRRInterval = (flags & 0x10) != 0;
    
    NSUInteger idx = 1;
    
    // Parse heart rate value
    int heartRate = 0;
    if (is16Bit)
    {
        if (length < idx + 2)
            return;
        heartRate = bytes[idx] | (bytes[idx + 1] << 8);
        idx += 2;
    }
    else
    {
        if (length < idx + 1)
            return;
        heartRate = bytes[idx];
        idx += 1;
    }
    
    // Skip energy expended if present
    if (hasEnergyExpended && length >= idx + 2)
    {
        idx += 2;
    }
    
    // Parse RR intervals if present
    std::vector<float> rrIntervals;
    if (hasRRInterval)
    {
        while (length >= idx + 2)
        {
            uint16_t rrRaw = bytes[idx] | (bytes[idx + 1] << 8);
            idx += 2;
            
            if (rrRaw > 0)
            {
                float rrSeconds = rrRaw / 1024.0f;
                rrIntervals.push_back(rrSeconds);
            }
        }
    }
    
    // Validate heart rate range
    if (heartRate < 30 || heartRate > 250)
    {
        NSLog(@"[HeartSyncBLE] Invalid HR value: %d", heartRate);
        return;
    }
    
    // Notify callback on message thread
    if (heartRateCallback)
    {
        juce::MessageManager::callAsync([cb = heartRateCallback, heartRate, rrIntervals]() {
            cb(heartRate, rrIntervals);
        });
    }
}

@end

// --- C++ Wrapper Implementation ---

class HeartSyncBLE::Impl
{
public:
    HeartSyncBLEDelegate* delegate;
    
    Impl()
    {
        delegate = [[HeartSyncBLEDelegate alloc] init];
    }
    
    ~Impl()
    {
        [delegate disconnect];
    }
};

HeartSyncBLE::HeartSyncBLE()
    : pimpl(std::make_unique<Impl>())
{
}

HeartSyncBLE::~HeartSyncBLE() = default;

void HeartSyncBLE::startScan(ScanCallback callback)
{
    pimpl->delegate->scanCallback = callback;
    [pimpl->delegate startScan];
}

void HeartSyncBLE::stopScan()
{
    [pimpl->delegate stopScan];
}

void HeartSyncBLE::connectToDevice(const juce::String& deviceAddress, ConnectCallback callback)
{
    pimpl->delegate->connectCallback = callback;
    NSString* uuid = [NSString stringWithUTF8String:deviceAddress.toRawUTF8()];
    [pimpl->delegate connectToDeviceWithUUID:uuid];
}

void HeartSyncBLE::disconnect()
{
    [pimpl->delegate disconnect];
}

bool HeartSyncBLE::isConnected() const
{
    return pimpl->delegate->isConnected;
}

juce::String HeartSyncBLE::getConnectedDeviceAddress() const
{
    if (pimpl->delegate->connectedPeripheral)
    {
        return [pimpl->delegate->connectedPeripheral.identifier.UUIDString UTF8String];
    }
    return juce::String();
}

void HeartSyncBLE::setHeartRateCallback(HeartRateCallback callback)
{
    pimpl->delegate->heartRateCallback = callback;
}

void HeartSyncBLE::setDisconnectCallback(DisconnectCallback callback)
{
    pimpl->delegate->disconnectCallback = callback;
}
