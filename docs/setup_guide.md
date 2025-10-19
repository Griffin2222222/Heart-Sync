## HeartSync Professional Setup (macOS)

Follow these steps to build and install the HeartSync VST3 plugin on macOS so it can be loaded in any VST3-compatible DAW (Ableton Live, Logic Pro via Blue Cat PatchWork, Reaper, etc.).

### 1. Install prerequisites

- **Xcode Command Line Tools** – provides Clang and system SDK headers.
	```bash
	xcode-select --install
	```
- **Homebrew** – standard macOS package manager (skip if already installed).
	```bash
	/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
	```
- **Build toolchain** – install CMake (3.22 or newer) and GNU Make from Homebrew.
	```bash
	brew install cmake make
	```

> If you prefer Apple’s `make`, you can omit the Homebrew `make` package. The build script checks for both tools before starting.

### 2. Create and activate the Python virtual environment (optional)

The HeartSync GUI app in the repository ships with a `.venv` folder containing its Python dependencies. If you intend to run the GUI alongside the plugin, activate that environment:

```bash
cd /Users/gmc/Documents/GitHub/Heart-Sync
source .venv/bin/activate
```

This step is not required for the JUCE plugin build itself, but it keeps tooling consistent across the project.

### 3. Build the VST3 plugin

```bash
cd /Users/gmc/Documents/GitHub/Heart-Sync/HeartSyncVST3
chmod +x build.sh   # first time only
./build.sh
```

What the script does:
- Configures a JUCE/CMake build in `HeartSyncVST3/build/`
- Downloads JUCE 7.0.9 automatically via CMake’s `FetchContent`
- Compiles both the VST3 plugin and standalone application
- Prompts to copy the `.vst3` bundle into `~/Library/Audio/Plug-Ins/VST3/`

When the build completes you should see output similar to:

```
📦 VST3 Plugin: /Users/.../HeartSyncVST3/build/HeartSyncVST3_artefacts/Release/VST3/HeartSync.vst3
🖥️  Standalone App: /Users/.../HeartSyncVST3/build/HeartSyncVST3_artefacts/Release/Standalone/HeartSync.app
```

Choosing `y` at the prompt copies the VST3 bundle into the standard user plug-in directory so it is immediately visible to your DAW.

### 4. Rescan plug-ins in your DAW

1. Launch your DAW.
2. Trigger a plug-in rescan (consult your DAW’s manual; in Ableton Live it’s under *Preferences → Plug-Ins → Rescan*).
3. Load “HeartSync” from the VST3 instrument/effect list.

### 5. Troubleshooting tips

- **Build fails with missing headers or SDK** – ensure the Xcode command line tools install finished successfully. Run `xcode-select --switch /Library/Developer/CommandLineTools` if multiple SDKs exist.
- **CMake cannot be found** – reinstall via Homebrew (`brew install cmake`) and confirm with `cmake --version`.
- **Plugin not visible in DAW** – verify the bundle exists at `~/Library/Audio/Plug-Ins/VST3/HeartSync.vst3`. Some hosts (e.g. Logic Pro) require an AU wrapper; use the standalone app or load via a VST bridge such as Blue Cat PatchWork.
- **DAW crashes when the VST3 loads** – many hosts do not ship the `NSBluetoothAlwaysUsageDescription` permission string in their own `Info.plist`. The plugin now detects this and disables its built-in Bluetooth stack, but if you are on an older build make sure you have re-ran `./build.sh` and re-installed the updated bundle.
- **macOS security warning** – because the bundle is ad-hoc signed, the first launch may require right-click → *Open* in Finder to approve the binary.

You are now ready to pair a Bluetooth LE heart-rate monitor through the plugin UI and drive automation in your DAW.
