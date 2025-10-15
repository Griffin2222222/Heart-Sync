# HeartSync - Heart Rate Reactive Audio Plugin

A professional VST3 audio plugin that connects to Bluetooth LE heart rate monitors and provides real-time heart rate data for DAW automation and audio modulation.

## 🎯 Features

- **Self-Contained VST3 Plugin** - No external dependencies or Python apps needed
- **Bluetooth LE Integration** - Native Core Bluetooth support for heart rate monitors
- **Professional Medical UI** - Quantum teal aesthetic with real-time waveform displays
- **DAW Automation** - 6 VST3 parameters for complete DAW integration
- **Real-time Processing** - Heart rate smoothing and wet/dry ratio calculations

## 🏗️ Project Structure

```
Heart-Sync/
├── HeartSyncProfessional.py    # Original Python prototype (reference only)
├── requirements.txt            # Python dependencies for prototype
├── HeartSyncVST3/             # VST3 Plugin Source
│   ├── Source/
│   │   ├── PluginProcessor.cpp/.h      # Main audio processor
│   │   ├── PluginEditor.cpp/.h         # Professional UI
│   │   └── Core/
│   │       ├── BluetoothManager.h      # Bluetooth LE interface
│   │       └── BluetoothManager_Native.mm  # macOS Core Bluetooth implementation
│   ├── CMakeLists.txt         # Build configuration
│   └── build.sh              # Easy build script
└── docs/                     # Documentation
```

## 🚀 Quick Start

### Building the VST3 Plugin

```bash
cd HeartSyncVST3
./build.sh
```

The script will:
1. Configure the build with CMake
2. Compile the VST3 plugin
3. Optionally install to your VST3 directory

### Manual Build

```bash
cd HeartSyncVST3
mkdir build && cd build
cmake .. -G "Unix Makefiles"
make -j4
```

### Installing

Copy the built VST3 to your plugins directory:
```bash
cp -R "build/HeartSyncVST3_artefacts/VST3/HeartSync Modulator.vst3" ~/Library/Audio/Plug-Ins/VST3/
```

## 🎛️ Using in Your DAW

1. **Load Plugin** - Add "HeartSync Modulator" to any track in your DAW
2. **Scan for Devices** - Click the "SCAN" button to discover heart rate monitors
3. **Connect** - Click "CONNECT" to connect to your heart rate monitor
4. **Automate** - Map the 6 VST3 parameters to any DAW controls:
   - Raw Heart Rate (40-180 BPM)
   - Smoothed Heart Rate (40-180 BPM) 
   - Wet/Dry Ratio (0-100%)
   - HR Offset (-100 to +100)
   - Smoothing Factor (0.01-10.0)
   - Wet/Dry Offset (-100 to +100)

## 🔧 System Requirements

- **macOS** 10.15+ (Core Bluetooth LE support)
- **Xcode Command Line Tools** or Xcode
- **CMake** 3.22+
- **Bluetooth LE Heart Rate Monitor** (Polar, Garmin, etc.)

## 🎵 Compatible DAWs

- Ableton Live
- Logic Pro
- Pro Tools
- Cubase
- Any VST3-compatible DAW

## 📚 Heart Rate Monitors

Compatible with any Bluetooth LE heart rate monitor that follows the standard Heart Rate Service (0x180D):
- Polar H10, H9, H7
- Garmin HRM-Pro, HRM-Run
- Wahoo TICKR
- Apple Watch (when broadcasting HR)
- Any ANT+ HR monitor with Bluetooth LE

## 🛠️ Development

The codebase is clean and minimal:
- **C++17** with JUCE framework
- **Objective-C++** for Core Bluetooth integration
- **CMake** build system
- **Professional medical UI** components

## 📄 License

See LICENSE file for details.