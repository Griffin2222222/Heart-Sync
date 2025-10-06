# HeartSync VST3 - Clean & Simple

A streamlined heart rate reactive VST3 plugin.

## What It Does
- Receives heart rate data via OSC on port 8000
- Converts BPM to MIDI CC messages (CC 74)
- Visual heart rate display in plugin GUI
- Clean, minimal architecture

## Build
```bash
cd HeartSyncVST3
./build.bat
```

## Files
- `Source/PluginProcessor.h/cpp` - Main plugin logic
- `Source/PluginEditor.h/cpp` - GUI interface  
- `CMakeLists.txt` - Build configuration
- `build.bat` - One-click build script

That's it. No more mess.
