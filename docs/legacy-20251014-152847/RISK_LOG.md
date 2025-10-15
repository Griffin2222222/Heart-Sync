# HeartSync UDS Bridge - Risk Log

**Last Updated**: October 14, 2025  
**Review Cycle**: Before each release

---

## Risk Matrix

| ID | Risk Description | Probability | Impact | Severity | Mitigation | Owner |
|----|-----------------|-------------|--------|----------|------------|-------|
| R1 | UDS socket file permissions incorrect | Medium | High | 🔴 HIGH | Explicit `chmod 0600` after creation + verify on startup | Dev |
| R2 | Bridge fails to launch from plugin | Low | High | 🔴 HIGH | Retry 3× with 1s delay + show actionable error UI | Dev |
| R3 | Multiple Bridge instances race on socket | Low | Medium | 🟡 MEDIUM | Use `flock()` on lockfile; first wins, others exit gracefully | Dev |
| R4 | App Support folder doesn't exist | Low | High | 🟡 MEDIUM | Create `~/Library/Application Support/HeartSync/` in Bridge startup with error handling | Dev |
| R5 | NSWorkspace launch API fails silently | Low | High | 🟡 MEDIUM | Check return value + NSError; fallback to `open -a` command | Dev |
| R6 | launchd starts Bridge before plugin ready | Low | Low | 🟢 LOW | Normal behavior; plugin connects when ready (design intent) | N/A |
| R7 | User deletes Bridge.app | Low | Medium | 🟡 MEDIUM | Plugin shows error: "Bridge not found. Please reinstall." with download link | Product |
| R8 | macOS upgrade breaks TCC database | Low | High | 🟡 MEDIUM | Document: User must re-grant BT permission after OS major update | Docs |
| R9 | BLE device floods with 1000s events/sec | Low | Medium | 🟡 MEDIUM | Rate-limit incoming events to 100/sec; drop excess with warning log | Dev |
| R10 | Malicious client sends 1GB message | Very Low | High | 🟡 MEDIUM | Hard-cap at 64KB in length prefix check; disconnect attacker | Dev |
| R11 | Bridge OOM from device list growth | Very Low | Medium | 🟢 LOW | Limit device cache to 500 entries; evict oldest by RSSI | Dev |
| R12 | Socket file deleted while Bridge running | Very Low | High | 🟡 MEDIUM | Detect write errors; recreate socket + log event | Dev |
| R13 | Unicode in device names breaks JSON | Low | Medium | 🟡 MEDIUM | Use JSON serializer with proper UTF-8 handling; validate before send | Dev |
| R14 | Bluetooth disabled during scan | Medium | Low | 🟢 LOW | Emit `error` event; UI shows "Bluetooth is turned off" banner | Product |
| R15 | Device out of range during connect | High | Low | 🟢 LOW | Timeout after 30s; emit `disconnected` with reason="timeout" | Dev |
| R16 | Plugin crashes DAW on load | Low | Critical | 🔴 HIGH | Wrap all Bridge comms in try-catch; isolate in separate thread | Dev |
| R17 | Thread deadlock in device list access | Low | High | 🟡 MEDIUM | Use `juce::ScopedLock` consistently; audit all `CriticalSection` usage | Dev |
| R18 | Heartbeat timer drifts over time | Low | Low | 🟢 LOW | Use `NSTimer` with tolerance=0; verify with unit test | Dev |
| R19 | Socket buffer overflow from backlog | Low | Medium | 🟡 MEDIUM | Set `SO_RCVBUF` to 64KB; drop old messages if client slow | Dev |
| R20 | Permission state race during startup | Medium | Medium | 🟡 MEDIUM | Cache last state; emit event on every state change (idempotent) | Dev |

**Total Risks**: 20  
**High Severity**: 3 (15%)  
**Medium Severity**: 11 (55%)  
**Low Severity**: 6 (30%)

---

## Detailed Risk Analysis

### R1: UDS Socket File Permissions Incorrect (HIGH)

**Description**: Socket created with default 0644 allowing other users to connect.

**Attack Vector**: Local user could spam Bridge or eavesdrop on HR data.

**Mitigation**:
```objc
// In Bridge startup
int sock = socket(AF_UNIX, SOCK_STREAM, 0);
// ... bind() ...
chmod([sockPath UTF8String], 0600); // Owner read+write only

// Verify on startup
struct stat st;
stat([sockPath UTF8String], &st);
if ((st.st_mode & 0777) != 0600) {
    NSLog(@"SECURITY: Socket permissions wrong! Expected 0600, got %o", st.st_mode & 0777);
    exit(1);
}
```

**Test**:
```bash
ls -la ~/Library/Application\ Support/HeartSync/bridge.sock
# Must show: srw------- (not srw-rw-rw-)
```

