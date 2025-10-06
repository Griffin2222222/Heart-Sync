# HeartSync VST3 - Professional Plugin Project

## Project Goal
Build a professional-tier VST3 plugin for heart rate reactive modulation, matching UAD plugin quality standards.

## Single Application Architecture

### Testing & Development
- **HeartSyncProfessional.py** - Python prototype for testing BLE HR monitoring, graphs, and UI concepts before VST3 integration

### Production VST3 Plugin
- **HeartSyncVST3/** - Main VST3 plugin codebase (JUCE-based)
  - `CMakeLists.txt` - Build configuration
  - `Source/PluginProcessor.cpp/h` - Audio processing and HR integration
  - `Source/PluginEditor.cpp/h` - Professional GUI
  - `Source/Core/` - Core functionality modules:
    - `BluetoothManager.cpp/h` - BLE HR device connection
    - `HeartRateProcessor.cpp/h` - HR data processing
    - `OscSenderManager.cpp/h` - OSC communication
    - `HeartRateParams.h` - Parameter definitions

### Build System
- `build.bat` - Main build script (auto-detects compiler)
- `build_with_msvc.bat` - MSVC/Visual Studio build
- `build_with_buildtools.bat` - VS Build Tools path
- `install.bat` - One-time C++ Build Tools installer

### Documentation
- `README.md` - Project overview and quick start
- `HeartSyncVST3/README.md` - VST3-specific build instructions
- `PROJECT_STRUCTURE.md` - This file

### Utilities
- `scan_devices.py` - BLE device scanner for testing
- `test_heartrate.py` - HR data testing utility
- `requirements.txt` - Python dependencies for testing tools

## Development Workflow
1. Test features in `HeartSyncProfessional.py` (quick Python iteration)
2. Implement proven features in VST3 C++ codebase
3. Build VST3 with `build_with_msvc.bat`
4. Test plugin in DAW
5. Iterate

## Clean Architecture Principles
- One main application (VST3 plugin)
- Python prototype for rapid feature testing
- No duplicate implementations
- Professional-grade quality target
