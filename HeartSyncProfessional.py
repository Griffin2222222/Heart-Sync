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
try:
    # Optional import; if missing user must install via requirements.txt
    from bleak import BleakScanner, BleakClient
except Exception:
    BleakScanner = BleakClient = None
import random
import asyncio
from collections import deque
from datetime import datetime

# --- Design Tokens (restored minimal versions) ---
class Typography:
    FAMILY_SYSTEM = 'Helvetica'
    FAMILY_MONO = 'Courier'
    FAMILY_GEOMETRIC = 'Helvetica'
    # Slightly enlarged futuristic scale
    SIZE_LABEL_SECONDARY = 11
    SIZE_LABEL_PRIMARY = 13
    SIZE_BODY = 12
    SIZE_BODY_LARGE = 14
    SIZE_LABEL = 12
    SIZE_SMALL = 10
    SIZE_CAPTION = 9
    SIZE_H1 = 20
    SIZE_LABEL_TERTIARY = 9
    SIZE_DISPLAY_VITAL = 22

class Colors:
    SURFACE_PANEL_LIGHT = '#001111'
    SURFACE_BASE_START = '#000000'
    # Remove pure white for softer advanced aesthetic
    TEXT_PRIMARY = '#d6fff5'
    TEXT_SECONDARY = '#00cccc'
    STATUS_CONNECTED = '#00ff88'
    VITAL_HEART_RATE = '#00ff88'
    VITAL_SMOOTHED = '#00ccff'
    VITAL_WET_DRY = '#ffdd00'
    ACCENT_PLASMA_TEAL = '#00ffaa'
    ACCENT_TEAL_GLOW = '#00ffcc'

class Grid:
    COL_VITAL_VALUE = 160
    COL_CONTROL = 180
    PANEL_HEIGHT_VITAL = 220
    PANEL_HEIGHT_BT = 200
    PANEL_HEIGHT_TERMINAL = 240
    MARGIN_WINDOW = 10
    MARGIN_PANEL = 6

class Spacing:
    LG = 16
    SM = 6

class BluetoothManager:
    HR_SERVICE_UUID = '0000180d-0000-1000-8000-00805f9b34fb'
    HR_MEAS_CHAR_UUID = '00002a37-0000-1000-8000-00805f9b34fb'

    def __init__(self, callback, tk_dispatch):
        # Public callback for heart rate data: callback(hr, raw_hex, rr_intervals)
        self.on_heart_rate = callback
        # Function to marshal calls back onto Tk main thread
        self._tk_dispatch = tk_dispatch
        # Dynamic state
        self.available_devices = []
        self.is_connected = False
        self.client = None
        self.device = None  # currently connected device address (string) or None
        # Internal flags
        self._scan_in_progress = False
        self._connect_in_progress = False
        # Async loop infra
        self.loop = None
        self._loop_thread = None

    # ---------------- Persistent Loop Management -----------------
    def start(self):
        if self.loop and self.loop.is_running():
            return
        import threading, asyncio
        def _runner():
            self.loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self.loop)
            self.loop.run_forever()
        self._loop_thread = threading.Thread(target=_runner, daemon=True)
        self._loop_thread.start()

    def submit(self, coro, done_cb=None):
        if not self.loop:
            raise RuntimeError('BLE loop not started')
        fut = asyncio.run_coroutine_threadsafe(coro, self.loop)
        if done_cb:
            def _wrap(f):
                try:
                    res = f.result()
                except Exception as e:
                    res = e
                self._tk_dispatch(lambda r=res: done_cb(r))
            fut.add_done_callback(_wrap)
        return fut

    # ---------------- Scanning -----------------
    async def scan_devices(self, timeout: float = 8.0):
        if BleakScanner is None:
            raise RuntimeError("bleak not installed; run: pip install bleak")
        if self._scan_in_progress:
            return self.available_devices
        self._scan_in_progress = True
        try:
            all_devices = await BleakScanner.discover(timeout=timeout, return_adv=True)
            hr_devices = []
            for dev, adv in all_devices.values():
                name = (dev.name or '').strip()
                if not name or name.lower() == 'none':
                    continue
                svc_uuids = getattr(adv, 'service_uuids', []) or []
                has_hr_service = any(u.lower() in ['0000180d-0000-1000-8000-00805f9b34fb', '180d'] for u in svc_uuids)
                hr_keywords = ['hr','heart','rate','polar','garmin','wahoo','tickr','whoop','fitbit','apple watch','samsung','watch','band']
                has_kw = any(k in name.lower() for k in hr_keywords)
                if has_hr_service or has_kw:
                    hr_devices.append(dev)
            self.available_devices = hr_devices
            return hr_devices
        finally:
            self._scan_in_progress = False

    # ---------------- Connection & Notifications -----------------
    async def connect(self, address: str, max_retries: int = 3):
        if BleakClient is None:
            raise RuntimeError("bleak not installed; run: pip install bleak")
        if self._connect_in_progress:
            return False
        self._connect_in_progress = True
        last_error = None
        for attempt in range(max_retries):
            try:
                # Ensure any previous client is gone
                if self.client:
                    try:
                        await self.client.disconnect()
                    except Exception:
                        pass
                    self.client = None
                # Create client
                self.client = BleakClient(address, disconnected_callback=self._on_disconnect, timeout=20.0)
                self._tk_dispatch(lambda a=attempt+1: print(f"[BLE] Connection attempt {a}/{max_retries}..."))
                await self.client.connect()
                if not self.client.is_connected:
                    raise RuntimeError("Connection reported success but is_connected is False")
                self.is_connected = True
                self._tk_dispatch(lambda: print("[BLE] Connected, discovering services..."))
                await asyncio.sleep(0.5)
                # Service discovery
                services = self.client.services
                if not services:
                    await self.client.get_services()
                    services = self.client.services
                # Find HR service
                hr_service = None
                for svc in services:
                    if svc.uuid.lower() in [self.HR_SERVICE_UUID.lower(), '180d']:
                        hr_service = svc
                        break
                if not hr_service:
                    svc_list = [s.uuid for s in services]
                    self._tk_dispatch(lambda svcs=svc_list: print(f"[BLE] Available services: {svcs}"))
                    raise RuntimeError("Heart Rate service (0x180D) not found on device")
                # Find HR measurement characteristic
                hr_char = None
                for c in hr_service.characteristics:
                    if c.uuid.lower() == self.HR_MEAS_CHAR_UUID.lower():
                        hr_char = c
                        break
                if not hr_char:
                    raise RuntimeError("Heart Rate Measurement characteristic not found")
                if 'notify' not in hr_char.properties:
                    self._tk_dispatch(lambda props=hr_char.properties: print(f"[BLE] WARNING: HR char props: {props}"))
                self._tk_dispatch(lambda: print("[BLE] Enabling heart rate notifications..."))
                await self.client.start_notify(hr_char.uuid, self._hr_handler)
                await asyncio.sleep(0.5)
                self.device = address
                self._tk_dispatch(lambda: print("[BLE] ✓ Connection successful, monitoring heart rate"))
                self._connect_in_progress = False
                return True
            except asyncio.TimeoutError:
                last_error = f"Connection timeout (attempt {attempt+1}/{max_retries})"
                self._tk_dispatch(lambda e=last_error: print(f"[BLE] {e}"))
                await asyncio.sleep(1.0)
            except Exception as e:
                last_error = f"{type(e).__name__}: {e}"
                self._tk_dispatch(lambda err=last_error, a=attempt+1: print(f"[BLE] Error on attempt {a}: {err}"))
                await asyncio.sleep(1.0)
        # Exhausted
        self.is_connected = False
        self.client = None
        self.device = None
        self._tk_dispatch(lambda err=last_error: print(f"[BLE] ✗ Connection failed after {max_retries} attempts: {err}"))
        self._connect_in_progress = False
        return False

    async def disconnect(self):
        self._tk_dispatch(lambda: print("[BLE] Disconnecting..."))
        try:
            if self.client and self.client.is_connected:
                try:
                    await self.client.stop_notify(self.HR_MEAS_CHAR_UUID)
                    await asyncio.sleep(0.2)
                except Exception as e:
                    self._tk_dispatch(lambda err=str(e): print(f"[BLE] Stop notify error: {err}"))
                try:
                    await self.client.disconnect()
                    await asyncio.sleep(0.5)
                except Exception as e:
                    self._tk_dispatch(lambda err=str(e): print(f"[BLE] Disconnect error: {err}"))
        except Exception as e:
            self._tk_dispatch(lambda err=str(e): print(f"[BLE] Disconnect exception: {err}"))
        finally:
            self.is_connected = False
            self.client = None
            self.device = None
            self._tk_dispatch(lambda: print("[BLE] ✓ Disconnected"))

    def _on_disconnect(self, _client):
        self.is_connected = False
        self.client = None
        self.device = None
        self._tk_dispatch(lambda: print("[BLE] ⚠ Device disconnected unexpectedly"))

    # ---------------- Notification Handler -----------------
    def _hr_handler(self, _sender, data: bytearray):
        try:
            hr, rr_list, raw_hex = self._parse_hr_measurement(bytes(data))
            if hr is not None and 30 <= hr <= 250:
                self._tk_dispatch(lambda h=hr, hx=raw_hex, rr=rr_list: self.on_heart_rate(h, hx, rr))
            else:
                self._tk_dispatch(lambda v=hr: print(f"[BLE] Invalid HR value: {v}"))
        except Exception as e:
            self._tk_dispatch(lambda err=str(e): print(f"[BLE] HR handler error: {err}"))

    @staticmethod
    def _parse_hr_measurement(payload: bytes):
        if not payload:
            return None, [], ""
        flags = payload[0]
        is_16 = flags & 0x01
        energy_present = bool(flags & 0x08)
        rr_present = bool(flags & 0x10)
        idx = 1
        if is_16:
            if len(payload) < idx + 2:
                return None, [], payload.hex()
            hr = int.from_bytes(payload[idx:idx+2], 'little')
            idx += 2
        else:
            if len(payload) < idx + 1:
                return None, [], payload.hex()
            hr = payload[idx]
            idx += 1
        if energy_present and len(payload) >= idx + 2:
            idx += 2
        rr_intervals = []
        if rr_present:
            while len(payload) >= idx + 2:
                rr_raw = int.from_bytes(payload[idx:idx+2], 'little')
                idx += 2
                if rr_raw:
                    rr_intervals.append(rr_raw / 1024.0)
        if not (0 < hr < 255):
            return None, rr_intervals, payload.hex()
        return hr, rr_intervals, payload.hex()

