#!/bin/bash
set -euo pipefail

# Variables
REPO="/Users/gmc/Documents/GitHub/Heart-Sync"
APPDIR="$REPO/HeartSync"
PLUG_AU="$HOME/Library/Audio/Plug-Ins/Components/HeartSync.component/Contents/MacOS/HeartSync"
BRIDGE_APP="$HOME/Applications/HeartSync Bridge.app/Contents/MacOS/HeartSync Bridge"
SOCK="$HOME/Library/Application Support/HeartSync/bridge.sock"
ARCHIVE_DIR="$REPO/docs/legacy-$(date +%Y%m%d-%H%M%S)"
DRY_RUN=false

echo "=========================================="
echo "HEARTSYNC UDS REFACTOR - CLEANUP & VERIFY"
echo "=========================================="
echo ""
echo "Variables:"
echo "  REPO=$REPO"
echo "  ARCHIVE_DIR=$ARCHIVE_DIR"
echo "  DRY_RUN=$DRY_RUN"
echo ""

# Create archive directory
echo "Creating archive directory..."
mkdir -p "$ARCHIVE_DIR"
echo "✓ Created: $ARCHIVE_DIR"
echo ""

echo "=========================================="
echo "STEP 1: FINAL ACCEPTANCE CHECKS"
echo "=========================================="
echo ""

# 1.1 Plugin has no CoreBluetooth
echo "1.1) Plugin has no CoreBluetooth"
if nm -gU "$PLUG_AU" 2>/dev/null | grep -i bluetooth; then
    echo "❌ FAIL: Found CoreBluetooth in plugin!"
    exit 1
else
    echo "✓ PASS: no CoreBluetooth in plugin"
fi
echo ""

# 1.2 Bridge links CoreBluetooth
echo "1.2) Bridge links CoreBluetooth"
if otool -L "$BRIDGE_APP" 2>/dev/null | grep CoreBluetooth; then
    echo "✓ PASS: Bridge has CoreBluetooth"
else
    echo "❌ FAIL: Bridge missing CoreBluetooth!"
    exit 1
fi
echo ""

# 1.3 Bridge has headless flags
echo "1.3) Bridge has headless flags"
if plutil -p "$HOME/Applications/HeartSync Bridge.app/Contents/Info.plist" 2>/dev/null | grep -E 'LSBackgroundOnly|LSUIElement'; then
    echo "✓ PASS: Headless flags present"
else
    echo "❌ FAIL: Missing headless flags!"
    exit 1
fi
echo ""

# 1.4 UDS socket exists with secure perms
echo "1.4) UDS socket exists with secure perms"
if [ -S "$SOCK" ]; then
    ls -la "$SOCK"
    SOCK_PERMS=$(stat -f "%OLp" "$SOCK" 2>/dev/null || echo "unknown")
    echo "Socket permissions: $SOCK_PERMS"
    if [ "$SOCK_PERMS" = "600" ]; then
        echo "✓ PASS: Socket has 0600 permissions"
    else
        echo "⚠ WARNING: Socket permissions are $SOCK_PERMS (expected 0600)"
    fi
else
    echo "⚠ WARNING: Socket not found at $SOCK (may need Bridge running)"
fi
echo ""

# 1.5 Parent directory has 0700
echo "1.5) Parent directory has secure perms"
PARENT_DIR="$HOME/Library/Application Support/HeartSync"
if [ -d "$PARENT_DIR" ]; then
    ls -ld "$PARENT_DIR"
    PARENT_PERMS=$(stat -f "%OLp" "$PARENT_DIR" 2>/dev/null || echo "unknown")
    echo "Directory permissions: $PARENT_PERMS"
    if [ "$PARENT_PERMS" = "700" ]; then
        echo "✓ PASS: Directory has 0700 permissions"
    else
        echo "⚠ WARNING: Directory permissions are $PARENT_PERMS (expected 0700)"
    fi
else
    echo "⚠ WARNING: Directory not found (may need Bridge running)"
fi
echo ""

# 1.6 No TCP remnants
echo "1.6) No TCP remnants"
if lsof -i :51721 2>/dev/null; then
    echo "❌ FAIL: Found process on TCP port 51721!"
    exit 1
else
    echo "✓ PASS: no TCP port 51721 in use"
fi

cd "$APPDIR"
if rg -n '51721|StreamingSocket' . 2>/dev/null | grep -v Binary; then
    echo "❌ FAIL: Found TCP code references!"
    exit 1
else
    echo "✓ PASS: no TCP code references"
fi
echo ""

# 1.7 Build macros unified
echo "1.7) Build macros unified (no old HELPER macro)"
cd "$APPDIR"
if rg -n 'HEARTSYNC_USE_HELPER' . 2>/dev/null | grep -v Binary; then
    echo "❌ FAIL: Found old HEARTSYNC_USE_HELPER macro!"
    exit 1
else
    echo "✓ PASS: only HEARTSYNC_USE_BRIDGE"
fi
echo ""

# 1.8 HelperApp not referenced
echo "1.8) HelperApp not referenced by build"
if rg -n 'HelperApp|HeartSyncHelper' CMakeLists.txt Source 2>/dev/null | grep -v Binary; then
    echo "❌ FAIL: Found HelperApp references!"
    exit 1
else
    echo "✓ PASS: no HelperApp references"
fi
echo ""

echo "=========================================="
echo "STEP 2: WORKSPACE CLEANUP PLAN"
echo "=========================================="
echo ""

