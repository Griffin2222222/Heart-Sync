#!/bin/bash
# HeartSync VST3 Build Script (macOS / Linux)

set -euo pipefail

echo "❖ Building HeartSync VST3 Plugin..."

# Ensure required tools exist before we start the build
for tool in cmake make; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "✗ Missing required tool: $tool"
        echo "  Install it via Homebrew (e.g. brew install cmake make) and retry."
        exit 1
    fi
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# Create and enter build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "⚙ Configuring build..."
cmake .. -G "Unix Makefiles"

# Build the plugin
echo "🔨 Building plugin..."
if command -v sysctl >/dev/null 2>&1; then
    JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
elif command -v nproc >/dev/null 2>&1; then
    JOBS=$(nproc)
else
    JOBS=4
fi
make -j"$JOBS"

echo "✓ Build successful!"

# Locate artefacts (supports Debug/Release layouts)
PLUGIN_DIR=$(find HeartSyncVST3_artefacts -maxdepth 3 -type d -name "HeartSync.vst3" | head -n 1)
APP_DIR=$(find HeartSyncVST3_artefacts -maxdepth 3 -type d -name "HeartSync.app" | head -n 1)

if [[ -z "$PLUGIN_DIR" ]]; then
    echo "✗ Could not locate built VST3 artefact"
else
    echo "📦 VST3 Plugin: $BUILD_DIR/$PLUGIN_DIR"
fi

if [[ -z "$APP_DIR" ]]; then
    echo "✗ Could not locate standalone app artefact"
else
    echo "🖥️  Standalone App: $BUILD_DIR/$APP_DIR"
fi

# Option to install
if [[ -n "$PLUGIN_DIR" ]]; then
    read -p "🔧 Install VST3 to ~/Library/Audio/Plug-Ins/VST3/? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        INSTALL_PATH="$HOME/Library/Audio/Plug-Ins/VST3"
        mkdir -p "$INSTALL_PATH"
        echo "📥 Installing VST3..."
        if cp -R "$PLUGIN_DIR" "$INSTALL_PATH/"; then
            echo "✓ Installed to $INSTALL_PATH/HeartSync.vst3"
        else
            echo "✗ Failed to copy VST3 bundle to $INSTALL_PATH" >&2
            exit 1
        fi
    fi
fi