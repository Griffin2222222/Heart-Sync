# HeartSync - Native VST3/AU Plugin

**Self-contained heart rate monitoring plugin with direct CoreBluetooth integration**

## Overview

HeartSync is a professional-grade VST3/AU plugin that connects directly to Bluetooth Low Energy (BLE) heart rate monitors and converts real-time heart rate data into MIDI CC messages for DAW automation. No external Python applications or OSC bridges required.

## Features

- **Native CoreBluetooth Integration** - Direct BLE communication without dependencies
- **Real-Time Heart Rate Monitoring** - Supports any BLE heart rate device (Polar H10, etc.)
- **Smart Smoothing** - Exponential Moving Average (EMA) with adjustable factor
- **MIDI CC Output** - 6 automation parameters (CC 1-6)
- **Zero Latency** - Audio passes through unchanged, MIDI generated instantly
- **DAW Automation Ready** - All parameters exposed for automation

## Parameters

| Parameter | Range | Default | MIDI CC | Description |
|-----------|-------|---------|---------|-------------|
| Raw Heart Rate | 40-180 BPM | 70 | CC1 | Direct HR reading from device |
| Smoothed Heart Rate | 40-180 BPM | 70 | CC2 | EMA-smoothed heart rate |
| Wet/Dry Ratio | 0-100% | 50 | CC3 | Normalized HR (40 BPM = 0%, 180 BPM = 100%) |
| HR Offset | -100 to +100 | 0 | CC4 | Shift heart rate values |
| Smoothing Factor | 0.01-10.0 | 0.15 | CC5 | EMA smoothing intensity |
| Wet/Dry Offset | -100 to +100 | 0 | CC6 | Shift wet/dry ratio |

## Build Instructions

### Prerequisites

- macOS 12.0 or later
- Xcode Command Line Tools
- CMake 3.22 or later

### Build Steps

```bash
cd HeartSync
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Built plugins will be automatically installed to:
- `~/Library/Audio/Plug-Ins/VST3/HeartSync.vst3`
- `~/Library/Audio/Plug-Ins/Components/HeartSync.component`

### Code Signing (Optional)

```bash
# Sign VST3
codesign --force --sign "Developer ID Application: Your Name" \
  ~/Library/Audio/Plug-Ins/VST3/HeartSync.vst3

# Sign AU
codesign --force --sign "Developer ID Application: Your Name" \
  ~/Library/Audio/Plug-Ins/Components/HeartSync.component
```

### Validation

```bash
# Validate AU
auval -v aufx HS54 QBA0

# Validate VST3 (requires pluginval)
pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/HeartSync.vst3
```

## Usage

1. **Load Plugin** - Insert HeartSync on any track in your DAW
2. **Scan for Devices** - Click "Scan for Devices" to find BLE heart rate monitors
3. **Connect** - Select your device from dropdown and click "Connect"
4. **Map Parameters** - Use MIDI Learn to map CC 1-6 to any DAW parameters
5. **Automate** - Adjust HR Offset, Smoothing Factor, and Wet/Dry Offset for your workflow

## Supported Devices

Any Bluetooth Low Energy heart rate monitor implementing:
- **Service UUID:** `0x180D` (Heart Rate Service)
- **Characteristic UUID:** `0x2A37` (Heart Rate Measurement)

Tested with:
- Polar H10
- Polar H9
- Wahoo TICKR

## Technical Details

- **Audio Framework:** JUCE 7.0.9
- **Build System:** CMake
- **BLE Stack:** CoreBluetooth (native macOS)
- **Plugin Formats:** VST3, AU
- **Architecture:** Universal Binary (arm64 + x86_64)
- **Code:** C++17, Objective-C++ for CoreBluetooth wrapper

## Project Structure

```
HeartSync/
├── CMakeLists.txt
├── Source/
│   ├── PluginProcessor.h/cpp    # Main plugin logic
│   ├── PluginEditor.h/cpp       # UI implementation
│   └── Core/
│       ├── HeartSyncBLE.h/mm    # CoreBluetooth wrapper
│       └── HeartRateProcessor.h/cpp  # Smoothing & calculations
```

## License

See LICENSE file in repository root.

## Credits

**Quantum Bio Audio** - Adaptive Audio Bio Technology
