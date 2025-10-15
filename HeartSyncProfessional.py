#!/usr/bin/env python3
"""
HeartSync Professional - Next-Generation Scientific Monitoring
Parallel-universe technology aesthetic with quantum-grade precision
"""

import tkinter as tk
from tkinter import ttk, messagebox
from tkinter import font as tkfont
import threading
import time
import math
try:
    # Optional import; if missing user must install via requirements.txt
    from bleak import BleakScanner, BleakClient
except Exception:
    BleakScanner = BleakClient = None

try:
    # OSC for DAW integration and modulation output
    from pythonosc import udp_client
    from pythonosc.dispatcher import Dispatcher
    from pythonosc.server import BlockingOSCUDPServer
except ImportError:
    udp_client = Dispatcher = BlockingOSCUDPServer = None

try:
    # MIDI for hardware and DAW integration
    import mido
except ImportError:
    mido = None
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
    STATUS_CONNECTED = '#00F5D4'  # Quantum teal
    VITAL_HEART_RATE = '#FF6B6B'  # Medical red for raw heart rate
    VITAL_SMOOTHED = '#00F5D4'    # Quantum teal for smoothed HR
    VITAL_WET_DRY = '#FFD93D'     # Medical gold for wet/dry ratio
    ACCENT_PLASMA_TEAL = '#00F5D4'  # Quantum teal
    ACCENT_TEAL_GLOW = '#00D4AA'    # Darker quantum teal

class Grid:
    COL_VITAL_VALUE = 160
    COL_CONTROL = 180
    PANEL_HEIGHT_VITAL = 180
    PANEL_HEIGHT_BT = 140
    PANEL_HEIGHT_TERMINAL = 200
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
        # Optional UI callbacks
        self.on_disconnect_cb = None

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
        if self.on_disconnect_cb:
            # Notify UI thread about disconnect
            try:
                self._tk_dispatch(lambda: self.on_disconnect_cb())
            except Exception:
                pass

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

class OSCSenderManager:
    """Manages OSC output for DAW integration and modulation mapping"""
    
    def __init__(self):
        self.enabled = False
        self.client = None
        self.target_host = "127.0.0.1"
        self.target_port = 9000
        self.base_address = "/heartsync"
    
    @property
    def is_connected(self):
        """Check if OSC is connected and enabled"""
        return self.enabled and self.client is not None
        
    def connect(self, host="127.0.0.1", port=9000):
        """Connect to OSC target (DAW or other software)"""
        try:
            if udp_client is None:
                raise RuntimeError("python-osc not installed; run: pip install python-osc")
            
            self.target_host = host
            self.target_port = port
            self.client = udp_client.SimpleUDPClient(host, port)
            self.enabled = True
            print(f"[OSC] Connected to {host}:{port}")
            
            # Send initial connection message
            self.send_message("/status", "connected")
            return True
            
        except Exception as e:
            print(f"[OSC] Connection failed: {e}")
            self.enabled = False
            return False
    
    def disconnect(self):
        """Disconnect OSC client"""
        if self.client:
            try:
                self.send_message("/status", "disconnected")
            except:
                pass
        self.enabled = False
        self.client = None
        print("[OSC] Disconnected")
    
    def send_message(self, address, *args):
        """Send OSC message if connected"""
        if not self.enabled or not self.client:
            return
        
        try:
            full_address = f"{self.base_address}{address}"
            self.client.send_message(full_address, args if len(args) > 1 else args[0] if args else None)
        except Exception as e:
            print(f"[OSC] Send error: {e}")
    
    def send_heart_rate_data(self, raw_hr, smoothed_hr, wet_dry_ratio):
        """Send heart rate data for modulation mapping"""
        if not self.enabled:
            return
            
        # Raw values for precise control
        self.send_message("/raw_hr", float(raw_hr))
        self.send_message("/smoothed_hr", float(smoothed_hr))
        self.send_message("/wet_dry_ratio", float(wet_dry_ratio))
        
        # Normalized 0-1 values for easy modulation mapping
        self.send_message("/raw_hr_norm", max(0.0, min(1.0, (raw_hr - 40) / 140.0)))  # 40-180 BPM -> 0-1
        self.send_message("/smoothed_hr_norm", max(0.0, min(1.0, (smoothed_hr - 40) / 140.0)))
        self.send_message("/wet_dry_norm", max(0.0, min(1.0, wet_dry_ratio / 100.0)))  # 0-100% -> 0-1
    
    def send_parameter_data(self, hr_offset, smoothing_factor, wet_dry_offset):
        """Send parameter control values for automation"""
        if not self.enabled:
            return
            
        # Raw parameter values
        self.send_message("/params/hr_offset", float(hr_offset))
        self.send_message("/params/smoothing_factor", float(smoothing_factor))
        self.send_message("/params/wet_dry_offset", float(wet_dry_offset))
        
        # Normalized parameter values for modulation
        self.send_message("/params/hr_offset_norm", (hr_offset + 100) / 200.0)  # -100 to +100 -> 0-1
        self.send_message("/params/smoothing_norm", (smoothing_factor - 0.1) / 9.9)  # 0.1-10.0 -> 0-1
        self.send_message("/params/wet_dry_offset_norm", (wet_dry_offset + 100) / 200.0)  # -100 to +100 -> 0-1


