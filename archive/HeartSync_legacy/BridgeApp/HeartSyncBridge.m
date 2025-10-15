//
//  HeartSyncBridge.m
//  HeartSync Bridge - Headless UDS+BLE Worker
//
//  Created: October 14, 2025
//  License: Proprietary
//

#import "HeartSyncBridge.h"
#import <AppKit/AppKit.h>
#import <sys/socket.h>
#import <sys/un.h>
#import <sys/stat.h>
#import <sys/file.h>
#import <arpa/inet.h>
#import <unistd.h>

// MARK: - Constants

static const uint32_t kMaxMessageSize = 65536; // 64KB
static const NSTimeInterval kHeartbeatInterval = 2.0; // seconds
static CBUUID *HR_SERVICE_UUID;
static CBUUID *HR_MEASUREMENT_CHAR_UUID;

// MARK: - Helper Functions

// Returns ~/Library/Application Support/HeartSync with 0700 permissions
static NSString* HSAppSupportDir(void) {
    NSString *appSupport = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) firstObject];
    NSString *hsDir = [appSupport stringByAppendingPathComponent:@"HeartSync"];
    
    NSError *error = nil;
    [[NSFileManager defaultManager] createDirectoryAtPath:hsDir
                              withIntermediateDirectories:YES
                                               attributes:@{NSFilePosixPermissions: @(0700)}
                                                    error:&error];
    if (error) {
        NSLog(@"[Bridge] Warning: Failed to create app support dir: %@", error);
    }
    
    return hsDir;
}

// Returns full path to bridge.sock
static NSString* HSSocketPath(void) {
    return [HSAppSupportDir() stringByAppendingPathComponent:@"bridge.sock"];
}

// Returns full path to bridge.lock
static NSString* HSLockfilePath(void) {
    return [HSAppSupportDir() stringByAppendingPathComponent:@"bridge.lock"];
}

// Test if socket is responsive (returns YES if active)
static BOOL HSIsSocketActive(NSString *socketPath) {
    int testSock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (testSock < 0) return NO;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, [socketPath UTF8String], sizeof(addr.sun_path) - 1);
    
    // Try to connect with 1s timeout
    fcntl(testSock, F_SETFL, O_NONBLOCK);
    int result = connect(testSock, (struct sockaddr *)&addr, sizeof(addr));
    
    if (result == 0) {
        close(testSock);
        return YES; // Connected immediately = active
    }
    
    if (errno == EINPROGRESS) {
        fd_set wfds;
        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        FD_ZERO(&wfds);
        FD_SET(testSock, &wfds);
        
        int selectResult = select(testSock + 1, NULL, &wfds, NULL, &tv);
        close(testSock);
        return (selectResult > 0); // Active if connected within timeout
    }
    
    close(testSock);
    return NO; // Connection refused = stale socket
}

// MARK: - UDS Server Implementation

@implementation HeartSyncUDSServer {
    int _serverSocket;
    int _lockfd;
    NSFileHandle *_clientHandle;
    dispatch_queue_t _readQueue;
}

+ (void)initialize {
    if (self == [HeartSyncUDSServer class]) {
        HR_SERVICE_UUID = [CBUUID UUIDWithString:@"180D"];
        HR_MEASUREMENT_CHAR_UUID = [CBUUID UUIDWithString:@"2A37"];
    }
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _serverSocket = -1;
        _lockfd = -1;
        _readQueue = dispatch_queue_create("com.quantumbioaudio.heartsync.bridge.read", DISPATCH_QUEUE_SERIAL);
    }
    return self;
}

