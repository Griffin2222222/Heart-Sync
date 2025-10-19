#include "HeartSyncBLEClient.h"

#if JUCE_MAC

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>

static constexpr uint32_t MAX_MESSAGE_SIZE = 65536; // 64KB
static constexpr double HEARTBEAT_TIMEOUT = 5.0;     // seconds

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
    reconnectAttempts = 0;
    lastLoggedFailureAttempt = -1;
    shouldReconnect = true;
        dispatchLog("Attempting to connect to HeartSync Bridge...");
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
    {
        const juce::ScopedLock lock(deviceStateLock);
        if (deviceConnected && currentDeviceId == deviceId)
            return;

        if (deviceConnected && currentDeviceId != deviceId)
        {
            disconnectDevice();
            juce::Thread::sleep(100);
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

juce::String HeartSyncBLEClient::getCurrentDeviceId() const
{
    const juce::ScopedLock lock(deviceStateLock);
    return currentDeviceId;
}

void HeartSyncBLEClient::launchBridge()
{
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
            juce::ChildProcess process;
            juce::StringArray args;
            args.add("open");
            args.add("-a");
            args.add(path);
            args.add("--background");

            if (process.start(args))
            {
                dispatchLog("Launch request sent for HeartSync Bridge.app at " + path);
                return;
            }
        }
    }

    dispatchLog("HeartSync Bridge.app not found; install to ~/Applications or /Applications.");
    if (onError)
    {
        juce::MessageManager::callAsync([this]() {
            if (onError)
                onError("Bridge app not found. Install HeartSync Bridge to ~/Applications.");
        });
    }
}

bool HeartSyncBLEClient::connectToSocket()
{
    juce::StringArray candidatePaths;
    const auto addCandidate = [&candidatePaths](const juce::String& path)
    {
        if (path.isNotEmpty())
            candidatePaths.addIfNotAlreadyThere(path);
    };

    const auto envSocket = juce::SystemStats::getEnvironmentVariable("HEARTSYNC_BRIDGE_SOCKET", {});
    if (envSocket.isNotEmpty())
        addCandidate(juce::File(envSocket).getFullPathName());

    const auto homeDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    const auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    const auto commonData = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);

    if (homeDir != juce::File())
    {
        const juce::StringArray homeSuffixes{
            "Library/Application Support/HeartSync/bridge.sock",
            "Library/Application Support/HeartSyncBridge/bridge.sock",
            "Library/Application Support/HeartSync Bridge/bridge.sock",
            "Library/Application Support/com.quantumbioaudio.heartsync.bridge/bridge.sock",
            "Library/Application Support/com.quantumbioaudio.HeartSyncBridge/bridge.sock",
            "Library/Application Support/com.quantumbio.heartsync.bridge/bridge.sock",
            "Library/Application Support/QuantumBioAudio/HeartSync/bridge.sock",
            "Library/HeartSync/bridge.sock"
        };

        for (const auto& suffix : homeSuffixes)
            addCandidate(homeDir.getChildFile(suffix).getFullPathName());

        auto containersDir = homeDir.getChildFile("Library/Containers");
        if (containersDir.isDirectory())
        {
            juce::Array<juce::File> containerDirs;
            containersDir.findChildFiles(containerDirs, juce::File::findDirectories, false);
            int guard = 0;
            for (const auto& container : containerDirs)
            {
                addCandidate(container.getChildFile("Data/Library/Application Support/HeartSync/bridge.sock").getFullPathName());
                addCandidate(container.getChildFile("Data/Library/Application Support/HeartSyncBridge/bridge.sock").getFullPathName());
                if (++guard >= 32)
                    break;
            }
        }
    }

    if (appData != juce::File())
    {
        addCandidate(appData.getChildFile("HeartSync/bridge.sock").getFullPathName());
        addCandidate(appData.getChildFile("HeartSyncBridge/bridge.sock").getFullPathName());
    }

    if (commonData != juce::File())
    {
        addCandidate(commonData.getChildFile("HeartSync/bridge.sock").getFullPathName());
        addCandidate(commonData.getChildFile("HeartSyncBridge/bridge.sock").getFullPathName());
    }

    juce::String lastError;

    for (const auto& socketPath : candidatePaths)
    {
        if (socketPath.isEmpty())
            continue;

        juce::File socketFile(socketPath);
        dispatchLog("Attempting bridge socket at " + socketPath + (socketFile.exists() ? " (exists)" : " (missing)"));

        socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socketFd < 0)
        {
            lastError = "socket(): " + juce::String(strerror(errno));
            continue;
        }

        fcntl(socketFd, F_SETFL, O_NONBLOCK);

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, socketPath.toRawUTF8(), sizeof(addr.sun_path) - 1);

        int result = ::connect(socketFd, (struct sockaddr*)&addr, sizeof(addr));

        if (result < 0 && errno != EINPROGRESS)
        {
            lastError = "connect(): " + juce::String(strerror(errno));
            ::close(socketFd);
            socketFd = -1;
            continue;
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
            lastError = (result == 0 ? "connect timeout" : "select(): " + juce::String(strerror(errno)));
            ::close(socketFd);
            socketFd = -1;
            continue;
        }

        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(socketFd, SOL_SOCKET, SO_ERROR, &error, &len);

        if (error != 0)
        {
            lastError = "socket error: " + juce::String(strerror(error));
            ::close(socketFd);
            socketFd = -1;
            continue;
        }

        fcntl(socketFd, F_SETFL, 0);
        dispatchLog("Bridge socket connected at " + socketPath);
        return true;
    }

    if (socketFd >= 0)
    {
        ::close(socketFd);
        socketFd = -1;
    }

    if (candidatePaths.isEmpty())
        dispatchLog("Bridge socket paths unavailable");
    else
        dispatchLog("Unable to connect to bridge socket: " + (lastError.isNotEmpty() ? lastError : juce::String("unknown error")));

    return false;
}