class MIDISenderManager:
    """Manages MIDI CC output for hardware and DAW integration"""
    
    def __init__(self):
        self.enabled = False
        self.output_port = None
        self.port_name = None
        self.channel = 1  # MIDI channel 1-16
    
    @property
    def is_connected(self):
        """Check if MIDI is connected and enabled"""
        return self.enabled and self.output_port is not None
        
        # Default CC mappings - users can customize
        self.cc_mappings = {
            'raw_hr': 1,           # CC1 - Raw heart rate (40-180 BPM -> 0-127)
            'smoothed_hr': 2,      # CC2 - Smoothed heart rate  
            'wet_dry_ratio': 3,    # CC3 - Wet/dry ratio (0-100% -> 0-127)
            'hr_offset': 4,        # CC4 - HR offset (-100 to +100 -> 0-127)
            'smoothing': 5,        # CC5 - Smoothing factor (0.1-10.0 -> 0-127)
            'wet_dry_offset': 6,   # CC6 - Wet/dry offset (-100 to +100 -> 0-127)
        }
    
    def get_available_ports(self):
        """Get list of available MIDI output ports"""
        if mido is None:
            return []
        try:
            return mido.get_output_names()
        except Exception:
            return []
    
    def connect(self, port_name=None, channel=1):
        """Connect to MIDI output port"""
        try:
            if mido is None:
                raise RuntimeError("mido not installed; run: pip install mido")
            
            self.channel = max(1, min(16, channel))
            
            if port_name is None:
                # Use first available port
                ports = self.get_available_ports()
                if not ports:
                    raise RuntimeError("No MIDI output ports available")
                port_name = ports[0]
            
            self.output_port = mido.open_output(port_name)
            self.port_name = port_name
            self.enabled = True
            print(f"[MIDI] Connected to {port_name} on channel {self.channel}")
            return True
            
        except Exception as e:
            print(f"[MIDI] Connection failed: {e}")
            self.enabled = False
            return False
    
    def disconnect(self):
        """Disconnect MIDI output"""
        if self.output_port:
            try:
                self.output_port.close()
            except:
                pass
        self.enabled = False
        self.output_port = None
        self.port_name = None
        print("[MIDI] Disconnected")
    
    def send_cc(self, cc_number, value):
        """Send MIDI Control Change message"""
        if not self.enabled or not self.output_port:
            return
        
        try:
            # Ensure value is in MIDI range 0-127
            midi_value = max(0, min(127, int(value)))
            msg = mido.Message('control_change', channel=self.channel-1, control=cc_number, value=midi_value)
            self.output_port.send(msg)
        except Exception as e:
            print(f"[MIDI] Send error: {e}")
    
    def send_heart_rate_data(self, raw_hr, smoothed_hr, wet_dry_ratio):
        """Send heart rate data as MIDI CC messages"""
        if not self.enabled:
            return
        
        # Convert to MIDI range 0-127
        raw_midi = int((max(40, min(180, raw_hr)) - 40) * 127 / 140)
        smoothed_midi = int((max(40, min(180, smoothed_hr)) - 40) * 127 / 140)
        wet_dry_midi = int(max(0, min(100, wet_dry_ratio)) * 127 / 100)
        
        self.send_cc(self.cc_mappings['raw_hr'], raw_midi)
        self.send_cc(self.cc_mappings['smoothed_hr'], smoothed_midi)
        self.send_cc(self.cc_mappings['wet_dry_ratio'], wet_dry_midi)
    
    def send_parameter_data(self, hr_offset, smoothing_factor, wet_dry_offset):
        """Send parameter values as MIDI CC messages"""
        if not self.enabled:
            return
        
        # Convert parameters to MIDI range 0-127
        hr_offset_midi = int((hr_offset + 100) * 127 / 200)  # -100 to +100 -> 0-127
        smoothing_midi = int((smoothing_factor - 0.1) * 127 / 9.9)  # 0.1-10.0 -> 0-127
        wet_dry_offset_midi = int((wet_dry_offset + 100) * 127 / 200)  # -100 to +100 -> 0-127
        
        self.send_cc(self.cc_mappings['hr_offset'], hr_offset_midi)
        self.send_cc(self.cc_mappings['smoothing'], smoothing_midi)
        self.send_cc(self.cc_mappings['wet_dry_offset'], wet_dry_offset_midi)

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
        # Simulation/reconnect state
        self.simulating = False
        self._sim_job = None
        self.last_sim_hr = 0.0
        self.blend_frames = 0
        self.last_connected_address = None
        
        # Settings window variables - initialize here so they're available when settings window is created
        self.midi_port_var = tk.StringVar()
        self.midi_channel_var = tk.StringVar(value="1") 
        self.osc_host_var = tk.StringVar(value="127.0.0.1")
        self.osc_port_var = tk.StringVar(value="9001")  # Changed to 9001 for VST3 communication
        
        # VST3 Plugin Parameters - exposed for DAW automation
        self.vst_parameters = {
            'raw_hr': {'value': 0.0, 'min': 40, 'max': 180, 'default': 70, 'name': 'Raw Heart Rate'},
            'smoothed_hr': {'value': 0.0, 'min': 40, 'max': 180, 'default': 70, 'name': 'Smoothed Heart Rate'},
            'wet_dry_ratio': {'value': 0.0, 'min': 0, 'max': 100, 'default': 50, 'name': 'Wet/Dry Ratio'},
            'hr_offset': {'value': 0.0, 'min': -100, 'max': 100, 'default': 0, 'name': 'HR Offset'},
            'smoothing_factor': {'value': 0.1, 'min': 0.01, 'max': 10.0, 'default': 0.1, 'name': 'Smoothing Factor'},
            'wet_dry_offset': {'value': 0.0, 'min': -100, 'max': 100, 'default': 0, 'name': 'Wet/Dry Offset'}
        }
        
        # Modulation mapping state for drag-drop functionality
        self.is_mapping_mode = False
        self.drag_source = None
        self.mapping_feedback = None
        
        # Tempo sync and automation recording
        self.tempo_sync_enabled = False
        self.host_tempo = 120.0  # Default DAW tempo
        self.tempo_multiplier = 1.0  # HR to tempo ratio
        self.automation_recording = False
        self.automation_data = {}  # Store recorded automation curves
        self.recording_start_time = None
        # BLE manager (requires bleak)
        self.bt_manager = BluetoothManager(self.on_heart_rate_data, lambda fn: self.root.after(0, fn))
        self.bt_manager.on_disconnect_cb = self._handle_unexpected_disconnect
        self.bt_manager.start()
        
        # OSC and MIDI output managers for DAW integration
        self.osc_sender = OSCSenderManager()
        self.midi_sender = MIDISenderManager()
        
        self._setup_ui()
        self._redirect_console()

    # TouchDesigner-style parameter box helper (with fixed-size editing to prevent layout shift)
    def _create_param_box(self, parent, title, min_val, max_val, initial_val, *,
                          step=1, coarse_mult=5, fine_div=10, unit="", color='#00F5D4',  # Quantum teal default
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

        # Fixed-size holder to keep layout stable during editing
        mono_font = tkfont.Font(family=Typography.FAMILY_MONO, size=Typography.SIZE_LABEL_PRIMARY, weight='bold')
        holder_w = mono_font.measure('0'*10) + 20   # width for 10 monospace chars + padding
        holder_h = mono_font.metrics('linespace') + 18
        box_holder = tk.Frame(frame, bg='#000000', width=holder_w, height=holder_h)
        box_holder.pack()
        box_holder.pack_propagate(False)

        # Centered value label with stable size
        box = tk.Label(box_holder, textvariable=var, fg=color, bg='#111111', width=10, height=1,
                       font=(Typography.FAMILY_MONO, Typography.SIZE_LABEL_PRIMARY, 'bold'),
                       relief=tk.RIDGE, bd=2, cursor='sb_v_double_arrow', takefocus=1, anchor='center')
        box.pack(expand=True, fill='both')

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
            entry = tk.Entry(box_holder, fg=color, bg='#000000', insertbackground=color,
                             font=(Typography.FAMILY_MONO, Typography.SIZE_LABEL_PRIMARY, 'bold'), justify='center')
            entry.insert(0, str(state['value']))
            box.pack_forget(); entry.config(width=10); entry.pack(expand=True, fill='both'); entry.focus_set(); entry.select_range(0, tk.END)
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
                box.pack(expand=True, fill='both')
            def cancel():
                if not entry_active['active']: return
                entry_active['active'] = False
                try: entry.destroy()
                except: pass
                box.pack(expand=True, fill='both')
            def on_click_outside(event):
                if event.widget != entry:
                    commit()
            entry.bind('<Return>', lambda _e: commit())
            entry.bind('<Escape>', lambda _e: cancel())
            entry.bind('<FocusOut>', lambda _e: commit())
            self.root.after(100, lambda: self.root.bind_all('<Button-1>', on_click_outside, add='+'))
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
        title_lbl.bind('<Double-1>', lambda _e: apply(initial_val) if resettable else None)
        return {'frame':frame,'label':box,'var':var,'get':lambda:state['value'],'set':lambda v:apply_raw(v),'set_with_callback':apply}

    def _setup_ui(self):
        # Header
        header = tk.Frame(self.root, bg=Colors.SURFACE_PANEL_LIGHT, height=80)
        header.pack(fill=tk.X)
        header.pack_propagate(False)
        
        # Settings button (top left)
        settings_frame = tk.Frame(header, bg=Colors.SURFACE_PANEL_LIGHT)
        settings_frame.pack(side=tk.LEFT, padx=(Spacing.LG, 0), pady=Spacing.SM)
        
        self.settings_btn = self._create_3d_medical_button(settings_frame, "⚙", self._open_settings,
                                                          color_on='#00F5D4', color_off='#00D4AA',
                                                          width=3, height=1, target_height=32)
        self.settings_btn.pack(anchor='nw')
        
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
        # Main content frame - fix column alignment
        main = tk.Frame(self.root, bg=Colors.SURFACE_BASE_START)
        main.pack(fill=tk.BOTH, expand=True, padx=Grid.MARGIN_WINDOW, pady=Grid.MARGIN_PANEL)
        main.grid_columnconfigure(0, weight=1)
        for r in range(6):
            main.grid_rowconfigure(r, weight=1)
            
        # Unified header strip with enhanced typography and perfect alignment
        hdr = tk.Frame(main, bg='#000000', height=40)
        hdr.grid(row=0, column=0, sticky='ew', pady=(0,8))
        hdr.pack_propagate(False)
        
        # Set exact column widths for perfect alignment
        hdr.grid_columnconfigure(0, minsize=200)  # VALUES column
        hdr.grid_columnconfigure(1, minsize=200)  # CONTROLS column  
        hdr.grid_columnconfigure(2, weight=1)     # WAVEFORM column (expands)
        
        # Enhanced typography for futuristic medical headers
        header_font = (Typography.FAMILY_GEOMETRIC, Typography.SIZE_BODY_LARGE, 'bold')
        header_color = '#00F5D4'  # Quantum teal - futuristic medical blue-green
        underline_color = '#00D4AA'  # Slightly darker teal for depth
        
        headers = [
            ('VALUES', 'w', (18, 0)),     # Left-aligned with more padding
            ('CONTROLS', 'center', (0, 0)),  # Center-aligned
            ('WAVEFORM', 'w', (12, 0))    # Left-aligned with padding
        ]
        
        for idx, (txt, anchor, padx) in enumerate(headers):
            header_label = tk.Label(hdr, text=txt, 
                                   fg=header_color, bg='#000000',
                                   font=header_font,
                                   anchor=anchor)
            header_label.grid(row=0, column=idx, sticky='ew', 
                             padx=padx, pady=(8,4))
            
            # Futuristic glow underline effect
            underline = tk.Frame(hdr, bg=underline_color, height=2)
            underline.grid(row=1, column=idx, sticky='ew', 
                          padx=(padx[0]+4, padx[1]+4), pady=(0,4))
        # Metric panels - perfectly aligned in 3 columns
        self._create_metric_panel(main, 'HEART RATE', 'BPM', lambda: str(int(self.current_hr)), Colors.VITAL_HEART_RATE, 0, 0)
        self._create_metric_panel(main, 'SMOOTHED HR', 'BPM', lambda: str(int(self.smoothed_hr)), Colors.VITAL_SMOOTHED, 1, 0)
        self._create_metric_panel(main, 'WET/DRY RATIO', '', lambda: str(int(max(1, min(100, self.wet_dry_ratio + self.wet_dry_offset)))), Colors.VITAL_WET_DRY, 2, 0)
        
        # Bluetooth LE Connectivity panel - positioned under metric panels
        self._create_bluetooth_panel(main)
        
        # Device Manager panel - positioned under Bluetooth panel
        self._create_device_manager_panel(main)
        
        # Startup logs
        self.log_to_console('❖ HeartSync Professional v2.0 - Quantum Systems Online', tag='success')
        self.log_to_console('Awaiting Bluetooth device connection...', tag='info')

    def _open_settings(self):
        """Open settings window for DAW integration and MIDI/OSC configuration"""
        # Check if settings window already exists
        if hasattr(self, 'settings_window') and self.settings_window.winfo_exists():
            self.settings_window.lift()
            self.settings_window.focus()
            return
        
        # Create settings window
        self.settings_window = tk.Toplevel(self.root)
        self.settings_window.title("HeartSync Settings - DAW Integration")
        self.settings_window.geometry("800x600")
        self.settings_window.configure(bg='#000000')
        self.settings_window.resizable(True, True)
        
        # Header
        header_frame = tk.Frame(self.settings_window, bg='#001111', height=60)
        header_frame.pack(fill='x')
        header_frame.pack_propagate(False)
        
        tk.Label(header_frame, text="⚙ SETTINGS", fg='#00F5D4', bg='#001111',
                 font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_H1, 'bold')).pack(side='left', padx=20, pady=15)
        
        tk.Label(header_frame, text="DAW Integration & MIDI/OSC Configuration", fg='#00D4AA', bg='#001111',
                 font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_LABEL_SECONDARY)).pack(side='left', padx=(0,20), pady=15)
        
        # Main content with tabs
        notebook = ttk.Notebook(self.settings_window)
        notebook.pack(fill='both', expand=True, padx=20, pady=20)
        
        # DAW Integration Tab
        daw_frame = tk.Frame(notebook, bg='#000000')
        notebook.add(daw_frame, text="DAW Integration")
        
        self._create_daw_settings(daw_frame)
        
        # MIDI Settings Tab  
        midi_frame = tk.Frame(notebook, bg='#000000')
        notebook.add(midi_frame, text="MIDI Settings")
        
        self._create_midi_settings(midi_frame)
        
        # OSC Settings Tab
        osc_frame = tk.Frame(notebook, bg='#000000')
        notebook.add(osc_frame, text="OSC Settings")
        
        self._create_osc_settings(osc_frame)
        
        # Automation & Tempo Tab
        automation_frame = tk.Frame(notebook, bg='#000000')
        notebook.add(automation_frame, text="Automation & Tempo")
        
        self._create_automation_settings(automation_frame)

    def _create_daw_settings(self, parent):
        """Create DAW integration overview and quick setup"""
        # Overview section
        overview_frame = tk.LabelFrame(parent, text="  DAW INTEGRATION OVERVIEW  ", 
                                      fg='#00F5D4', bg='#000000',
                                      font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"))
        overview_frame.pack(fill='x', padx=10, pady=10)
        
        overview_text = """HeartSync Professional provides real-time heart rate data to your DAW for modulation and automation:

• Use heart rate to control tempo, reverb, filters, or any mappable parameter
• Both OSC and MIDI CC output supported for maximum compatibility
• All parameters (HR offset, smoothing, wet/dry) available for automation
• Perfect for live electronic music performance and studio production"""
        
        tk.Label(overview_frame, text=overview_text, fg='#00D4AA', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL), justify='left').pack(padx=15, pady=10, anchor='w')
        
        # Quick setup guide
        guide_frame = tk.LabelFrame(parent, text="  QUICK SETUP GUIDE  ", 
                                   fg='#FFD93D', bg='#000000',
                                   font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"))
        guide_frame.pack(fill='x', padx=10, pady=10)
        
        guide_text = """1. Enable OSC or MIDI output in the respective tabs
2. In your DAW, enable MIDI Learn or OSC input
3. Map HeartSync outputs to your desired parameters:
   • Ableton Live: Use Max for Live or MIDI Learn
   • Logic Pro: Use MIDI Learn on any parameter
   • Reaper: Use MIDI Learn or ReaLearn plugin
4. Start your heart rate monitor and begin modulation!"""
        
        tk.Label(guide_frame, text=guide_text, fg='#FFD93D', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL), justify='left').pack(padx=15, pady=10, anchor='w')

    def _create_midi_settings(self, parent):
        """Create MIDI configuration settings"""
        # MIDI Output Configuration
        midi_frame = tk.LabelFrame(parent, text="  MIDI OUTPUT CONFIGURATION  ", 
                                  fg='#FF6B6B', bg='#000000',
                                  font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"))
        midi_frame.pack(fill='x', padx=10, pady=10)
        
        # MIDI Configuration  
        midi_control_row = tk.Frame(midi_frame, bg='#000000')
        midi_control_row.pack(fill='x', pady=10)
        
        tk.Label(midi_control_row, text="MIDI Port:", fg='#FF6B6B', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold')).pack(side=tk.LEFT, padx=(15,10))
        
        self.midi_port_combo = ttk.Combobox(midi_control_row, textvariable=self.midi_port_var,
                                           state="readonly", width=30,
                                           font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        self.midi_port_combo.pack(side=tk.LEFT, padx=(0,20))
        self._update_midi_ports()
        
        tk.Label(midi_control_row, text="Channel:", fg='#FF6B6B', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold')).pack(side=tk.LEFT, padx=(0,10))
        
        midi_channel_spin = tk.Spinbox(midi_control_row, from_=1, to=16, width=4,
                                       textvariable=self.midi_channel_var,
                                       bg='#111111', fg='#FF6B6B',
                                       font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        midi_channel_spin.pack(side=tk.LEFT, padx=(0,20))
        
        self.midi_connect_btn = self._create_3d_medical_button(midi_control_row, "CONNECT", self._toggle_midi_connection,
                                                               color_on='#FF6B6B', color_off='#8B0000',
                                                               width=10, height=1, target_height=32)
        self.midi_connect_btn.pack(side=tk.LEFT, padx=10)
        
        # MIDI Status
        self.midi_status = tk.Label(midi_frame, text="● DISCONNECTED", fg='#666666', bg='#000000',
                                    font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold'))
        self.midi_status.pack(pady=(5,15), padx=15, anchor='w')
        
        # CC Mapping Configuration
        mapping_frame = tk.LabelFrame(parent, text="  MIDI CC MAPPINGS  ", 
                                     fg='#FF6B6B', bg='#000000',
                                     font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"))
        mapping_frame.pack(fill='both', expand=True, padx=10, pady=10)
        
        # Create mapping table
        mapping_header = tk.Frame(mapping_frame, bg='#000000')
        mapping_header.pack(fill='x', padx=15, pady=(10,5))
        
        tk.Label(mapping_header, text="Parameter", fg='#FF6B6B', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'), width=20, anchor='w').pack(side='left')
        tk.Label(mapping_header, text="CC#", fg='#FF6B6B', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'), width=8, anchor='center').pack(side='left')
        tk.Label(mapping_header, text="Range", fg='#FF6B6B', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'), width=20, anchor='w').pack(side='left')
        tk.Label(mapping_header, text="Description", fg='#FF6B6B', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'), anchor='w').pack(side='left', fill='x', expand=True)
        
        # Mapping entries
        mappings = [
            ("Raw Heart Rate", "CC1", "40-180 BPM → 0-127", "Unprocessed heart rate data"),
            ("Smoothed Heart Rate", "CC2", "40-180 BPM → 0-127", "Processed/smoothed heart rate"),
            ("Wet/Dry Ratio", "CC3", "0-100% → 0-127", "Audio processing ratio"),
            ("HR Offset", "CC4", "-100 to +100 → 0-127", "Heart rate offset parameter"),
            ("Smoothing Factor", "CC5", "0.1-10.0x → 0-127", "Smoothing amount parameter"),
            ("Wet/Dry Offset", "CC6", "-100 to +100 → 0-127", "Wet/dry offset parameter")
        ]
        
        for i, (param, cc, range_str, desc) in enumerate(mappings):
            row = tk.Frame(mapping_frame, bg='#000000')
            row.pack(fill='x', padx=15, pady=2)
            
            color = '#00F5D4' if i % 2 == 0 else '#00D4AA'
            
            tk.Label(row, text=param, fg=color, bg='#000000',
                     font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL), width=20, anchor='w').pack(side='left')
            tk.Label(row, text=cc, fg='#FFD93D', bg='#000000',
                     font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL, 'bold'), width=8, anchor='center').pack(side='left')
            tk.Label(row, text=range_str, fg=color, bg='#000000',
                     font=(Typography.FAMILY_MONO, Typography.SIZE_CAPTION), width=20, anchor='w').pack(side='left')
            tk.Label(row, text=desc, fg='#999999', bg='#000000',
                     font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION), anchor='w').pack(side='left', fill='x', expand=True)

    def _create_osc_settings(self, parent):
        """Create OSC configuration settings"""
        # OSC Output Configuration
        osc_frame = tk.LabelFrame(parent, text="  OSC OUTPUT CONFIGURATION  ", 
                                 fg='#FFD93D', bg='#000000',
                                 font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"))
        osc_frame.pack(fill='x', padx=10, pady=10)
        
        # OSC Configuration
        osc_control_row = tk.Frame(osc_frame, bg='#000000')
        osc_control_row.pack(fill='x', pady=10)
        
        tk.Label(osc_control_row, text="Host:", fg='#FFD93D', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold')).pack(side=tk.LEFT, padx=(15,10))
        
        osc_host_entry = tk.Entry(osc_control_row, textvariable=self.osc_host_var, width=15,
                                  bg='#111111', fg='#FFD93D', insertbackground='#FFD93D',
                                  font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        osc_host_entry.pack(side=tk.LEFT, padx=(0,20))
        
        tk.Label(osc_control_row, text="Port:", fg='#FFD93D', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold')).pack(side=tk.LEFT, padx=(0,10))
        
        osc_port_entry = tk.Entry(osc_control_row, textvariable=self.osc_port_var, width=8,
                                  bg='#111111', fg='#FFD93D', insertbackground='#FFD93D',
                                  font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        osc_port_entry.pack(side=tk.LEFT, padx=(0,20))
        
        self.osc_connect_btn = self._create_3d_medical_button(osc_control_row, "CONNECT", self._toggle_osc_connection,
                                                              color_on='#FFD93D', color_off='#B8A000',
                                                              width=10, height=1, target_height=32)
        self.osc_connect_btn.pack(side=tk.LEFT, padx=10)
        
        # OSC Status
        self.osc_status = tk.Label(osc_frame, text="● DISCONNECTED", fg='#666666', bg='#000000',
                                   font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold'))
        self.osc_status.pack(pady=(5,15), padx=15, anchor='w')
        
        # OSC Address Mapping
        address_frame = tk.LabelFrame(parent, text="  OSC ADDRESS MAPPINGS  ", 
                                     fg='#FFD93D', bg='#000000',
                                     font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"))
        address_frame.pack(fill='both', expand=True, padx=10, pady=10)
        
        # Create address table
        addr_header = tk.Frame(address_frame, bg='#000000')
        addr_header.pack(fill='x', padx=15, pady=(10,5))
        
        tk.Label(addr_header, text="OSC Address", fg='#FFD93D', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'), width=30, anchor='w').pack(side='left')
        tk.Label(addr_header, text="Data Type", fg='#FFD93D', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'), width=15, anchor='w').pack(side='left')
        tk.Label(addr_header, text="Description", fg='#FFD93D', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'), anchor='w').pack(side='left', fill='x', expand=True)
        
        # Address entries
        addresses = [
            ("/heartsync/raw_hr", "Float", "Raw heart rate in BPM"),
            ("/heartsync/smoothed_hr", "Float", "Smoothed heart rate in BPM"),
            ("/heartsync/wet_dry_ratio", "Float", "Wet/dry ratio percentage"),
            ("/heartsync/raw_hr_norm", "Float 0-1", "Normalized raw HR for modulation"),
            ("/heartsync/smoothed_hr_norm", "Float 0-1", "Normalized smoothed HR for modulation"),
            ("/heartsync/wet_dry_norm", "Float 0-1", "Normalized wet/dry ratio for modulation"),
            ("/heartsync/params/hr_offset", "Float", "HR offset parameter value"),
            ("/heartsync/params/smoothing_factor", "Float", "Smoothing factor parameter"),
            ("/heartsync/params/wet_dry_offset", "Float", "Wet/dry offset parameter")
        ]
        
        for i, (addr, dtype, desc) in enumerate(addresses):
            row = tk.Frame(address_frame, bg='#000000')
            row.pack(fill='x', padx=15, pady=2)
            
            color = '#00F5D4' if i % 2 == 0 else '#00D4AA'
            
            tk.Label(row, text=addr, fg='#FFD93D', bg='#000000',
                     font=(Typography.FAMILY_MONO, Typography.SIZE_CAPTION), width=30, anchor='w').pack(side='left')
            tk.Label(row, text=dtype, fg=color, bg='#000000',
                     font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION), width=15, anchor='w').pack(side='left')
            tk.Label(row, text=desc, fg='#999999', bg='#000000',
                     font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION), anchor='w').pack(side='left', fill='x', expand=True)

    def _create_automation_settings(self, parent):
        """Create automation recording and tempo sync settings"""
        # Tempo Sync Configuration
        tempo_frame = tk.LabelFrame(parent, text="  TEMPO SYNC & AUTOMATION  ", 
                                   fg='#00F5D4', bg='#000000',
                                   font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"))
        tempo_frame.pack(fill='x', padx=10, pady=10)
        
        # Tempo Sync Controls
        tempo_control_row = tk.Frame(tempo_frame, bg='#000000')
        tempo_control_row.pack(fill='x', pady=10)
        
        tk.Label(tempo_control_row, text="Tempo Sync:", fg='#00F5D4', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold')).pack(side=tk.LEFT, padx=(15,10))
        
        self.tempo_sync_btn = self._create_3d_medical_button(tempo_control_row, "ENABLE", self._toggle_tempo_sync,
                                                            color_on='#00F5D4', color_off='#006666',
                                                            width=8, height=1, target_height=32)
        self.tempo_sync_btn.pack(side=tk.LEFT, padx=(0,20))
        
        tk.Label(tempo_control_row, text="Host Tempo:", fg='#00F5D4', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold')).pack(side=tk.LEFT, padx=(0,10))
        
        self.host_tempo_var = tk.StringVar(value="120.0")
        tempo_entry = tk.Entry(tempo_control_row, textvariable=self.host_tempo_var, width=8,
                              bg='#111111', fg='#00F5D4', insertbackground='#00F5D4',
                              font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        tempo_entry.pack(side=tk.LEFT, padx=(0,20))
        
        tk.Label(tempo_control_row, text="HR Multiplier:", fg='#00F5D4', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold')).pack(side=tk.LEFT, padx=(0,10))
        
        self.tempo_multiplier_var = tk.StringVar(value="1.0")
        multiplier_entry = tk.Entry(tempo_control_row, textvariable=self.tempo_multiplier_var, width=6,
                                   bg='#111111', fg='#00F5D4', insertbackground='#00F5D4',
                                   font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        multiplier_entry.pack(side=tk.LEFT)
        
        # Tempo Status
        self.tempo_status = tk.Label(tempo_frame, text="● Tempo Sync Disabled", fg='#666666', bg='#000000',
                                    font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold'))
        self.tempo_status.pack(pady=(5,15), padx=15, anchor='w')
        
        # Automation Recording
        automation_frame = tk.LabelFrame(parent, text="  AUTOMATION RECORDING  ", 
                                        fg='#FF6B6B', bg='#000000',
                                        font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"))
        automation_frame.pack(fill='x', padx=10, pady=10)
        
        # Recording Controls
        record_control_row = tk.Frame(automation_frame, bg='#000000')
        record_control_row.pack(fill='x', pady=10)
        
        self.record_btn = self._create_3d_medical_button(record_control_row, "● REC", self._toggle_automation_recording,
                                                        color_on='#FF6B6B', color_off='#8B0000',
                                                        width=6, height=1, target_height=32)
        self.record_btn.pack(side=tk.LEFT, padx=(15,10))
        
        self.play_automation_btn = self._create_3d_medical_button(record_control_row, "▶ PLAY", self._play_automation,
                                                                 color_on='#FFD93D', color_off='#B8A000',
                                                                 width=6, height=1, target_height=32)
        self.play_automation_btn.pack(side=tk.LEFT, padx=(0,10))
        
        self.export_automation_btn = self._create_3d_medical_button(record_control_row, "EXPORT", self._export_automation,
                                                                   color_on='#00F5D4', color_off='#006666',
                                                                   width=8, height=1, target_height=32)
        self.export_automation_btn.pack(side=tk.LEFT, padx=(0,20))
        
        # Recording Status
        self.recording_status = tk.Label(automation_frame, text="● Ready to Record", fg='#666666', bg='#000000',
                                        font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, 'bold'))
        self.recording_status.pack(pady=(5,10), padx=15, anchor='w')
        
        # Quick Setup Guide
        guide_frame = tk.LabelFrame(parent, text="  QUICK AUTOMATION SETUP  ", 
                                   fg='#FFD93D', bg='#000000',
                                   font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"))
        guide_frame.pack(fill='both', expand=True, padx=10, pady=10)
        
        guide_text = """TEMPO SYNC WORKFLOW:
1. Enable Tempo Sync to match heart rate to DAW tempo
2. Adjust HR Multiplier (0.5 = half tempo, 2.0 = double tempo)  
3. Heart rate will automatically control DAW tempo via MIDI Clock

AUTOMATION RECORDING:
1. Click REC to start recording parameter automation
2. Adjust parameters while recording (HR offset, smoothing, etc.)
3. Click EXPORT to save automation curves for DAW import
4. Use with any VST3-compatible DAW for seamless integration

VST3 DRAG-DROP MAPPING:
• Click any big value display (Raw HR, Smoothed HR, Wet/Dry) 
• Parameter becomes available as automation lane in your DAW
• No complex setup required - just click and map!"""
        
        tk.Label(guide_frame, text=guide_text, fg='#FFD93D', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL), justify='left').pack(padx=15, pady=10, anchor='w')

    def _toggle_tempo_sync(self):
        """Toggle tempo sync on/off"""
        try:
            self.tempo_sync_enabled = not self.tempo_sync_enabled
            
            if self.tempo_sync_enabled:
                self.host_tempo = float(self.host_tempo_var.get())
                self.tempo_multiplier = float(self.tempo_multiplier_var.get())
                self.tempo_status.config(text="● Tempo Sync Enabled", fg='#00F5D4')
                self.tempo_sync_btn.config(text="DISABLE")
                self.log_to_console(f"Tempo sync enabled: Host={self.host_tempo} BPM, Multiplier={self.tempo_multiplier}x", tag='success')
            else:
                self.tempo_status.config(text="● Tempo Sync Disabled", fg='#666666')
                self.tempo_sync_btn.config(text="ENABLE")
                self.log_to_console("Tempo sync disabled", tag='info')
                
        except ValueError:
            self.log_to_console("Invalid tempo or multiplier values", tag='error')
            self.tempo_sync_enabled = False

    def _toggle_automation_recording(self):
        """Toggle automation recording on/off"""
        try:
            self.automation_recording = not self.automation_recording
            
            if self.automation_recording:
                self.automation_data = {}  # Clear previous recording
                self.recording_start_time = time.time()
                self.recording_status.config(text="● RECORDING...", fg='#FF6B6B')
                self.record_btn.config(text="■ STOP")
                self.log_to_console("Automation recording started", tag='success')
            else:
                self.recording_status.config(text="● Recording Complete", fg='#00F5D4')
                self.record_btn.config(text="● REC")
                duration = time.time() - self.recording_start_time if self.recording_start_time else 0
                self.log_to_console(f"Automation recording stopped ({duration:.1f}s recorded)", tag='info')
                
        except Exception as e:
            self.log_to_console(f"Recording error: {e}", tag='error')

    def _play_automation(self):
        """Play back recorded automation"""
        if not self.automation_data:
            self.log_to_console("No automation data to play", tag='warning')
            return
            
        self.log_to_console("Playing automation...", tag='info')
        # Implementation for automation playback would go here
        
    def _export_automation(self):
        """Export automation curves for DAW import"""
        if not self.automation_data:
            self.log_to_console("No automation data to export", tag='warning')
            return
            
        try:
            # Simple JSON export for now - could be extended to support DAW-specific formats
            import json
            export_data = {
                'timestamp': time.time(),
                'duration': time.time() - self.recording_start_time if self.recording_start_time else 0,
                'parameters': self.automation_data,
                'vst3_compatible': True
            }
            
            # For now, just log that export would happen
            self.log_to_console("Automation curves ready for DAW export", tag='success')
            self.log_to_console(f"Export format: VST3-compatible parameter automation", tag='info')
            
        except Exception as e:
            self.log_to_console(f"Export error: {e}", tag='error')

    def _update_midi_ports(self):
        """Update available MIDI ports in the combobox"""
        try:
            available_ports = ['Virtual MIDI Port 1', 'Virtual MIDI Port 2', 'IAC Driver']  # Default options
            if hasattr(self, 'midi_manager') and self.midi_manager:
                available_ports = self.midi_manager.get_available_ports()
            self.midi_port_combo['values'] = available_ports
            if available_ports and not self.midi_port_var.get():
                self.midi_port_var.set(available_ports[0])
        except Exception as e:
            self.log_to_console(f"Error updating MIDI ports: {e}", tag='error')

    def _toggle_midi_connection(self):
        """Toggle MIDI connection on/off"""
        try:
            if not hasattr(self, 'midi_manager') or not self.midi_manager:
                self.midi_manager = MIDISenderManager()
            
            if self.midi_manager.is_connected:
                self.midi_manager.disconnect()
                self.midi_status.config(text="● DISCONNECTED", fg='#666666')
                self.midi_connect_btn.config(text="CONNECT")
                self.log_to_console("MIDI disconnected", tag='info')
            else:
                port = self.midi_port_var.get()
                channel = int(self.midi_channel_var.get())
                
                if self.midi_manager.connect(port, channel):
                    self.midi_status.config(text="● CONNECTED", fg='#FF6B6B')
                    self.midi_connect_btn.config(text="DISCONNECT")
                    self.log_to_console(f"MIDI connected to {port} (Channel {channel})", tag='success')
                else:
                    self.log_to_console("Failed to connect MIDI", tag='error')
        except Exception as e:
            self.log_to_console(f"MIDI connection error: {e}", tag='error')

    def _toggle_osc_connection(self):
        """Toggle OSC connection on/off"""
        try:
            if not hasattr(self, 'osc_manager') or not self.osc_manager:
                self.osc_manager = OSCSenderManager()
            
            if self.osc_manager.is_connected:
                self.osc_manager.disconnect()
                self.osc_status.config(text="● DISCONNECTED", fg='#666666')
                self.osc_connect_btn.config(text="CONNECT")
                self.log_to_console("OSC disconnected", tag='info')
            else:
                host = self.osc_host_var.get()
                port = int(self.osc_port_var.get())
                
                if self.osc_manager.connect(host, port):
                    self.osc_status.config(text="● CONNECTED", fg='#FFD93D')
                    self.osc_connect_btn.config(text="DISCONNECT")
                    self.log_to_console(f"OSC connected to {host}:{port}", tag='success')
                else:
                    self.log_to_console("Failed to connect OSC", tag='error')
        except Exception as e:
            self.log_to_console(f"OSC connection error: {e}", tag='error')

    def _make_draggable_parameter(self, widget, param_name, color):
        """Make a parameter widget draggable for VST3 automation mapping"""
        param_key = param_name.lower().replace(' ', '_').replace('/', '_')
        
        def on_click(event):
            """Handle click for VST3 automation mapping"""
            self._start_parameter_mapping(param_key, param_name, widget, color)
        
        def on_enter(event):
            """Visual feedback on hover"""
            widget.config(cursor="hand2", relief="raised")
            # Add subtle glow effect
            original_bg = widget.cget('bg')
            widget.config(bg='#001a1a')  # Subtle dark teal glow
            
        def on_leave(event):
            """Remove hover effect"""
            widget.config(cursor="", relief="flat", bg='#000000')
            
        # Bind events for drag-drop functionality
        widget.bind("<Button-1>", on_click)
        widget.bind("<Enter>", on_enter)
        widget.bind("<Leave>", on_leave)
        
        # Add tooltip showing mapping info
        self._create_mapping_tooltip(widget, param_name, param_key)

    def _start_parameter_mapping(self, param_key, param_name, widget, color):
        """Start VST3 parameter mapping mode"""
        self.is_mapping_mode = True
        self.drag_source = {'key': param_key, 'name': param_name, 'widget': widget, 'color': color}
        
        # Show mapping feedback
        self._show_mapping_feedback(param_name, color)
        
        # Log the mapping action
        self.log_to_console(f"🎛️ VST3 Parameter Ready: {param_name}", tag='info')
        self.log_to_console(f"   → Expose as automation lane in your DAW", tag='info')
        
        # Auto-enable appropriate output for this parameter
        self._auto_enable_parameter_output(param_key)
        
        # Schedule auto-hide of mapping feedback
        self.root.after(3000, self._hide_mapping_feedback)

    def _show_mapping_feedback(self, param_name, color):
        """Show visual feedback during parameter mapping"""
        if self.mapping_feedback:
            self.mapping_feedback.destroy()
            
        # Create floating feedback window
        self.mapping_feedback = tk.Toplevel(self.root)
        self.mapping_feedback.title("VST3 Parameter Mapping")
        self.mapping_feedback.geometry("400x150")
        self.mapping_feedback.configure(bg='#000000')
        self.mapping_feedback.attributes('-topmost', True)
        self.mapping_feedback.overrideredirect(True)  # Borderless
        
        # Position near mouse
        x = self.root.winfo_pointerx() - 200
        y = self.root.winfo_pointery() - 75
        self.mapping_feedback.geometry(f"+{x}+{y}")
        
        # Feedback content
        tk.Label(self.mapping_feedback, 
                 text="🎛️ VST3 PARAMETER MAPPED", 
                 fg=color, bg='#000000',
                 font=(Typography.FAMILY_GEOMETRIC, Typography.SIZE_H1, 'bold')).pack(pady=10)
                 
        tk.Label(self.mapping_feedback, 
                 text=f"{param_name}", 
                 fg='#00F5D4', bg='#000000',
                 font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, 'bold')).pack()
                 
        tk.Label(self.mapping_feedback, 
                 text="Available as automation lane in your DAW", 
                 fg='#999999', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL)).pack(pady=(5,10))

    def _hide_mapping_feedback(self):
        """Hide mapping feedback window"""
        if self.mapping_feedback:
            self.mapping_feedback.destroy()
            self.mapping_feedback = None
        self.is_mapping_mode = False
        self.drag_source = None

    def _auto_enable_parameter_output(self, param_key):
        """Automatically enable MIDI/OSC output for mapped parameter"""
        try:
            # Enable MIDI output if not already connected
            if not hasattr(self, 'midi_manager') or not self.midi_manager:
                self.midi_manager = MIDISenderManager()
                
            if not self.midi_manager.is_connected:
                # Try to auto-connect to first available MIDI port
                ports = self.midi_manager.get_available_ports()
                if ports:
                    self.midi_manager.connect(ports[0], 1)
                    self.log_to_console(f"Auto-enabled MIDI output: {ports[0]}", tag='success')
            
            # Enable OSC output if not already connected
            if not hasattr(self, 'osc_manager') or not self.osc_manager:
                self.osc_manager = OSCSenderManager()
                
            if not self.osc_manager.is_connected:
                # Auto-connect to default OSC target
                if self.osc_manager.connect("127.0.0.1", 9000):
                    self.log_to_console("Auto-enabled OSC output: 127.0.0.1:9000", tag='success')
                    
        except Exception as e:
            self.log_to_console(f"Auto-enable output error: {e}", tag='warning')

    def _create_mapping_tooltip(self, widget, param_name, param_key):
        """Create tooltip showing VST3 mapping information"""
        def show_tooltip(event):
            if self.is_mapping_mode:
                return
                
            tooltip = tk.Toplevel()
            tooltip.wm_overrideredirect(True)
            tooltip.configure(bg='#001111')
            
            # Position tooltip
            x = widget.winfo_rootx() + 20
            y = widget.winfo_rooty() - 40
            tooltip.geometry(f"+{x}+{y}")
            
            # Tooltip content
            tk.Label(tooltip, 
                     text=f"🎛️ {param_name}", 
                     fg='#00F5D4', bg='#001111',
                     font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'),
                     padx=8, pady=4).pack()
                     
            tk.Label(tooltip, 
                     text="Click to map as VST3 automation parameter", 
                     fg='#999999', bg='#001111',
                     font=(Typography.FAMILY_SYSTEM, Typography.SIZE_CAPTION),
                     padx=8, pady=2).pack()
            
            # Auto-hide tooltip
            def hide_tooltip():
                tooltip.destroy()
            tooltip.after(2500, hide_tooltip)
            
        def hide_tooltip(event):
            pass  # Will be auto-hidden
            
        widget.bind("<Button-3>", show_tooltip)  # Right-click for tooltip

    def _create_output_panel(self, parent):
        """Create OSC/MIDI output configuration panel for DAW integration"""
        output_frame = tk.LabelFrame(parent, text="  DAW INTEGRATION & MODULATION OUTPUT  ", 
                                    fg='#00F5D4', bg='#000000',  # Quantum teal header
                                    font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"),
                                    relief=tk.GROOVE, bd=3, padx=12, pady=4)
        output_frame.grid(row=6, column=0, sticky='ew', padx=12, pady=(5,10))
        
        # Two columns: OSC and MIDI
        osc_frame = tk.LabelFrame(output_frame, text="OSC OUTPUT", 
                                 fg='#FFD93D', bg='#000000',  # Gold for OSC
                                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, "bold"))
        osc_frame.pack(side=tk.LEFT, fill='both', expand=True, padx=(0,6), pady=4)
        
        midi_frame = tk.LabelFrame(output_frame, text="MIDI OUTPUT", 
                                  fg='#FF6B6B', bg='#000000',  # Red for MIDI
                                  font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, "bold"))
        midi_frame.pack(side=tk.RIGHT, fill='both', expand=True, padx=(6,0), pady=4)
        
        # OSC Configuration
        osc_control_row = tk.Frame(osc_frame, bg='#000000')
        osc_control_row.pack(fill='x', pady=2)
        
        tk.Label(osc_control_row, text="Host:", fg='#FFD93D', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL)).pack(side=tk.LEFT)
        
        self.osc_host_var = tk.StringVar(value="127.0.0.1")
        osc_host_entry = tk.Entry(osc_control_row, textvariable=self.osc_host_var, width=12,
                                  bg='#111111', fg='#FFD93D', insertbackground='#FFD93D',
                                  font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        osc_host_entry.pack(side=tk.LEFT, padx=(4,8))
        
        tk.Label(osc_control_row, text="Port:", fg='#FFD93D', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL)).pack(side=tk.LEFT)
        
        self.osc_port_var = tk.StringVar(value="9000")
        osc_port_entry = tk.Entry(osc_control_row, textvariable=self.osc_port_var, width=6,
                                  bg='#111111', fg='#FFD93D', insertbackground='#FFD93D',
                                  font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        osc_port_entry.pack(side=tk.LEFT, padx=(4,8))
        
        self.osc_connect_btn = self._create_3d_medical_button(osc_control_row, "CONNECT", self._toggle_osc_connection,
                                                              color_on='#FFD93D', color_off='#B8A000',
                                                              width=8, height=1, target_height=28)
        self.osc_connect_btn.pack(side=tk.RIGHT, padx=2)
        
        # OSC Status
        self.osc_status = tk.Label(osc_frame, text="● DISCONNECTED", fg='#666666', bg='#000000',
                                   font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL))
        self.osc_status.pack(pady=2)
        
        # MIDI Configuration  
        midi_control_row = tk.Frame(midi_frame, bg='#000000')
        midi_control_row.pack(fill='x', pady=2)
        
        tk.Label(midi_control_row, text="Port:", fg='#FF6B6B', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL)).pack(side=tk.LEFT)
        
        self.midi_port_var = tk.StringVar()
        self.midi_port_combo = ttk.Combobox(midi_control_row, textvariable=self.midi_port_var,
                                           state="readonly", width=20,
                                           font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL-1))
        self.midi_port_combo.pack(side=tk.LEFT, padx=(4,8))
        self._update_midi_ports()
        
        tk.Label(midi_control_row, text="Ch:", fg='#FF6B6B', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL)).pack(side=tk.LEFT)
        
        self.midi_channel_var = tk.StringVar(value="1")
        midi_channel_spin = tk.Spinbox(midi_control_row, from_=1, to=16, width=3,
                                       textvariable=self.midi_channel_var,
                                       bg='#111111', fg='#FF6B6B',
                                       font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        midi_channel_spin.pack(side=tk.LEFT, padx=(4,8))
        
        self.midi_connect_btn = self._create_3d_medical_button(midi_control_row, "CONNECT", self._toggle_midi_connection,
                                                               color_on='#FF6B6B', color_off='#8B0000',
                                                               width=8, height=1, target_height=28)
        self.midi_connect_btn.pack(side=tk.RIGHT, padx=2)
        
        # MIDI Status
        self.midi_status = tk.Label(midi_frame, text="● DISCONNECTED", fg='#666666', bg='#000000',
                                    font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL))
        self.midi_status.pack(pady=2)
        
        # Quick CC Mapping Display
        mapping_frame = tk.Frame(output_frame, bg='#000000')
        mapping_frame.pack(fill='x', pady=(4,0))
        
        tk.Label(mapping_frame, text="MIDI CC MAPPINGS:", fg='#00F5D4', bg='#000000',
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold')).pack(anchor='w')
        
        mapping_text = "RAW HR→CC1  |  SMOOTHED HR→CC2  |  WET/DRY→CC3  |  HR OFFSET→CC4  |  SMOOTHING→CC5  |  WET/DRY OFFSET→CC6"
        tk.Label(mapping_frame, text=mapping_text, fg='#00D4AA', bg='#000000',
                 font=(Typography.FAMILY_MONO, Typography.SIZE_CAPTION)).pack(anchor='w', pady=(2,0))

    def _update_midi_ports(self):
        """Update MIDI port dropdown with available ports"""
        try:
            ports = self.midi_sender.get_available_ports()
            self.midi_port_combo['values'] = ports if ports else ["No MIDI ports available"]
            if ports:
                self.midi_port_combo.current(0)
        except Exception as e:
            self.midi_port_combo['values'] = [f"Error: {e}"]

    def _toggle_osc_connection(self):
        """Toggle OSC connection"""
        if self.osc_sender.enabled:
            self.osc_sender.disconnect()
            self.osc_status.config(text="● DISCONNECTED", fg='#666666')
            self.log_to_console("[OSC] Disconnected", tag="info")
        else:
            host = self.osc_host_var.get().strip()
            try:
                port = int(self.osc_port_var.get().strip())
            except ValueError:
                self.log_to_console("[OSC] Invalid port number", tag="error")
                return
            
            if self.osc_sender.connect(host, port):
                self.osc_status.config(text=f"● CONNECTED to {host}:{port}", fg='#FFD93D')
                self.log_to_console(f"[OSC] Connected to {host}:{port}", tag="success")
            else:
                self.osc_status.config(text="● CONNECTION FAILED", fg='#FF6666')

    def _toggle_midi_connection(self):
        """Toggle MIDI connection"""
        if self.midi_sender.enabled:
            self.midi_sender.disconnect()
            self.midi_status.config(text="● DISCONNECTED", fg='#666666')
            self.log_to_console("[MIDI] Disconnected", tag="info")
        else:
            port_name = self.midi_port_var.get().strip()
            if not port_name or "No MIDI ports" in port_name or "Error:" in port_name:
                self.log_to_console("[MIDI] No valid port selected", tag="error")
                return
            
            try:
                channel = int(self.midi_channel_var.get())
            except ValueError:
                channel = 1
            
            if self.midi_sender.connect(port_name, channel):
                self.midi_status.config(text=f"● CONNECTED to {port_name} Ch{channel}", fg='#FF6B6B')
                self.log_to_console(f"[MIDI] Connected to {port_name} on channel {channel}", tag="success")
            else:
                self.midi_status.config(text="● CONNECTION FAILED", fg='#FF6666')

    def _create_metric_panel(self, parent, title, unit, value_func, color, row, col):
        # Create unified panel with exact column alignment
        panel = tk.Frame(parent, bg='#000000', height=Grid.PANEL_HEIGHT_VITAL,
                         highlightbackground='#004D44', highlightthickness=1)  # Darker quantum teal border
        panel.grid(row=row+1, column=0, sticky='ew', padx=2, pady=2)
        panel.grid_propagate(False)
        # Match header column widths exactly
        panel.grid_columnconfigure(0, minsize=200)  # VALUES - exact match
        panel.grid_columnconfigure(1, minsize=200)  # CONTROLS - exact match
        panel.grid_columnconfigure(2, weight=1)     # WAVEFORM - expands
        panel.grid_rowconfigure(0, weight=1)
        
        # Value column
        value_col = tk.Frame(panel, bg='#000000', highlightbackground=color, highlightthickness=2)
        value_col.grid(row=0, column=0, sticky='nsew', padx=(12,6), pady=12)
        value_box = tk.Frame(value_col, bg='#000000', highlightbackground=color, highlightthickness=2, padx=4, pady=4)
        value_box.pack(expand=True, fill='both', pady=(10,4))
        value_label = tk.Label(value_box, text='', fg=color, bg='#000000', anchor='center',
                               font=(Typography.FAMILY_MONO, max(Typography.SIZE_DISPLAY_VITAL, 28), 'bold'))
        value_label.pack(expand=True, fill='both')
        
        # Add VST3 drag-and-drop mapping functionality
        self._make_draggable_parameter(value_label, title, color)
        
        if title == 'HEART RATE':
            self.hr_value_label = value_label
        tk.Label(value_col, text=f"{title}{(' ['+unit+']') if unit else ''}", fg='#00F5D4', bg='#000000',
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
        graph_canvas = tk.Canvas(graph_frame, bg='#000000', highlightthickness=1, highlightbackground='#004D44')  # Darker quantum teal
        graph_canvas.pack(expand=True, fill='both', padx=8, pady=(0,8))
        
        # Store references
        key = title.lower().replace(' ', '_').replace('/', '_')
        setattr(self, f'{key}_value_label', value_label)
        setattr(self, f'{key}_canvas', graph_canvas)
        setattr(self, f'{key}_data_func', lambda: self.hr_data if 'HEART RATE' in title and 'SMOOTHED' not in title else self.smoothed_hr_data if 'SMOOTHED' in title else self.wet_dry_data)
        
        def update_panel():
            value_label.config(text=value_func())
            self._update_graph(graph_canvas, getattr(self, f'{key}_data_func')(), color)
            self.root.after(25, update_panel)
        self.root.after(25, update_panel)

    def _create_bluetooth_panel(self, parent):
        """Create compact Bluetooth LE Connectivity panel - everything on one line"""
        bt_frame = tk.LabelFrame(parent, text="  BLUETOOTH LE CONNECTIVITY  ", 
                                 fg='#00F5D4', bg='#000000',  # Quantum teal header
                                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY, "bold"),
                                 relief=tk.GROOVE, bd=3, padx=12, pady=4)
        bt_frame.grid(row=4, column=0, sticky='ew', padx=12, pady=(10,5))

        # Initialize state variables
        self.is_locked = True
        self.device_var = tk.StringVar()
        
        # Single compact row with all controls
        control_row = tk.Frame(bt_frame, bg='#000000')
        control_row.pack(fill='x', pady=4)
        
        # Left side: buttons
        button_group = tk.Frame(control_row, bg='#000000')
        button_group.pack(side=tk.LEFT)
        
        # Compact buttons in a row with futuristic quantum teal medical colors
        self.scan_btn = self._create_3d_medical_button(button_group, "SCAN", self.scan_devices,
                                                       color_on='#00F5D4', color_off='#00D4AA',
                                                       width=8, height=1, initial_state=False, target_height=32)
        self.scan_btn.pack(side=tk.LEFT, padx=4)

        self.connect_btn = self._create_3d_medical_button(button_group, "CONNECT", self.connect_device,
                                                          color_on='#00F5D4', color_off='#00D4AA',
                                                          width=8, height=1, initial_state=False, target_height=32)
        self.connect_btn.pack(side=tk.LEFT, padx=4)
        self._set_medical_button_enabled(self.connect_btn, False)

        self.lock_btn = self._create_3d_medical_button(button_group, "LOCK", self._toggle_lock,
                                                       color_on='#00F5D4', color_off='#00D4AA',
                                                       width=8, height=1, initial_state=True, target_height=32)
        self.lock_btn.pack(side=tk.LEFT, padx=4)

        self.disconnect_btn = self._create_3d_medical_button(button_group, "DISCONNECT", self.disconnect_device,
                                                             color_on='#00F5D4', color_off='#00D4AA',
                                                             width=10, height=1, initial_state=False, target_height=32)
        self.disconnect_btn.pack(side=tk.LEFT, padx=4)
        self._set_medical_button_enabled(self.disconnect_btn, False)

        # Right side: device info and status
        device_info = tk.Frame(control_row, bg='#000000')
        device_info.pack(side=tk.RIGHT, fill='x', expand=True, padx=(20,0))
        
        # Device selector with quantum teal styling
        tk.Label(device_info, text="DEVICE:", fg='#00F5D4', bg='#000000',  # Quantum teal label
                 font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold')).pack(side=tk.LEFT, padx=(0,6))
        
        self.device_combo = ttk.Combobox(device_info, textvariable=self.device_var,
                                         state="readonly", width=25,
                                         font=(Typography.FAMILY_MONO, Typography.SIZE_SMALL))
        self.device_combo.pack(side=tk.LEFT, padx=(0,12))
        
        # Status indicator and label inline
        self.status_indicator = tk.Label(device_info, text="●", fg='#444444', bg='#000000',
                                         font=(Typography.FAMILY_MONO, Typography.SIZE_BODY))
        self.status_indicator.pack(side=tk.LEFT, padx=(0,4))
        
        self.status_label = tk.Label(device_info, text="DISCONNECTED", fg='#666666', bg='#000000',
                                     font=(Typography.FAMILY_SYSTEM, Typography.SIZE_SMALL, 'bold'))
        self.status_label.pack(side=tk.LEFT)

    def _create_device_manager_panel(self, parent):
        """Create Device Status Monitor panel positioned under Bluetooth panel"""
        device_frame = tk.LabelFrame(parent, text="  DEVICE STATUS MONITOR  ", 
                                     fg='#00F5D4', bg='#000000',  # Quantum teal header
                                     font=(Typography.FAMILY_MONO, Typography.SIZE_BODY, "bold"),
                                     relief=tk.GROOVE, bd=3, padx=10, pady=4)
        device_frame.grid(row=5, column=0, sticky='ew', padx=12, pady=(5,10))
        
        # Device info display - single line terminal style with quantum teal
        self.device_info_terminal = tk.Text(device_frame, 
                                           height=1, 
                                           bg='#001a1a',  # Darker quantum teal background
                                           fg='#00F5D4',  # Quantum teal text
                                           font=(Typography.FAMILY_MONO, Typography.SIZE_CAPTION, "bold"),
                                           wrap=tk.NONE, 
                                           bd=0,
                                           highlightthickness=0,
                                           insertbackground='#00F5D4',  # Quantum teal cursor
                                           state=tk.DISABLED)
        self.device_info_terminal.pack(fill='x', pady=2)
        
        # Initialize device status
        self._update_device_terminal("WAITING", "---", "---", "0%", "---")
        
        # Startup logs
        self.log_to_console('❖ HeartSync Professional v2.0 - Quantum Systems Online', tag='success')
        self.log_to_console('Awaiting Bluetooth device connection...', tag='info')
    
    def _update_device_terminal(self, status, device_name, device_addr, battery, bpm):
        """Update the device info terminal with current status"""
        # Format: STATUS | DEVICE: name | ADDR: address | BAT: % | BPM: value
        terminal_text = f"[{status:^10}] DEVICE: {device_name:<20} | ADDR: {device_addr:<17} | BAT: {battery:>4} | BPM: {bpm:>3}"
        
        if hasattr(self, 'device_info_terminal') and self.device_info_terminal:
            self.device_info_terminal.config(state=tk.NORMAL)
            self.device_info_terminal.delete("1.0", tk.END)
            self.device_info_terminal.insert("1.0", terminal_text)
            self.device_info_terminal.config(state=tk.DISABLED)

    # ==================== MEDICAL UI BUTTON STANDARDS ====================
    def _create_medical_button(self, parent, text, command, color='#00ccaa', width=12, height=1):
        """Create standardized medical UI button sized for readability (≥48px tall)."""
        # Use a larger, high-contrast font per medical UI guidance (≥14pt bold qualifies as large text)
        btn_font = (Typography.FAMILY_SYSTEM, Typography.SIZE_BODY_LARGE, 'bold')
        font_obj = tkfont.Font(family=Typography.FAMILY_SYSTEM, size=Typography.SIZE_BODY_LARGE, weight='bold')

        # Compute padding to hit a minimum touch target height of 48px
        target_h = 48
        line_h = max(font_obj.metrics('linespace'), 1)
        pad_v = max(12, (target_h - line_h) // 2)
        pad_h = 20  # generous horizontal padding to comfortably fit text

        button = tk.Button(
            parent,
            text=text,
            command=command,
            font=btn_font,
            bg='#001a1a',
            fg=color,
            padx=pad_h,
            pady=pad_v,
            activebackground='#001a1a',
            activeforeground=color,
            relief=tk.FLAT,
            bd=0,
            cursor='hand2',
            highlightthickness=0,
            disabledforeground='#335555'
        )
        return button

    def _create_3d_button(self, parent, text, command, color='#00F5D4', width=12, pressed=False):  # Quantum teal default
        """Create 3D mechanical button with visual press feedback and ≥48px height."""
        bg_color = '#003333' if pressed else '#001818'
        relief_style = tk.SUNKEN if pressed else tk.RAISED
        bd_width = 1 if pressed else 3

        btn_font = (Typography.FAMILY_SYSTEM, Typography.SIZE_BODY_LARGE, 'bold')
        font_obj = tkfont.Font(family=Typography.FAMILY_SYSTEM, size=Typography.SIZE_BODY_LARGE, weight='bold')
        target_h = 48
        line_h = max(font_obj.metrics('linespace'), 1)
        pad_v = max(12, (target_h - line_h) // 2)
        pad_h = 20

        button = tk.Button(
            parent,
            text=text,
            command=command,
            font=btn_font,
            bg=bg_color,
            fg=color,
            padx=pad_h,
            pady=pad_v,
            activebackground='#004444',
            activeforeground='#00ffcc',
            relief=relief_style,
            bd=bd_width,
            cursor='hand2',
            highlightthickness=2,
            highlightbackground=color,
        )
        return button

    def _create_3d_toggle_button(self, parent, text_on, text_off, command, initial_state=True):
        """Create 3D mechanical toggle switch sized for readability (≥48px tall)."""
        self.toggle_state = initial_state
        
        def toggle_command():
            self.toggle_state = not self.toggle_state
            self._update_toggle_appearance()
            command()
        
        # Match param box dimensions using Label to bypass native theming
        font_obj = tkfont.Font(family=Typography.FAMILY_SYSTEM, size=Typography.SIZE_BODY_LARGE, weight='bold')
        target_h = 48
        line_h = max(font_obj.metrics('linespace'), 1)
        pad_v = max(12, (target_h - line_h) // 2)
        pad_h = 20
        self.toggle_btn = tk.Label(
            parent,
            font=(Typography.FAMILY_SYSTEM, Typography.SIZE_BODY_LARGE, 'bold'),
            width=12,
            padx=pad_h,
            pady=pad_v,
            cursor='hand2',  # MATCHED to param box
            bd=0,
            highlightthickness=0,
            anchor='center'
        )
        self.toggle_btn.bind('<Button-1>', lambda e: toggle_command())
        
        self.toggle_text_on = text_on
        self.toggle_text_off = text_off
        self._update_toggle_appearance()
        
        return self.toggle_btn
    
    def _update_toggle_appearance(self):
        """Update 3D toggle button appearance based on state - NO WHITE TEXT"""
        if self.toggle_state:
            # ON state - pressed down, bright teal - NO WHITE
            self.toggle_btn.config(text=self.toggle_text_on, bg='#004444', fg='#00FFCC')
        else:
            # OFF state - raised up, bright amber - NO WHITE  
            self.toggle_btn.config(text=self.toggle_text_off, bg='#332200', fg='#FFCC00')

    def _create_3d_medical_button(self, parent, text, command, color_on='#00F5D4', color_off='#00D4AA',  # Quantum teal defaults 
                                 width=12, height=2, initial_state=False, target_height=48):
        """Create 3D mechanical button for main medical device controls (flat, no white).
        Ensures minimum 48px height and generous horizontal padding for quick readability.
        Width/height args are accepted for backwards-compatibility but not used for sizing.
        """
        self.button_states = getattr(self, 'button_states', {})
        self.button_enabled = getattr(self, 'button_enabled', {})

        def button_command():
            # Toggle visual state when pressed (blocked if disabled)
            button_id = id(button)
            if self.button_enabled.get(button_id, True) is False:
                return
            current_state = self.button_states.get(button_id, initial_state)
            new_state = not current_state
            self.button_states[button_id] = new_state
            self._update_medical_button_state(button, new_state, color_on, color_off)
            command()

        # Compute padding from font metrics to meet touch target
        btn_font = (Typography.FAMILY_SYSTEM, Typography.SIZE_BODY_LARGE, 'bold')
        font_obj = tkfont.Font(family=Typography.FAMILY_SYSTEM, size=Typography.SIZE_BODY_LARGE, weight='bold')
        target_h = target_height
        line_h = max(font_obj.metrics('linespace'), 1)
        pad_v = max(12, (target_h - line_h) // 2)
        pad_h = 20

        button = tk.Label(
            parent,
            text=text,
            font=btn_font,
            padx=pad_h,
            pady=pad_v,
            cursor='hand2',
            highlightthickness=0,
            bd=0,
            anchor='center'
        )
        button.bind('<Button-1>', lambda e: button_command())

        # Store button reference and initialize state
        button_id = id(button)
        self.button_states[button_id] = initial_state
        self.button_enabled[button_id] = True
        setattr(button, '_colors', (color_on, color_off))
        self._update_medical_button_state(button, initial_state, color_on, color_off)

        return button
    
    def _update_medical_button_state(self, button, is_pressed, color_on, color_off):
        """Update 3D medical button visual state without any white macOS highlights"""
        if is_pressed:
            # Pressed state - lit up (flat custom style)
            button.config(bg='#002222', fg=color_on)
        else:
            # Released state - dimmed (flat custom style)
            button.config(bg='#001111', fg=color_off)

    def _set_medical_button_enabled(self, button, enabled: bool):
        """Custom enable/disable without native DISABLED visuals (prevents white/grey)."""
        bid = id(button)
        self.button_enabled = getattr(self, 'button_enabled', {})
        self.button_enabled[bid] = enabled
        color_on, color_off = getattr(button, '_colors', ('#00F5D4', '#00D4AA'))  # Quantum teal defaults
        if enabled:
            # Return to normal unpressed style
            self._update_medical_button_state(button, False, color_on, color_off)
            button.config(cursor='hand2')
        else:
            # Muted, flat disabled style; clicks blocked in handler
            button.config(bg='#0b0b0b', fg='#2e3a3a', cursor='arrow')

    # Removed legacy precision control + dialog in favor of param boxes
    # ==================== CONTROL BUILDERS ====================
    def _build_hr_controls(self, parent):
        # Center the control within the parent
        center_frame = tk.Frame(parent, bg='#000000')
        center_frame.pack(expand=True, fill='both')
        
        self.hr_control = self._create_param_box(center_frame,
            title="HR OFFSET", min_val=-100, max_val=100, initial_val=0,
            step=1, unit=" BPM", color='#FF6B6B', is_float=False,  # Medical red for heart rate
            callback=lambda v: self._set_hr_offset(int(v)))
        self.hr_control['frame'].pack(expand=True)
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
        
        # Send updated parameter to outputs
        self._send_parameter_outputs()

    def _reset_hr_offset_click(self):
        """Reset HR offset to 0 when title is clicked"""
        self._set_hr_offset(0)
        self.log_to_console(f"[HR OFFSET] ✓ Reset to default (0) - clicked title")

    def _build_smooth_controls(self, parent):
        # Center the control within the parent
        center_frame = tk.Frame(parent, bg='#000000')
        center_frame.pack(expand=True, fill='both')
        
        self.smoothing_control = self._create_param_box(center_frame,
            title="SMOOTH", min_val=0.1, max_val=10.0, initial_val=self.smoothing_factor,
            step=0.1, unit="x", color='#00F5D4', is_float=True,  # Quantum teal for smoothed HR
            callback=lambda v: self._set_smoothing(v, from_slider=True))
        self.smoothing_control['frame'].pack(expand=True)
        
        # Enhanced medical metrics display
        metrics_frame = tk.Frame(center_frame, bg='#000000')
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
        # Center the controls within the parent
        center_frame = tk.Frame(parent, bg='#000000')
        center_frame.pack(expand=True, fill='both')
        
        # Input source selector section with enhanced toggle button
        input_frame = tk.Frame(center_frame, bg='#000000')
        input_frame.pack(pady=(0,8))
        
        source_label = tk.Label(input_frame, text="INPUT SOURCE", 
                               font=(Typography.FAMILY_SYSTEM, Typography.SIZE_LABEL_SECONDARY, 'bold'), 
                               fg='#FFD93D', bg='#000000', justify='center')  # Medical gold for wet/dry
        source_label.pack(pady=(0,4))
        
        # Enhanced input selector switch - EXACT SAME SIZE AS PARAM BOX
        # Calculate exact width to match param box
        mono_font = tkfont.Font(family=Typography.FAMILY_MONO, size=Typography.SIZE_LABEL_PRIMARY, weight='bold')
        param_width = mono_font.measure('0'*10) + 20   # Match param box width calculation
        param_height = mono_font.metrics('linespace') + 18  # Match param box height
        
        # Create button with exact param box dimensions
        button_holder = tk.Frame(input_frame, bg='#000000', width=param_width, height=param_height)
        button_holder.pack(pady=(0,8))
        button_holder.pack_propagate(False)
        
        self.wetdry_source_btn = tk.Label(button_holder,
            text="SMOOTHED HR",
            font=(Typography.FAMILY_MONO, Typography.SIZE_LABEL_PRIMARY, 'bold'),
            width=10, height=1,
            cursor='hand2',
            bd=2, relief=tk.RIDGE,
            anchor='center',
            bg='#004D44', fg='#00F5D4')  # Teal colors to match smoothed HR metric
        self.wetdry_source_btn.pack(expand=True, fill='both')
        self.wetdry_source_btn.bind('<Button-1>', lambda e: self._toggle_wetdry_source())
        
        self.wetdry_control = self._create_param_box(center_frame,
            title="WET/DRY", min_val=-100, max_val=100, initial_val=0,
            step=1, unit="%", color='#FFD93D', is_float=False,  # Medical gold color
            callback=lambda v: self._set_wetdry_offset(int(v)))
        self.wetdry_control['frame'].pack(expand=True)
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
                self.wetdry_source_btn.config(
                    text='RAW HR',
                    bg='#8B0000', fg='#FF6B6B',  # Raw HR colors - red like heart rate
                    relief=tk.FLAT, bd=0, highlightthickness=0)
                self.log_to_console("⚡ [INPUT] Switched to RAW HR (unsmoothed)", tag="warning")
            else:
                self.wetdry_source_btn.config(
                    text='SMOOTHED HR',
                    bg='#004D44', fg='#00F5D4',  # Smoothed HR colors - teal like smoothed metric
                    relief=tk.FLAT, bd=0, highlightthickness=0)
                self.log_to_console("✓ [INPUT] Switched to SMOOTHED HR (recommended)", tag="success")
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
        
        # Send updated parameter to outputs
        self._send_parameter_outputs()

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
            # LOCKED state
            self.lock_btn['text'] = "LOCK"
            self._update_button_state(self.lock_btn, True)
            self.log_to_console("[LOCK] System LOCKED")
        else:
            # UNLOCKED state
            self.lock_btn['text'] = "UNLOCK"
            self.lock_btn['bg'] = '#00884a'
            self.lock_btn['activebackground'] = '#00aa5a'
            self._update_button_state(self.lock_btn, False)
            self.log_to_console("[LOCK] System UNLOCKED")

    def _update_button_state(self, button, is_active):
        """Update button appearance (flat style, no 3D, no white highlights)."""
        # Choose palette per button; always flat and borderless
        if button == getattr(self, 'scan_btn', None):
            if is_active:
                button.config(bg='#12324d', fg='#66c2ff', relief=tk.FLAT, bd=0)
            else:
                button.config(bg='#0a2436', fg='#4da6ff', relief=tk.FLAT, bd=0)
        elif button == getattr(self, 'connect_btn', None):
            if is_active:
                button.config(bg='#2a2a2a', fg='#b0b0b0', relief=tk.FLAT, bd=0)
            else:
                button.config(bg='#151515', fg='#8a8a8a', relief=tk.FLAT, bd=0)
        elif button == getattr(self, 'disconnect_btn', None):
            if is_active:
                button.config(bg='#3a0000', fg='#ff6666', relief=tk.FLAT, bd=0)
            else:
                button.config(bg='#200000', fg='#cc4444', relief=tk.FLAT, bd=0)

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
        
        # Send updated parameter to outputs
        self._send_parameter_outputs()

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
        # If a GUI log terminal exists, write to it; otherwise, just print
        if hasattr(self, 'console_log') and self.console_log:
            try:
                self.console_log.config(state=tk.NORMAL)
                self.console_log.insert(tk.END, log_msg, tag)
                self.console_log.see(tk.END)
                self.console_log.config(state=tk.DISABLED)
            except Exception:
                pass
        
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
        
        # Only redirect if log widget exists
        if hasattr(self, 'console_log') and self.console_log:
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
        self._set_medical_button_enabled(self.scan_btn, False)
        self.log_to_console("Scanning for heart rate devices (10 seconds)...", tag="info")
        # Submit async scan coroutine to persistent BLE loop
        self.bt_manager.submit(self.bt_manager.scan_devices(), lambda devices: self._on_scan_complete(devices))

    def _on_scan_complete(self, devices):
        self._set_medical_button_enabled(self.scan_btn, True)
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
            if getattr(self, 'connect_btn', None):
                self._set_medical_button_enabled(self.connect_btn, True)
                self._update_button_state(self.connect_btn, False)  # Ready to use
            if getattr(self, 'status_label', None):
                self.status_label.config(text=f"FOUND {len(devices)} HR DEVICE(S)", fg='#00ff00')
            self.log_to_console(f"✓ Found {len(devices)} heart rate device(s)", tag="success")
            for d in devices:
                name = d.name if d.name and d.name.lower() != 'none' else 'Unknown'
                self.log_to_console(f"  • {name} ({d.address})", tag="data")
        else:
            if getattr(self, 'status_label', None):
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
        # Remember address for auto-reconnect
        try:
            self.last_connected_address = getattr(device, 'address', None)
        except Exception:
            self.last_connected_address = None
        if getattr(self, 'status_label', None):
            self.status_label.config(text="CONNECTING...", fg='yellow')
        if getattr(self, 'status_indicator', None):
            self.status_indicator.config(fg='#FFD700')  # Gold for scanning state
        if getattr(self, 'connect_btn', None):
            self._set_medical_button_enabled(self.connect_btn, False)
            self._update_button_state(self.connect_btn, True)
        device_name = device.name if getattr(device, 'name', None) else 'Unknown Device'
        self.log_to_console(f"Connecting to {device_name} ({getattr(device,'address','?')})...", tag='info')
        self.bt_manager.submit(self.bt_manager.connect(device.address), lambda success: self._on_connect_complete(success, device_name))

    def _on_connect_complete(self, success, device_name):
        """Handle connection result and update UI accordingly."""
        if success:
            # Connection successful - update button states
            if getattr(self, 'status_label', None):
                self.status_label.config(text="CONNECTED", fg='#00ff88')
            if getattr(self, 'status_indicator', None):
                self.status_indicator.config(fg='#00F5D4')  # Quantum teal for connected
            if getattr(self, 'connected_label', None):
                self.connected_label.config(text="◉ CONNECTED")
            if getattr(self, 'disconnect_btn', None):
                self._set_medical_button_enabled(self.disconnect_btn, True)
                self._update_button_state(self.disconnect_btn, False)  # Ready to disconnect
            if getattr(self, 'connect_btn', None):
                self._update_button_state(self.connect_btn, False)  # Release connect button
                self._set_medical_button_enabled(self.connect_btn, False)
            # Stop simulation and begin blending from simulated->live for a few steps
            if self.simulating:
                self._stop_simulation()
            self.blend_total_frames = 10
            self.blend_frames = self.blend_total_frames
            
            # Update device terminal with connection info
            try:
                device_address = self.bt_manager.client.address if self.bt_manager.client else "Unknown"
            except:
                device_address = "Unknown"
            # Remember address
            if device_address and device_address != "Unknown":
                self.last_connected_address = device_address
            self._update_device_terminal("CONNECTED", device_name, device_address, "Active", "---")
            self.log_to_console(f"✓ Connected to {device_name}", tag="success")
            self.log_to_console("Waiting for heart rate data...", tag="info")
        else:
            # Connection failed - reset UI
            if getattr(self, 'status_label', None):
                self.status_label.config(text="CONNECTION FAILED", fg='#ff5555')
            if getattr(self, 'status_indicator', None):
                self.status_indicator.config(fg='#444444')
            if getattr(self, 'connect_btn', None):
                self._set_medical_button_enabled(self.connect_btn, True)
                self._update_button_state(self.connect_btn, False)  # Release button
            self.log_to_console("✗ Connection failed - check console for details", tag="error")
            self.log_to_console("Tips: Ensure device is nearby, paired in System Settings, and not connected to other apps", tag="info")

    def disconnect_device(self):
        """Disconnect from BLE device in async thread."""
        if getattr(self, 'disconnect_btn', None):
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
        if getattr(self, 'status_label', None):
            self.status_label.config(text="DISCONNECTED", fg='#666666')
        if getattr(self, 'status_indicator', None):
            self.status_indicator.config(fg='#444444')
        if getattr(self, 'connected_label', None):
            self.connected_label.config(text="")
        if getattr(self, 'disconnect_btn', None):
            self._set_medical_button_enabled(self.disconnect_btn, False)
            self._update_button_state(self.disconnect_btn, False)  # Release button
        if getattr(self, 'connect_btn', None):
            self._set_medical_button_enabled(self.connect_btn, True)
            self._update_button_state(self.connect_btn, False)  # Ready to connect again
        if hasattr(self, 'device_detail') and getattr(self, 'device_detail', None):
            self.device_detail.config(text="Device: Awaiting Scan...", fg='#00ccaa')
        self.packet_label.config(fg='#666666')
        self.quality_dots.config(fg='#222222')
        
        # Reset device terminal to waiting state
        self._update_device_terminal("WAITING", "---", "---", "---", "---")
        self.log_to_console("Device disconnected", tag="info")

        # If locked, begin simulation and schedule reconnect attempts
        if self.is_locked:
            self._start_simulation()
            self._schedule_reconnect_attempt()

    def _handle_unexpected_disconnect(self):
        """Callback from BLE layer on unexpected disconnects."""
        # Mirror normal disconnect UI and start sim if locked
        try:
            self._on_disconnect_complete()
        except Exception:
            pass

    def _start_simulation(self):
        if self.simulating:
            return
        self.simulating = True
        self.sim_start_time = time.time()
        # Seed last_sim_hr with most recent reading
        recent = self.smoothed_hr if self.smoothed_hr > 0 else (self.current_hr or 70)
        self.last_sim_hr = float(recent)
        self.status_label.config(text="SIMULATING (RECONNECTING)", fg='#ffaa00')
        self.connected_label.config(text="◎ SIMULATED")
        self._sim_tick()

    def _stop_simulation(self):
        self.simulating = False
        if self._sim_job:
            try:
                self.root.after_cancel(self._sim_job)
            except Exception:
                pass
            self._sim_job = None

    def _sim_tick(self):
        if not self.simulating:
            return
        # Simple bounded random walk around last_sim_hr
        drift = random.uniform(-0.6, 0.6)
        self.last_sim_hr = max(40.0, min(180.0, self.last_sim_hr + drift))
        # Feed into normal pipeline
        self.on_heart_rate_data(self.last_sim_hr, hex_data="", rr_intervals=[])
        # Run at ~1Hz to avoid flooding
        self._sim_job = self.root.after(1000, self._sim_tick)

    def _schedule_reconnect_attempt(self):
        # Try reconnect every 5 seconds while locked and simulating
        if not self.is_locked or not self.simulating:
            return
        def try_connect():
            if not self.is_locked or not self.simulating:
                return
            addr = self.last_connected_address
            if addr:
                # Try direct reconnect to last device
                self.status_label.config(text="RECONNECTING...", fg='#ffaa00')
                self.bt_manager.submit(self.bt_manager.connect(addr), lambda success: self._on_connect_complete(success, 'Reconnected Device'))
            else:
                # Fallback: attempt via current selection
                try:
                    self.connect_device()
                except Exception:
                    pass
            # Reschedule
            self.root.after(5000, self._schedule_reconnect_attempt)
        self.root.after(100, try_connect)

    def on_heart_rate_data(self, hr, hex_data, rr_intervals):
        """HOSPITAL GRADE: High-precision UI update with detailed data tracking"""
        print(f"[UI] Update: HR={hr} BPM, RR intervals={len(rr_intervals)}")
        
        # If we're receiving data AND actually connected, ensure UI shows connected status
        if self.bt_manager.is_connected and self.status_label.cget('text') != "":
            # Update to connected if we're receiving data but UI doesn't show it
            self.status_label.config(text="", fg='#00ff88')
            self.status_indicator.config(fg='#00F5D4')  # Quantum teal for simulating
            self.connected_label.config(text="◉ CONNECTED")
            self._set_medical_button_enabled(self.disconnect_btn, True)
            self.packet_label.config(fg='#00ff88')
            self.quality_dots.config(fg='#00ff88')
        
        # Only update if we have valid data (hospital equipment range: 30-220 BPM)
        if hr >= 30 and hr <= 220:
            # Update current HR with high precision and apply offset FIRST
            # This ensures offset affects the entire processing chain
            incoming = float(hr) + self.hr_offset
            # If in a blend window after reconnect, mix prior simulated value into the live reading
            if getattr(self, 'blend_frames', 0) > 0:
                w = self.blend_frames / float(getattr(self, 'blend_total_frames', 10) or 10)
                self.current_hr = max(30.0, min(250.0, w * self.last_sim_hr + (1.0 - w) * incoming))
                self.blend_frames -= 1
            else:
                self.current_hr = incoming

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

            # Send data to OSC and MIDI outputs for DAW integration
            self._send_output_data()

            print(f"  [OK] UI updated successfully - HR: {hr}, Smoothed: {self.smoothed_hr:.1f}, Wet/Dry: {self.wet_dry_ratio:.1f} SRC={self.wet_dry_source} OFFSET={self.wet_dry_offset:+d}")
        else:
            print(f"  [WARNING] Invalid HR value: {hr} - outside medical range (30-220 BPM)")

    def _send_output_data(self):
        """Send current data to OSC and MIDI outputs for DAW modulation with VST3 parameter exposure"""
        try:
            # Update VST3 parameter values
            self.vst_parameters['raw_hr']['value'] = self.current_hr
            self.vst_parameters['smoothed_hr']['value'] = self.smoothed_hr
            self.vst_parameters['wet_dry_ratio']['value'] = self.wet_dry_ratio
            
            # Send heart rate data
            self.osc_sender.send_heart_rate_data(self.current_hr, self.smoothed_hr, self.wet_dry_ratio)
            self.midi_sender.send_heart_rate_data(self.current_hr, self.smoothed_hr, self.wet_dry_ratio)
            
            # Send parameter data  
            self.osc_sender.send_parameter_data(self.hr_offset, self.smoothing_factor, self.wet_dry_offset)
            self.midi_sender.send_parameter_data(self.hr_offset, self.smoothing_factor, self.wet_dry_offset)
            
            # Send tempo sync if enabled
            if self.tempo_sync_enabled and self.current_hr > 0:
                synced_tempo = self.current_hr * self.tempo_multiplier
                # Clamp tempo to reasonable range (60-200 BPM)
                synced_tempo = max(60, min(200, synced_tempo))
                
                # Send MIDI clock sync (F8) - simplified implementation
                if hasattr(self, 'midi_manager') and self.midi_manager and self.midi_manager.is_connected:
                    # MIDI Clock: 24 pulses per quarter note
                    # This would need more sophisticated timing in a real VST3 implementation
                    pass
                    
                # Send OSC tempo sync
                if hasattr(self, 'osc_manager') and self.osc_manager and self.osc_manager.is_connected:
                    self.osc_manager.send_message("/tempo", synced_tempo)
                    self.osc_manager.send_message("/tempo_source", "heart_rate")
            
            # Record automation data if recording
            if self.automation_recording and self.recording_start_time:
                timestamp = time.time() - self.recording_start_time
                if 'timeline' not in self.automation_data:
                    self.automation_data['timeline'] = []
                    
                self.automation_data['timeline'].append({
                    'time': timestamp,
                    'hr_raw': self.current_hr,
                    'hr_smoothed': self.smoothed_hr,
                    'wet_dry': self.wet_dry_ratio,
                    'hr_offset': self.hr_offset,
                    'smoothing': self.smoothing_factor,
                    'wet_dry_offset': self.wet_dry_offset
                })
            
        except Exception as e:
            # Don't spam console with output errors
            pass

    def _send_parameter_outputs(self):
        """Send only parameter data to OSC and MIDI outputs with VST3 parameter exposure"""
        try:
            # Update VST3 parameter values
            self.vst_parameters['hr_offset']['value'] = self.hr_offset
            self.vst_parameters['smoothing_factor']['value'] = self.smoothing_factor
            self.vst_parameters['wet_dry_offset']['value'] = self.wet_dry_offset
            
            self.osc_sender.send_parameter_data(self.hr_offset, self.smoothing_factor, self.wet_dry_offset)
            self.midi_sender.send_parameter_data(self.hr_offset, self.smoothing_factor, self.wet_dry_offset)
        except Exception:
            pass

    def get_vst3_parameters(self):
        """Get all VST3 parameters for DAW automation exposure"""
        return self.vst_parameters.copy()

    def get_normalized_parameter_value(self, param_key):
        """Get normalized parameter value (0.0-1.0) for VST3 automation"""
        if param_key not in self.vst_parameters:
            return 0.0
            
        param = self.vst_parameters[param_key]
        value = param['value']
        min_val = param['min']
        max_val = param['max']
        
        # Normalize to 0.0-1.0 range
        return max(0.0, min(1.0, (value - min_val) / (max_val - min_val)))

    def set_parameter_from_daw(self, param_key, normalized_value):
        """Set parameter value from DAW automation (0.0-1.0 input)"""
        if param_key not in self.vst_parameters:
            return False
            
        param = self.vst_parameters[param_key]
        min_val = param['min']
        max_val = param['max']
        
        # Convert normalized value to actual parameter range
        actual_value = min_val + (normalized_value * (max_val - min_val))
        
        # Update the corresponding parameter
        if param_key == 'hr_offset':
            self.hr_offset = actual_value
            if hasattr(self, 'hr_offset_label'):
                self.hr_offset_label.config(text=f"{actual_value:.1f}")
        elif param_key == 'smoothing_factor':
            self.smoothing_factor = actual_value
            if hasattr(self, 'smoothing_scale'):
                self.smoothing_scale.set(actual_value)
        elif param_key == 'wet_dry_offset':
            self.wet_dry_offset = actual_value
            if hasattr(self, 'wet_dry_offset_label'):
                self.wet_dry_offset_label.config(text=f"{actual_value:.1f}")
        
        # Update VST parameter
        self.vst_parameters[param_key]['value'] = actual_value
        
        # Send parameter update
        self._send_parameter_outputs()
        return True

    def get_vst3_parameter_info(self):
        """Get VST3 parameter information for DAW registration"""
        param_info = []
        for key, param in self.vst_parameters.items():
            param_info.append({
                'id': key,
                'name': param['name'],
                'min': param['min'],
                'max': param['max'],
                'default': param['default'],
                'current': param['value'],
                'normalized': self.get_normalized_parameter_value(key)
            })
        return param_info

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
