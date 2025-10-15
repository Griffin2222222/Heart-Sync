#include "HeartSyncBLEClient.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

static constexpr uint32_t MAX_MESSAGE_SIZE = 65536; // 64KB
static constexpr double HEARTBEAT_TIMEOUT = 5.0; // seconds

HeartSyncBLEClient::HeartSyncBLEClient()
    : juce::Thread("HeartSyncBLEClient")
{
    startThread();
}

HeartSyncBLEClient::~HeartSyncBLEClient()
{
    disconnect();
    stopThread(5000);
}

void HeartSyncBLEClient::connectToBridge()
{
    shouldReconnect = true;
    notify();
}

void HeartSyncBLEClient::disconnect()
{
    shouldReconnect = false;
    connected = false;
    
    if (socketFd >= 0)
    {
        ::close(socketFd);
        socketFd = -1;
    }
}

juce::Array<HeartSyncBLEClient::DeviceInfo> HeartSyncBLEClient::getDevicesSnapshot()
{
    const juce::ScopedLock lock(deviceListLock);
    return devices;
}

void HeartSyncBLEClient::startScan(bool enable)
{
    juce::var command = juce::var(new juce::DynamicObject());
    command.getDynamicObject()->setProperty("type", "scan");
    command.getDynamicObject()->setProperty("on", enable);
    sendCommand(command);
}

void HeartSyncBLEClient::connectToDevice(const juce::String& deviceId)
{
    // Check if already connected to this device
    {
        const juce::ScopedLock lock(deviceStateLock);
        if (deviceConnected && currentDeviceId == deviceId)
        {
            DBG("Already connected to device: " << deviceId);
            return;
        }
        
        // If connected to different device, disconnect first
        if (deviceConnected && currentDeviceId != deviceId)
        {
            DBG("Switching devices - disconnecting from: " << currentDeviceId);
            disconnectDevice();
            juce::Thread::sleep(100); // Brief delay for disconnect to process
        }
    }
    
    juce::var command = juce::var(new juce::DynamicObject());
    command.getDynamicObject()->setProperty("type", "connect");
    command.getDynamicObject()->setProperty("id", deviceId);
    sendCommand(command);
}

void HeartSyncBLEClient::disconnectDevice()
{
    juce::var command = juce::var(new juce::DynamicObject());
    command.getDynamicObject()->setProperty("type", "disconnect");
    sendCommand(command);
}

void HeartSyncBLEClient::launchBridge()
{
    #if JUCE_MAC
    juce::String bridgePaths[] = {
        juce::File::getSpecialLocation(juce::File::userHomeDirectory)
            .getChildFile("Applications/HeartSync Bridge.app").getFullPathName(),
        "/Applications/HeartSync Bridge.app"
    };
    
    for (const auto& path : bridgePaths)
    {
        juce::File bridgeApp(path);
        if (bridgeApp.exists())
        {
            DBG("Launching Bridge: " << path);
            
            juce::ChildProcess process;
            juce::StringArray args;
            args.add("open");
            args.add("-a");
            args.add(path);
            args.add("--background");
            
            if (process.start(args))
            {
                DBG("Bridge launched successfully");
                return;
            }
        }
    }
    
    DBG("Failed to launch Bridge - not found");
    
    if (onError)
    {
        juce::MessageManager::callAsync([this]() {
            if (onError)
                onError("Bridge app not found. Please install to ~/Applications/");
        });
    }
    #endif
}

