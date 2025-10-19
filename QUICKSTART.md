# HeartSync UDS Bridge - Quick Start Guide

**For Developers** | **Status**: Review Only - Do Not Commit

---

## Prerequisites

```bash
# Check versions
cmake --version      # Need 3.22+
xcodebuild -version  # Need Xcode 14+ (C++17, macOS 12+ SDK)
sw_vers              # Need macOS 12.0+
```

---

## Clean Build

```bash
cd /Users/gmc/Documents/GitHub/Heart-Sync/HeartSync

# 1. Clean previous builda
rm -rf build
git checkout ble-helper-refactor  # Or create new branch for UDS work

# 2. Create build directory
mkdir build && cd build

# 3. Configure with Bridge enabled
cmake .. \
  -DHEARTSYNC_USE_BRIDGE=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror" \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

# 4. Build (will take 2-5 minutes)
cmake --build . --config Release -j$(sysctl -n hw.ncpu)

# 5. Verify build succeeded
ls -lh "HeartSync Bridge.app/Contents/MacOS/HeartSync Bridge"
ls -lh HeartSync_artefacts/AU/HeartSync.component/Contents/MacOS/HeartSync
```

---

## Installation

```bash
# From build directory (HeartSync/build/)

# 1. Install Bridge (headless background app)
cp -R "HeartSync Bridge.app" ~/Applications/
chmod 755 ~/Applications/"HeartSync Bridge.app"/Contents/MacOS/*

# 2. Plugin auto-installs during build, but verify:
ls -la ~/Library/Audio/Plug-Ins/Components/HeartSync.component
ls -la ~/Library/Audio/Plug-Ins/VST3/HeartSync.vst3  # If VST3 built

# 3. Code sign for macOS Gatekeeper (development only)
codesign --force --deep --sign - \
  ~/Applications/"HeartSync Bridge.app"
codesign --force --deep --sign - \
  ~/Library/Audio/Plug-Ins/Components/HeartSync.component

# 4. Optional: Install launchd plist for auto-start
cp ../packaging/macos/com.heartsync.bridge.plist ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/com.heartsync.bridge.plist
```

---

## Verification

### 1. Check CoreBluetooth Isolation

```bash
# Plugin must have NO CoreBluetooth
nm -gU ~/Library/Audio/Plug-Ins/Components/HeartSync.component/Contents/MacOS/HeartSync | \
  grep -i bluetooth
# Expected output: (nothing)

# Bridge must HAVE CoreBluetooth
otool -L ~/Applications/"HeartSync Bridge.app"/Contents/MacOS/"HeartSync Bridge" | \
  grep CoreBluetooth
# Expected output: /System/Library/Frameworks/CoreBluetooth.framework/...
```

### 2. Check UDS Socket

```bash
# Launch Bridge manually for testing
open ~/Applications/"HeartSync Bridge.app"

# Wait 2 seconds, then check socket
ls -la ~/Library/Application\ Support/HeartSync/
# Expected output: srw------- ... bridge.sock (permissions 0600)

# Verify Bridge is running headless (no Dock/menu bar icon)
ps aux | grep "HeartSync Bridge" | grep -v grep
# Expected output: Process running, no visible UI
```

### 3. Test Socket Communication

```bash
# Send handshake to Bridge (length-prefixed JSON)
# This is manual - normally the plugin does this
printf '\x00\x00\x00\x2F{"type":"handshake","version":1,"client":"Test"}' | \
  nc -U ~/Library/Application\ Support/HeartSync/bridge.sock | xxd

# Expected output: You'll see length prefix + JSON response
# Example: {"event":"ready","version":1,"bridge":"HeartSync Bridge"}
```

### 4. Check Logs

```bash
# Bridge logs
tail -f ~/Library/Logs/HeartSync/Bridge.log

# Plugin logs (when running in DAW)
# Check Xcode Console or DAW's log window
```

---

## Testing in DAW

### Ableton Live 12

