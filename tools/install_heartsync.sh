#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: install_heartsync.sh [--vst3-dir DIR] [--standalone-dir DIR] [--no-standalone] [--dry-run]

Removes older HeartSync builds from common plug-in locations and installs the
fresh binaries created under HeartSyncVST3/build/HeartSyncVST3_artefacts.

Options:
  --vst3-dir DIR         Destination directory for the VST3 bundle (default: $HOME/.vst3)
  --standalone-dir DIR   Destination directory for the standalone binary (default: $HOME/.local/bin)
  --no-standalone        Skip installing the standalone build; only deploy the VST3 bundle
  --dry-run              Show the planned actions without performing any filesystem changes
  -h, --help             Show this help message
USAGE
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_ROOT="$REPO_ROOT/HeartSyncVST3/build/HeartSyncVST3_artefacts"
VST3_SRC="$BUILD_ROOT/VST3/HeartSync Modulator.vst3"
STANDALONE_SRC="$BUILD_ROOT/Standalone/HeartSync Modulator"

vst3_dest="${HOME}/.vst3"
standalone_dest="${HOME}/.local/bin"
install_standalone=1
dry_run=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --vst3-dir)
            [[ $# -lt 2 ]] && { echo "Missing value for --vst3-dir" >&2; exit 1; }
            vst3_dest="$2"
            shift 2
            ;;
        --standalone-dir)
            [[ $# -lt 2 ]] && { echo "Missing value for --standalone-dir" >&2; exit 1; }
            standalone_dest="$2"
            shift 2
            ;;
        --no-standalone)
            install_standalone=0
            shift
            ;;
        --dry-run)
            dry_run=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
done

require_path() {
    local path="$1"
    if [[ ! -e "$path" ]]; then
        echo "Error: expected artefact not found: $path" >&2
        echo "Run 'cmake --build build' inside HeartSyncVST3 first." >&2
        exit 1
    fi
}

require_path "$VST3_SRC"
if [[ "$install_standalone" -eq 1 ]]; then
    require_path "$STANDALONE_SRC"
fi

purge_targets=(
    "${HOME}/.vst3/HeartSync Modulator.vst3"
    "/usr/local/lib/vst3/HeartSync Modulator.vst3"
    "/usr/lib/vst3/HeartSync Modulator.vst3"
    "${HOME}/.local/share/VST3/HeartSync Modulator.vst3"
)

purge_file() {
    local target="$1"
    if [[ -e "$target" ]]; then
        if [[ $dry_run -eq 1 ]]; then
            echo "[dry-run] rm -rf '$target'"
        else
            echo "Removing old build: $target"
            rm -rf "$target"
        fi
    fi
}

purge_suffixes=("" ".old" ".bak" ".backup" ".orig" ".previous")
legacy_names=(
    "HeartSync Modulator.vst3"
    "HeartSync Modulator"
    "HeartSync.vst3"
    "HeartSync"
    "HeartSyncProfessional.vst3"
    "HeartSyncProfessional"
    "HeartInfusion.vst3"
    "HeartInfusion"
)

for candidate in "${purge_targets[@]}"; do
    base_dir="$(dirname "$candidate")"

    for suffix in "${purge_suffixes[@]}"; do
        purge_file "${candidate}${suffix}"
        purge_file "${candidate%.vst3}${suffix}"
    done

    for name in "${legacy_names[@]}"; do
        for suffix in "${purge_suffixes[@]}"; do
            purge_file "$base_dir/${name}${suffix}"
        done
    done
done

prepare_dest() {
    local dest="$1"
    if [[ $dry_run -eq 1 ]]; then
        echo "[dry-run] mkdir -p '$dest'"
    else
        mkdir -p "$dest"
    fi
}

copy_bundle() {
    local source="$1"
    local dest_dir="$2"
    local name
    name="$(basename "$source")"
    prepare_dest "$dest_dir"
    if [[ $dry_run -eq 1 ]]; then
        echo "[dry-run] rsync -a '$source/' '$dest_dir/$name/'"
    else
        echo "Installing $name -> $dest_dir"
        rsync -a "$source/" "$dest_dir/$name/"
    fi
}

prepare_dest "$vst3_dest"
if [[ $dry_run -eq 1 ]]; then
    echo "[dry-run] rsync -a '$VST3_SRC/' '$vst3_dest/HeartSync Modulator.vst3/'"
else
    echo "Deploying VST3 bundle to $vst3_dest"
    rsync -a "$VST3_SRC/" "$vst3_dest/HeartSync Modulator.vst3/"
fi

if [[ $install_standalone -eq 1 ]]; then
    prepare_dest "$standalone_dest"
    if [[ $dry_run -eq 1 ]]; then
        echo "[dry-run] install -m 755 '$STANDALONE_SRC' '$standalone_dest/HeartSync Modulator'"
    else
        echo "Deploying standalone binary to $standalone_dest"
        install -m 755 "$STANDALONE_SRC" "$standalone_dest/HeartSync Modulator"
    fi
fi

echo "Deployment complete. Restart your host or rescan plug-ins to load the new build."