class HeartSyncMonitor:
    def __init__(self, root):
        self.root = root
        self.root.title("HeartSync Professional")
        self.root.geometry("1600x1000")
        self.root.minsize(1200, 800)
        self.root.configure(bg=Colors.SURFACE_BASE_START)
        # Data buffers
        self.hr_data = deque(maxlen=300)
        self.smoothed_hr_data = deque(maxlen=300)
        self.wet_dry_data = deque(maxlen=300)
        self.time_data = deque(maxlen=300)
        self.start_time = time.time()
        # Live values
        self.current_hr = 0
        self.smoothed_hr = 0
        self.wet_dry_ratio = 0
        self.connection_status = "DISCONNECTED"
        self.signal_quality = 0
        # Parameters
        self.smoothing_factor = 0.1
        self.hr_offset = 0
        self.wet_dry_offset = 0
        self.wet_dry_source = 'smoothed'
        self.is_locked = False
        # Layout constants
        self.VALUE_COL_WIDTH = Grid.COL_VITAL_VALUE
        self.CONTROL_COL_WIDTH = Grid.COL_CONTROL
        self.VALUE_LABEL_WIDTH = 6
        # BLE manager (requires bleak)
        self.bt_manager = BluetoothManager(self.on_heart_rate_data, lambda fn: self.root.after(0, fn))
        self.bt_manager.start()
        self._setup_ui()
        self._redirect_console()

    # TouchDesigner-style parameter box helper
    def _create_param_box(self, parent, title, min_val, max_val, initial_val, *,
                          step=1, coarse_mult=5, fine_div=10, unit="", color='#00FF88',
                          is_float=False, callback=None, resettable=True):
        frame = tk.Frame(parent, bg='#000000')
        frame.pack(pady=(4,6))
        title_lbl = tk.Label(frame, text=title, fg=color, bg='#000000',
                             font=(Typography.FAMILY_SYSTEM, Typography.SIZE_LABEL_SECONDARY, 'bold'))
        title_lbl.pack(pady=(0,2))
        var = tk.StringVar()
        def fmt(v):
            if is_float:
                return (f"{v:.1f}" if step < 1 else f"{v:.1f}") + unit
            return f"{int(v)}{unit}" if v != 0 or unit else f"{int(v)}{unit}"
        current = float(initial_val)
        var.set(fmt(current))
        # Centered value label with proper sizing
        box = tk.Label(frame, textvariable=var, fg=color, bg='#111111', width=10, height=2,
                       font=(Typography.FAMILY_MONO, Typography.SIZE_LABEL_PRIMARY, 'bold'),
                       relief=tk.RIDGE, bd=2, cursor='sb_v_double_arrow', takefocus=1, anchor='center')
        box.pack()
        state = {'dragging': False,'start_y':0,'base_val':current,'value':current}
        def clamp(v): return max(min_val, min(max_val, v))
        def apply(v):
            state['value'] = clamp(v)
            var.set(fmt(state['value']))
            if callback: callback(state['value'])
        def apply_raw(v):
            state['value'] = clamp(v)
            var.set(fmt(state['value']))
        def compute_step(event):
            base = step
            if event.state & 0x0001: base *= coarse_mult
            if event.state & 0x0008: base /= fine_div
            return base
        def on_press(e):
            state['dragging']=True; state['start_y']=e.y_root; state['base_val']=state['value']; box.focus_set()
        def on_motion(e):
            if not state['dragging']: return
            dy = state['start_y'] - e.y_root
            s = compute_step(e)
            new_v = state['base_val'] + (dy/4.0)*s
            new_v = round(new_v/step)*step if is_float else round(new_v)
            apply(new_v)
        def on_release(e): state['dragging']=False
        def on_wheel(e):
            s = compute_step(e); delta = 1 if e.delta>0 else -1
            new_v = state['value'] + s*delta
            new_v = round(new_v/step)*step if is_float else round(new_v)
            apply(new_v)
        def enter_edit():
            entry = tk.Entry(frame, fg=color, bg='#000000', insertbackground=color,
                             font=(Typography.FAMILY_MONO, Typography.SIZE_LABEL_PRIMARY, 'bold'), justify='center')
            entry.insert(0, str(state['value']))
            box.pack_forget(); entry.pack(); entry.focus_set(); entry.select_range(0, tk.END)
            
            # Track if we should exit on click outside
            entry_active = {'active': True}
            
            def commit():
                if not entry_active['active']: return
                entry_active['active'] = False
                t = entry.get().strip()
                try:
                    v = float(t) if is_float else int(float(t))
                    apply(v)
                except: pass
                try: entry.destroy()
                except: pass
                box.pack()
                
            def cancel():
                if not entry_active['active']: return
                entry_active['active'] = False
                try: entry.destroy()
                except: pass
                box.pack()
            
            # Click outside to exit
            def on_click_outside(event):
                # Check if click is outside the entry widget
                if event.widget != entry:
                    commit()
            
            entry.bind('<Return>', lambda _e: commit())
            entry.bind('<Escape>', lambda _e: cancel())
            entry.bind('<FocusOut>', lambda _e: commit())
            # Bind click outside after a short delay to avoid immediate trigger
            self.root.after(100, lambda: self.root.bind_all('<Button-1>', on_click_outside, add='+'))
            
            # Cleanup binding when entry is destroyed
            def cleanup():
                try: self.root.unbind_all('<Button-1>')
                except: pass
            entry.bind('<Destroy>', lambda _e: cleanup())
        def on_key(e):
            if e.keysym in ('Return','KP_Enter'): enter_edit(); return 'break'
            if e.keysym in ('Up','Down'):
                s = compute_step(e); delta = s if e.keysym=='Up' else -s
                new_v = state['value'] + delta
                new_v = round(new_v/step)*step if is_float else round(new_v)
                apply(new_v); return 'break'
            if resettable and e.keysym=='BackSpace': apply(0)
        for seq, handler in [('<Button-1>',on_press),('<B1-Motion>',on_motion),('<ButtonRelease-1>',on_release),
                              ('<MouseWheel>',on_wheel),('<Double-1>',lambda e: enter_edit()),('<Key>',on_key)]:
            box.bind(seq, handler)
        # Double-click title to reset to initial value
        title_lbl.bind('<Double-1>', lambda _e: apply(initial_val) if resettable else None)
        return {'frame':frame,'label':box,'var':var,'get':lambda:state['value'],'set':lambda v:apply_raw(v),'set_with_callback':apply}

    def _setup_ui(self):
        # Header
        header = tk.Frame(self.root, bg=Colors.SURFACE_PANEL_LIGHT, height=80)
        header.pack(fill=tk.X)
        header.pack_propagate(False)
        left = tk.Frame(header, bg=Colors.SURFACE_PANEL_LIGHT)
        left.pack(side=tk.LEFT, padx=Spacing.LG, pady=Spacing.SM, fill=tk.Y)
        tk.Label(left, text="❖ HEART SYNC SYSTEM", fg=Colors.ACCENT_PLASMA_TEAL,
                 font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_H1, 'bold'),
                 bg=Colors.SURFACE_PANEL_LIGHT).pack(anchor='w', pady=(4,0))
        tk.Label(left, text="Adaptive Audio Bio Technology", fg=Colors.TEXT_SECONDARY,
                 font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_LABEL_TERTIARY),
                 bg=Colors.SURFACE_PANEL_LIGHT).pack(anchor='w', pady=(2,0))
        right = tk.Frame(header, bg=Colors.SURFACE_PANEL_LIGHT)
        right.pack(side=tk.RIGHT, padx=Spacing.LG, pady=Spacing.SM, fill=tk.Y)
        time_lbl = tk.Label(right, text='', fg=Colors.TEXT_PRIMARY,
                            font=(Typography.FAMILY_MONO, Typography.SIZE_BODY_LARGE, 'bold'),
                            bg=Colors.SURFACE_PANEL_LIGHT)
        time_lbl.pack(anchor='e', pady=(4,0))
        tk.Label(right, text='◆ SYSTEM OPERATIONAL', fg=Colors.STATUS_CONNECTED,
                 font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_LABEL_TERTIARY, 'bold'),
                 bg=Colors.SURFACE_PANEL_LIGHT).pack(anchor='e', pady=(2,0))
        def _tick():
            time_lbl.config(text=datetime.now().strftime('%Y-%m-%d  %H:%M:%S'))
            self.root.after(1000, _tick)
        _tick()
        # Main content frame
        main = tk.Frame(self.root, bg=Colors.SURFACE_BASE_START)
        main.pack(fill=tk.BOTH, expand=True, padx=Grid.MARGIN_WINDOW, pady=Grid.MARGIN_PANEL)
        main.grid_columnconfigure(0, weight=1)
        for r in range(4):
            main.grid_rowconfigure(r, weight=1)
        # Unified header strip
        hdr = tk.Frame(main, bg='#000000')
        hdr.grid(row=0, column=0, columnspan=2, sticky='ew')
        hdr.grid_columnconfigure(0, minsize=self.VALUE_COL_WIDTH)
        hdr.grid_columnconfigure(1, minsize=self.CONTROL_COL_WIDTH)
        hdr.grid_columnconfigure(2, weight=1)
        for idx, txt in enumerate(['VALUES','CONTROLS','WAVEFORM']):
            tk.Label(hdr, text=txt, fg=Colors.TEXT_SECONDARY, bg='#000000',
                     font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold')).grid(
                        row=0, column=idx, sticky='w', padx=(14 if idx==0 else 6, 12 if idx==2 else 6), pady=(0,2))
        # Metric panels
        self._create_metric_panel(main, 'HEART RATE', 'BPM', lambda: str(int(self.current_hr)), Colors.VITAL_HEART_RATE, 0, 0)
        self._create_metric_panel(main, 'SMOOTHED HR', 'BPM', lambda: str(int(self.smoothed_hr)), Colors.VITAL_SMOOTHED, 1, 0)
        self._create_metric_panel(main, 'WET/DRY RATIO', '', lambda: str(int(max(1, min(100, self.wet_dry_ratio + self.wet_dry_offset)))), Colors.VITAL_WET_DRY, 2, 0)
        # Control + terminal panels
        self._create_control_panel()
        self._create_terminal_displays()
        # Startup logs
        self.log_to_console('❖ HeartSync Professional v2.0 - Quantum Systems Online', tag='success')
        self.log_to_console('Awaiting Bluetooth device connection...', tag='info')

    def _create_metric_panel(self, parent, title, unit, value_func, color, row, col):
        panel = tk.Frame(parent, bg='#000000', height=Grid.PANEL_HEIGHT_VITAL,
                         highlightbackground='#003333', highlightthickness=1)
        panel.grid(row=row+1, column=0, columnspan=2, sticky='ew', padx=2, pady=2)
        panel.grid_propagate(False)
        panel.grid_columnconfigure(0, minsize=self.VALUE_COL_WIDTH)
        panel.grid_columnconfigure(1, minsize=self.CONTROL_COL_WIDTH)
        panel.grid_columnconfigure(2, weight=1)
        panel.grid_rowconfigure(0, weight=1)
        # Value column
        value_col = tk.Frame(panel, bg='#000000', highlightbackground=color, highlightthickness=2)
        value_col.grid(row=0, column=0, sticky='nsew', padx=(12,6), pady=12)
        value_box = tk.Frame(value_col, bg='#000000', highlightbackground=color, highlightthickness=2, padx=4, pady=4)
        value_box.pack(expand=True, fill='both', pady=(10,4))
        value_label = tk.Label(value_box, text='', fg=color, bg='#000000', anchor='center',
                               font=(Typography.FAMILY_MONO, max(Typography.SIZE_DISPLAY_VITAL, 28), 'bold'))
        value_label.pack(expand=True, fill='both')
        if title == 'HEART RATE':
            # Store reference for flashing animation
            self.hr_value_label = value_label
        tk.Label(value_col, text=f"{title}{(' ['+unit+']') if unit else ''}", fg='#00cccc', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL)).pack(pady=(0,8))
        # Control column
        control_col = tk.Frame(panel, bg='#000000', highlightbackground=color, highlightthickness=2)
        control_col.grid(row=0, column=1, sticky='nsew', padx=6, pady=12)
        if title == 'HEART RATE':
            self._build_hr_controls(control_col)
        elif title == 'SMOOTHED HR':
            self._build_smooth_controls(control_col)
        elif title == 'WET/DRY RATIO':
            self._build_wetdry_controls(control_col)
        # Graph column
        graph_frame = tk.Frame(panel, bg='#000000', highlightbackground=color, highlightthickness=2)
        graph_frame.grid(row=0, column=2, sticky='nsew', padx=(6,12), pady=12)
        tk.Frame(graph_frame, height=4, bg='#000000').pack(fill='x')
        graph_canvas = tk.Canvas(graph_frame, bg='#000000', highlightthickness=1, highlightbackground='#004444')
        graph_canvas.pack(expand=True, fill='both', padx=8, pady=(0,8))
        # Store refs
        key = title.lower().replace(' ', '_').replace('/', '_')
        setattr(self, f'{key}_value_label', value_label)
        setattr(self, f'{key}_canvas', graph_canvas)
        setattr(self, f'{key}_data_func', lambda: self.hr_data if 'HEART RATE' in title and 'SMOOTHED' not in title else self.smoothed_hr_data if 'SMOOTHED' in title else self.wet_dry_data)
        def update_panel():
            value_label.config(text=value_func())
            self._update_graph(graph_canvas, getattr(self, f'{key}_data_func')(), color)
            self.root.after(25, update_panel)
        self.root.after(25, update_panel)

    def _create_control_panel(self):
        # Medical monitor control panel with standardized height
        control_frame = tk.Frame(self.root, bg='#000000', height=Grid.PANEL_HEIGHT_BT)
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
        
        # ENHANCED 3D MECHANICAL BUTTONS - Next-Gen Medical Device Style
        # Row 1: SCAN and CONNECT
        row1 = tk.Frame(left_section, bg='#000000')
        row1.pack(pady=4)
        
        # SCAN DEVICES - 3D Mechanical with lit/unlit states
        self.scan_btn = self._create_3d_medical_button(row1, "SCAN DEVICES", 
                                                      self.scan_devices,
                                                      color_on='#00AAFF', color_off='#004477',
                                                      width=12, height=2)
        self.scan_btn.pack(side=tk.LEFT, padx=4)
        
        # CONNECT DEVICE - 3D Mechanical with lit/unlit states  
        self.connect_btn = self._create_3d_medical_button(row1, "CONNECT", 
                                                         self.connect_device,
                                                         color_on='#00FF66', color_off='#004422',
                                                         width=12, height=2, 
                                                         initial_state=False)
        self.connect_btn.pack(side=tk.LEFT, padx=4)
        
        # Row 2: LOCK and DISCONNECT  
        row2 = tk.Frame(left_section, bg='#000000')
        row2.pack(pady=4)
        
        # LOCK/UNLOCK - 3D Mechanical with consistent sizing
        self.lock_btn = self._create_3d_medical_button(row2, "LOCKED", 
                                                      self._toggle_lock,
                                                      color_on='#FF8800', color_off='#FF8800',
                                                      width=12, height=2,
                                                      initial_state=True)  # Start locked
        self.lock_btn.pack(side=tk.LEFT, padx=4)
        
        # DISCONNECT - 3D Mechanical with consistent sizing (start disabled)
        self.disconnect_btn = self._create_3d_medical_button(row2, "DISCONNECT", 
                                                            self.disconnect_device,
                                                            color_on='#FF4444', color_off='#FF4444',
                                                            width=12, height=2,
                                                            initial_state=False)
        self.disconnect_btn['state'] = tk.DISABLED  # Start disabled until connected
        self.disconnect_btn.pack(side=tk.LEFT, padx=4)
        
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
        
        # ==================== TERMINAL PANEL (NEW) ====================
        # Add TERMINAL panel directly below Bluetooth connectivity
        terminal_panel = tk.LabelFrame(control_frame, text="  TERMINAL  ", 
                                fg='#00ff88', bg='#000a0a', 
                                font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"),
                                relief=tk.GROOVE, bd=3, padx=10, pady=8)
        terminal_panel.pack(fill=tk.BOTH, expand=True, padx=15, pady=(5, 10))
        
        # Terminal text widget - styled log display
        self.terminal_text = tk.Text(terminal_panel, 
                                  height=4,
                                  bg='#000a0a', 
                                  fg='#00ff88',
                                  font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL),
                                  wrap=tk.WORD, 
                                  bd=0,
                                  highlightthickness=0,
                                  insertbackground='#00ff88',
                                  state=tk.DISABLED)
        self.terminal_text.pack(fill=tk.BOTH, expand=True)
        
        # Add initial terminal message
        self.terminal_text.config(state=tk.NORMAL)
        self.terminal_text.insert(tk.END, "[TERMINAL] System ready. Awaiting commands...\n")
        self.terminal_text.insert(tk.END, "[TERMINAL] All subsystems operational.\n")
        self.terminal_text.config(state=tk.DISABLED)
    
    def _create_terminal_displays(self):
        """Create high-tech terminal windows with medical UI standards"""
        # Main terminal container with standardized dimensions
        terminal_container = tk.Frame(self.root, bg='#000000', height=Grid.PANEL_HEIGHT_TERMINAL)
        terminal_container.pack(fill=tk.X, expand=False, padx=15, pady=(5, 0))
        terminal_container.pack_propagate(False)
        
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

    # ==================== MEDICAL UI BUTTON STANDARDS ====================
    def _create_medical_button(self, parent, text, command, color='#00ccaa', width=12, height=1):
        """Create standardized medical UI button according to IEC 62366-1 standards"""
        button = tk.Button(parent, text=text, command=command,
                          font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION, 'bold'),
                          bg='#001a1a', fg=color, width=width, height=height,
                          activebackground='#002a2a', activeforeground='#00ffcc',
                          relief=tk.RAISED, bd=2, cursor='hand2',
                          highlightthickness=2, highlightbackground=color,
                          highlightcolor='#00ffcc')
        return button

    def _create_3d_button(self, parent, text, command, color='#00FFAA', width=12, pressed=False):
        """Create 3D mechanical button with visual press feedback"""
        bg_color = '#003333' if pressed else '#001818'
        relief_style = tk.SUNKEN if pressed else tk.RAISED
        bd_width = 1 if pressed else 3
        
        button = tk.Button(parent, text=text, command=command,
                          font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION, 'bold'),
                          bg=bg_color, fg=color, width=width, height=1,
                          activebackground='#004444', activeforeground='#00ffcc',
                          relief=relief_style, bd=bd_width, cursor='hand2',
                          highlightthickness=2, highlightbackground=color)
        return button

    def _create_3d_toggle_button(self, parent, text_on, text_off, command, initial_state=True):
        """Create 3D mechanical toggle switch for input selection - ENHANCED READABILITY"""
        self.toggle_state = initial_state
        
        def toggle_command():
            self.toggle_state = not self.toggle_state
            self._update_toggle_appearance()
            command()
        
        self.toggle_btn = tk.Button(parent, command=toggle_command,
                                   font=(Typography.FAMILY_SYSTEM, Typography.SIZE_LABEL_SECONDARY, 'bold'),  # LARGER: SIZE_TINY→SIZE_LABEL_SECONDARY
                                   width=14, height=3, cursor='hand2',  # LARGER: width 12→14, height 2→3
                                   highlightthickness=3)  # More prominent highlight
        
        self.toggle_text_on = text_on
        self.toggle_text_off = text_off
        self._update_toggle_appearance()
        
        return self.toggle_btn
    
    def _update_toggle_appearance(self):
        """Update 3D toggle button appearance based on state - ENHANCED VISIBILITY"""
        if self.toggle_state:
            # ON state - pressed down, bright green - MORE CONTRAST
            self.toggle_btn.config(
                text=self.toggle_text_on,
                bg='#004444', fg='#00FFCC',  # Enhanced contrast
                relief=tk.SUNKEN, bd=2,  # More prominent border
                highlightbackground='#00FFCC',
                activebackground='#006666', activeforeground='#00ffcc'
            )
        else:
            # OFF state - raised up, bright amber - MORE CONTRAST  
            self.toggle_btn.config(
                text=self.toggle_text_off,
                bg='#332200', fg='#FFCC00',  # Enhanced contrast
                relief=tk.RAISED, bd=3,
                highlightbackground='#FFCC00',
                activebackground='#554400', activeforeground='#ffcc00'
            )

    def _create_3d_medical_button(self, parent, text, command, color_on='#00FFAA', color_off='#004444', 
                                 width=12, height=2, initial_state=False):
        """Create 3D mechanical button for main medical device controls"""
        self.button_states = getattr(self, 'button_states', {})
        
        def button_command():
            # Toggle visual state when pressed
            button_id = id(button)
            current_state = self.button_states.get(button_id, initial_state)
            new_state = not current_state
            self.button_states[button_id] = new_state
            self._update_medical_button_state(button, new_state, color_on, color_off)
            # Execute the actual command
            command()
        
        button = tk.Button(parent, text=text, command=button_command,
                          font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION, 'bold'),
                          width=width, height=height, cursor='hand2',
                          highlightthickness=2)
        
        # Store button reference and initialize state
        button_id = id(button)
        self.button_states[button_id] = initial_state
        self._update_medical_button_state(button, initial_state, color_on, color_off)
        
        return button
    
    def _update_medical_button_state(self, button, is_pressed, color_on, color_off):
        """Update 3D medical button visual state"""
        if is_pressed:
            # Pressed state - lit up, sunken
            button.config(
                bg='#002222', fg=color_on,
                relief=tk.SUNKEN, bd=1,
                highlightbackground=color_on
            )
        else:
            # Released state - dimmed, raised
            button.config(
                bg='#001111', fg=color_off,
                relief=tk.RAISED, bd=3,
                highlightbackground=color_off
            )

    # Removed legacy precision control + dialog in favor of param boxes
    # ==================== CONTROL BUILDERS ====================
    def _build_hr_controls(self, parent):
        self.hr_control = self._create_param_box(parent,
            title="HR OFFSET", min_val=-100, max_val=100, initial_val=0,
            step=1, unit=" BPM", color='#00FF88', is_float=False,
            callback=lambda v: self._set_hr_offset(int(v)))
        self.hr_offset_label = self.hr_control['label']
    
    def _on_hr_offset_change_direct(self, value):
        """Direct callback for HR offset changes"""
        self.hr_offset = int(value)
        self.log_to_console(f"[HR OFFSET] Set to {self.hr_offset:+d} BPM")

    def _set_hr_offset(self, val):
        val = max(-100, min(100, int(val)))
        self.hr_offset = val
        if hasattr(self, 'hr_control'):
            self.hr_control['set'](val)
        self.log_to_console(f"[HR OFFSET] Set to {val:+d} BPM")

    def _reset_hr_offset_click(self):
        """Reset HR offset to 0 when title is clicked"""
        self._set_hr_offset(0)
        self.log_to_console(f"[HR OFFSET] ✓ Reset to default (0) - clicked title")

    def _build_smooth_controls(self, parent):
        self.smoothing_control = self._create_param_box(parent,
            title="SMOOTH", min_val=0.1, max_val=10.0, initial_val=self.smoothing_factor,
            step=0.1, unit="x", color='#00CCFF', is_float=True,
            callback=lambda v: self._set_smoothing(v, from_slider=True))
        
        # Enhanced medical metrics display
        metrics_frame = tk.Frame(parent, bg='#000000')
        metrics_frame.pack(pady=(4,0))
        
        self.smoothing_metrics_label = tk.Label(metrics_frame, text="α=--\nT½=--s\n≈-- samples",
                                                font=(Typography.FAMILY_MONO, Typography.SIZE_LABEL_TERTIARY),
                                                fg='#00BBDD', bg='#000000', justify='center')
        self.smoothing_metrics_label.pack()
        
        # Initialize metrics display
        self._update_smoothing_metrics()
    
    def _on_smoothing_change_direct(self, value):
        """Direct callback for smoothing changes"""
        self.smoothing_factor = float(value)
        self._update_smoothing_metrics()
        self.log_to_console(f"[SMOOTHING] Factor set to {self.smoothing_factor:.1f}x")

    def _build_wetdry_controls(self, parent):
        # Input source selector section with enhanced toggle button
        input_frame = tk.Frame(parent, bg='#000000')
        input_frame.pack(pady=(0,8), fill='x')
        
        source_label = tk.Label(input_frame, text="INPUT SOURCE", 
                               font=(Typography.FAMILY_SYSTEM, Typography.SIZE_LABEL_SECONDARY, 'bold'), 
                               fg='#00FFFF', bg='#000000', justify='center')
        source_label.pack(pady=(0,4))
        
        # Enhanced input selector switch
        self.wetdry_source_btn = self._create_3d_toggle_button(input_frame, 
                                                              text_on="SMOOTHED HR", 
                                                              text_off="RAW HR",
                                                              command=self._toggle_wetdry_source,
                                                              initial_state=True)
        self.wetdry_source_btn.pack(pady=(0,8))
        
        self.wetdry_control = self._create_param_box(parent,
            title="WET/DRY", min_val=-100, max_val=100, initial_val=0,
            step=1, unit="%", color='#FFDD00', is_float=False,
            callback=lambda v: self._set_wetdry_offset(int(v)))
        self.wetdry_offset_label = self.wetdry_control['label']
    
    def _on_wetdry_offset_change_direct(self, value):
        """Direct callback for wet/dry offset changes"""
        self.wet_dry_offset = int(value)
        self.log_to_console(f"[WET/DRY OFFSET] Set to {self.wet_dry_offset:+d}%")

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
        # Update new param box control (silent set; UI handles formatting via unit)
        if hasattr(self, 'wetdry_control'):
            try:
                self.wetdry_control['set'](val)
            except Exception:
                pass  # Fail-safe; prevents drag callback cascades
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
            # LOCKED state - button stays pressed, orange
            self.lock_btn['text'] = "LOCKED"
            self._update_button_state(self.lock_btn, True)  # Keep pressed down
            self.log_to_console("[LOCK] System LOCKED")
        else:
            # UNLOCKED state - button release, green
            self.lock_btn['text'] = "UNLOCKED"
            self.lock_btn['bg'] = '#00884a'  # Change to green
            self.lock_btn['activebackground'] = '#00aa5a'
            self._update_button_state(self.lock_btn, False)  # Release button
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
        # Update new param box control only (silent set; UI formats via unit)
        if hasattr(self, 'smoothing_control'):
            try:
                self.smoothing_control['set'](factor)
            except Exception:
                pass
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
        """Medical monitor style: fixed-speed strip, consistent grid; remove verbose seconds labels."""
        canvas.delete('all')
        if len(data) < 2:
            return
        w = canvas.winfo_width(); h = canvas.winfo_height()
        if w <= 1 or h <= 1:
            return
        # Layout constants tuned for monitor style
        LM, RM, TM, BM = 48, 18, 14, 18
        gw = w - LM - RM; gh = h - TM - BM
        data_list = list(data)
        # Y scale padding
        dmin = min(data_list); dmax = max(data_list)
        rng = max(1.0, dmax - dmin)
        pad = max(4.0, rng * 0.12)
        dmin -= pad; dmax += pad
        if dmax <= dmin: dmax = dmin + 5
        # Determine vertical major step targeting ~6 bands using 1/2/5 rule
        target = 6
        raw_step = (dmax - dmin) / target
        mag = 10 ** int(math.floor(math.log10(raw_step))) if raw_step>0 else 1
        norm = raw_step / mag
        if norm < 1.5: vs = 1
        elif norm < 3.5: vs = 2
        elif norm < 7.5: vs = 5
        else: vs = 10
        y_step = vs * mag
        y_start = math.floor(dmin / y_step) * y_step
        # Background panel
        canvas.create_rectangle(LM, TM, LM+gw, TM+gh, fill='#000000', outline='#003333', width=1)
        # Grid colors
        major_c = '#003f3f'
        minor_c = '#001e1e'
        # Horizontal grid (major + 4 minor subdivisions)
        minor_sub = 5
        y_val = y_start
        while y_val <= dmax + 1e-6:
            y = TM + (1 - (y_val - dmin) / (dmax - dmin)) * gh
            canvas.create_line(LM, y, LM+gw, y, fill=major_c, width=1)
            # Label at left
            canvas.create_text(LM - 6, y, text=f"{y_val:.0f}", anchor='e',
                               fill='#00b3b3', font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
            nxt = y_val + y_step
            for m in range(1, minor_sub):
                mv = y_val + m * (y_step / minor_sub)
                if mv >= dmax: break
                my = TM + (1 - (mv - dmin) / (dmax - dmin)) * gh
                canvas.create_line(LM, my, LM+gw, my, fill=minor_c, width=1)
            y_val += y_step
        # Time grid: treat last N samples as fixed window strip
        sample_rate = 10.0  # Hz
        total_secs = (len(data_list)-1) / sample_rate
        # Choose fixed major = 1s, minor = 0.2s (like ECG 0.2s large, 0.04s small, adapted to 10Hz)
        major_sec = 1.0
        minor_sec = 0.2
        # number of pixels per second
        px_per_sec = gw / total_secs if total_secs>0 else gw
        # Draw vertical lines from right to left so they feel stationary as strip scrolls
        # Align newest sample at right edge
        for s in range(int(total_secs//major_sec)+2):
            x = LM + gw - s * px_per_sec * major_sec
            if x < LM: break
            canvas.create_line(x, TM, x, TM+gh, fill=major_c, width=1)
            # Minor lines between majors
            for m in range(1, int(major_sec/minor_sec)):
                xm = x - m * px_per_sec * minor_sec
                if xm < LM: break
                canvas.create_line(xm, TM, xm, TM+gh, fill=minor_c, width=1)
        # Waveform path (scrolling strip: newest on right)
        pts = []
        scale_y = gh / (dmax - dmin)
        n = len(data_list)
        for i, v in enumerate(data_list):
            # Map so index n-1 -> right edge
            frac = i / (n - 1)
            x = LM + frac * gw
            y = TM + gh - (v - dmin) * scale_y
            pts.extend([x, y])
        if len(pts) >= 4:
            canvas.create_line(pts, fill=color, width=2, smooth=True, splinesteps=12)
        # Left Y axis label
        canvas.create_text(LM - 28, TM + gh/2, text='BPM', angle=90, anchor='center',
                           fill='#007777', font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL))
        # Window info (top right)
        canvas.create_text(LM + gw - 4, TM + 6, text=f"LAST {total_secs:.0f}s", anchor='ne',
                           fill='#00cccc', font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        # Peak / Min inside frame
        peak = max(data_list); low = min(data_list)
        canvas.create_text(LM + gw - 4, TM + 18, text=f"PEAK {peak:.0f}", anchor='ne',
                           fill='#00ff88', font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        canvas.create_text(LM + gw - 4, TM + gh - 4, text=f"MIN {low:.0f}", anchor='se',
                           fill='#ff6666', font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))

    def scan_devices(self):
        """Initiate BLE heart rate device scan (async via BluetoothManager loop)."""
        # Prevent concurrent scans
        if getattr(self.bt_manager, '_scan_in_progress', False):
            self.log_to_console("Scan already in progress...", tag='warning')
            return
        self.status_label.config(text="SCANNING...", fg='yellow')
        self._update_button_state(self.scan_btn, True)
        self.scan_btn.config(state=tk.DISABLED)
        self.log_to_console("Scanning for heart rate devices (10 seconds)...", tag="info")
        # Submit async scan coroutine to persistent BLE loop
        self.bt_manager.submit(self.bt_manager.scan_devices(), lambda devices: self._on_scan_complete(devices))

    def _on_scan_complete(self, devices):
        self.scan_btn.config(state=tk.NORMAL)
        self._update_button_state(self.scan_btn, False)  # Release button
        
        if devices:
            # Format device names nicely: prioritize name over address
            device_names = []
            for d in devices:
                name = d.name if d.name and d.name.lower() != 'none' else 'Unknown Device'
                # Shorten long MAC addresses for cleaner display
                addr_short = d.address[-8:] if len(d.address) > 8 else d.address
                device_names.append(f"{name} ({addr_short})")
            
            self.device_combo['values'] = device_names
            self.device_combo.current(0)
            self.connect_btn.config(state=tk.NORMAL)
            self._update_button_state(self.connect_btn, False)  # Ready to use
            self.status_label.config(text=f"FOUND {len(devices)} HR DEVICE(S)", fg='#00ff00')
            self.log_to_console(f"✓ Found {len(devices)} heart rate device(s)", tag="success")
            for d in devices:
                name = d.name if d.name and d.name.lower() != 'none' else 'Unknown'
                self.log_to_console(f"  • {name} ({d.address})", tag="data")
        else:
            self.status_label.config(text="NO HR DEVICES FOUND", fg='red')
            self.log_to_console("No heart rate devices found. Try pairing your watch in System Settings first.", tag="warning")

    def connect_device(self):
        if not self.device_combo.get():
            return
        selected_idx = self.device_combo.current()
        if selected_idx < 0 or selected_idx >= len(self.bt_manager.available_devices):
            self.log_to_console("Invalid device selection", tag='error')
            return
        device = self.bt_manager.available_devices[selected_idx]
        self.status_label.config(text="CONNECTING...", fg='yellow')
        self.connect_btn.config(state=tk.DISABLED)
        self._update_button_state(self.connect_btn, True)
        device_name = device.name if getattr(device, 'name', None) else 'Unknown Device'
        self.log_to_console(f"Connecting to {device_name} ({getattr(device,'address','?')})...", tag='info')
        self.bt_manager.submit(self.bt_manager.connect(device.address), lambda success: self._on_connect_complete(success, device_name))

    def _on_connect_complete(self, success, device_name):
        """Handle connection result and update UI accordingly."""
        if success:
            # Connection successful - update button states
            self.status_label.config(text="CONNECTED", fg='#00ff88')
            self.status_indicator.config(fg='#00ff88')
            self.connected_label.config(text="◉ CONNECTED")
            self.disconnect_btn['state'] = tk.NORMAL  # Enable disconnect button
            self._update_button_state(self.disconnect_btn, False)  # Ready to disconnect
            self._update_button_state(self.connect_btn, False)  # Release connect button
            self.connect_btn.config(state=tk.DISABLED)  # Disable connect while connected
            self.device_detail.config(text=f"Device: {device_name}", fg='#00ff88')
            self.packet_label.config(fg='#00ff88')
            self.quality_dots.config(fg='#00ff88')
            
            # Update device terminal with connection info
            try:
                device_address = self.bt_manager.client.address if self.bt_manager.client else "Unknown"
            except:
                device_address = "Unknown"
            self._update_device_terminal("CONNECTED", device_name, device_address, "Active", "---")
            self.log_to_console(f"✓ Connected to {device_name}", tag="success")
            self.log_to_console("Waiting for heart rate data...", tag="info")
        else:
            # Connection failed - reset UI
            self.status_label.config(text="CONNECTION FAILED", fg='#ff5555')
            self.status_indicator.config(fg='#444444')
            self.connect_btn['state'] = tk.NORMAL
            self._update_button_state(self.connect_btn, False)  # Release button
            self.log_to_console("✗ Connection failed - check console for details", tag="error")
            self.log_to_console("Tips: Ensure device is nearby, paired in System Settings, and not connected to other apps", tag="info")

    def disconnect_device(self):
        """Disconnect from BLE device in async thread."""
        self._update_button_state(self.disconnect_btn, True)  # Press button down
        self.log_to_console("Disconnecting from device...", tag="info")
        
        def disconnect_thread():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            loop.run_until_complete(self.bt_manager.disconnect())
            loop.close()
            self.root.after(0, self._on_disconnect_complete)
        
        threading.Thread(target=disconnect_thread, daemon=True).start()
    
    def _on_disconnect_complete(self):
        """Handle UI updates after disconnection"""
        # Reset UI state
        self.status_label.config(text="DISCONNECTED", fg='#666666')
        self.status_indicator.config(fg='#444444')
        self.connected_label.config(text="")
        self.disconnect_btn['state'] = tk.DISABLED  # Disable disconnect button
        self._update_button_state(self.disconnect_btn, False)  # Release button
        self.connect_btn['state'] = tk.NORMAL
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
            self.disconnect_btn['state'] = tk.NORMAL  # Enable disconnect button
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
            # Resync subtle heart rate flash
            self._schedule_hr_flash()
            
            # Update device terminal with current BPM if connected
            if self.bt_manager.is_connected and self.bt_manager.device:
                # self.bt_manager.device now stores the address (string). Try to map back to scanned device for name.
                addr = self.bt_manager.device
                # Attempt to find matching device object from last scan for a friendly name.
                match = None
                for d in getattr(self.bt_manager, 'available_devices', []):
                    if getattr(d, 'address', None) == addr:
                        match = d
                        break
                device_name = (getattr(match, 'name', None) or 'Unknown').strip() if match else 'Unknown'
                device_address = addr
                self._update_device_terminal("CONNECTED", device_name, device_address, "N/A", f"{int(self.current_hr)}")

            print(f"  [OK] UI updated successfully - HR: {hr}, Smoothed: {self.smoothed_hr:.1f}, Wet/Dry: {self.wet_dry_ratio:.1f} SRC={self.wet_dry_source} OFFSET={self.wet_dry_offset:+d}")
        else:
            print(f"  [WARNING] Invalid HR value: {hr} - outside medical range (30-220 BPM)")

    # --- BOLD HR PULSE FLASH - Synced to Real Heart Rate -------------------------------------------------
    def _schedule_hr_flash(self):
        """Schedule next heartbeat flash precisely synced to BPM."""
        if not hasattr(self, 'hr_value_label'):
            return
        bpm = self.current_hr
        if bpm <= 0 or bpm > 250:
            return
        # Cancel pending pulse if any
        if hasattr(self, '_hr_flash_job') and self._hr_flash_job:
            try: self.root.after_cancel(self._hr_flash_job)
            except: pass
        # Calculate precise interval: 60000ms per minute / BPM = ms per beat
        interval = int(60000 / max(1, bpm))
        interval = max(240, min(2000, interval))  # Safety clamp (250 BPM max, 30 BPM min)
        self._hr_flash_job = self.root.after(interval, self._hr_flash_pulse)

    def _hr_flash_pulse(self):
        """Execute a bold, clearly visible pulse flash that user can feel matches their heartbeat."""
        if not hasattr(self, 'hr_value_label'):
            return

        # Flash sequence
        lbl = self.hr_value_label
        base_fg = getattr(self, '_hr_base_fg', '#00ff88')
        self._hr_base_fg = base_fg

        # Phase 1 flash (peak)
        flash_peak = '#00ffe0'
        lbl.config(fg=flash_peak, font=(Typography.FAMILY_MONO, 32, 'bold'))

        # Phase 2 (quick decay)
        flash_bright = '#35ffd9'
        self.root.after(60, lambda: lbl.config(fg=flash_bright, font=(Typography.FAMILY_MONO, 30, 'bold')))

        # Phase 3 (return to baseline)
        self.root.after(170, lambda: lbl.config(fg=base_fg, font=(Typography.FAMILY_MONO, max(Typography.SIZE_DISPLAY_VITAL, 28), 'bold')))

        # Reschedule next beat
        self._schedule_hr_flash()


def main():
    print("[START] Starting HeartSync...")
    root = tk.Tk()
    app = HeartSyncMonitor(root)
    root.mainloop()


if __name__ == '__main__':
    main()