```bash
# 1. Ensure Bridge is running
ps aux | grep "HeartSync Bridge"

# 2. Launch Ableton
open /Applications/Ableton\ Live\ 12.app

# 3. Create new MIDI track
# 4. Insert HeartSync AU plugin
#    - Should load WITHOUT crash (critical success criterion)
#    - UI should show either:
#      a) "Checking Bluetooth permissions..." (spinner)
#      b) "Ready to scan for devices" (if already authorized)
#      c) "⚠ Bluetooth access denied" (banner with Settings button)

# 5. Click "Scan for Devices"
#    - Should see devices within 5 seconds
#    - Click device to connect
#    - Should see heart rate data flowing

# 6. Close plugin, reopen
#    - Should reconnect automatically to Bridge
#    - Previous device should still be available

# 7. Kill Bridge while plugin is open
killall "HeartSync Bridge"
#    - Plugin should show "Reconnecting..." for ~5s
#    - Bridge should auto-restart (if launchd installed)
#    - Plugin should reconnect automatically
```

### Logic Pro / Reaper

Same flow as Ableton. Test in at least 2 different DAWs to ensure compatibility.

---

## Troubleshooting

### Bridge Won't Start

**Symptom**: Plugin shows "Failed to connect to Bridge"

**Solutions**:
```bash
# 1. Check if Bridge app exists
ls -la ~/Applications/"HeartSync Bridge.app"

# 2. Try manual launch
open ~/Applications/"HeartSync Bridge.app"

# 3. Check for errors in system log
log show --predicate 'process == "HeartSync Bridge"' --last 5m

# 4. Check crash reports
ls -la ~/Library/Logs/DiagnosticReports/ | grep HeartSync

# 5. Verify code signature
codesign -vvv ~/Applications/"HeartSync Bridge.app"
```

### Socket Permission Errors

**Symptom**: "Permission denied" in logs

**Solutions**:
```bash
# 1. Check socket permissions
ls -la ~/Library/Application\ Support/HeartSync/bridge.sock
# Should be: srw------- (0600)

# 2. Fix if wrong
chmod 600 ~/Library/Application\ Support/HeartSync/bridge.sock

# 3. Check directory ownership
ls -la ~/Library/Application\ Support/ | grep HeartSync
# Should be owned by your user

# 4. Recreate directory if needed
rm -rf ~/Library/Application\ Support/HeartSync
mkdir -p ~/Library/Application\ Support/HeartSync
```

### No Bluetooth Permission Prompt

**Symptom**: Plugin stays stuck on "Checking permissions..."

**Solutions**:
```bash
# 1. Check current TCC status
sqlite3 ~/Library/Application\ Support/com.apple.TCC/TCC.db \
  "SELECT service, client, auth_value FROM access WHERE service = 'kTCCServiceBluetoothAlways';"

# 2. Reset TCC permissions (forces new prompt)
tccutil reset BluetoothAlways com.quantumbioaudio.heartsync.bridge

# 3. Kill Bridge and restart
killall "HeartSync Bridge"
open ~/Applications/"HeartSync Bridge.app"

# 4. Check Info.plist has TCC key
plutil -p ~/Applications/"HeartSync Bridge.app"/Contents/Info.plist | \
  grep NSBluetoothAlwaysUsageDescription
```

### Firewall Still Prompts

**Symptom**: macOS Firewall asks about HeartSync

**This Should Never Happen with UDS! Investigate:**
```bash
# 1. Verify we're using UDS, not TCP
lsof -p $(pgrep "HeartSync Bridge") | grep LISTEN
# Should show: UNIX ... bridge.sock
# Should NOT show: TCP ... *:51721

# 2. Check for old TCP-based builds
grep -r "51721" ~/Library/Audio/Plug-Ins/Components/HeartSync.component/

# 3. Clean rebuild if TCP code found
cd HeartSync/build
cmake --build . --target clean
cmake ..
cmake --build . --config Release
```

### High CPU Usage

**Symptom**: Bridge uses >10% CPU

**Check**:
```bash
# 1. Monitor CPU
top -pid $(pgrep "HeartSync Bridge") -s 1

# 2. Check for scan loop bug
# Bridge should stop scanning after 30 seconds
tail -f ~/Library/Logs/HeartSync/Bridge.log | grep -i scan

# 3. Check for message flood
# Should be <100 events/second
tail -f ~/Library/Logs/HeartSync/Bridge.log | grep -c "Received message"

# 4. Profile with Instruments (advanced)
xcrun xctrace record --template 'Time Profiler' \
  --target $(pgrep "HeartSync Bridge") \
  --output BridgeProfile.trace
```

---

## Debugging

### Enable Debug Logging

```bash
# Terminal 1: Set debug env var and launch Bridge
export HEARTSYNC_DEBUG=1
~/Applications/"HeartSync Bridge.app"/Contents/MacOS/"HeartSync Bridge"

# Terminal 2: Watch logs in realtime
tail -f ~/Library/Logs/HeartSync/Bridge.log
```

