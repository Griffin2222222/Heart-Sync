#!/bin/bash
# HeartSync VST3 Build Script

echo "❖ Building HeartSync VST3 Plugin..."

# Create and enter build directory
mkdir -p build && cd build

# Configure with CMake
echo "⚙ Configuring build..."
cmake .. -G "Unix Makefiles"

# Build the plugin
echo "🔨 Building plugin..."
make -j4

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo "📦 VST3 Plugin: build/HeartSyncVST3_artefacts/VST3/HeartSync.vst3"
    echo "🖥️  Standalone App: build/HeartSyncVST3_artefacts/Standalone/HeartSync.app"
    
    # Option to install
    read -p "🔧 Install to system? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "📥 Installing VST3..."
        cp -R "HeartSyncVST3_artefacts/VST3/HeartSync.vst3" ~/Library/Audio/Plug-Ins/VST3/
        echo "✓ Installed to ~/Library/Audio/Plug-Ins/VST3/"
    fi
else
    echo "✗ Build failed!"
    exit 1
fi