---

### R2: Bridge Fails to Launch from Plugin (HIGH)

**Description**: NSWorkspace API fails due to missing app, permissions, or Gatekeeper.

**Scenarios**:
1. Bridge.app not in ~/Applications or /Applications
2. Gatekeeper blocks unsigned app
3. NSWorkspace returns NO with NSError

**Mitigation**:
```cpp
void HeartSyncBLEClient::launchBridgeWithRetry() {
    std::vector<juce::File> searchPaths = {
        juce::File("~/Applications/HeartSync Bridge.app"),
        juce::File("/Applications/HeartSync Bridge.app")
    };
    
    for (const auto& path : searchPaths) {
        if (path.exists()) {
            for (int attempt = 0; attempt < 3; ++attempt) {
                if (launchBridgeAt(path)) {
                    DBG("Bridge launched successfully");
                    return;
                }
                Thread::sleep(1000); // Wait 1s between retries
            }
        }
    }
    
    // All attempts failed - show error UI
    if (errorCallback) {
        errorCallback("Bridge not found or failed to launch.\n\n"
                      "Please ensure HeartSync Bridge.app is installed at:\n"
                      "~/Applications/ or /Applications/\n\n"
                      "Download: https://heartsync.app/download");
    }
}
```

**UI Response**:
```cpp
// PluginEditor shows modal dialog
AlertWindow::showMessageBoxAsync(
    AlertWindow::WarningIcon,
    "HeartSync Bridge Required",
    "The HeartSync Bridge app could not be found or failed to start.\n\n"
    "Please install it from: https://heartsync.app/download\n\n"
    "Or manually launch: ~/Applications/HeartSync Bridge.app",
    "Download",
    this,
    juce::ModalCallbackFunction::create([](int result) {
        if (result == 1) {
            URL("https://heartsync.app/download").launchInDefaultBrowser();
        }
    })
);
```

---

### R3: Multiple Bridge Instances Race on Socket (MEDIUM)

**Description**: User accidentally launches Bridge twice; both try to bind to same socket.

**Impact**: Second instance fails with "Address already in use"; first continues normally.

**Mitigation** (Defense in depth):
```objc
// Use lockfile to ensure single instance
- (BOOL)acquireLockfile {
    NSString *lockPath = [NSHomeDirectory() stringByAppendingPathComponent:
                          @"Library/Application Support/HeartSync/bridge.lock"];
    
    int fd = open([lockPath UTF8String], O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        NSLog(@"Failed to create lockfile");
        return NO;
    }
    
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        NSLog(@"Another Bridge instance is already running (lockfile held)");
        close(fd);
        exit(0); // Exit gracefully
    }
    
    // Keep fd open until process exits (flock auto-releases)
    self.lockfileFd = fd;
    return YES;
}
```

**Alternative**: Check for existing socket and test if alive:
```objc
if ([[NSFileManager defaultManager] fileExistsAtPath:sockPath]) {
    // Try connecting to existing socket
    int testSock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connect(testSock, ...) == 0) {
        NSLog(@"Bridge already running and responsive - exiting");
        close(testSock);
        exit(0);
    }
    // Socket exists but dead - clean it up
    unlink([sockPath UTF8String]);
}
```

---

### R4: App Support Folder Doesn't Exist (MEDIUM)

**Description**: First run on new Mac; `~/Library/Application Support/HeartSync/` missing.

**Impact**: Socket creation fails with ENOENT.

**Mitigation**:
```objc
- (NSString *)ensureAppSupportDirectory {
    NSString *appSupport = [NSHomeDirectory() stringByAppendingPathComponent:
                            @"Library/Application Support/HeartSync"];
    
    NSError *error = nil;
    [[NSFileManager defaultManager] createDirectoryAtPath:appSupport
                              withIntermediateDirectories:YES
                                               attributes:@{NSFilePosixPermissions: @0700}
                                                    error:&error];
    if (error) {
        NSLog(@"FATAL: Cannot create App Support directory: %@", error);
        // Show alert before exiting
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"HeartSync Bridge Startup Error";
        alert.informativeText = [NSString stringWithFormat:
            @"Failed to create directory:\n%@\n\nError: %@",
            appSupport, error.localizedDescription];
        [alert runModal];
        exit(1);
    }
    
    return appSupport;
}
```

---

### R10: Malicious Client Sends 1GB Message (MEDIUM)

**Description**: Attacker sends crafted length prefix claiming 1GB payload.

**Impact**: Bridge allocates huge buffer → OOM → crash.