void HeartSyncBLEClient::dispatchLog(const juce::String& message)
{
    if (! onLog)
        return;

    auto callback = onLog;
    juce::MessageManager::callAsync([callback, message]() mutable {
        if (callback)
            callback(message);
    });
}

void HeartSyncBLEClient::sendCommand(const juce::var& command)
{
    if (!connected || socketFd < 0 || !command.isObject())
        return;

    const juce::String type = command.getProperty("type", juce::String()).toString();
    if (type.isNotEmpty())
    {
        juce::String detail;
        if (type == "scan")
            detail = command.getProperty("on", false) ? " (on)" : " (off)";
        else if (type == "connect")
            detail = " -> " + command.getProperty("id", juce::String()).toString();
        dispatchLog("Sending bridge command: " + type + detail);
    }

    const juce::String jsonString = juce::JSON::toString(command, false);
    juce::MemoryOutputStream jsonData;
    jsonData.write(jsonString.toRawUTF8(), (size_t)jsonString.getNumBytesAsUTF8());

    const char* payload = static_cast<const char*>(jsonData.getData());
    size_t payloadSize = jsonData.getDataSize();

    if (payloadSize > MAX_MESSAGE_SIZE)
        return;

    uint32_t length = htonl(static_cast<uint32_t>(payloadSize));
    ssize_t sent = ::send(socketFd, &length, 4, 0);

    if (sent != 4)
    {
        connected = false;
        return;
    }

    sent = ::send(socketFd, payload, payloadSize, 0);

    if (sent != static_cast<ssize_t>(payloadSize))
    {
        connected = false;
        return;
    }
}

