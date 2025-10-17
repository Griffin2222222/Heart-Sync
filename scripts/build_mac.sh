#!/bin/bash
# HeartSync macOS build helper

set -euo pipefail

if [[ "${OSTYPE:-}" != darwin* ]]; then
    echo "This script must be run on macOS." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_DIR="${REPO_ROOT}/HeartSyncVST3"
BUILD_DIR="${PROJECT_DIR}/build-macos"
CONFIG="Release"

usage() {
    cat <<EOF
Usage: $(basename "$0") [--config <Debug|Release>] [--install] [--codesign]

  --config     Build configuration (default: Release)
  --install    Copy built plug-ins to ~/Library/Audio/Plug-Ins/{VST3,Components}
  --codesign   Apply ad-hoc codesign to installed bundles (implies --install)
EOF
}

INSTALL=false
CODESIGN=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --config)
            CONFIG="${2:-}"
            shift 2
            ;;
        --install)
            INSTALL=true
            shift
            ;;
        --codesign)
            INSTALL=true
            CODESIGN=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

mkdir -p "${BUILD_DIR}"

echo "⚙️  Configuring (Unix Makefiles, ${CONFIG})..."
cmake -Wno-dev -S "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

echo "🔨 Building HeartSync (${CONFIG})..."
cmake --build "${BUILD_DIR}" --parallel

echo "✓ Build complete"

ARTEFACTS_DIR="${BUILD_DIR}/HeartSyncVST3_artefacts"
VST3_SRC="${ARTEFACTS_DIR}/VST3/HeartSync Modulator.vst3"
AU_SRC="${ARTEFACTS_DIR}/AU/HeartSync Modulator.component"
APP_SRC="${ARTEFACTS_DIR}/Standalone/HeartSync Modulator.app"

if [[ ! -d "${VST3_SRC}" ]]; then
    echo "⚠️  Expected artefact not found: ${VST3_SRC}" >&2
fi
if [[ ! -d "${AU_SRC}" ]]; then
    echo "⚠️  Expected artefact not found: ${AU_SRC}" >&2
fi
if [[ ! -d "${APP_SRC}" ]]; then
    echo "⚠️  Expected artefact not found: ${APP_SRC}" >&2
fi

if ${INSTALL}; then
    VST3_DEST="${HOME}/Library/Audio/Plug-Ins/VST3"
    AU_DEST="${HOME}/Library/Audio/Plug-Ins/Components"

    mkdir -p "${VST3_DEST}" "${AU_DEST}"

    if [[ -d "${VST3_SRC}" ]]; then
        echo "📥 Installing VST3 to ${VST3_DEST}"
        rm -rf "${VST3_DEST}/HeartSync Modulator.vst3"
        cp -R "${VST3_SRC}" "${VST3_DEST}/"
    fi

    if [[ -d "${AU_SRC}" ]]; then
        echo "📥 Installing AU to ${AU_DEST}"
        rm -rf "${AU_DEST}/HeartSync Modulator.component"
        cp -R "${AU_SRC}" "${AU_DEST}/"
    fi

    if ${CODESIGN}; then
        if [[ -d "${VST3_DEST}/HeartSync Modulator.vst3" ]]; then
            echo "🔐 Codesigning VST3..."
            codesign --force --deep --sign - "${VST3_DEST}/HeartSync Modulator.vst3"
        fi
        if [[ -d "${AU_DEST}/HeartSync Modulator.component" ]]; then
            echo "🔐 Codesigning AU..."
            codesign --force --deep --sign - "${AU_DEST}/HeartSync Modulator.component"
        fi
    fi

    echo "✓ Installation complete"
fi

cat <<EOF
Artefacts:
  VST3:       ${VST3_SRC}
  AU:         ${AU_SRC}
  Standalone: ${APP_SRC}
EOF