- (BOOL)startServerAtSocketPath:(NSString *)socketPath {
    // Use helper to get canonical socket path
    socketPath = HSSocketPath();
    NSString *lockPath = HSLockfilePath();
    
    NSLog(@"[Bridge] Socket path: %@", socketPath);
    
    // Single-instance lockfile
    _lockfd = open([lockPath UTF8String], O_CREAT | O_RDWR, 0600);
    if (_lockfd < 0) {
        NSLog(@"[Bridge] Failed to create lockfile: %s", strerror(errno));
        return NO;
    }
    
    if (flock(_lockfd, LOCK_EX | LOCK_NB) != 0) {
        NSLog(@"[Bridge] Another instance already running (lockfile held)");
        close(_lockfd);
        _lockfd = -1;
        return NO;
    }
    
    // Check if socket exists and is active
    if ([[NSFileManager defaultManager] fileExistsAtPath:socketPath]) {
        if (HSIsSocketActive(socketPath)) {
            NSLog(@"[Bridge] Socket is active from another instance - exiting");
            flock(_lockfd, LOCK_UN);
            close(_lockfd);
            _lockfd = -1;
            return NO;
        } else {
            NSLog(@"[Bridge] Removing stale socket");
            [[NSFileManager defaultManager] removeItemAtPath:socketPath error:nil];
        }
    }
    
    // Create UDS socket
    _serverSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
        NSLog(@"[Bridge] Failed to create UDS socket: %s", strerror(errno));
        flock(_lockfd, LOCK_UN);
        close(_lockfd);
        return NO;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, [socketPath UTF8String], sizeof(addr.sun_path) - 1);
    
    if (bind(_serverSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        NSLog(@"[Bridge] Failed to bind socket at %@: %s", socketPath, strerror(errno));
        close(_serverSocket);
        _serverSocket = -1;
        flock(_lockfd, LOCK_UN);
        close(_lockfd);
        return NO;
    }
    
    // Set permissions to 0600 (owner-only read/write)
    if (chmod([socketPath UTF8String], 0600) != 0) {
        NSLog(@"[Bridge] Warning: Failed to chmod socket: %s", strerror(errno));
    }
    
    // Verify permissions with stat
    struct stat st;
    if (stat([socketPath UTF8String], &st) == 0) {
        mode_t mode = st.st_mode & 0777;
        if (mode == 0600) {
            NSLog(@"[Bridge] Socket permissions verified: 0600 ✓");
        } else {
            NSLog(@"[Bridge] Warning: Socket permissions are 0%o (expected 0600)", mode);
        }
    } else {
        NSLog(@"[Bridge] Warning: Failed to stat socket: %s", strerror(errno));
    }
    
    if (listen(_serverSocket, 5) < 0) {
        NSLog(@"[Bridge] Failed to listen on socket: %s", strerror(errno));
        close(_serverSocket);
        _serverSocket = -1;
        flock(_lockfd, LOCK_UN);
        close(_lockfd);
        return NO;
    }
    
    NSLog(@"[Bridge] UDS server listening at %@", socketPath);
    
    // Accept connections in background
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [self acceptConnections];
    });
    
    return YES;
}

- (void)acceptConnections {
    while (_serverSocket >= 0) {
        int clientSocket = accept(_serverSocket, NULL, NULL);
        if (clientSocket < 0) {
            break;
        }
        
        NSLog(@"[Bridge] Client connected");
        
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_clientHandle = [[NSFileHandle alloc] initWithFileDescriptor:clientSocket closeOnDealloc:YES];
            [self startReadingFromClient];
            
            // Send ready event
            [self sendMessage:@{
                @"type": @"ready",
                @"version": @1,
                @"timestamp": @([[NSDate date] timeIntervalSince1970])
            }];
        });
    }
}

- (void)startReadingFromClient {
    if (!_clientHandle) return;
    
    __weak typeof(self) weakSelf = self;
    dispatch_async(_readQueue, ^{
        [weakSelf readLoop];
    });
}