bool HeartSyncBLEClient::connectToSocket()
{
    auto socketPath = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("HeartSync/bridge.sock")
                        .getFullPathName();
    
    DBG("Connecting to UDS: " << socketPath);
    
    socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketFd < 0)
    {
        DBG("Failed to create socket");
        return false;
    }
    
    fcntl(socketFd, F_SETFL, O_NONBLOCK);
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.toRawUTF8(), sizeof(addr.sun_path) - 1);
    
    int result = ::connect(socketFd, (struct sockaddr*)&addr, sizeof(addr));
    
    if (result < 0 && errno != EINPROGRESS)
    {
        DBG("Failed to connect: " << strerror(errno));
        ::close(socketFd);
        socketFd = -1;
        return false;
    }
    
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(socketFd, &writefds);
    
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    
    result = select(socketFd + 1, nullptr, &writefds, nullptr, &timeout);
    
    if (result <= 0)
    {
        DBG("Connection timeout or error");
        ::close(socketFd);
        socketFd = -1;
        return false;
    }
    
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(socketFd, SOL_SOCKET, SO_ERROR, &error, &len);
    
    if (error != 0)
    {
        DBG("Connection failed: " << strerror(error));
        ::close(socketFd);
        socketFd = -1;
        return false;
    }
    
    fcntl(socketFd, F_SETFL, 0);
    
    DBG("Connected to Bridge");
    return true;
}

void HeartSyncBLEClient::sendCommand(const juce::var& command)
{
    if (!connected || socketFd < 0)
        return;
    
    try
    {
        juce::String jsonString = juce::JSON::toString(command, false);
        juce::MemoryOutputStream jsonData;
        jsonData.writeString(jsonString);
        
        const char* payload = static_cast<const char*>(jsonData.getData());
        size_t payloadSize = jsonData.getDataSize();
        
        if (payloadSize > MAX_MESSAGE_SIZE)
        {
            DBG("Message too large: " << payloadSize);
            return;
        }
        
        uint32_t length = htonl(static_cast<uint32_t>(payloadSize));
        ssize_t sent = ::send(socketFd, &length, 4, 0);
        
        if (sent != 4)
        {
            DBG("Failed to send length prefix");
            connected = false;
            return;
        }
        
        sent = ::send(socketFd, payload, payloadSize, 0);
        
        if (sent != static_cast<ssize_t>(payloadSize))
        {
            DBG("Failed to send payload");
            connected = false;
            return;
        }
        
        DBG("Sent: " << jsonString.substring(0, 80));
    }
    catch (...)
    {
        DBG("Exception sending command");
        connected = false;
    }
}

void HeartSyncBLEClient::run()
{
    while (!threadShouldExit())
    {
        if (shouldReconnect && !connected)
        {
            attemptReconnect();
        }
        
        if (connected && socketFd >= 0)
        {
            uint32_t lengthPrefix = 0;
            ssize_t bytesRead = ::recv(socketFd, &lengthPrefix, 4, 0);
            
            if (bytesRead != 4)
            {
                if (bytesRead == 0)
                    DBG("Bridge disconnected");
                else if (bytesRead < 0)
                    DBG("Read error: " << strerror(errno));
                
                connected = false;
                ::close(socketFd);
                socketFd = -1;
                continue;
            }
            
            uint32_t messageLength = ntohl(lengthPrefix);
            
            if (messageLength > MAX_MESSAGE_SIZE)
            {
                DBG("Message too large: " << messageLength << " bytes, dropping");
                connected = false;
                ::close(socketFd);
                socketFd = -1;
                continue;
            }
            
            juce::MemoryBlock messageData(messageLength);
            char* buffer = static_cast<char*>(messageData.getData());
            size_t totalRead = 0;
            
            while (totalRead < messageLength)
            {
                bytesRead = ::recv(socketFd, buffer + totalRead, messageLength - totalRead, 0);
                
                if (bytesRead <= 0)
                {
                    DBG("Failed to read complete message");
                    connected = false;
                    ::close(socketFd);
                    socketFd = -1;
                    break;
                }
                
                totalRead += bytesRead;
            }
            
            if (totalRead == messageLength)
            {
                juce::String jsonString = juce::String::fromUTF8(buffer, static_cast<int>(messageLength));
                juce::var parsed = juce::JSON::parse(jsonString);
                
                if (parsed.isObject())
                    processMessage(parsed);
            }
            
            checkHeartbeat();
        }
        else
        {
            wait(100);
        }
    }
}

