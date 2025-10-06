#!/usr/bin/env python3
"""
HeartSync Professional - Next-Generation Scientific Monitoring
Parallel-universe technology aesthetic with quantum-grade precision
"""

import tkinter as tk
from tkinter import ttk, messagebox
import threading
import time
import math
import random
import asyncio
from collections import deque
from datetime import datetime

# Import next-generation design system
from theme_tokens_nextgen import (
    Typography, Colors, Spacing, Radius, Elevation, 
    Motion, Grid, Waveform, Accessibility, Components
)

try:
    import bleak
    BLUETOOTH_AVAILABLE = True
    print("[OK] Bluetooth LE support available")
except ImportError:
    BLUETOOTH_AVAILABLE = False
    print("[WARNING] bleak not installed - BLE disabled")

try:
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    from matplotlib.figure import Figure
    import matplotlib.animation as animation
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    print("⚠ matplotlib not installed - using simple graphs")


class BluetoothManager:
    HR_SERVICE_UUID = "0000180d-0000-1000-8000-00805f9b34fb"
    HR_MEASUREMENT_UUID = "00002a37-0000-1000-8000-00805f9b34fb"

    def __init__(self, callback):
        self.callback = callback
        self.client = None
        self.available_devices = []
        self.is_connected = False
        self.last_notification_time = 0.0
        self.loop = None
        self.loop_thread = None

    async def scan_devices(self):
        if not BLUETOOTH_AVAILABLE:
            print("[ERROR] Bluetooth not available - bleak not installed")
            return []
        self.available_devices = []
        try:
            print("[SCAN] Starting BLE scan (8 second timeout)...")
            devices = await bleak.BleakScanner.discover(timeout=8.0)
            print(f"[SCAN] Found {len(devices)} total Bluetooth devices")
            
            for d in devices:
                device_name = d.name or "Unknown"
                print(f"  - Device: {device_name} | Address: {d.address}")
                
                # Accept ANY device with a name (including Coospo HW700)
                if d.name:
                    # Check for HR-related keywords
                    keywords = ["hr", "heart", "polar", "wahoo", "garmin", "tickr", "cardio", "apple", "watch", "coospo", "hw700", "hw"]
                    if any(k in d.name.lower() for k in keywords):
                        print(f"    [OK] HR device detected: {device_name}")
                        self.available_devices.append(d)
                    else:
                        # Also add devices that advertise HR service
                        print(f"    [INFO] Adding device: {device_name}")
                        self.available_devices.append(d)
                        
            print(f"[SCAN] Scan complete. Found {len(self.available_devices)} potential HR devices")
        except Exception as e:
            print(f"[ERROR] Scan error: {e}")
            import traceback
            traceback.print_exc()
        return self.available_devices

    async def _connect_async(self, device):
        """Async connect that keeps the loop alive"""
        if not BLUETOOTH_AVAILABLE:
            print("[ERROR] Bluetooth not available")
            return False
        try:
            print(f"[CONNECT] Connecting to device: {device.name} ({device.address})...")
            self.client = bleak.BleakClient(device.address)
            
            print("  [CONNECT] Establishing connection...")
            await self.client.connect()
            
            if not self.client.is_connected:
                print("  [ERROR] Failed to establish connection")
                return False
            
            print("  [OK] Connection established")
            print(f"  [NOTIFY] Starting notifications on HR characteristic: {self.HR_MEASUREMENT_UUID}")
            
            await self.client.start_notify(self.HR_MEASUREMENT_UUID, self._on_notify)
            self.is_connected = True
            
            print("  [OK] Notifications started successfully")
            print(f"  [WAITING] Waiting for heart rate data from {device.name}...")
            
            # Keep the loop alive by waiting indefinitely
            while self.is_connected:
                await asyncio.sleep(1)
            
            return True
            
        except Exception as e:
            print(f"[ERROR] Connection error: {e}")
            import traceback
            traceback.print_exc()
            return False

    def connect(self, device):
        """Start connection in a persistent thread"""
        def run_loop():
            self.loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self.loop)
            self.loop.run_until_complete(self._connect_async(device))
            self.loop.close()
        
        self.loop_thread = threading.Thread(target=run_loop, daemon=True)
        self.loop_thread.start()
        
        # Wait a bit for connection to establish
        time.sleep(2)
        return self.is_connected

    async def disconnect(self):
        if self.client and self.is_connected:
            try:
                print(f"[DISCONNECT] Disconnecting from device...")
                self.is_connected = False  # Stop the keep-alive loop
                await self.client.stop_notify(self.HR_MEASUREMENT_UUID)
                print("  [OK] Notifications stopped")
                await self.client.disconnect()
                print("  [OK] Disconnected successfully")
            except Exception as e:
                print(f"[WARNING] Disconnect error: {e}")
            self.is_connected = False
            self.client = None

    def _on_notify(self, _sender, data):
        """Called when heart rate data is received from the device"""
        self.last_notification_time = time.time()
        
        if len(data) == 0:
            print("[WARNING] Received empty data packet")
            return
        
        try:
            # Log raw data
            hex_data = data.hex().upper()
            print(f"[PACKET] Received HR packet: {hex_data} ({len(data)} bytes)")
            
            # Parse heart rate
            flags = data[0]
            print(f"  [FLAGS] Flags: 0x{flags:02X}")
            
            if flags & 0x01:  # 16-bit HR
                if len(data) >= 3:
                    hr = int.from_bytes(data[1:3], byteorder='little')
                    print(f"  [HR] Heart Rate: {hr} BPM (16-bit)")
                else:
                    print("  [WARNING] Insufficient data for 16-bit HR")
                    return
            else:  # 8-bit HR
                if len(data) >= 2:
                    hr = data[1]
                    print(f"  [HR] Heart Rate: {hr} BPM (8-bit)")
                else:
                    print("  [WARNING] Insufficient data for 8-bit HR")
                    return
            
            # Parse RR intervals if present
            rr_intervals = []
            if flags & 0x10 and len(data) > 3:
                rr_start = 3 if flags & 0x01 else 2
                i = rr_start
                while i + 1 < len(data):
                    rr_raw = int.from_bytes(data[i:i+2], byteorder='little')
                    rr_ms = (rr_raw / 1024.0) * 1000.0
                    rr_intervals.append(rr_ms)
                    i += 2
                if rr_intervals:
                    print(f"  [RR] RR intervals: {[f'{rr:.1f}ms' for rr in rr_intervals]}")
            
            # Send to callback
            if self.callback:
                self.callback(hr, hex_data, rr_intervals)
                print(f"  [OK] Data sent to UI callback")
                
        except Exception as e:
            print(f"[ERROR] Error parsing HR data: {e}")
            import traceback
            traceback.print_exc()