- (void)readLoop {
    while (_clientHandle) {
        @try {
            // Read 4-byte length prefix (big-endian)
            NSData *lenData = [_clientHandle readDataOfLength:4];
            if (lenData.length != 4) {
                NSLog(@"[Bridge] Client disconnected (length read failed)");
                dispatch_async(dispatch_get_main_queue(), ^{
                    self->_clientHandle = nil;
                });
                break;
            }
            
            uint32_t msgLen = 0;
            [lenData getBytes:&msgLen length:4];
            msgLen = ntohl(msgLen);
            
            // Enforce size limit
            if (msgLen > kMaxMessageSize) {
                NSLog(@"[Bridge] Message too large: %u bytes (max %u)", msgLen, kMaxMessageSize);
                dispatch_async(dispatch_get_main_queue(), ^{
                    self->_clientHandle = nil;
                });
                break;
            }
            
            // Read payload
            NSData *payloadData = [_clientHandle readDataOfLength:msgLen];
            if (payloadData.length != msgLen) {
                NSLog(@"[Bridge] Client disconnected (payload read failed)");
                dispatch_async(dispatch_get_main_queue(), ^{
                    self->_clientHandle = nil;
                });
                break;
            }
            
            // Parse JSON
            NSError *error = nil;
            NSDictionary *message = [NSJSONSerialization JSONObjectWithData:payloadData options:0 error:&error];
            if (message && self.messageHandler) {
                __weak typeof(self) weakSelf = self;
                dispatch_async(dispatch_get_main_queue(), ^{
                    __strong typeof(self) strongSelf = weakSelf;
                    if (strongSelf && strongSelf.messageHandler) {
                        strongSelf.messageHandler(message);
                    }
                });
            }
        } @catch (NSException *exception) {
            NSLog(@"[Bridge] Read error: %@", exception);
            dispatch_async(dispatch_get_main_queue(), ^{
                self->_clientHandle = nil;
            });
            break;
        }
    }
}

- (void)sendMessage:(NSDictionary *)message {
    if (!_clientHandle) return;
    
    NSError *error = nil;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:message options:0 error:&error];
    if (!jsonData) {
        NSLog(@"[Bridge] Failed to serialize message: %@", error);
        return;
    }
    
    // Length-prefixed: 4-byte big-endian + payload
    uint32_t len = htonl((uint32_t)jsonData.length);
    NSMutableData *packet = [NSMutableData dataWithBytes:&len length:4];
    [packet appendData:jsonData];
    
    @try {
        [_clientHandle writeData:packet];
    } @catch (NSException *exception) {
        NSLog(@"[Bridge] Write error: %@", exception);
        _clientHandle = nil;
    }
}

- (void)stop {
    if (_serverSocket >= 0) {
        close(_serverSocket);
        _serverSocket = -1;
    }
    if (_lockfd >= 0) {
        flock(_lockfd, LOCK_UN);
        close(_lockfd);
        _lockfd = -1;
    }
    _clientHandle = nil;
}

- (void)dealloc {
    [self stop];
}

@end

// MARK: - BLE Manager Implementation

@implementation HeartSyncBLEManager {
    CBCentralManager *_centralManager;
    NSMutableDictionary<NSString *, CBPeripheral *> *_discoveredPeripherals;
    CBPeripheral *_connectedPeripheral;
    BOOL _isScanning;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _discoveredPeripherals = [NSMutableDictionary dictionary];
        _centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
    }
    return self;
}

- (NSString *)permissionState {
    CBManagerAuthorization auth = [CBManager authorization];
    switch (auth) {
        case CBManagerAuthorizationNotDetermined:
            return @"unknown";
        case CBManagerAuthorizationRestricted:
        case CBManagerAuthorizationDenied:
            return @"denied";
        case CBManagerAuthorizationAllowedAlways:
            return @"authorized";
    }
    return @"unknown";
}

- (BOOL)isConnected {
    return _connectedPeripheral != nil;
}

- (void)startScanning {
    if (_centralManager.state == CBManagerStatePoweredOn && !_isScanning) {
        _isScanning = YES;
        [_discoveredPeripherals removeAllObjects];
        [_centralManager scanForPeripheralsWithServices:@[HR_SERVICE_UUID] options:nil];
        NSLog(@"[Bridge] Started BLE scan");
    }
}

- (void)stopScanning {
    if (_isScanning) {
        _isScanning = NO;
        [_centralManager stopScan];
        NSLog(@"[Bridge] Stopped BLE scan");
    }
}

- (void)connectToDevice:(NSString *)deviceId {
    CBPeripheral *peripheral = _discoveredPeripherals[deviceId];
    if (peripheral) {
        [_centralManager connectPeripheral:peripheral options:nil];
        NSLog(@"[Bridge] Connecting to %@", deviceId);
    } else {
        NSLog(@"[Bridge] Device not found: %@", deviceId);
        if (self.errorOccurred) {
            self.errorOccurred(@"device_not_found", [NSString stringWithFormat:@"Device %@ not in scan cache", deviceId]);
        }
    }
}