void HeartSyncBLEClient::attemptReconnect()
{
    if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS)
    {
        DBG("Max reconnect attempts reached");
        shouldReconnect = false;
        return;
    }
    
    int baseDelay = 100 * (1 << reconnectAttempts);
    if (baseDelay > 5000)
        baseDelay = 5000;
    
    juce::Random random;
    double jitter = 0.9 + (random.nextDouble() * 0.2);
    int delay = static_cast<int>(baseDelay * jitter);
    
    DBG("Reconnect attempt " << (reconnectAttempts + 1) << " after " << delay << "ms");
    
    wait(delay);
    
    if (threadShouldExit())
        return;
    
    if (reconnectAttempts == 2)
    {
        DBG("Attempting to launch Bridge");
        launchBridge();
        wait(2000);
    }
    
    if (connectToSocket())
    {
        connected = true;
        reconnectAttempts = 0;
        lastHeartbeatTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        
        DBG("Reconnected successfully");
        
        juce::var handshake = juce::var(new juce::DynamicObject());
        handshake.getDynamicObject()->setProperty("type", "handshake");
        handshake.getDynamicObject()->setProperty("version", 1);
        sendCommand(handshake);
    }
    else
    {
        reconnectAttempts++;
    }
}

void HeartSyncBLEClient::checkHeartbeat()
{
    double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    
    if ((now - lastHeartbeatTime) > HEARTBEAT_TIMEOUT)
    {
        DBG("Heartbeat timeout - reconnecting");
        connected = false;
        
        if (socketFd >= 0)
        {
            ::close(socketFd);
            socketFd = -1;
        }
    }
}

void HeartSyncBLEClient::processMessage(const juce::var& message)
{
    if (!message.isObject())
        return;
    
    // Robust: accept both "type" and "event" keys
    juce::String typeKey = message.hasProperty("event") ? "event" : "type";
    juce::String type = message.getProperty(typeKey, "").toString();
    
    if (type == "bridge_heartbeat")
    {
        lastHeartbeatTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    }
    else if (type == "ready")
    {
        DBG("Bridge ready");
    }
    else if (type == "permission")
    {
        juce::String state = message.getProperty("state", "unknown").toString();
        currentPermissionState = state;
        
        if (onPermissionChanged)
        {
            juce::MessageManager::callAsync([this, state]() {
                if (onPermissionChanged)
                    onPermissionChanged(state);
            });
        }
    }
    else if (type == "device_found")
    {
        DeviceInfo device;
        device.id = message.getProperty("id", "").toString();
        device.rssi = message.getProperty("rssi", -100);
        device.name = message.getProperty("name", "Unknown").toString();
        
        {
            const juce::ScopedLock lock(deviceListLock);
            
            bool found = false;
            for (auto& d : devices)
            {
                if (d.id == device.id)
                {
                    d.rssi = device.rssi;
                    d.name = device.name;
                    found = true;
                    break;
                }
            }
            
            if (!found)
                devices.add(device);
        }
        
        if (onDeviceFound)
        {
            juce::MessageManager::callAsync([this, device]() {
                if (onDeviceFound)
                    onDeviceFound(device);
            });
        }
    }
    else if (type == "hr_data")
    {
        int bpm = message.getProperty("bpm", 0);
        // Robust: accept both "ts" and "timestamp"
        double timestamp = message.hasProperty("ts") 
            ? (double)message.getProperty("ts", 0.0)
            : (double)message.getProperty("timestamp", 0.0);
        
        if (onHrData)
        {
            juce::MessageManager::callAsync([this, bpm, timestamp]() {
                if (onHrData)
                    onHrData(bpm, timestamp);
            });
        }
        
        // Also trigger onHeartRate callback with RR intervals for UI updates
        if (onHeartRate)
        {
            const float bpmFloat = (float)bpm;
            juce::Array<float> rrIntervals;
            
            // Extract RR intervals if present
            if (message.hasProperty("rr"))
            {
                auto rrVar = message.getProperty("rr", juce::var());
                if (rrVar.isArray())
                {
                    if (auto* arr = rrVar.getArray())
                    {
                        for (const auto& v : *arr)
                        {
                            if (v.isInt() || v.isDouble() || v.isInt64())
                                rrIntervals.add((float)v);
                        }
                    }
                }
            }
            
            juce::MessageManager::callAsync([this, bpmFloat, rrIntervals]() {
                if (onHeartRate)
                    onHeartRate(bpmFloat, rrIntervals);
            });
        }
    }
    else if (type == "connected")
    {
        juce::String deviceId = message.getProperty("id", "").toString();
        
        {
            const juce::ScopedLock lock(deviceStateLock);
            deviceConnected = true;
            currentDeviceId = deviceId;
        }
        
        DBG("Connected to device: " << deviceId);
        
        if (onConnected)
        {
            juce::MessageManager::callAsync([this, deviceId]() {
                if (onConnected)
                    onConnected(deviceId);
            });
        }
    }
    else if (type == "disconnected")
    {
        juce::String reason = message.getProperty("reason", "unknown").toString();
        
        {
            const juce::ScopedLock lock(deviceStateLock);
            deviceConnected = false;
            currentDeviceId.clear();
        }
        
        DBG("Disconnected: " << reason);
        
        if (onDisconnected)
        {
            juce::MessageManager::callAsync([this, reason]() {
                if (onDisconnected)
                    onDisconnected(reason);
            });
        }
    }
    else if (type == "error")
    {
        juce::String errorMsg = message.getProperty("message", "Unknown error").toString();
        
        if (onError)
        {
            juce::MessageManager::callAsync([this, errorMsg]() {
                if (onError)
                    onError(errorMsg);
            });
        }
    }
}