void HeartSyncBLEClient::run()
{
    while (!threadShouldExit())
    {
        if (shouldReconnect && !connected)
            attemptReconnect();

        if (connected && socketFd >= 0)
        {
            uint32_t lengthPrefix = 0;
            ssize_t bytesRead = ::recv(socketFd, &lengthPrefix, 4, 0);

            if (bytesRead != 4)
            {
                connected = false;
                if (socketFd >= 0)
                {
                    ::close(socketFd);
                    socketFd = -1;
                }
                if (onBridgeDisconnected)
                {
                    juce::MessageManager::callAsync([this]() {
                        if (onBridgeDisconnected)
                            onBridgeDisconnected();
                    });
                }
                continue;
            }

            uint32_t messageLength = ntohl(lengthPrefix);
            if (messageLength > MAX_MESSAGE_SIZE)
            {
                connected = false;
                if (socketFd >= 0)
                {
                    ::close(socketFd);
                    socketFd = -1;
                }
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
                    connected = false;
                    if (socketFd >= 0)
                    {
                        ::close(socketFd);
                        socketFd = -1;
                    }
                    break;
                }
                totalRead += (size_t)bytesRead;
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
    int cappedAttempt = juce::jmin(reconnectAttempts, MAX_RECONNECT_ATTEMPTS);
    int baseDelay = 100 * (1 << cappedAttempt);
    if (baseDelay > 5000)
        baseDelay = 5000;

    juce::Random random;
    double jitter = 0.9 + (random.nextDouble() * 0.2);
    int delay = static_cast<int>(baseDelay * jitter);

    wait(delay);

    if (threadShouldExit())
        return;

    if (reconnectAttempts == 2)
    {
        dispatchLog("Bridge helper not responding, attempting to launch helper app...");
        launchBridge();
        wait(2000);
    }

    if (connectToSocket())
    {
        connected = true;
        reconnectAttempts = 0;
        lastLoggedFailureAttempt = -1;
        lastHeartbeatTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        dispatchLog("Bridge helper socket connected");

        if (onBridgeConnected)
        {
            juce::MessageManager::callAsync([this]() {
                if (onBridgeConnected)
                    onBridgeConnected();
            });
        }

        juce::var handshake = juce::var(new juce::DynamicObject());
        handshake.getDynamicObject()->setProperty("type", "handshake");
        handshake.getDynamicObject()->setProperty("version", 1);
        handshake.getDynamicObject()->setProperty("client", "HeartSync VST3");
        sendCommand(handshake);

        juce::var statusRequest = juce::var(new juce::DynamicObject());
        statusRequest.getDynamicObject()->setProperty("type", "status");
        sendCommand(statusRequest);
    }
    else
    {
        if (reconnectAttempts == 0)
            dispatchLog("Waiting for HeartSync Bridge helper socket...");

        reconnectAttempts++;
        if (reconnectAttempts % 5 == 0 && reconnectAttempts != lastLoggedFailureAttempt)
        {
            lastLoggedFailureAttempt = reconnectAttempts;
            dispatchLog("Still waiting for HeartSync Bridge helper (attempt " + juce::String(reconnectAttempts) + ")");
        }
    }
}

void HeartSyncBLEClient::checkHeartbeat()
{
    double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    if ((now - lastHeartbeatTime) > HEARTBEAT_TIMEOUT)
    {
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

    juce::String typeKey = message.hasProperty("event") ? "event" : "type";
    juce::String type = message.getProperty(typeKey, juce::String()).toString();

    if (type == "bridge_heartbeat")
    {
        lastHeartbeatTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    }
    else if (type == "ready")
    {
        // bridge ready notification only used for logging
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
        device.id = message.getProperty("id", juce::String()).toString();
        device.rssi = message.getProperty("rssi", -100);
        device.name = message.getProperty("name", "Unknown").toString();

        if (message.hasProperty("services"))
        {
            auto servicesVar = message.getProperty("services", juce::var());
            if (servicesVar.isArray())
            {
                if (auto* arr = servicesVar.getArray())
                {
                    for (const auto& entry : *arr)
                    {
                        if (entry.isString())
                            device.services.add(entry.toString());
                    }
                }
            }
        }

        juce::String servicesText;
        if (!device.services.isEmpty())
            servicesText = " services=" + device.services.joinIntoString(",");

        dispatchLog("Bridge: Device found - id: '" + device.id + "', name: '" + device.name + "', rssi: " + juce::String(device.rssi) + servicesText);

        {
            const juce::ScopedLock lock(deviceListLock);
            bool found = false;
            for (auto& existing : devices)
            {
                if (existing.id == device.id)
                {
                    existing.rssi = device.rssi;
                    existing.name = device.name;
                    existing.services = device.services;
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
        const float bpm = (float)message.getProperty("bpm", 0);
        juce::Array<float> rrIntervals;

        if (message.hasProperty("rr"))
        {
            auto rrVar = message.getProperty("rr", juce::var());
            if (rrVar.isArray())
            {
                if (auto* arr = rrVar.getArray())
                {
                    for (const auto& v : *arr)
                    {
                        if (v.isInt() || v.isInt64() || v.isDouble())
                            rrIntervals.add((float)v);
                    }
                }
            }
        }

        if (onHeartRate)
        {
            juce::MessageManager::callAsync([this, bpm, rrIntervals]() {
                if (onHeartRate)
                    onHeartRate(bpm, rrIntervals);
            });
        }
    }
    else if (type == "connected")
    {
        juce::String deviceId = message.getProperty("id", juce::String()).toString();

        {
            const juce::ScopedLock lock(deviceStateLock);
            deviceConnected = true;
            currentDeviceId = deviceId;
        }

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
// Debug helpers omitted for brevity but intact from original implementation
void HeartSyncBLEClient::__debugInjectPermission(const juce::String& state)
{
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
    DeviceInfo device{ id, rssi, name };
    {
        const juce::ScopedLock lock(deviceListLock);
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

void HeartSyncBLEClient::__debugInjectConnected(const juce::String& id)
{
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
    if (onHeartRate)
    {
        juce::MessageManager::callAsync([this, bpm]() {
            if (onHeartRate)
                onHeartRate((float)bpm, {});
        });
    }
}
#endif

#else

HeartSyncBLEClient::HeartSyncBLEClient() : juce::Thread("HeartSyncBLEClient") {}
HeartSyncBLEClient::~HeartSyncBLEClient() = default;
void HeartSyncBLEClient::connectToBridge() {}
void HeartSyncBLEClient::disconnect() {}
void HeartSyncBLEClient::launchBridge() {}
void HeartSyncBLEClient::startScan(bool) {}
void HeartSyncBLEClient::connectToDevice(const juce::String&) {}
void HeartSyncBLEClient::disconnectDevice() {}
juce::Array<HeartSyncBLEClient::DeviceInfo> HeartSyncBLEClient::getDevicesSnapshot() { return {}; }
juce::String HeartSyncBLEClient::getCurrentDeviceId() const { return {}; }
void HeartSyncBLEClient::run() {}
void HeartSyncBLEClient::sendCommand(const juce::var&) {}
void HeartSyncBLEClient::processMessage(const juce::var&) {}
bool HeartSyncBLEClient::connectToSocket() { return false; }
void HeartSyncBLEClient::attemptReconnect() {}
void HeartSyncBLEClient::checkHeartbeat() {}

#endif // JUCE_MAC
