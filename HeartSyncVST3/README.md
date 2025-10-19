# HeartSync VST3 - Clean & Simple

A streamlined heart rate reactive VST3 plugin.

## What It Does
- Receives heart rate data via OSC on port 8000
- Converts BPM to MIDI CC messages (CC 74)
- Visual heart rate display in plugin GUI
- Clean, minimal architecture

## Build

### macOS
```bash
cd HeartSyncVST3
chmod +x build.sh  # once per machine
./build.sh
```

Requirements:
- Xcode command line tools (`xcode-select --install`)
- CMake 3.22+ (`brew install cmake`)

The script outputs the paths to the generated `.vst3` bundle and standalone app, and can optionally copy the plugin into `~/Library/Audio/Plug-Ins/VST3/` for use in your DAW.

> **Heads up:** Some DAWs (Ableton Live, Reaper, etc.) do not declare the `NSBluetoothAlwaysUsageDescription` permission in their own `Info.plist`. In those hosts the plugin will automatically disable Bluetooth features to avoid a crash. Use the standalone app if you need direct Bluetooth pairing.

### Windows
```bash
cd HeartSyncVST3
build.bat
```

## Files
- `Source/PluginProcessor.h/cpp` - Main plugin logic
- `Source/PluginEditor.h/cpp` - GUI interface  
- `CMakeLists.txt` - Build configuration
- `build.bat` - One-click build script

That's it. No more mess.