#if JUCE_DEBUG
// ============================================================================
// Debug Injection Methods (JUCE_DEBUG only)
// ============================================================================

void HeartSyncBLEClient::__debugInjectPermission(const juce::String& state)
{
    DBG("[DEBUG] Injecting permission state: " << state);
    currentPermissionState = state;
    
    if (onPermissionChanged)
    {
        juce::MessageManager::callAsync([this, state]() {
            if (onPermissionChanged)
                onPermissionChanged(state);
        });
    }
}

void HeartSyncBLEClient::__debugInjectDevice(const juce::String& id, const juce::String& name, int rssi)
{
    DBG("[DEBUG] Injecting device: " << name << " (" << id << ") RSSI: " << rssi);
    
    DeviceInfo device;
    device.id = id;
    device.name = name;
    device.rssi = rssi;
    
    // Upsert into device cache (same logic as real device_found)
    {
        const juce::ScopedLock lock(deviceListLock);
        
        bool found = false;
        for (auto& d : devices)
        {
            if (d.id == device.id)
            {
                d.rssi = device.rssi;
                d.name = device.name;
                found = true;
                break;
            }
        }
        
        if (!found)
            devices.add(device);
    }
    
    // Fire callback
    if (onDeviceFound)
    {
        juce::MessageManager::callAsync([this, device]() {
            if (onDeviceFound)
                onDeviceFound(device);
        });
    }
}

void HeartSyncBLEClient::__debugInjectConnected(const juce::String& id)
{
    DBG("[DEBUG] Injecting connected event: " << id);
    
    {
        const juce::ScopedLock lock(deviceStateLock);
        deviceConnected = true;
        currentDeviceId = id;
    }
    
    if (onConnected)
    {
        juce::MessageManager::callAsync([this, id]() {
            if (onConnected)
                onConnected(id);
        });
    }
}

void HeartSyncBLEClient::__debugInjectDisconnected(const juce::String& reason)
{
    DBG("[DEBUG] Injecting disconnected event: " << reason);
    
    {
        const juce::ScopedLock lock(deviceStateLock);
        deviceConnected = false;
        currentDeviceId.clear();
    }
    
    if (onDisconnected)
    {
        juce::MessageManager::callAsync([this, reason]() {
            if (onDisconnected)
                onDisconnected(reason);
        });
    }
}

void HeartSyncBLEClient::__debugInjectHr(int bpm)
{
    DBG("[DEBUG] Injecting HR data: " << bpm << " BPM");
    
    double timestamp = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    
    if (onHrData)
    {
        juce::MessageManager::callAsync([this, bpm, timestamp]() {
            if (onHrData)
                onHrData(bpm, timestamp);
        });
    }
}
#endif // JUCE_DEBUG

