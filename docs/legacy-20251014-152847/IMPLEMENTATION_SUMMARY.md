# HeartSync UDS Bridge - Implementation Summary

**Status**: ⚠️ **REVIEW ONLY - DO NOT COMMIT**  
**Date**: October 14, 2025

---

## Executive Summary

This patch transforms HeartSync from a TCP-based helper to a Unix Domain Socket (UDS) based headless bridge, eliminating firewall prompts and improving user experience to true zero-friction.

**Approval Required Before Implementation**

---

## Critical Changes Required

### 1. CMakeLists.txt
**Change**: Rename `HEARTSYNC_USE_HELPER` → `HEARTSYNC_USE_BRIDGE`  
**Impact**: All conditional compilation directives must update  
**Files Affected**: 7 files with `#ifdef` statements

### 2. Socket Transport Layer
**Current**: TCP on `localhost:51721`  
**New**: UDS at `~/Library/Application Support/HeartSync/bridge.sock`  
**Rationale**: UDS never triggers macOS firewall (no network stack involved)

**Code Change** (HeartSyncBLEClient.cpp):
```cpp
// OLD:
socket->connect("127.0.0.1", 51721, 2000);

// NEW:
#include <sys/un.h>
#include <sys/socket.h>

int sock = socket(AF_UNIX, SOCK_STREAM, 0);
struct sockaddr_un addr;
addr.sun_family = AF_UNIX;
std::string sockPath = juce::File::getSpecialLocation(
    juce::File::userApplicationDataDirectory
).getChildFile("HeartSync/bridge.sock").getFullPathName().toStdString();
strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path)-1);

if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    // Launch bridge and retry
}
```

### 3. Message Framing
**Current**: Newline-delimited JSON (vulnerable to truncation)  
**New**: Length-prefixed (4-byte big-endian) + JSON

**Implementation**:
```cpp
// Send
uint32_t length = htonl(json.length());
write(sock, &length, 4);
write(sock, json.data(), json.length());

// Receive
uint32_t length;
read(sock, &length, 4);
length = ntohl(length);
if (length > 65536) { disconnect(); return; } // DoS prevention
std::vector<char> buffer(length);
read(sock, buffer.data(), length);
```

### 4. Bridge Headless Mode
**Current Info.plist**:
```xml
<!-- MISSING: LSBackgroundOnly -->
<key>NSBluetoothAlwaysUsageDescription</key>
<string>HeartSync uses Bluetooth...</string>
```

**New Info.plist**:
```xml
<key>LSBackgroundOnly</key>
<true/>
<key>LSUIElement</key>
<true/>  <!-- Extra safety -->
<key>NSBluetoothAlwaysUsageDescription</key>
<string>HeartSync Bridge needs Bluetooth to communicate with your heart rate sensor.</string>
<key>CFBundleIdentifier</key>
<string>com.quantumbioaudio.heartsync.bridge</string>
```

**Remove from Bridge code**:
- All `NSStatusItem` / menu bar code
- All `NSWindow` creation
- All `AppKit.framework` UI components

### 5. Permission State Machine

**New API** (HeartSyncBLEClient.h):
```cpp
enum class PermissionState {
    Unknown,
    Requesting,
    Denied,
    Authorized
};

std::function<void(PermissionState)> onPermissionChanged;
std::atomic<PermissionState> permissionState{PermissionState::Unknown};
```

**Bridge Implementation**:
```objc
- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    NSString *state;
    switch (central.state) {
        case CBManagerStateUnknown:
            state = @"unknown";
            break;
        case CBManagerStatePoweredOff:
        case CBManagerStateUnsupported:
        case CBManagerStateUnauthorized:
            state = @"denied";
            break;
        case CBManagerStatePoweredOn:
            state = @"authorized";
            break;
    }
    [self sendEvent:@{@"event": @"permission", @"state": state}];
}
```

**Plugin UI Response**:
```cpp
bleClient.onPermissionChanged = [this](PermissionState state) {
    switch (state) {
        case PermissionState::Unknown:
        case PermissionState::Requesting:
            statusLabel.setText("Checking Bluetooth permissions...", dontSendNotification);
            scanButton.setEnabled(false);
            break;
        case PermissionState::Denied:
            statusLabel.setText("⚠ Bluetooth access denied. Click to open System Settings", dontSendNotification);
            statusLabel.setColour(Label::textColourId, Colours::red);
            scanButton.setEnabled(false);
            showPermissionBanner();
            break;
        case PermissionState::Authorized:
            statusLabel.setText("Ready to scan for devices", dontSendNotification);
            scanButton.setEnabled(true);
            hidePermissionBanner();
            break;
    }
};
```

### 6. DeviceInfo Normalization

**Current Inconsistency**:
- `HeartSyncBLEClient::DeviceInfo` has `{id, rssi}`
- `PluginEditor` expects `{name, uuid}`
- Bridge sends `{id, rssi, name}` sometimes