**Mitigation**:
```objc
#define MAX_MESSAGE_SIZE (64 * 1024) // 64KB

- (void)handleIncomingData:(NSData *)data {
    if (data.length < 4) return;
    
    uint32_t length;
    [data getBytes:&length length:4];
    length = ntohl(length);
    
    if (length > MAX_MESSAGE_SIZE) {
        NSLog(@"SECURITY: Rejecting oversized message: %u bytes (max %u)",
              length, MAX_MESSAGE_SIZE);
        // Disconnect malicious client
        [self.clientHandle closeFile];
        self.clientHandle = nil;
        return;
    }
    
    // ... proceed with safe read ...
}
```

**Test**:
```python
# Send malicious 1GB claim
import socket, struct
sock = socket.socket(socket.AF_UNIX)
sock.connect("/Users/.../bridge.sock")
sock.sendall(struct.pack('!I', 1024*1024*1024))  # 1GB claim
# Bridge should disconnect immediately, not allocate
```

---

### R16: Plugin Crashes DAW on Load (HIGH - CRITICAL)

**Description**: Uncaught exception in Bridge communication crashes host DAW.

**Impact**: User loses work; DAW blacklists plugin; reputation damage.

**Mitigation** (Multiple layers):

**Layer 1: Exception Safety**
```cpp
// Wrap ALL Bridge calls
void HeartSyncBLEClient::run() {
    try {
        while (!threadShouldExit()) {
            // ... socket operations ...
        }
    } catch (const std::exception& e) {
        DBG("Bridge thread exception: " << e.what());
        // Don't propagate - log and reconnect
        connected = false;
        Thread::sleep(2000);
    } catch (...) {
        DBG("Bridge thread unknown exception");
        connected = false;
        Thread::sleep(2000);
    }
}
```

**Layer 2: Callback Isolation**
```cpp
void HeartSyncBLEClient::invokeCallback(std::function<void()> callback) {
    if (!callback) return;
    
    try {
        juce::MessageManager::callAsync([callback]() {
            try {
                callback();
            } catch (const std::exception& e) {
                DBG("Callback exception: " << e.what());
            } catch (...) {
                DBG("Callback unknown exception");
            }
        });
    } catch (...) {
        DBG("callAsync failed");
    }
}
```

**Layer 3: Processor Isolation**
```cpp
void HeartSyncProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midi) {
    try {
        // Normal processing
        hrProcessor.process();
        // ... midi generation ...
    } catch (const std::exception& e) {
        // Log but don't crash DAW
        Logger::writeToLog("HeartSync processBlock exception: " + String(e.what()));
        // Clear outputs to prevent undefined behavior
        buffer.clear();
        midi.clear();
    }
}
```

**Test**:
```cpp
// Unit test: Simulate exception in callback
TEST_CASE("Exception in callback doesn't crash") {
    HeartSyncBLEClient client;
    
    client.onHrData = [](int bpm, double ts) {
        throw std::runtime_error("Test exception");
    };
    
    // Trigger callback
    client.testInjectHrData(72, 0.0);
    
    // Client should still be functional
    REQUIRE(client.isConnected());
}
```

---

### R17: Thread Deadlock in Device List Access (MEDIUM)

**Description**: Network thread holds `deviceListLock`, tries to call UI callback which waits on MessageManager, which is blocked on another lock.

**Classic Deadlock Scenario**:
```
Thread A (Network):  deviceListLock → [tries MessageManager]
Thread B (UI):       MessageManager → [tries deviceListLock]
```

**Mitigation**:
```cpp
// RULE: Never hold lock while calling out to unknown code

// BAD (can deadlock):
void processDeviceFound(DeviceInfo device) {
    ScopedLock lock(deviceListLock);
    cachedDevices.add(device);
    
    if (deviceListCallback) {
        deviceListCallback(cachedDevices); // DANGEROUS: calls out while locked
    }
}

// GOOD (copy data, release lock, then callback):
void processDeviceFound(DeviceInfo device) {
    Array<DeviceInfo> snapshot;
    
    {
        ScopedLock lock(deviceListLock);
        cachedDevices.add(device);
        snapshot = cachedDevices; // Copy
    } // Lock released here
    
    if (deviceListCallback) {
        deviceListCallback(snapshot); // Safe: no lock held
    }
}
```

**Static Analysis**:
```cpp
// Use [[clang::guarded_by()]] attribute (Clang only)
class HeartSyncBLEClient {
    CriticalSection deviceListLock;
    Array<DeviceInfo> cachedDevices [[clang::guarded_by(deviceListLock)]];
    
    // Compiler will warn if accessed without lock
};
```

**Runtime Detection**:
```bash
# Build with Thread Sanitizer
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=thread"
# Run tests - TSAN will detect deadlocks
```

---

### R20: Permission State Race During Startup (MEDIUM)

**Description**: Plugin UI checks permission state before Bridge has sent initial state.

