#!/bin/bash
# HeartSync VST3 Build Script

echo "❖ Building HeartSync VST3 Plugin..."

if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "⚠️  Building on non-macOS platform ($OSTYPE). Output binaries are for this platform only." >&2
    echo "    Run this script on macOS/Xcode for production AU & macOS VST3 bundles." >&2
fi

# Create and enter build directory
mkdir -p build
cd build

BUILD_DIR="$(pwd)"

# Configure with CMake
echo "⚙ Configuring build..."
cmake .. -G "Unix Makefiles"

# Build the plugin
echo "🔨 Building plugin..."
make -j4

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo "📦 VST3 Plugin: ${BUILD_DIR}/HeartSyncVST3_artefacts/VST3/HeartSync Modulator.vst3"
    echo "🖥️  Standalone App: ${BUILD_DIR}/HeartSyncVST3_artefacts/Standalone/HeartSync Modulator.app"

    if [[ "$OSTYPE" == "darwin"* ]]; then
        read -p "🔧 Install to system? (y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo "📥 Installing VST3..."
            cp -R "HeartSyncVST3_artefacts/VST3/HeartSync Modulator.vst3" "${HOME}/Library/Audio/Plug-Ins/VST3/"
            echo "✓ Installed to ~/Library/Audio/Plug-Ins/VST3/"
        fi
    else
        echo "ℹ️  Install step skipped; run on macOS to install into ~/Library/Audio/Plug-Ins." >&2
    fi
else
    echo "✗ Build failed!"
    exit 1
fi