**Normalized Structure**:
```cpp
struct DeviceInfo {
    juce::String id;      // Bluetooth MAC address (XX:XX:XX:XX:XX:XX)
    int rssi;             // Signal strength (-100 to 0)
    juce::String name;    // Advertised name (may be empty)
    
    juce::String getDisplayName() const {
        if (name.isEmpty())
            return id.substring(0, 17); // Just MAC
        return name + " (" + id.substring(0, 8) + "...)";
    }
};
```

**Protocol**:
```json
{
  "event": "device_found",
  "id": "AA:BB:CC:DD:EE:FF",
  "rssi": -65,
  "name": "Polar H10 12345678"
}
```

### 7. Auto-Launch Implementation

**Current**: Manual launch required  
**New**: Plugin auto-launches with backoff

```cpp
void HeartSyncBLEClient::launchBridge() {
    #if JUCE_MAC
    auto bridgePath = juce::File::getSpecialLocation(
        juce::File::userApplicationDirectory
    ).getChildFile("HeartSync Bridge.app");
    
    if (!bridgePath.exists()) {
        // Try /Applications
        bridgePath = juce::File("/Applications/HeartSync Bridge.app");
    }
    
    if (!bridgePath.exists()) {
        if (errorCallback)
            errorCallback("Bridge app not found at ~/Applications or /Applications");
        return;
    }
    
    // Launch without activation (headless)
    @autoreleasepool {
        NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
        NSURL *url = [NSURL fileURLWithPath:bridgePath.getFullPathName().toNSString()];
        NSError *error = nil;
        
        BOOL success = [workspace launchApplicationAtURL:url
                                                 options:NSWorkspaceLaunchWithoutActivation
                                           configuration:@{}
                                                   error:&error];
        if (!success) {
            NSLog(@"Failed to launch bridge: %@", error);
        }
    }
    #endif
}

void HeartSyncBLEClient::connectWithRetry() {
    int attempt = 0;
    int delays[] = {100, 200, 400, 800, 1600, 3200, 5000}; // ms, capped at 5s
    
    while (attempt < 10 && !connected) {
        if (tryConnect()) {
            connected = true;
            return;
        }
        
        if (attempt == 2) {
            // After 2 failures, try launching bridge
            launchBridge();
        }
        
        int delay = delays[std::min(attempt, 6)];
        int jitter = (rand() % 20) - 10; // ±10% jitter
        delay += (delay * jitter) / 100;
        
        Thread::sleep(delay);
        attempt++;
    }
    
    if (!connected && errorCallback) {
        errorCallback("Failed to connect to Bridge after 10 attempts. Check ~/Library/Logs/HeartSync/Bridge.log");
    }
}
```

### 8. Heartbeat Mechanism

**Purpose**: Detect silent bridge crashes  
**Interval**: 2 seconds  
**Timeout**: 5 seconds (miss 2-3 beats)

**Bridge sends**:
```json
{
  "event": "bridge_heartbeat",
  "ts": 1729000000.123
}
```

**Client watches**:
```cpp
std::atomic<double> lastHeartbeat{0.0};

void processMessage(const juce::String& message) {
    auto json = juce::JSON::parse(message);
    auto event = json["event"].toString();
    
    if (event == "bridge_heartbeat") {
        lastHeartbeat = Time::getMillisecondCounterHiRes();
    }
    // ... handle other events
}

void checkHeartbeat() {
    double now = Time::getMillisecondCounterHiRes();
    if (now - lastHeartbeat > 5000.0) {
        // Bridge is dead/hung
        DBG("Bridge heartbeat timeout - reconnecting");
        disconnect();
        connectWithRetry();
    }
}
```

### 9. Thread Safety (Critical!)

**Device List** must be thread-safe (accessed from UI and network threads):

```cpp
// HeartSyncBLEClient.h
class HeartSyncBLEClient {
    juce::CriticalSection deviceListLock;
    juce::Array<DeviceInfo> cachedDevices;
    
public:
    // Thread-safe copy
    juce::Array<DeviceInfo> getDevicesSnapshot() {
        juce::ScopedLock lock(deviceListLock);
        return cachedDevices; // Returns copy
    }
    
    void setDeviceListCallback(DeviceListCallback callback) {
        juce::ScopedLock lock(callbackLock);
        deviceListCallback = std::move(callback);
    }
};

// Usage in PluginEditor
void scanForDevices() {
    bleClient.startScan();
    
    // Poll for updates (or use callback)
    Timer::callAfterDelay(100, [this]() {
        auto devices = bleClient.getDevicesSnapshot();
        updateDeviceList(devices);
    });
}
```

### 10. Logging Infrastructure

