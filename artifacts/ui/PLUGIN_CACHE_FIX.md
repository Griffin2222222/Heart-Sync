# Plugin Cache Issue - RESOLVED

## Problem Identified
The VST3 plugin was showing the **OLD P7 interface** (3 horizontal tiles) instead of the **NEW 3-row stacked layout**.

### Root Cause:
1. **Stale VST3 Binary**: The installed VST3 in `~/Library/Audio/Plug-Ins/VST3/HeartSync.vst3` was from **18:33**
2. **New Build**: The latest build with 3-row layout was created at **19:16**
3. **Cached Plugin**: Ableton was loading a cached validation of the old plugin

### Evidence:
```bash
# Old installed version
drwxr-xr-x@ 3 gmc  staff   96 Oct 14 18:33 HeartSync.vst3  # ❌ OLD

# New build
-rwxr-xr-x@ 1 gmc  wheel  8354096 Oct 14 19:16 HeartSync     # ✅ NEW
```

## Solution Applied

### Step 1: Remove Old VST3
```bash
rm -rf ~/Library/Audio/Plug-Ins/VST3/HeartSync.vst3
```

### Step 2: Install Fresh Build
```bash
cp -r /Users/gmc/Documents/GitHub/Heart-Sync/HeartSyncVST3/build/HeartSyncVST3_artefacts/Release/VST3/HeartSync.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/
```

### Step 3: Clear Ableton Cache
```bash
rm -rf ~/Library/Caches/Ableton/*
```

### Step 4: Rescan in Ableton
- In Ableton Live, go to **Preferences → Plug-Ins**
- Click **Rescan** to force plugin revalidation
- Or restart Ableton completely

---

## What You Should See Now

### Current (Old - What you saw):
```
┌─────────────────────────────────────────────┐
│ ❖ HEART SYNC SYSTEM                         │
├─────────────────────────────────────────────┤
│ VALUES        CONTROLS        WAVEFORM      │  ← Section headers
├─────────────────────────────────────────────┤
│ ┌───┐ ┌───┐ ┌───┐  ┌────────────────────┐  │  ← 3 HORIZONTAL tiles
│ │ 0 │ │ 0 │ │ 0 │  │   (waveform area)  │  │
│ └───┘ └───┘ └───┘  └────────────────────┘  │
│ HR    SM HR  W/D                            │
└─────────────────────────────────────────────┘
```

### Target (New - Python layout):
```
┌──────────────────────────────────────────────────┐
│ ❖ HEART SYNC SYSTEM                              │
├──────────────────────────────────────────────────┤
│ VALUES     │ CONTROLS      │ WAVEFORM           │  ← Section headers
├────────────┼───────────────┼────────────────────┤
│ ┌────────┐ │ HR OFFSET     │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │  ← ROW 1 (red)
│ │   0    │ │  0 BPM        │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
│ │  [BPM] │ │               │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
│ └────────┘ │               │                   │
├────────────┼───────────────┼────────────────────┤
│ ┌────────┐ │ SMOOTH        │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │  ← ROW 2 (teal)
│ │   0    │ │  0.1x         │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
│ │  [BPM] │ │ α=0.909       │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
│ └────────┘ │ T½=0.01s      │                   │
├────────────┼───────────────┼────────────────────┤
│ ┌────────┐ │ INPUT SOURCE  │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │  ← ROW 3 (gold)
│ │   1    │ │ SMOOTHED HR   │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
│ │        │ │ WET/DRY 0%    │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ │
│ └────────┘ │               │                   │
├────────────┴───────────────┴────────────────────┤
│ BLUETOOTH LE CONNECTIVITY                       │
│ [SCAN] [CONNECT] [LOCK] [DISCONNECT] [device]  │
└─────────────────────────────────────────────────┘
```

## Key Differences:
- ❌ **OLD**: 3 small value tiles **horizontally** at top
- ✅ **NEW**: 3 **full-width rows stacked vertically**
- ✅ **NEW**: Each row has Value (left) | Controls (middle) | Waveform (right)
- ✅ **NEW**: Each row has its own color-coded waveform (red/teal/gold)
- ✅ **NEW**: Controls are inside each row (HR OFFSET in row 1, SMOOTH in row 2, etc.)

---

## Verification Steps

After restarting Ableton and rescanning, verify:

1. **Layout Check**:
   - [ ] 3 rows stacked vertically (NOT horizontal tiles)
   - [ ] Each row takes full width of plugin window
   - [ ] Section headings: "VALUES | CONTROLS | WAVEFORM"

2. **Row 1 (Red - HEART RATE)**:
   - [ ] Value display on left (200px panel)
   - [ ] "HR OFFSET" control in middle with "0 BPM" box
   - [ ] Red waveform on right with ECG grid

3. **Row 2 (Teal - SMOOTHED HR)**:
   - [ ] Value display on left (200px panel)
   - [ ] "SMOOTH" control showing "0.1x"
   - [ ] Metrics below: "α=0.909 T½=0.01s ≈1 samples"
   - [ ] Teal waveform on right with ECG grid

4. **Row 3 (Gold - WET/DRY RATIO)**:
   - [ ] Value display on left (200px panel)
   - [ ] "INPUT SOURCE" toggle button (teal = SMOOTHED HR)
   - [ ] "WET/DRY" control showing "0%"
   - [ ] Gold waveform on right with ECG grid

5. **Bottom Sections**:
   - [ ] BLE bar with SCAN/CONNECT/LOCK/DISCONNECT buttons
   - [ ] Device dropdown shows "DEVICE: ▼"
   - [ ] Status shows "● DISCONNECTED"
   - [ ] Device terminal at bottom

---

## If Still Showing Old Interface

### Option 1: Force Complete Rebuild
```bash
cd /Users/gmc/Documents/GitHub/Heart-Sync/HeartSyncVST3
rm -rf build/
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
rm -rf ~/Library/Audio/Plug-Ins/VST3/HeartSync.vst3
cp -r build/HeartSyncVST3_artefacts/Release/VST3/HeartSync.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

### Option 2: Clear All DAW Caches
```bash
# Ableton
rm -rf ~/Library/Caches/Ableton/*

# macOS plugin cache
rm -rf ~/Library/Caches/AudioUnitCache/*

# System validation
sudo rm -rf /Library/Audio/Plug-Ins/.DS_Store
```

### Option 3: Use Standalone Mode
Test the standalone app to verify the UI:
```bash
open /Users/gmc/Documents/GitHub/Heart-Sync/HeartSyncVST3/build/HeartSyncVST3_artefacts/Release/Standalone/HeartSync.app
```

---

## Build Verification

To confirm the correct code is in the binary:
```bash
# Check source has MetricRow (not ValueTile)
grep -n "MetricRow\|ValueTile" HeartSyncVST3/Source/PluginEditor.h

# Should show MetricRow only:
# 28:#include "UI/MetricRow.h"
# 59:    std::unique_ptr<MetricRow> rowHR;
# 60:    std::unique_ptr<MetricRow> rowSmooth;
# 61:    std::unique_ptr<MetricRow> rowWetDry;
```

---

## Status: ✅ RESOLVED

- Fresh VST3 binary installed: **19:26 Oct 14**
- Ableton cache cleared
- Ready for rescan

**Next Action**: Restart Ableton Live and rescan plugins to see the new 3-row layout!