- (void)disconnect {
    if (_connectedPeripheral) {
        [_centralManager cancelPeripheralConnection:_connectedPeripheral];
        _connectedPeripheral = nil;
    }
}

// MARK: - CBCentralManagerDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    NSString *state = [self permissionState];
    NSLog(@"[Bridge] BLE state changed: %@", state);
    
    if (self.permissionStateChanged) {
        self.permissionStateChanged(state);
    }
}

- (void)centralManager:(CBCentralManager *)central
 didDiscoverPeripheral:(CBPeripheral *)peripheral
     advertisementData:(NSDictionary<NSString *,id> *)advertisementData
                  RSSI:(NSNumber *)RSSI {
    NSString *deviceId = peripheral.identifier.UUIDString;
    _discoveredPeripherals[deviceId] = peripheral;
    
    NSString *name = advertisementData[CBAdvertisementDataLocalNameKey] ?: peripheral.name ?: @"Unknown";
    
    if (self.deviceFound) {
        self.deviceFound(@{
            @"id": deviceId,
            @"rssi": RSSI,
            @"name": name
        });
    }
}

- (void)centralManager:(CBCentralManager *)central
  didConnectPeripheral:(CBPeripheral *)peripheral {
    NSLog(@"[Bridge] Connected to %@", peripheral.identifier.UUIDString);
    _connectedPeripheral = peripheral;
    peripheral.delegate = self;
    [peripheral discoverServices:@[HR_SERVICE_UUID]];
    
    if (self.connectionChanged) {
        self.connectionChanged(YES, peripheral.identifier.UUIDString);
    }
}

- (void)centralManager:(CBCentralManager *)central
didDisconnectPeripheral:(CBPeripheral *)peripheral
                 error:(NSError *)error {
    NSLog(@"[Bridge] Disconnected from %@", peripheral.identifier.UUIDString);
    
    if (_connectedPeripheral == peripheral) {
        _connectedPeripheral = nil;
    }
    
    if (self.connectionChanged) {
        self.connectionChanged(NO, peripheral.identifier.UUIDString);
    }
}

// MARK: - CBPeripheralDelegate

- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error {
    if (error) {
        NSLog(@"[Bridge] Service discovery error: %@", error);
        return;
    }
    
    for (CBService *service in peripheral.services) {
        if ([service.UUID isEqual:HR_SERVICE_UUID]) {
            [peripheral discoverCharacteristics:@[HR_MEASUREMENT_CHAR_UUID] forService:service];
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral
didDiscoverCharacteristicsForService:(CBService *)service
             error:(NSError *)error {
    if (error) {
        NSLog(@"[Bridge] Characteristic discovery error: %@", error);
        return;
    }
    
    for (CBCharacteristic *characteristic in service.characteristics) {
        if ([characteristic.UUID isEqual:HR_MEASUREMENT_CHAR_UUID]) {
            [peripheral setNotifyValue:YES forCharacteristic:characteristic];
            NSLog(@"[Bridge] Subscribed to HR notifications");
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral
didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
             error:(NSError *)error {
    if (error || ![characteristic.UUID isEqual:HR_MEASUREMENT_CHAR_UUID]) {
        return;
    }
    
    NSData *data = characteristic.value;
    if (data.length < 2) return;
    
    const uint8_t *bytes = data.bytes;
    uint8_t flags = bytes[0];
    
    // Parse BPM
    NSInteger bpm = 0;
    if (flags & 0x01) {
        // 16-bit BPM
        if (data.length >= 3) {
            bpm = bytes[1] | (bytes[2] << 8);
        }
    } else {
        // 8-bit BPM
        bpm = bytes[1];
    }
    
    if (bpm > 0 && self.heartRateDataReceived) {
        self.heartRateDataReceived(bpm, [[NSDate date] timeIntervalSince1970], nil);
    }
}

@end

// MARK: - Bridge Application

@implementation HeartSyncBridgeApp

- (instancetype)init {
    self = [super init];
    if (self) {
        _protocolVersion = 1;
    }
    return self;
}

- (void)start {
    NSLog(@"[Bridge] HeartSync Bridge starting...");
    
    // Initialize components
    _server = [[HeartSyncUDSServer alloc] init];
    _bleManager = [[HeartSyncBLEManager alloc] init];
    
    // Wire up BLE events → server messages
    __weak typeof(self) weakSelf = self;
    
    _bleManager.permissionStateChanged = ^(NSString *state) {
        [weakSelf.server sendMessage:@{
            @"type": @"permission",
            @"state": state
        }];
    };
    
    _bleManager.deviceFound = ^(NSDictionary *device) {
        [weakSelf.server sendMessage:@{
            @"type": @"device_found",
            @"device": device
        }];
    };
    
    _bleManager.connectionChanged = ^(BOOL connected, NSString *deviceId) {
        [weakSelf.server sendMessage:@{
            @"type": connected ? @"connected" : @"disconnected",
            @"device_id": deviceId
        }];
    };
    
    _bleManager.heartRateDataReceived = ^(NSInteger bpm, NSTimeInterval ts, NSArray *rr) {
        NSMutableDictionary *msg = [@{
            @"type": @"hr_data",
            @"bpm": @(bpm),
            @"timestamp": @(ts)
        } mutableCopy];
        if (rr) msg[@"rr"] = rr;
        [weakSelf.server sendMessage:msg];
    };
    
    _bleManager.errorOccurred = ^(NSString *code, NSString *message) {
        [weakSelf.server sendMessage:@{
            @"type": @"error",
            @"code": code,
            @"message": message
        }];
    };
    
    // Handle incoming commands
    _server.messageHandler = ^(NSDictionary *message) {
        [weakSelf handleCommand:message];
    };
    
    // Start UDS server
    NSString *socketPath = HSSocketPath();
    if (![_server startServerAtSocketPath:socketPath]) {
        NSLog(@"[Bridge] Failed to start server");
        [[NSApplication sharedApplication] terminate:nil];
        return;
    }
    
    // Start heartbeat timer
    _heartbeatTimer = [NSTimer scheduledTimerWithTimeInterval:kHeartbeatInterval
                                                      repeats:YES
                                                        block:^(NSTimer *timer) {
        [weakSelf sendHeartbeat];
    }];
    
    NSLog(@"[Bridge] HeartSync Bridge ready");
}

- (void)handleCommand:(NSDictionary *)command {
    NSString *type = command[@"type"];
    
    if ([type isEqualToString:@"handshake"]) {
        // Respond with version info
        [_server sendMessage:@{
            @"type": @"ready",
            @"version": @(_protocolVersion)
        }];
    }
    else if ([type isEqualToString:@"scan"]) {
        BOOL on = [command[@"on"] boolValue];
        if (on) {
            [_bleManager startScanning];
        } else {
            [_bleManager stopScanning];
        }
    }
    else if ([type isEqualToString:@"connect"]) {
        NSString *deviceId = command[@"id"];
        if (deviceId) {
            [_bleManager connectToDevice:deviceId];
        }
    }
    else if ([type isEqualToString:@"disconnect"]) {
        [_bleManager disconnect];
    }
    else if ([type isEqualToString:@"status"]) {
        [_server sendMessage:@{
            @"type": @"status",
            @"permission": [_bleManager permissionState],
            @"connected": @([_bleManager isConnected])
        }];
    }
}

- (void)sendHeartbeat {
    [_server sendMessage:@{
        @"type": @"bridge_heartbeat",
        @"timestamp": @([[NSDate date] timeIntervalSince1970])
    }];
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    NSLog(@"[Bridge] Shutting down");
    [_heartbeatTimer invalidate];
    [_server stop];
    [_bleManager disconnect];
}

@end

// MARK: - Main Entry Point

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        HeartSyncBridgeApp *bridge = [[HeartSyncBridgeApp alloc] init];
        [app setDelegate:bridge];
        
        [bridge start];
        [app run];
    }
    return 0;
}