### Attach Debugger

```bash
# 1. Build Debug version
cd HeartSync/build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# 2. Install Debug build
cp -R "HeartSync Bridge.app" ~/Applications/

# 3. Launch in lldb
lldb ~/Applications/"HeartSync Bridge.app"/Contents/MacOS/"HeartSync Bridge"
(lldb) run

# 4. Set breakpoints
(lldb) b HeartSyncBLEManager::startScanning
(lldb) b HeartSyncUDSServer::sendMessage
```

### Test Protocol Manually

```bash
# Create test JSON files
cat > handshake.json <<'EOF'
{"type":"handshake","version":1,"client":"Test"}
EOF

cat > scan.json <<'EOF'
{"type":"scan","on":true}
EOF

# Send via python script (handles length prefix)
python3 - <<'PYEOF'
import socket, json, struct, os

sock_path = os.path.expanduser("~/Library/Application Support/HeartSync/bridge.sock")
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(sock_path)

def send_msg(obj):
    msg = json.dumps(obj).encode('utf-8')
    length = struct.pack('!I', len(msg))
    sock.sendall(length + msg)
    
def recv_msg():
    length_bytes = sock.recv(4)
    length = struct.unpack('!I', length_bytes)[0]
    data = sock.recv(length)
    return json.loads(data)

send_msg({"type": "handshake", "version": 1, "client": "Python Test"})
print("Response:", recv_msg())

send_msg({"type": "status"})
print("Status:", recv_msg())
PYEOF
```

---

## Performance Benchmarks

### CPU Usage
```bash
# Idle (should be <2%)
top -pid $(pgrep "HeartSync Bridge") -l 1 | tail -1

# During scan (should be <5%)
# (trigger scan via plugin, monitor for 10 seconds)
```

### Memory Usage
```bash
# Should be <50MB
ps -o rss= -p $(pgrep "HeartSync Bridge") | awk '{print $1/1024 " MB"}'
```

### Socket Latency
```bash
# Round-trip should be <5ms
time python3 - <<'PYEOF'
import socket, json, struct, os, time
sock_path = os.path.expanduser("~/Library/Application Support/HeartSync/bridge.sock")
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect(sock_path)

def send_msg(obj):
    msg = json.dumps(obj).encode('utf-8')
    length = struct.pack('!I', len(msg))
    sock.sendall(length + msg)

def recv_msg():
    length_bytes = sock.recv(4)
    length = struct.unpack('!I', length_bytes)[0]
    data = sock.recv(length)
    return json.loads(data)

start = time.time()
send_msg({"type": "status"})
recv_msg()
print(f"Latency: {(time.time() - start)*1000:.2f}ms")
PYEOF
```

---

## Uninstall

```bash
# 1. Stop Bridge if running
killall "HeartSync Bridge" 2>/dev/null

# 2. Unload launchd plist (if installed)
launchctl unload ~/Library/LaunchAgents/com.heartsync.bridge.plist 2>/dev/null
rm ~/Library/LaunchAgents/com.heartsync.bridge.plist

# 3. Remove Bridge app
rm -rf ~/Applications/"HeartSync Bridge.app"

# 4. Remove plugin
rm -rf ~/Library/Audio/Plug-Ins/Components/HeartSync.component
rm -rf ~/Library/Audio/Plug-Ins/VST3/HeartSync.vst3

# 5. Remove data directory (optional - keeps settings)
rm -rf ~/Library/Application\ Support/HeartSync

# 6. Remove logs (optional)
rm -rf ~/Library/Logs/HeartSync

# 7. Reset TCC permissions (optional)
tccutil reset BluetoothAlways com.quantumbioaudio.heartsync.bridge
```

---

## Next Steps

After successful local testing:

1. **CI/CD**: Set up automated builds + tests
2. **Code Signing**: Obtain Developer ID certificate for distribution
3. **Notarization**: Submit to Apple for Gatekeeper approval
4. **Installer**: Create `.pkg` with postinstall scripts
5. **Documentation**: User guide, troubleshooting, FAQ
6. **Distribution**: Upload to website, send to beta testers

---

**Questions?** Check `IMPLEMENTATION_SUMMARY.md` for detailed technical docs.  
**Issues?** Check `RISK_LOG.md` for known edge cases.  
**Changes?** Check `CHANGELOG.md` for what was modified.