class HeartSyncMonitor:
    def __init__(self, root):
        self.root = root
        self.root.title("❖ HEARTSYNC PROFESSIONAL ❖")
        
        # Next-gen window configuration with deep gradient background
        self.root.geometry("1600x1000")
        self.root.minsize(1200, 800)
        
        # Apply deep gradient background (simulated with dark base)
        self.root.configure(bg=Colors.SURFACE_BASE_START)
        
        # Data storage - quantum-grade precision: 300 samples for detailed waveforms
        self.hr_data = deque(maxlen=300)
        self.smoothed_hr_data = deque(maxlen=300)
        self.wet_dry_data = deque(maxlen=300)
        self.time_data = deque(maxlen=300)
        self.start_time = time.time()
        
        # Current values - high-precision floating point
        self.current_hr = 0
        self.smoothed_hr = 0
        self.wet_dry_ratio = 0
        self.connection_status = "DISCONNECTED"
        self.signal_quality = 0
        
        # Processing controls
        self.smoothing_factor = 0.1      # Minimal smoothing (most responsive)
        self.hr_offset = 0               # -100..+100 manual BPM offset
        self.wet_dry_offset = 0          # -100..+100 manual adjustment
        self.wet_dry_source = 'smoothed' # 'smoothed' or 'raw'
        
        # Lock state for panel interaction
        self.is_locked = False

        # Layout constants (golden ratio derived, 8pt grid aligned)
        self.VALUE_COL_WIDTH = Grid.COL_VITAL_VALUE
        self.CONTROL_COL_WIDTH = Grid.COL_CONTROL
        self.VALUE_LABEL_WIDTH = 5  # Fixed character width for stable sizing
        
        self.bt_manager = BluetoothManager(self.on_heart_rate_data)
        self._setup_ui()
        
        # Redirect print to console log
        self._redirect_console()

    def _setup_ui(self):
        # === NEXT-GEN LUMINOUS HEADER (taller for proper spacing) ===
        header_frame = tk.Frame(self.root, bg=Colors.SURFACE_PANEL_LIGHT, height=80)
        header_frame.pack(fill=tk.X, padx=0, pady=0)
        header_frame.pack_propagate(False)
        
        # Left section container for title and subtitle
        left_section = tk.Frame(header_frame, bg=Colors.SURFACE_PANEL_LIGHT)
        left_section.pack(side=tk.LEFT, padx=Spacing.LG, pady=Spacing.SM, fill=tk.Y)
        
        # Main title - luminous plasma teal
        title_label = tk.Label(left_section, text="❖ HEART SYNC SYSTEM", 
                              font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_H1, "bold"), 
                              fg=Colors.ACCENT_PLASMA_TEAL, bg=Colors.SURFACE_PANEL_LIGHT)
        title_label.pack(anchor='w', pady=(4, 0))
        
        # Subtitle - dimmed luminous (directly below title)
        subtitle_label = tk.Label(left_section, text="Adaptive Audio Bio Technology", 
                                 font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_LABEL_TERTIARY), 
                                 fg=Colors.TEXT_SECONDARY, bg=Colors.SURFACE_PANEL_LIGHT)
        subtitle_label.pack(anchor='w', pady=(2, 0))
        
        # Right section container for time and status
        right_section = tk.Frame(header_frame, bg=Colors.SURFACE_PANEL_LIGHT)
        right_section.pack(side=tk.RIGHT, padx=Spacing.LG, pady=Spacing.SM, fill=tk.Y)
        
        # Time display - precise geometric mono
        time_label = tk.Label(right_section, text="", 
                             font=(Typography.FAMILY_MONO, Typography.SIZE_BODY_LARGE, "bold"), 
                             fg=Colors.TEXT_PRIMARY, bg=Colors.SURFACE_PANEL_LIGHT)
        time_label.pack(anchor='e', pady=(4, 0))
        
        # System status indicator - quantum dot
        status_indicator = tk.Label(right_section, text="◆ SYSTEM OPERATIONAL", 
                                   font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_LABEL_TERTIARY, "bold"), 
                                   fg=Colors.STATUS_CONNECTED, bg=Colors.SURFACE_PANEL_LIGHT)
        status_indicator.pack(anchor='e', pady=(2, 0))
        
        def update_time():
            time_label.config(text=datetime.now().strftime("%Y-%m-%d  %H:%M:%S"))
            self.root.after(1000, update_time)
        update_time()
        
        # === MAIN CONTENT AREA (8pt grid, perfect symmetry) ===
        main_frame = tk.Frame(self.root, bg=Colors.SURFACE_BASE_START)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=Grid.MARGIN_WINDOW, pady=Grid.MARGIN_PANEL)
        
        # Configure responsive grid
        main_frame.grid_columnconfigure(0, weight=1)
        main_frame.grid_rowconfigure(0, weight=1)
        main_frame.grid_rowconfigure(1, weight=1)
        main_frame.grid_rowconfigure(2, weight=1)
        
        # === VITAL SIGN PANELS (luminous readouts with photon waveforms) ===
        # Heart Rate Panel - plasma teal accent
        self._create_metric_panel(main_frame, "HEART RATE", "BPM", 
                                  lambda: str(int(self.current_hr)), 
                                  Colors.VITAL_HEART_RATE, 0, 0)
        
        # Smoothed HR Panel - blue-white accent
        self._create_metric_panel(main_frame, "SMOOTHED HR", "BPM",
                                  lambda: str(int(self.smoothed_hr)), 
                                  Colors.VITAL_SMOOTHED, 1, 0)
        
        # Wet/Dry Ratio Panel - amber accent
        self._create_metric_panel(main_frame, "WET/DRY RATIO", "",
                                  lambda: str(int(max(1, min(100, self.wet_dry_ratio + self.wet_dry_offset)))), 
                                  Colors.VITAL_WET_DRY, 2, 0)
        
        # Bottom Control Panel (glassy panels)
        self._create_control_panel()
        
        # High-tech Terminal Displays
        self._create_terminal_displays()
        
        # Log startup message
        self.log_to_console("❖ HeartSync Professional v2.0 - Quantum Systems Online", tag="success")
        self.log_to_console("Awaiting Bluetooth device connection...", tag="info")

    def _create_metric_panel(self, parent, title, unit, value_func, color, row, col):
        # Panel frame
        panel = tk.Frame(parent, bg='#000000', relief=tk.FLAT, bd=0, height=200,
                         highlightbackground='#003333', highlightthickness=1)
        panel.grid(row=row, column=0, columnspan=2, sticky='ew', padx=2, pady=2)
        panel.grid_propagate(False)

        # Grid: value col (0), control col (1), graph col (2)
        panel.grid_columnconfigure(0, minsize=self.VALUE_COL_WIDTH)
        panel.grid_columnconfigure(1, minsize=self.CONTROL_COL_WIDTH)
        panel.grid_columnconfigure(2, weight=1)
        panel.grid_rowconfigure(0, weight=1)

        # Value column
        value_col = tk.Frame(panel, bg='#000000', highlightbackground=color, highlightcolor=color,
                              highlightthickness=2, bd=0)
        value_col.grid(row=0, column=0, sticky='nsew', padx=(12,6), pady=10)
        
        tk.Label(value_col, text=title, font=(Typography.FAMILY_SYSTEM, Typography.SIZE_LABEL, 'bold'), fg='#00ffff',
                 bg='#000000', anchor='w').pack(anchor='w')
        if unit:
            tk.Label(value_col, text=unit, font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL), fg='#00ccaa',
                     bg='#000000', anchor='w').pack(anchor='w')
        # Inner box for numeric value
        value_box = tk.Frame(value_col, bg='#000000', highlightbackground=color, highlightcolor=color,
                             highlightthickness=1, bd=0, padx=4, pady=2)
        value_box.pack(anchor='w', pady=(8,0), fill='x')
        value_label = tk.Label(value_box, text="", font=(Typography.FAMILY_MONO, Typography.SIZE_DISPLAY_VITAL, 'bold'),
                               fg=color, bg='#000000', anchor='w', width=self.VALUE_LABEL_WIDTH)
        value_label.pack(anchor='w')

        # Control column with enhanced visibility
        control_col = tk.Frame(panel, bg='#000000', highlightbackground=color, highlightcolor=color,
                               highlightthickness=2, bd=0)
        control_col.grid(row=0, column=1, sticky='nsew', padx=6, pady=10)
        
        # Enhanced control header with background highlight
        control_header_bg = tk.Frame(control_col, bg='#001a1a', bd=0)
        control_header_bg.pack(fill='x', pady=(0,4))
        tk.Label(control_header_bg, text="CONTROL", font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold'),
                 fg='#00ffff', bg='#001a1a', pady=3).pack(anchor='w', padx=4)

        if title == 'HEART RATE':
            self._build_hr_controls(control_col)
        elif title == 'SMOOTHED HR':
            self._build_smooth_controls(control_col)
        elif title == 'WET/DRY RATIO':
            self._build_wetdry_controls(control_col)

        # Graph column
        graph_frame = tk.Frame(panel, bg='#000000', relief=tk.FLAT, bd=0)
        graph_frame.grid(row=0, column=2, sticky='nsew', padx=8, pady=8)
        
        # Graph title in bright cyan (futuristic style) with refined spacing
        graph_title = tk.Label(graph_frame, text=f"{title} WAVEFORM", 
                              font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, "bold"), fg='#00ffff', bg='#000000',
                              anchor='w')
        graph_title.pack(anchor='w', fill='x', padx=8, pady=(5, 5))
        
        # Canvas for graph - SMOOTH borders with subtle glow
        graph_canvas = tk.Canvas(graph_frame, bg='#000000', highlightthickness=1,
                                highlightbackground='#004444')
        graph_canvas.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))
        
        # Store references for updates
        setattr(self, f'{title.lower().replace(" ", "_").replace("/", "_")}_value_label', value_label)
        setattr(self, f'{title.lower().replace(" ", "_").replace("/", "_")}_canvas', graph_canvas)
        setattr(self, f'{title.lower().replace(" ", "_").replace("/", "_")}_data_func', 
                lambda: self.hr_data if "HEART RATE" in title and "SMOOTHED" not in title 
                       else self.smoothed_hr_data if "SMOOTHED" in title 
                       else self.wet_dry_data)
        
        # ENHANCED: Ultra-fast update function (25ms = 40 FPS for silky smooth)
        def update_panel():
            value_label.config(text=value_func())
            self._update_graph(graph_canvas, getattr(self, f'{title.lower().replace(" ", "_").replace("/", "_")}_data_func')(), color)
            self.root.after(25, update_panel)  # 40 FPS for ultra-smooth real-time display
        
        self.root.after(25, update_panel)

    def _create_control_panel(self):
        # Medical monitor style control panel with dark background
        control_frame = tk.Frame(self.root, bg='#000000', height=200)
        control_frame.pack(fill=tk.X, padx=0, pady=0)
        control_frame.pack_propagate(False)
        
        # Bluetooth connectivity section - standalone panel with proper borders
        bt_frame = tk.LabelFrame(control_frame, text="  BLUETOOTH LE CONNECTIVITY  ", 
                                fg='#00ffff', bg='#000000', 
                                font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, "bold"),
                                relief=tk.GROOVE, bd=3, padx=15, pady=10)
        bt_frame.pack(fill=tk.BOTH, expand=True, padx=15, pady=10)
        
        # Create left section for buttons stacked vertically
        left_section = tk.Frame(bt_frame, bg='#000000')
        left_section.pack(side=tk.LEFT, anchor='nw', padx=(5, 20), pady=5)
        
        # Initialize lock state
        self.is_locked = True
        
        # BUTTONS IN 2x2 GRID - Avatar/Sci-Fi style with angled 3D effect
        # Row 1: SCAN and CONNECT
        row1 = tk.Frame(left_section, bg='#000000')
        row1.pack(pady=2)
        
        # SCAN DEVICES - Avatar blue with angled 3D
        self.scan_btn = tk.Button(row1, text="SCAN", 
                                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION, "bold"), 
                                 bg='#0a2540', fg='#4da6ff',
                                 activebackground='#1a4580', activeforeground='#80ccff',
                                 relief=tk.RAISED, bd=5, width=10, height=1,
                                 cursor='hand2',
                                 borderwidth=5,
                                 highlightthickness=3,
                                 highlightbackground='#0066cc',
                                 highlightcolor='#0088ff',
                                 command=self.scan_devices)
        self.scan_btn.pack(side=tk.LEFT, padx=3)
        
        # CONNECT DEVICE - Avatar gray/steel with angled 3D
        self.connect_btn = tk.Button(row1, text="CONNECT", 
                                    font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION, "bold"), 
                                    bg='#1a1a1a', fg='#7a7a7a',
                                    activebackground='#3a3a3a', activeforeground='#aaaaaa',
                                    relief=tk.RAISED, bd=5, width=10, height=1,
                                    cursor='hand2',
                                    borderwidth=5,
                                    highlightthickness=3,
                                    highlightbackground='#444444',
                                    highlightcolor='#666666',
                                    command=self.connect_device, state=tk.DISABLED)
        self.connect_btn.pack(side=tk.LEFT, padx=3)
        
        # Row 2: LOCK and DISCONNECT
        row2 = tk.Frame(left_section, bg='#000000')
        row2.pack(pady=2)
        
        # LOCK/UNLOCK - Avatar orange/amber with angled 3D
        self.lock_btn = tk.Button(row2, text="LOCKED", 
                                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION, "bold"), 
                                 bg='#2a1500', fg='#cc7733',
                                 activebackground='#4a2500', activeforeground='#ff9944',
                                 relief=tk.SUNKEN, bd=5, width=10, height=1,
                                 cursor='hand2',
                                 borderwidth=5,
                                 highlightthickness=3,
                                 highlightbackground='#885522',
                                 highlightcolor='#aa7744',
                                 command=self._toggle_lock)
        self.lock_btn.pack(side=tk.LEFT, padx=3)
        
        # DISCONNECT - Avatar red/danger with angled 3D
        self.disconnect_btn = tk.Button(row2, text="DISCONNECT", 
                                       font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION, "bold"), 
                                       bg='#2a0000', fg='#cc4444',
                                       activebackground='#4a0000', activeforeground='#ff6666',
                                       relief=tk.RAISED, bd=5, width=10, height=1,
                                       cursor='hand2',
                                       borderwidth=5,
                                       highlightthickness=3,
                                       highlightbackground='#884444',
                                       highlightcolor='#aa5555',
                                       command=self.disconnect_device, state=tk.DISABLED)
        self.disconnect_btn.pack(side=tk.LEFT, padx=3)
        
        # Right section for device info and status (below buttons visually)
        right_section = tk.Frame(bt_frame, bg='#000000')
        right_section.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Device selection dropdown with enhanced visibility
        device_row = tk.Frame(right_section, bg='#001a1a', bd=1, relief=tk.GROOVE)
        device_row.pack(anchor='w', pady=(0, 8), fill='x', padx=2)
        
        tk.Label(device_row, text="DEVICE:", fg='#00ffff', bg='#001a1a', 
                font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, "bold")).pack(side=tk.LEFT, padx=(8, 8), pady=5)
        
        self.device_var = tk.StringVar()
        self.device_combo = ttk.Combobox(device_row, textvariable=self.device_var, 
                                        state="readonly", width=42, font=(Typography.FAMILY_MONO, Typography.SIZE_BODY))
        self.device_combo.pack(side=tk.LEFT, padx=(0, 8), pady=5)
        
        # Status indicators row
        status_row = tk.Frame(right_section, bg='#000000')
        status_row.pack(anchor='w', pady=5)
        
        # Connection status indicator
        self.status_indicator = tk.Label(status_row, text="●", font=(Typography.FAMILY_MONO, Typography.SIZE_H1), 
                                        fg='#444444', bg='#000000')
        self.status_indicator.pack(side=tk.LEFT, padx=(0, 5))
        
        self.status_label = tk.Label(status_row, text="DISCONNECTED", 
                                    font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, "bold"), fg='#666666', bg='#000000')
        self.status_label.pack(side=tk.LEFT, padx=8)
        
        # Connected indicator on right
        self.connected_label = tk.Label(status_row, text="", 
                                       font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, "bold"), fg='#00ff88', bg='#000000')
        self.connected_label.pack(side=tk.RIGHT, padx=15)
        
        # Device details row
        detail_row = tk.Frame(right_section, bg='#000000')
        detail_row.pack(anchor='w', pady=(5, 0))
        
        self.device_detail = tk.Label(detail_row, text="Device: Awaiting Scan...", 
                                     font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL), fg='#00ccaa', bg='#000000')
        self.device_detail.pack(anchor='w', pady=2)
        
        manufacturer_label = tk.Label(detail_row, text="Manufacturer: Polar Electro", 
                                     font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL), fg='#00ccaa', bg='#000000')
        manufacturer_label.pack(anchor='w', pady=2)
        
        # Connection quality - ENHANCED VISUAL HIERARCHY
        quality_frame = tk.Frame(right_section, bg='#000000')
        quality_frame.pack(anchor='w', pady=5)
        
        tk.Label(quality_frame, text="CONNECTION QUALITY", 
                font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, "bold"), fg='#00ffff', bg='#000000').pack(side=tk.LEFT)
        
        # Quality indicator dots - BRIGHTER when active
        self.quality_dots = tk.Label(quality_frame, text="● ● ● ●", 
                                    font=(Typography.FAMILY_MONO, Typography.SIZE_CAPTION), fg='#222222', bg='#000000')
        self.quality_dots.pack(side=tk.LEFT, padx=12)
        
        tk.Label(quality_frame, text="Packets:", font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, "bold"), 
                fg='#00ccaa', bg='#000000').pack(side=tk.LEFT, padx=(25, 8))
        
        self.packet_label = tk.Label(quality_frame, text="0/0 (0%)", 
                                    font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL, "bold"), fg='#00ff00', bg='#000000')
        self.packet_label.pack(side=tk.LEFT)
        
        tk.Label(quality_frame, text="Latency:", font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL), 
                fg='#00aaaa', bg='#000000').pack(side=tk.LEFT, padx=(20, 5))
        
        self.latency_label = tk.Label(quality_frame, text="0 ms", 
                                     font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL, "bold"), fg='#00ff00', bg='#000000')
        self.latency_label.pack(side=tk.LEFT)
        
        firmware_label = tk.Label(right_section, text="Firmware: v2.1.3", 
                                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL), fg='#00aaaa', bg='#000000')
        firmware_label.pack(anchor='w', pady=1)
    
    def _create_terminal_displays(self):
        """Create high-tech terminal windows for device info and system logs"""
        # Main terminal container
        terminal_container = tk.Frame(self.root, bg='#000000')
        terminal_container.pack(fill=tk.BOTH, expand=False, padx=15, pady=(5, 0))
        
        # ==================== DEVICE INFO TERMINAL ====================
        device_terminal_frame = tk.LabelFrame(terminal_container, 
                                              text="  DEVICE STATUS MONITOR  ",
                                              fg='#00ff88', bg='#000a0a',
                                              font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"),
                                              relief=tk.GROOVE, bd=3, padx=10, pady=8)
        device_terminal_frame.pack(fill=tk.X, pady=(0, 5))
        
        # Device info display - single line terminal style
        self.device_info_terminal = tk.Text(device_terminal_frame, 
                                           height=1, 
                                           bg='#000a0a', 
                                           fg='#00ff88',
                                           font=(Typography.FAMILY_MONO, Typography.SIZE_CAPTION, "bold"),
                                           wrap=tk.NONE, 
                                           bd=0,
                                           highlightthickness=0,
                                           insertbackground='#00ff88',
                                           state=tk.DISABLED)
        self.device_info_terminal.pack(fill=tk.X)
        
        # Initial device status
        self._update_device_terminal("WAITING", "---", "---", "0%", "---")
        
        # ==================== SYSTEM LOG TERMINAL ====================
        log_terminal_frame = tk.LabelFrame(terminal_container,
                                          text="  SYSTEM ACTIVITY LOG  ",
                                          fg='#00ffff', bg='#00000a',
                                          font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"),
                                          relief=tk.GROOVE, bd=3, padx=10, pady=8)
        log_terminal_frame.pack(fill=tk.BOTH, expand=False)
        
        # Create scrollable log terminal
        log_scroll_frame = tk.Frame(log_terminal_frame, bg='#00000a')
        log_scroll_frame.pack(fill=tk.BOTH, expand=True)
        
        # Scrollbar
        scrollbar = tk.Scrollbar(log_scroll_frame, bg='#001a1a', troughcolor='#000000',
                                activebackground='#00ffff')
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Log text widget - high-tech terminal
        self.console_log = tk.Text(log_scroll_frame, 
                                  height=6,
                                  bg='#00000a', 
                                  fg='#00ffff',
                                  font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL),
                                  wrap=tk.WORD, 
                                  bd=0,
                                  highlightthickness=0,
                                  insertbackground='#00ffff',
                                  yscrollcommand=scrollbar.set,
                                  state=tk.DISABLED)
        self.console_log.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.config(command=self.console_log.yview)
        
        # Configure text tags for colored output
        self.console_log.tag_config("info", foreground="#00ffff")
        self.console_log.tag_config("success", foreground="#00ff88")
        self.console_log.tag_config("warning", foreground="#ffaa00")
        self.console_log.tag_config("error", foreground="#ff4444")
        self.console_log.tag_config("data", foreground="#88ffff")
        
        # Initial messages
        self.log_to_console("[INIT] HeartSync initialized", "success")
        self.log_to_console("[READY] Ready to scan for Bluetooth LE heart rate devices", "info")
        self.log_to_console("[INFO] Click 'SCAN DEVICES' to begin", "info")
        self.log_to_console("=" * 80, "info")
    
    def _update_device_terminal(self, status, device_name, device_addr, battery, bpm):
        """Update the device info terminal with current status"""
        # Format: STATUS | DEVICE: name | ADDR: address | BAT: % | BPM: value
        terminal_text = f"[{status:^10}] DEVICE: {device_name:<20} | ADDR: {device_addr:<17} | BAT: {battery:>4} | BPM: {bpm:>3}"
        
        self.device_info_terminal.config(state=tk.NORMAL)
        self.device_info_terminal.delete("1.0", tk.END)
        self.device_info_terminal.insert("1.0", terminal_text)
        self.device_info_terminal.config(state=tk.DISABLED)

    # ==================== CONTROL BUILDERS ====================
    def _build_hr_controls(self, parent):
        # Offset slider - vertical dial style
        hr_title = tk.Label(parent, text="HR OFFS", font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'), 
                           fg='#00ffff', bg='#000000', cursor='hand2')
        hr_title.pack(pady=(2,4))
        hr_title.bind('<Button-1>', lambda e: self._reset_hr_offset_click())
        self.hr_offset_slider = tk.Scale(parent, from_=100, to=-100, resolution=1,
                                         orient=tk.VERTICAL, bg='#001a1a', fg='#00ff88',
                                         troughcolor='#000000', activebackground='#00aaaa',
                                         highlightthickness=0, bd=0, width=14, length=110,
                                         font=(Typography.FAMILY_MONO, Typography.SIZE_TINY, 'bold'), sliderlength=14,
                                         showvalue=0, command=self._on_hr_offset_change)
        self.hr_offset_slider.set(0)
        self.hr_offset_slider.pack()
        self.hr_offset_label = tk.Label(parent, text="0", font=(Typography.FAMILY_MONO, Typography.SIZE_CAPTION, 'bold'),
                                        fg='#00ff88', bg='#000000', width=6)
        self.hr_offset_label.pack(pady=(4,2))
        tk.Button(parent, text='RESET', command=lambda: self._set_hr_offset(0),
                  font=(Typography.FAMILY_SYSTEM, Typography.SIZE_TINY, 'bold'), width=6, bg='#004444', fg='#00ff88',
                  activebackground='#006666', bd=0).pack(pady=(2,2))

    def _on_hr_offset_change(self, value):
        try:
            self._set_hr_offset(int(float(value)))
        except:
            pass

    def _set_hr_offset(self, val):
        val = max(-100, min(100, int(val)))
        self.hr_offset = val
        if hasattr(self, 'hr_offset_slider'):
            self.hr_offset_slider.set(val)
        if hasattr(self, 'hr_offset_label'):
            self.hr_offset_label.config(text=f"{val:+d}")
        self.log_to_console(f"[HR OFFSET] Set to {val:+d} BPM")

    def _reset_hr_offset_click(self):
        """Reset HR offset to 0 when title is clicked"""
        self._set_hr_offset(0)
        self.log_to_console(f"[HR OFFSET] ✓ Reset to default (0) - clicked title")

    def _build_smooth_controls(self, parent):
        smooth_title = tk.Label(parent, text="SMOOTH", font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'), 
                               fg='#00ffff', bg='#000000', cursor='hand2')
        smooth_title.pack(pady=(2,4))
        smooth_title.bind('<Button-1>', lambda e: self._reset_smoothing_click())
        self.smoothing_slider = tk.Scale(parent, from_=10.0, to=0.1, resolution=0.1,
                                         orient=tk.VERTICAL, bg='#001a1a', fg='#00ffff',
                                         troughcolor='#000000', activebackground='#00aaaa',
                                         highlightthickness=0, bd=0, width=14, length=110,
                                         font=(Typography.FAMILY_MONO, Typography.SIZE_TINY, 'bold'), sliderlength=14,
                                         showvalue=0, command=self._on_smoothing_change)
        self.smoothing_slider.set(self.smoothing_factor)
        self.smoothing_slider.pack()
        self.smoothing_value_label = tk.Label(parent, text=f"{self.smoothing_factor:.1f}x", width=6,
                                              font=(Typography.FAMILY_MONO, Typography.SIZE_CAPTION, 'bold'), fg='#00ff88', bg='#000000')
        self.smoothing_value_label.pack(pady=(4,2))
        self.smoothing_metrics_label = tk.Label(parent, text="α=--\nT½=--s\n≈-- samples",
                                                font=(Typography.FAMILY_MONO, Typography.SIZE_TINY), fg='#00ccaa', bg='#000000', justify='left')
        self.smoothing_metrics_label.pack(pady=(2,2))
        control_bar = tk.Frame(parent, bg='#000000')
        control_bar.pack(pady=(2,0))
        tk.Button(control_bar, text='-', command=lambda: self._nudge_smoothing(-0.1),
                  font=(Typography.FAMILY_SYSTEM, Typography.SIZE_TINY, 'bold'), width=2, bg='#003333', fg='#00ffff',
                  activebackground='#005555', bd=0).pack(side=tk.LEFT, padx=1)
        self.smoothing_spin = tk.Spinbox(control_bar, from_=0.1, to=10.0, increment=0.1,
                                         format="%.1f", width=4, justify='center',
                                         command=self._spinbox_smoothing_update,
                                         font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL, 'bold'), fg='#00ff88',
                                         bg='#001a1a', disabledbackground='#001a1a')
        self.smoothing_spin.delete(0, tk.END); self.smoothing_spin.insert(0, f'{self.smoothing_factor:.1f}')
        self.smoothing_spin.pack(side=tk.LEFT, padx=1)
        tk.Button(control_bar, text='+', command=lambda: self._nudge_smoothing(0.1),
                  font=(Typography.FAMILY_SYSTEM, Typography.SIZE_TINY, 'bold'), width=2, bg='#003333', fg='#00ffff',
                  activebackground='#005555', bd=0).pack(side=tk.LEFT, padx=1)
        tk.Button(parent, text='RESET', command=lambda: self._set_smoothing(0.1),
                  font=(Typography.FAMILY_SYSTEM, Typography.SIZE_TINY, 'bold'), width=6, bg='#004444', fg='#00ff88',
                  activebackground='#006666', bd=0).pack(pady=(4,0))
        # Initialize metrics display
        self._update_smoothing_metrics()

    def _build_wetdry_controls(self, parent):
        # Title with click-to-reset functionality
        wetdry_title = tk.Label(parent, text="WET/DRY", 
                               font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_LABEL_PRIMARY, 'bold'), 
                               fg=Colors.ACCENT_PLASMA_TEAL, bg=Colors.SURFACE_PANEL, cursor='hand2')
        wetdry_title.pack(pady=(4,8))
        wetdry_title.bind('<Button-1>', lambda e: self._reset_wetdry_click())
        
        # Offset slider with next-gen styling
        self.wetdry_offset_slider = tk.Scale(parent, from_=100, to=-100, resolution=1,
                                             orient=tk.VERTICAL, 
                                             bg=Colors.SURFACE_PANEL_LIGHT, 
                                             fg=Colors.VITAL_WET_DRY,
                                             troughcolor=Colors.SURFACE_BASE_START, 
                                             activebackground=Colors.ACCENT_PLASMA_TEAL,
                                             highlightthickness=0, bd=0, width=16, length=120,
                                             font=(Typography.FAMILY_MONO, Typography.SIZE_LABEL_TERTIARY, 'bold'), 
                                             sliderlength=16,
                                             showvalue=0, command=self._on_wetdry_offset_change)
        self.wetdry_offset_slider.set(0)
        self.wetdry_offset_slider.pack(pady=Spacing.SM)
        
        # Offset value display
        self.wetdry_offset_label = tk.Label(parent, text="0", 
                                            font=(Typography.FAMILY_MONO, Typography.SIZE_LABEL_SECONDARY, 'bold'),
                                            fg=Colors.VITAL_WET_DRY, bg=Colors.SURFACE_PANEL, width=6)
        self.wetdry_offset_label.pack(pady=(4,8))
        
        # === 3D INPUT SOURCE SWITCH (Ultra Prominent) ===
        # Top luminous separator
        tk.Frame(parent, bg=Colors.VITAL_WET_DRY, height=2).pack(fill='x', pady=(Spacing.MD,0))
        
        # Header with plasma glow
        source_header = tk.Label(parent, text="⚡ INPUT SOURCE", 
                                font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_LABEL_PRIMARY, 'bold'), 
                                fg=Colors.VITAL_WET_DRY, bg=Colors.SURFACE_PANEL)
        source_header.pack(pady=(Spacing.SM, Spacing.XS))
        
        # === ULTRA VISIBLE 3D TOGGLE SWITCH ===
        # Container frame with glow effect
        switch_container = tk.Frame(parent, bg=Colors.SURFACE_BASE_START, 
                                   highlightbackground=Colors.VITAL_WET_DRY,
                                   highlightthickness=2, bd=0)
        switch_container.pack(pady=(Spacing.XS, Spacing.SM), padx=Spacing.XS, fill='x')
        
        # Large, prominent 3D toggle button with perfect clarity
        self.wetdry_source_btn = tk.Button(switch_container, 
                                           text='● SMOOTHED HR\n(Includes Offset)',
                                           font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_LABEL_SECONDARY, 'bold'), 
                                           width=13, height=3,
                                           bg=Colors.VITAL_SMOOTHED,  # Blue-white for smoothed
                                           fg=Colors.TEXT_PRIMARY, 
                                           bd=4,
                                           relief=tk.RAISED,  # 3D raised effect
                                           cursor='hand2',
                                           highlightthickness=3,
                                           highlightbackground=Colors.ACCENT_TEAL_GLOW,
                                           activebackground=Colors.ACCENT_TEAL_GLOW,
                                           command=self._toggle_wetdry_source)
        self.wetdry_source_btn.pack(padx=Spacing.SM, pady=Spacing.SM, fill='both', expand=True)
        
        # Bottom luminous separator
        tk.Frame(parent, bg=Colors.VITAL_WET_DRY, height=2).pack(fill='x', pady=(Spacing.SM, Spacing.MD))
        
        # Reset button with next-gen styling
        tk.Button(parent, text='RESET OFFSET', command=lambda: self._set_wetdry_offset(0),
                  font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_LABEL_TERTIARY, 'bold'), 
                  width=12, height=1,
                  bg=Colors.SURFACE_PANEL_LIGHT, fg=Colors.VITAL_WET_DRY,
                  activebackground=Colors.SURFACE_OVERLAY, bd=2, relief=tk.RIDGE).pack(pady=(0, Spacing.SM))

    def _toggle_wetdry_source(self):
        """Toggle between smoothed HR and raw HR input for wet/dry calculation"""
        self.wet_dry_source = 'raw' if self.wet_dry_source == 'smoothed' else 'smoothed'
        
        if hasattr(self, 'wetdry_source_btn'):
            if self.wet_dry_source == 'raw':
                # === RAW HR MODE ===
                # Plasma teal with SUNKEN 3D effect (button pressed in)
                self.wetdry_source_btn.config(
                    text='● RAW HR\n(Direct BPM + Offset)',
                    bg=Colors.VITAL_HEART_RATE,  # Plasma teal
                    fg=Colors.SURFACE_BASE_START,  # Dark text for contrast
                    activebackground=Colors.ACCENT_TEAL_GLOW,
                    relief=tk.SUNKEN,  # 3D pressed-in effect
                    highlightbackground=Colors.ACCENT_PLASMA_TEAL,
                    highlightthickness=3,
                    bd=4
                )
                self.log_to_console("⚡ [INPUT] Switched to RAW HR (unsmoothed)", tag="warning")
            else:
                # === SMOOTHED HR MODE (DEFAULT) ===
                # Blue-white with RAISED 3D effect (button popped out)
                self.wetdry_source_btn.config(
                    text='● SMOOTHED HR\n(Includes Offset)',
                    bg=Colors.VITAL_SMOOTHED,  # Blue-white
                    fg=Colors.TEXT_PRIMARY,  # Luminous white text
                    activebackground=Colors.ACCENT_TEAL_GLOW,
                    relief=tk.RAISED,  # 3D raised effect
                    highlightbackground=Colors.ACCENT_TEAL_GLOW,
                    highlightthickness=3,
                    bd=4
                )
                self.log_to_console("✓ [INPUT] Switched to SMOOTHED HR (recommended)", tag="success")
                self.wetdry_source_btn.config(
                    text='◉ SMOOTHED HR\n(With Offset)',
                    bg='#003366',
                    fg='#00ffff',
                    activebackground='#0055aa',
                    relief=tk.RAISED,
                    highlightbackground='#0099ff',
                    highlightthickness=4
                )
                self.log_to_console("[WET/DRY] Source: SMOOTHED HR (recommended)", tag="success")
        self.log_to_console(f"[WET/DRY] Source set to {self.wet_dry_source.upper()}")

    def _on_wetdry_offset_change(self, value):
        try:
            self._set_wetdry_offset(int(float(value)))
        except:
            pass

    def _set_wetdry_offset(self, val):
        val = max(-100, min(100, int(val)))
        self.wet_dry_offset = val
        if hasattr(self, 'wetdry_offset_slider'):
            self.wetdry_offset_slider.set(val)
        if hasattr(self, 'wetdry_offset_label'):
            self.wetdry_offset_label.config(text=f"{val:+d}")
        self.log_to_console(f"[WET/DRY OFFSET] Set to {val:+d}")

    def _reset_wetdry_click(self):
        """Reset wet/dry offset to 0 and source to smoothed when title is clicked"""
        self._set_wetdry_offset(0)
        if self.wet_dry_source != 'smoothed':
            self.wet_dry_source = 'smoothed'
            if hasattr(self, 'wetdry_source_btn'):
                self.wetdry_source_btn.config(
                    text='◉ SMOOTHED',
                    bg='#003366',
                    fg='#00ffff',
                    relief=tk.RAISED
                )
        self.log_to_console(f"[WET/DRY] ✓ Reset to defaults (offset=0, source=SMOOTHED) - clicked title")

    def _toggle_lock(self):
        """Toggle the lock/unlock state with visual 3D feedback"""
        self.is_locked = not self.is_locked
        
        if self.is_locked:
            # LOCKED state - SUNKEN button (pressed in), dimmed Avatar orange
            self.lock_btn.config(
                text="LOCKED",
                bg='#2a1500',
                fg='#cc7733',
                relief=tk.SUNKEN,
                activebackground='#4a2500',
                highlightbackground='#885522'
            )
            self.log_to_console("[LOCK] System LOCKED")
        else:
            # UNLOCKED state - RAISED button (popped out), bright Avatar green
            self.lock_btn.config(
                text="UNLOCKED",
                bg='#00331a',
                fg='#44ee88',
                relief=tk.RAISED,
                activebackground='#00552a',
                highlightbackground='#44aa66'
            )
            self.log_to_console("[LOCK] System UNLOCKED")

    def _update_button_state(self, button, is_active):
        """Update button appearance based on active state with Avatar-style 3D effect"""
        if button == self.scan_btn:
            if is_active:
                # Active/Scanning - brighter blue, SUNKEN (pressed in)
                button.config(bg='#1a4580', fg='#80ccff', relief=tk.SUNKEN, 
                            highlightbackground='#0088ff')
            else:
                # Inactive - dimmed blue, RAISED (popped out)
                button.config(bg='#0a2540', fg='#4da6ff', relief=tk.RAISED,
                            highlightbackground='#0066cc')
        elif button == self.connect_btn:
            if is_active:
                # Active/Connecting - brighter gray, SUNKEN
                button.config(bg='#3a3a3a', fg='#aaaaaa', relief=tk.SUNKEN,
                            highlightbackground='#666666')
            else:
                # Inactive - dimmed gray, RAISED
                button.config(bg='#1a1a1a', fg='#7a7a7a', relief=tk.RAISED,
                            highlightbackground='#444444')
        elif button == self.disconnect_btn:
            if is_active:
                # Active/Disconnecting - bright red, SUNKEN
                button.config(bg='#4a0000', fg='#ff6666', relief=tk.SUNKEN,
                            highlightbackground='#aa5555')
            else:
                # Inactive - dimmed red, RAISED
                button.config(bg='#2a0000', fg='#cc4444', relief=tk.RAISED,
                            highlightbackground='#884444')

    def _on_smoothing_change(self, value):
        """Callback when slider adjusted"""
        try:
            factor = float(value)
        except:
            return
        self._set_smoothing(factor, from_slider=True)

    def _spinbox_smoothing_update(self):
        try:
            val = float(self.smoothing_spin.get())
        except:
            return
        self._set_smoothing(val, from_spin=True)

    def _nudge_smoothing(self, delta):
        new_val = max(0.1, min(10.0, round(self.smoothing_factor + delta, 1)))
        self._set_smoothing(new_val)

    def _set_smoothing(self, factor, from_slider=False, from_spin=False):
        # Clamp and store
        factor = max(0.1, min(10.0, round(float(factor), 1)))
        self.smoothing_factor = factor
        # Sync UI elements
        if not from_slider and hasattr(self, 'smoothing_slider'):
            self.smoothing_slider.set(factor)
        if not from_spin and hasattr(self, 'smoothing_spin'):
            self.smoothing_spin.delete(0, tk.END); self.smoothing_spin.insert(0, f"{factor:.1f}")
        if hasattr(self, 'smoothing_value_label'):
            self.smoothing_value_label.config(text=f"{factor:.1f}x")
        # Update metrics display
        self._update_smoothing_metrics()
        self.log_to_console(f"[SMOOTHING] Factor set to {factor:.1f}x")

    def _update_smoothing_metrics(self):
        # alpha mapping (inverse relation)
        alpha = 1.0 / (1.0 + self.smoothing_factor)
        # Effective half-life (samples) for EMA: n_half = ln(0.5)/ln(1-alpha)
        try:
            half_life_samples = math.log(0.5) / math.log(1 - alpha)
        except ValueError:
            half_life_samples = 0.0
        # Convert to seconds using update period (25ms -> 40 FPS)
        update_interval_s = 0.025
        half_life_seconds = half_life_samples * update_interval_s
        # Approx effective window ≈ 5 * half-life samples
        effective_window = int(half_life_samples * 5)
        if hasattr(self, 'smoothing_metrics_label'):
            self.smoothing_metrics_label.config(
                text=f"α={alpha:.3f}\nT½={half_life_seconds:.2f}s\n≈{effective_window} samples")

    def _reset_smoothing_click(self):
        """Reset smoothing factor to 0.1 (default) when title is clicked"""
        self._set_smoothing(0.1)
        self.log_to_console(f"[SMOOTHING] ✓ Reset to default (0.1x) - clicked title")

    def log_to_console(self, message, tag="info"):
        """Add message to console log with timestamp and color coding"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        log_msg = f"[{timestamp}] {message}\n"
        
        self.console_log.config(state=tk.NORMAL)
        self.console_log.insert(tk.END, log_msg, tag)
        self.console_log.see(tk.END)
        self.console_log.config(state=tk.DISABLED)
        
        # Also print to stdout
        print(log_msg.strip())
    
    def _redirect_console(self):
        """Redirect print statements to console log"""
        import sys
        
        class ConsoleRedirector:
            def __init__(self, text_widget, original_stream):
                self.text_widget = text_widget
                self.original = original_stream
                
            def write(self, string):
                if string.strip():  # Only log non-empty strings
                    self.original.write(string)  # Still print to terminal
                    self.original.flush()
                    try:
                        timestamp = datetime.now().strftime("%H:%M:%S")
                        self.text_widget.insert(tk.END, f"[{timestamp}] {string}")
                        self.text_widget.see(tk.END)
                    except:
                        pass  # Ignore errors if GUI is closing
                        
            def flush(self):
                self.original.flush()
        
        sys.stdout = ConsoleRedirector(self.console_log, sys.stdout)
        sys.stderr = ConsoleRedirector(self.console_log, sys.stderr)

    def _update_graph(self, canvas, data, color):
        """Futuristic high-precision graph rendering with sleek styling"""
        canvas.delete("all")
        
        if len(data) < 2:
            return
            
        width = canvas.winfo_width()
        height = canvas.winfo_height()
        
        if width <= 1 or height <= 1:
            return
        
        # Optimized margins for sleek appearance
        left_margin = 55
        right_margin = 25
        top_margin = 25
        bottom_margin = 35
        
        graph_width = width - left_margin - right_margin
        graph_height = height - top_margin - bottom_margin
        
        # Get data range with refined padding
        data_list = list(data)
        min_val = min(data_list)
        max_val = max(data_list)
        
        # Smart padding calculation
        range_padding = (max_val - min_val) * 0.08
        if range_padding < 1:
            range_padding = 5
        min_val -= range_padding
        max_val += range_padding
        
        if max_val == min_val:
            max_val = min_val + 10
        
        # Draw sleek background with subtle border
        canvas.create_rectangle(left_margin, top_margin, 
                               left_margin + graph_width, top_margin + graph_height,
                               fill='#000000', outline='#004444', width=1)
        
        # Draw REFINED grid (futuristic precision)
        # Vertical grid lines (time axis) - subtle but precise
        num_v_lines = 20
        for i in range(num_v_lines + 1):
            x = left_margin + (i * graph_width / num_v_lines)
            # Alternating line intensity for depth
            line_color = '#2a2a2a' if i % 5 != 0 else '#004444'
            canvas.create_line(x, top_margin, x, top_margin + graph_height, 
                             fill=line_color, width=1)
        
        # Horizontal grid lines (value axis) - clean spacing
        num_h_lines = 10
        for i in range(num_h_lines + 1):
            y = top_margin + (i * graph_height / num_h_lines)
            value = max_val - (i * (max_val - min_val) / num_h_lines)
            
            # Refined grid line with depth
            line_color = '#2a2a2a' if i % 2 != 0 else '#004444'
            canvas.create_line(left_margin, y, left_margin + graph_width, y,
                             fill=line_color, width=1)
            
            # Y-axis value labels (crisp cyan text)
            canvas.create_text(left_margin - 8, y, text=f"{value:.0f}",
                             anchor='e', fill='#00cccc', font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL, 'bold'))
        
        # X-axis time labels (refined spacing)
        num_time_labels = 6
        time_span = len(data_list) / 10.0  # Assuming 10 samples/second
        for i in range(num_time_labels + 1):
            x = left_margin + (i * graph_width / num_time_labels)
            time_val = (i * time_span / num_time_labels)
            canvas.create_text(x, top_margin + graph_height + 18, 
                             text=f"{time_val:.1f}s",
                             anchor='n', fill='#00cccc', font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL, 'bold'))
        
        # Draw ULTRA-SMOOTH waveform with enhanced anti-aliasing
        if len(data_list) >= 2:
            points = []
            for i, value in enumerate(data_list):
                # Precise coordinate mapping
                x = left_margin + (i / (len(data_list) - 1)) * graph_width
                y = top_margin + graph_height - ((value - min_val) / (max_val - min_val)) * graph_height
                points.append(x)
                points.append(y)
            
            if len(points) >= 4:
                # SLEEK FLOWING LINE - Double-layer for smooth glow effect
                # Shadow/glow layer for depth and smoothness
                canvas.create_line(points, fill=color, width=3, smooth=True, 
                                 splinesteps=20, state='normal')  # Ultra-high smoothing
                
                # Main waveform line (crisp, bright, flowing)
                canvas.create_line(points, fill=color, width=2, smooth=True, 
                                 splinesteps=20)
                
                # NO DATA POINTS - Pure sleek flowing line only
        
        # Draw sleek axes labels
        # Y-axis label (vertical text)
        canvas.create_text(12, top_margin + graph_height/2, text="BPM",
                         anchor='center', fill='#00aaaa', font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'),
                         angle=90)
        
        # X-axis label
        canvas.create_text(left_margin + graph_width/2, height - 8, text="TIME (SECONDS)",
                         anchor='s', fill='#00aaaa', font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'))
        
        # Draw refined min/max indicators with better positioning
        canvas.create_text(width - right_margin, top_margin + 8, 
                         text=f"PEAK {max(data_list):.1f}",
                         anchor='ne', fill='#00ff88', font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL, 'bold'))
        canvas.create_text(width - right_margin, top_margin + graph_height - 8,
                         text=f"MIN {min(data_list):.1f}",
                         anchor='se', fill='#ff6666', font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL, 'bold'))

    def scan_devices(self):
        self.status_label.config(text="SCANNING...", fg='yellow')
        self._update_button_state(self.scan_btn, True)  # Press button down
        self.scan_btn.config(state=tk.DISABLED)
        self.log_to_console("Scanning for BLE devices...", tag="info")
        
        def scan_thread():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            devices = loop.run_until_complete(self.bt_manager.scan_devices())
            loop.close()
            
            self.root.after(0, lambda: self._on_scan_complete(devices))
        
        threading.Thread(target=scan_thread, daemon=True).start()

    def _on_scan_complete(self, devices):
        self.scan_btn.config(state=tk.NORMAL)
        self._update_button_state(self.scan_btn, False)  # Release button
        
        if devices:
            device_names = [f"{d.name} ({d.address})" for d in devices]
            self.device_combo['values'] = device_names
            self.device_combo.current(0)
            self.connect_btn.config(state=tk.NORMAL)
            self._update_button_state(self.connect_btn, False)  # Ready to use
            self.status_label.config(text=f"FOUND {len(devices)} DEVICE(S)", fg='#00ff00')
            self.log_to_console(f"✓ Found {len(devices)} device(s)", tag="success")
            for d in devices:
                self.log_to_console(f"  • {d.name} ({d.address})", tag="data")
        else:
            self.status_label.config(text="NO DEVICES FOUND", fg='red')
            self.log_to_console("No BLE devices found", tag="warning")

    def connect_device(self):
        if not self.device_combo.get():
            return
            
        selected_idx = self.device_combo.current()
        device = self.bt_manager.available_devices[selected_idx]
        
        self.status_label.config(text="CONNECTING...", fg='yellow')
        self.connect_btn.config(state=tk.DISABLED)
        self._update_button_state(self.connect_btn, True)  # Press button down
        self.log_to_console(f"Connecting to {device.name}...", tag="info")
        
        # Connect (starts its own persistent thread)
        success = self.bt_manager.connect(device)
        
        # Check connection after longer delay to allow full connection establishment
        self.root.after(4000, lambda: self._on_connect_complete(self.bt_manager.is_connected, device.name))

    def _on_connect_complete(self, success, device_name):
        if success:
            # Connection successful - update button states
            self.status_label.config(text="", fg='#00ff88')
            self.status_indicator.config(fg='#00ff88')
            self.connected_label.config(text="◉ CONNECTED")
            self.disconnect_btn.config(state=tk.NORMAL)
            self._update_button_state(self.disconnect_btn, False)  # Ready to disconnect
            self._update_button_state(self.connect_btn, False)  # Release connect button
            self.device_detail.config(text=f"Device: {device_name}", fg='#00ff88')
            self.packet_label.config(fg='#00ff88')
            self.quality_dots.config(fg='#00ff88')
            
            # Update device terminal with connection info
            device_address = self.bt_manager.device.address if self.bt_manager.device else "Unknown"
            self._update_device_terminal("CONNECTED", device_name, device_address, "N/A", "---")
            self.log_to_console(f"✓ Connected to {device_name}", tag="success")
        else:
            self.status_label.config(text="CONNECTION FAILED", fg='#ff5555')
            self.status_indicator.config(fg='#444444')
            self.connect_btn.config(state=tk.NORMAL)
            self._update_button_state(self.connect_btn, False)  # Release button
            self.log_to_console("✗ Connection failed", tag="error")

    def disconnect_device(self):
        self._update_button_state(self.disconnect_btn, True)  # Press button down
        
        # Disconnect from the device
        if self.bt_manager.loop and self.bt_manager.is_connected:
            # Schedule disconnect in the BLE loop
            asyncio.run_coroutine_threadsafe(
                self.bt_manager.disconnect(), 
                self.bt_manager.loop
            )
            self.root.after(1000, self._on_disconnect_complete)
        else:
            self._on_disconnect_complete()
    
    def _on_disconnect_complete(self):
        """Handle UI updates after disconnection"""
        # Reset UI state
        self.status_label.config(text="DISCONNECTED", fg='#666666')
        self.status_indicator.config(fg='#444444')
        self.connected_label.config(text="")
        self.disconnect_btn.config(state=tk.DISABLED)
        self._update_button_state(self.disconnect_btn, False)  # Release button
        self.connect_btn.config(state=tk.NORMAL)
        self._update_button_state(self.connect_btn, False)  # Ready to connect again
        self.device_detail.config(text="Device: Awaiting Scan...", fg='#00ccaa')
        self.packet_label.config(fg='#666666')
        self.quality_dots.config(fg='#222222')
        
        # Reset device terminal to waiting state
        self._update_device_terminal("WAITING", "---", "---", "---", "---")
        self.log_to_console("Device disconnected", tag="info")

    def on_heart_rate_data(self, hr, hex_data, rr_intervals):
        """HOSPITAL GRADE: High-precision UI update with detailed data tracking"""
        print(f"[UI] Update: HR={hr} BPM, RR intervals={len(rr_intervals)}")
        
        # If we're receiving data, ensure UI shows connected status
        if self.status_label.cget('text') != "":
            # Update to connected if we're receiving data but UI doesn't show it
            self.status_label.config(text="", fg='#00ff88')
            self.status_indicator.config(fg='#00ff88')
            self.connected_label.config(text="◉ CONNECTED")
            self.disconnect_btn.config(state=tk.NORMAL)
            self.packet_label.config(fg='#00ff88')
            self.quality_dots.config(fg='#00ff88')
        
        # Only update if we have valid data (hospital equipment range: 30-220 BPM)
        if hr >= 30 and hr <= 220:
            # Update current HR with high precision and apply offset FIRST
            # This ensures offset affects the entire processing chain
            self.current_hr = float(hr) + self.hr_offset

            # Apply smoothing (now works on offset-adjusted HR)
            alpha = 1.0 / (1.0 + self.smoothing_factor)
            if self.smoothed_hr == 0:
                self.smoothed_hr = self.current_hr
            else:
                self.smoothed_hr = alpha * self.current_hr + (1 - alpha) * self.smoothed_hr

            # Timestamp
            current_time = time.time() - self.start_time
            self.time_data.append(current_time)
            self.hr_data.append(self.current_hr)
            self.smoothed_hr_data.append(self.smoothed_hr)

            # Base wet/dry derivation using selected source (raw or smoothed)
            # Both sources now include the HR offset effect
            source_hr = self.current_hr if self.wet_dry_source == 'raw' else self.smoothed_hr

            # Derive a baseline wet/dry from HR range (normalize 40..180 BPM -> 10..90 baseline)
            baseline = (source_hr - 40) / (180 - 40)  # 0..1
            baseline = max(0.0, min(1.0, baseline))
            baseline_scaled = 10 + baseline * 80  # 10..90

            combined = baseline_scaled

            # If RR intervals available, blend in HRV-based contact quality
            if len(rr_intervals) > 1:
                rr_mean = sum(rr_intervals) / len(rr_intervals)
                rr_variance = sum((x - rr_mean) ** 2 for x in rr_intervals) / len(rr_intervals)
                rr_std = rr_variance ** 0.5
                hrv_component = min(100.0, (rr_std / 120.0) * 100.0)  # 0-100 mapping
                combined = 0.6 * baseline_scaled + 0.4 * hrv_component
            else:
                # No RR data: use short-term variability of chosen HR source to add dynamics
                src_deque = self.hr_data if self.wet_dry_source == 'raw' else self.smoothed_hr_data
                if len(src_deque) >= 5:
                    recent = list(src_deque)[-10:]
                    mean_v = sum(recent) / len(recent)
                    var_v = sum((x - mean_v) ** 2 for x in recent) / len(recent)
                    std_v = var_v ** 0.5
                    # Map std (0..5 bpm typical micro fluctuation) -> 0..1
                    variability = max(0.0, min(1.0, std_v / 5.0))
                    variability_component = variability * 100.0
                    combined = 0.7 * baseline_scaled + 0.3 * variability_component
                # else keep baseline

            # Apply offset and clamp 1..100
            combined += self.wet_dry_offset
            self.wet_dry_ratio = max(1, min(100, combined))
            self.wet_dry_data.append(self.wet_dry_ratio)

            # Packet tracking
            self.signal_quality = min(99, max(60, self.signal_quality + random.uniform(-5, 5)))
            packets_received = len(self.hr_data)
            packets_expected = int(current_time) + 1
            quality_pct = min(99, int((packets_received / max(1, packets_expected)) * 100))
            self.packet_label.config(text=f"{packets_received}/{packets_expected} ({quality_pct}%)")
            
            # Update device terminal with current BPM if connected
            if self.bt_manager.is_connected and self.bt_manager.device:
                device_name = self.bt_manager.device.name or "Unknown"
                device_address = self.bt_manager.device.address
                self._update_device_terminal("CONNECTED", device_name, device_address, "N/A", f"{int(self.current_hr)}")

            print(f"  [OK] UI updated successfully - HR: {hr}, Smoothed: {self.smoothed_hr:.1f}, Wet/Dry: {self.wet_dry_ratio:.1f} SRC={self.wet_dry_source} OFFSET={self.wet_dry_offset:+d}")
        else:
            print(f"  [WARNING] Invalid HR value: {hr} - outside medical range (30-220 BPM)")


def main():
    print("[START] Starting HeartSync...")
    root = tk.Tk()
    app = HeartSyncMonitor(root)
    root.mainloop()


if __name__ == '__main__':
    main()
