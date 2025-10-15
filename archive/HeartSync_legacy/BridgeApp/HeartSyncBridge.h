//
//  HeartSyncBridge.h
//  HeartSync Bridge - Headless BLE Worker
//
//  Created: October 14, 2025
//  License: Proprietary
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <CoreBluetooth/CoreBluetooth.h>

NS_ASSUME_NONNULL_BEGIN

// ============================================================================
// MARK: - Unix Domain Socket Server
// ============================================================================

/**
 * @brief UDS server for plugin IPC
 * 
 * Listens on ~/Library/Application Support/HeartSync/bridge.sock
 * Uses length-prefixed JSON messages (4-byte big-endian length + payload)
 * Max message size: 64KB to prevent DoS
 */
@interface HeartSyncUDSServer : NSObject

@property (nonatomic, copy, nullable) void (^messageHandler)(NSDictionary *message);
@property (nonatomic, strong, nullable) NSFileHandle *clientHandle;

- (BOOL)startServerAtSocketPath:(NSString *)socketPath;
- (void)sendMessage:(NSDictionary *)message;
- (void)stop;

@end

// ============================================================================
// MARK: - BLE Manager
// ============================================================================

/**
 * @brief CoreBluetooth wrapper for heart rate monitoring
 * 
 * Scans for Heart Rate Service (0x180D)
 * Connects to single device at a time
 * Emits events for device discovery, HR data, state changes
 */
@interface HeartSyncBLEManager : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>

@property (nonatomic, copy, nullable) void (^permissionStateChanged)(NSString *state);
@property (nonatomic, copy, nullable) void (^deviceFound)(NSDictionary *device);
@property (nonatomic, copy, nullable) void (^connectionChanged)(BOOL connected, NSString * _Nullable deviceId);
@property (nonatomic, copy, nullable) void (^heartRateDataReceived)(NSInteger bpm, NSTimeInterval timestamp, NSArray<NSNumber *> * _Nullable rrIntervals);
@property (nonatomic, copy, nullable) void (^errorOccurred)(NSString *code, NSString *message);

- (void)startScanning;
- (void)stopScanning;
- (void)connectToDevice:(NSString *)deviceId;
- (void)disconnect;
- (NSString *)permissionState;
- (BOOL)isConnected;

@end

// ============================================================================
// MARK: - Bridge Application
// ============================================================================

/**
 * @brief Headless background app
 * 
 * No UI, no Dock icon, no menu bar (LSBackgroundOnly=YES)
 * Manages UDS server and BLE connection
 * Sends heartbeat every 2 seconds
 */
@interface HeartSyncBridgeApp : NSObject <NSApplicationDelegate>

@property (nonatomic, strong) HeartSyncUDSServer *server;
@property (nonatomic, strong) HeartSyncBLEManager *bleManager;
@property (nonatomic, strong, nullable) NSTimer *heartbeatTimer;
@property (nonatomic, assign) NSInteger protocolVersion;

- (void)start;

@end

NS_ASSUME_NONNULL_END