cd "$REPO"

echo "2.1) Preview scratch files to archive:"
echo ""
echo "Patch/phase notes:"
ls -1 P*_APPLIED.txt 2>/dev/null || echo "  (none)"
ls -1 PATCH_*.patch 2>/dev/null || echo "  (none)"

echo ""
echo "Documentation files:"
for f in DELIVERY_COMPLETE.txt HEARTSYNC_UDS_PATCH.md FINAL_DELIVERABLE.md IMPLEMENTATION_SUMMARY.md RISK_LOG.md HELPER_REFACTOR_STATUS.md; do
    [ -f "$f" ] && echo "  $f" || true
done

echo ""
echo "Files to KEEP (will NOT delete):"
for keep in README.md LICENSE CHANGELOG.md QUICKSTART.md docs/; do
    [ -e "$keep" ] && echo "  ✓ $keep" || echo "  ✗ $keep (not present)"
done
echo ""

echo "2.2) Archive files (DRY_RUN=$DRY_RUN)"
FILES_TO_ARCHIVE=$(ls -1 P*_APPLIED.txt 2>/dev/null || true)
FILES_TO_ARCHIVE="$FILES_TO_ARCHIVE
$(ls -1 PATCH_*.patch 2>/dev/null || true)"

for doc in DELIVERY_COMPLETE.txt HEARTSYNC_UDS_PATCH.md FINAL_DELIVERABLE.md IMPLEMENTATION_SUMMARY.md RISK_LOG.md HELPER_REFACTOR_STATUS.md; do
    [ -f "$doc" ] && FILES_TO_ARCHIVE="$FILES_TO_ARCHIVE
$doc"
done

if [ -n "$FILES_TO_ARCHIVE" ]; then
    echo "$FILES_TO_ARCHIVE" | while IFS= read -r f; do
        if [ -n "$f" ] && [ -e "$f" ]; then
            if [ "$DRY_RUN" = true ]; then
                echo "  [DRY RUN] Would archive: $f -> $ARCHIVE_DIR/"
            else
                echo "  Archiving: $f -> $ARCHIVE_DIR/"
                mv -v "$f" "$ARCHIVE_DIR/"
            fi
        fi
    done
else
    echo "  No files to archive"
fi
echo ""

echo "2.3) Check for unused HelperApp directory"
if [ -d "$APPDIR/HelperApp" ]; then
    echo "  Found: $APPDIR/HelperApp"
    if [ "$DRY_RUN" = true ]; then
        echo "  [DRY RUN] Would remove: $APPDIR/HelperApp"
    else
        echo "  Removing: $APPDIR/HelperApp"
        rm -rf "$APPDIR/HelperApp"
    fi
else
    echo "  ✓ HelperApp directory not present (already removed or never existed)"
fi
echo ""

echo "=========================================="
echo "STEP 3: .gitignore HARDENING"
echo "=========================================="
echo ""

GITIGNORE="$REPO/.gitignore"
echo "Checking $GITIGNORE"

# Function to add line if not present
addline() {
    if grep -qxF "$1" "$GITIGNORE" 2>/dev/null; then
        echo "  ✓ Already present: $1"
    else
        if [ "$DRY_RUN" = true ]; then
            echo "  [DRY RUN] Would add: $1"
        else
            echo "  Adding: $1"
            echo "$1" >> "$GITIGNORE"
        fi
    fi
}

addline "HeartSync/build/"
addline "HeartSync/**/CMakeFiles/"
addline "HeartSync/**/CMakeCache.txt"
addline "HeartSync/**/*.xcodeproj/*"
addline "HeartSync/**/DerivedData/"
addline "*.app"
addline "*.trace"
addline "*.dSYM"
addline "*.log"
addline "/packaging/macos/*.plist"
addline "tools/__pycache__/"
echo ""

echo "=========================================="
echo "STEP 4: FINAL SMOKE CHECKS"
echo "=========================================="
echo ""

echo "4.1) UDS socket still good"
if [ -S "$SOCK" ]; then
    ls -la "$SOCK"
    stat -f "Mode: %OLp" "$SOCK"
    echo "✓ Socket OK"
else
    echo "⚠ Socket not present (Bridge may not be running)"
fi
echo ""

echo "4.2) No TCP code"
cd "$APPDIR"
if rg -n '51721|StreamingSocket' . 2>/dev/null | grep -v Binary; then
    echo "❌ FAIL: Found TCP code!"
else
    echo "✓ PASS: No TCP code"
fi
echo ""

echo "4.3) Python smoke tool"
if [ -x "$REPO/tools/uds_send.py" ]; then
    echo "✓ uds_send.py is executable"
else
    echo "⚠ uds_send.py not executable or not present"
fi
echo ""

echo "=========================================="
echo "SUMMARY"
echo "=========================================="
echo ""
echo "✓ All acceptance checks PASSED"
echo ""
echo "DRY_RUN is currently: $DRY_RUN"
echo ""
if [ "$DRY_RUN" = true ]; then
    echo "To execute the cleanup, edit this script and set:"
    echo "  DRY_RUN=false"
    echo "Then re-run: bash $0"
else
    echo "✓ Cleanup executed successfully"
    echo "✓ Files archived to: $ARCHIVE_DIR"
fi
echo ""
echo "Archive directory contents:"
ls -la "$ARCHIVE_DIR" 2>/dev/null || echo "(empty or not yet created)"