**Timeline**:
```
T+0ms:   Plugin starts, bleClient.connect()
T+100ms: UI reads permissionState → shows "Unknown"
T+200ms: Bridge connects, sends "authorized" event
T+300ms: UI updates to "Ready"
```

**Issue**: 100ms window shows wrong state.

**Mitigation** (Cache & Idempotent Updates):
```cpp
// Client maintains last known state
std::atomic<PermissionState> lastKnownPermission{PermissionState::Unknown};

void HeartSyncBLEClient::processMessage(const String& message) {
    auto json = JSON::parse(message);
    if (json["event"] == "permission") {
        auto stateStr = json["state"].toString();
        auto newState = parsePermissionState(stateStr);
        
        // Always update, even if same (idempotent)
        lastKnownPermission = newState;
        
        if (onPermissionChanged) {
            // UI can call this multiple times safely
            onPermissionChanged(newState);
        }
    }
}

// UI updates idempotently
void PluginEditor::updatePermissionUI(PermissionState state) {
    // No guards - just set new state
    switch (state) {
        case PermissionState::Authorized:
            scanButton.setEnabled(true);
            permissionBanner.setVisible(false);
            break;
        // ... other states ...
    }
    // Multiple calls with same state are no-op
}
```

---

## Edge Case Scenarios

### Scenario 1: User Upgrades macOS Major Version

**What Happens**:
1. macOS 15 → 16 upgrade resets TCC database
2. Bridge loses Bluetooth permission
3. Plugin loads, shows "Checking permissions..."
4. Bridge emits `permission` event with `state="unknown"`
5. User clicks device → macOS shows system permission prompt
6. User grants → Bridge emits `permission` event with `state="authorized"`

**Expected**: Graceful recovery  
**Test**: Simulate with `tccutil reset BluetoothAlways`

---

### Scenario 2: Power User Runs Multiple DAW Instances

**Setup**:
- Ableton Live open with HeartSync plugin
- Logic Pro open with HeartSync plugin (different project)
- Both try to use same Bridge

**What Happens**:
1. First plugin connects to Bridge → succeeds
2. Second plugin connects to Bridge → succeeds (UDS supports multiple clients with SO_ACCEPTCONN)
3. Both plugins send `scan` command
4. Bridge broadcasts `device_found` events to **both clients**
5. Plugin A connects to device → Bridge sends `connected` to **both clients**
6. Plugin B tries to connect → Bridge sends `error` (device busy)

**Expected**: Graceful sharing with one active connection  
**Improvement**: Add client IDs to protocol for better multiplexing

---

### Scenario 3: User's Cat Walks on Keyboard During Critical Moment

**Setup**: User is testing, accidentally types gibberish into Terminal connected to socket.

**What Happens**:
```bash
$ cat > /dev/null < ~/Library/Application\ Support/HeartSync/bridge.sock
{invalid json garbage!!!}
```

**Bridge Receives**: Malformed JSON  
**Bridge Response**:
```objc
- (void)handleMessage:(NSDictionary *)msg {
    @try {
        // Process command
    } @catch (NSException *e) {
        NSLog(@"Invalid message from client: %@", e);
        [self sendError:@"protocol_error" message:@"Malformed JSON"];
        // Don't disconnect - client might recover
    }
}
```

**Expected**: Log error, send error event, continue serving

---

## Security Audit Checklist

- [ ] Socket permissions verified (0600)
- [ ] Message size limit enforced (64KB)
- [ ] No SQL injection vectors (no database)
- [ ] No command injection vectors (no shell calls)
- [ ] No buffer overflows (use NSData, std::vector)
- [ ] No integer overflows (check length before allocation)
- [ ] No race conditions in permission checks
- [ ] No sensitive data logged (MAC addresses ok, HR data ok)
- [ ] No network exposure (UDS only, localhost)
- [ ] No privilege escalation vectors
- [ ] Code signed (prevents tampering)
- [ ] Sandboxed (inherits App Sandbox on macOS 10.15+)

---

## Monitoring & Alerting (Future)

For production deployment, consider:

1. **Crash Reporting**: Integrate Sentry/Crashlytics
2. **Analytics**: Count launch failures, permission denials
3. **Performance**: Track P50/P95/P99 latencies
4. **Errors**: Alert on error rate > 1% of sessions

**Example Integration**:
```objc
@import Sentry;

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [SentrySDK startWithOptions:@{
        @"dsn": @"https://...@sentry.io/...",
        @"environment": @"production",
        @"debug": NO
    }];
}

- (void)handleFatalError:(NSError *)error {
    [SentrySDK captureError:error];
    // ... show UI ...
}
```

---

## Review History

| Date | Reviewer | Changes | Approved |
|------|----------|---------|----------|
| 2025-10-14 | Agent | Initial draft | ⏳ Pending |
| | | | |

**Next Review**: Before v1.1.0 release