**Bridge Logs** (Objective-C):
```objc
@implementation HSLogger

+ (void)setupLogging {
    NSString *logDir = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Logs/HeartSync"];
    [[NSFileManager defaultManager] createDirectoryAtPath:logDir
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];
    
    NSString *logPath = [logDir stringByAppendingPathComponent:@"Bridge.log"];
    freopen([logPath UTF8String], "a", stderr);
    freopen([logPath UTF8String], "a", stdout);
}

+ (void)log:(NSString *)level message:(NSString *)msg {
    NSString *timestamp = [[NSDate date] descriptionWithLocale:nil];
    fprintf(stderr, "[%s] [%s] %s\n",
            [timestamp UTF8String],
            [level UTF8String],
            [msg UTF8String]);
}

@end
```

**Plugin Logs** (JUCE):
```cpp
class HeartSyncLogger : public juce::Logger {
    void logMessage(const juce::String& message) override {
        auto timestamp = Time::getCurrentTime().toString(true, true, true, true);
        std::cout << "[" << timestamp << "] [HeartSync Plugin] " << message << std::endl;
    }
};

// In plugin constructor
Logger::setCurrentLogger(new HeartSyncLogger());
```

---

## Files to Create/Modify

### New Files (7)
1. `HeartSync/BridgeApp/HeartSyncBridge.h` (interface definitions)
2. `HeartSync/BridgeApp/HeartSyncBridge.m` (full implementation, ~800 lines)
3. `HeartSync/BridgeApp/Info.plist` (LSBackgroundOnly, TCC keys)
4. `packaging/macos/com.heartsync.bridge.plist` (launchd config)
5. `QUICKSTART.md` (build/test instructions)
6. `RISK_LOG.md` (edge cases/mitigations)
7. `CHANGELOG.md` (what/why)

### Modified Files (6)
1. `HeartSync/CMakeLists.txt` (rename HELPER→BRIDGE, update paths)
2. `HeartSync/Source/Core/HeartSyncBLEClient.h` (UDS, permission API)
3. `HeartSync/Source/Core/HeartSyncBLEClient.cpp` (UDS transport, state machine)
4. `HeartSync/Source/PluginProcessor.h` (permission callbacks)
5. `HeartSync/Source/PluginProcessor.cpp` (handle permission events)
6. `HeartSync/Source/PluginEditor.cpp` (permission banner/spinner)

### Deleted Files (3)
1. `HeartSync/HelperApp/HeartSyncHelper.h` (replaced by Bridge)
2. `HeartSync/HelperApp/HeartSyncHelper.m` (replaced by Bridge)
3. `HeartSync/HelperApp/Info.plist` (replaced by Bridge version)

---

## Risk Assessment

| Risk Level | Count | Description |
|------------|-------|-------------|
| 🔴 HIGH | 2 | UDS socket permissions, Bridge launch failure |
| 🟡 MEDIUM | 5 | Race conditions, heartbeat timeout, permission denial, device flooding, migration from TCP |
| 🟢 LOW | 4 | Multiple Bridge instances, log rotation, launchd timing, macOS upgrade |

**Mitigation Strategy**: Each HIGH risk has fallback (manual launch, error UI guidance)

---

## Testing Verification Checklist

### Before Declaring "Done"

- [ ] `nm -gU` shows NO CoreBluetooth in plugin binary
- [ ] `otool -L` shows CoreBluetooth ONLY in Bridge binary
- [ ] UDS socket created with 0600 permissions
- [ ] No firewall prompt when loading plugin
- [ ] Bridge runs headless (no Dock/menu bar icon)
- [ ] First-run shows permission spinner → OS prompt → authorized
- [ ] Permission denial shows banner with "Open Settings" button
- [ ] Auto-launch works (Bridge starts within 5s)
- [ ] Reconnect works (kill Bridge → plugin reconnects)
- [ ] Device scan returns results within 5s
- [ ] HR data streams continuously
- [ ] DAW quit doesn't crash (clean shutdown)
- [ ] CPU < 2% idle, < 5% scanning
- [ ] Memory < 50MB
- [ ] Latency < 5ms (socket round-trip)
- [ ] Unit tests pass (message framing, backoff, thread safety)
- [ ] Manual tests pass (3 DAWs: Ableton, Logic, Reaper)

---

## Implementation Estimate

**Complexity**: High (architectural change, IPC rewrite)  
**Risk**: Medium-High (requires careful testing)  
**Estimated Effort**: 16-24 hours

### Breakdown
- UDS transport layer: 4-6 hours
- Bridge headless refactor: 4-6 hours
- Permission state machine: 2-3 hours
- Auto-launch + retry logic: 2-3 hours
- Thread safety + locking: 2 hours
- Testing + debugging: 4-6 hours
- Documentation: 2 hours

---

## Approval Required

**Before Proceeding**:
- [ ] Technical review (architecture, security, performance)
- [ ] Product review (UX flow, error states)
- [ ] Timeline approval (16-24 hours)
- [ ] Testing plan approval (manual + automated)

**Approved By**: ________________  
**Date**: ________________

**Proceed with Implementation**: YES / NO

