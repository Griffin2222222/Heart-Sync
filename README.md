# ❖ Heart Sync Professional

**Next-Generation Heart Rate Monitoring with Adaptive Audio Bio Technology**

A parallel-universe scientific technology aesthetic meets medical-grade precision in this advanced heart rate monitoring system. Features real-time Bluetooth LE connectivity, quantum-grade waveform visualization, and adaptive wet/dry ratio calculation for audio synthesis applications.

![Version](https://img.shields.io/badge/version-2.0-00F5D4)
![Python](https://img.shields.io/badge/python-3.8+-00F5D4)
![License](https://img.shields.io/badge/license-MIT-00F5D4)

## ✨ Features

### 🫀 Real-Time Heart Rate Monitoring
- Bluetooth LE connectivity with Polar H10 and compatible HR monitors
- High-precision BPM tracking with 300-sample buffering
- Customizable HR offset (-100 to +100 BPM)
- Exponential moving average smoothing (0.1x to 10x)

### 📊 Advanced Waveform Visualization
- Real-time waveform rendering at 40 FPS
- Photon grid with glowing effects
- 60-120 BPM normal range highlighting
- Anti-aliased rendering for smooth visuals

### 🎚️ Adaptive Wet/Dry Control
- Intelligent wet/dry ratio calculation
- Switchable input source (Raw HR or Smoothed HR)
- Manual offset adjustment (-100 to +100)
- Real-time HRV integration when available

### 🎨 Next-Gen UI Design
- Deep gradient backgrounds with plasma teal accents
- Luminous typography with perfect 8pt grid alignment
- Glass morphism panels with inner glows
- Quantum dot status indicators
- Golden ratio spacing throughout

### 🔧 Professional Controls
- 3D toggle switches with tactile feedback
- Lock/Unlock system state management
- High-tech terminal displays
- Comprehensive activity logging

## 🚀 Quick Start

### Prerequisites

```bash
python >= 3.8
bleak >= 0.21.0  # Bluetooth LE support
tkinter           # Usually included with Python
```

### Installation

1. Clone the repository:
```bash
git clone https://github.com/yourusername/Heart-Sync.git
cd Heart-Sync
```

2. Install dependencies:
```bash
pip install -r requirements.txt
```

3. Run the application:
```bash
python HeartSyncProfessional.py
```

## 🎮 Usage

### Connecting a Heart Rate Monitor

1. **Scan for Devices**: Click the "SCAN" button in the Bluetooth LE Connectivity panel
2. **Select Device**: Choose your heart rate monitor from the dropdown
3. **Connect**: Click "CONNECT" to establish connection
4. **Monitor**: Watch real-time heart rate data in the waveform displays

### Adjusting Parameters

#### Heart Rate Offset
- Use the vertical slider in the HR control panel
- Range: -100 to +100 BPM
- Click the title "HR OFFS" to reset to 0

#### Smoothing Factor
- Adjust with +/- buttons or spinner
- Range: 0.1x (most responsive) to 10x (most stable)
- Click "SMOOTH" title to reset to default (0.1x)

#### Wet/Dry Ratio
- **Input Source Toggle**: Click the large 3D button to switch between:
  - **SMOOTHED HR** (blue-white, raised) - Recommended
  - **RAW HR** (plasma teal, sunken) - Direct BPM
- **Offset**: Use vertical slider (-100 to +100)
- Click "WET/DRY" title to reset both to defaults

### Lock/Unlock System
- Click the "LOCKED"/"UNLOCKED" button to toggle system state
- Prevents accidental parameter changes when locked

## 🏗️ Architecture

### Design System
The application uses a comprehensive token-based design system (`theme_tokens_nextgen.py`) with:
- **Colors**: Deep gradients, plasma accents, luminous text
- **Typography**: Geometric sans with tabular numerals
- **Spacing**: Golden ratio + 8pt grid system
- **Motion**: Harmonic cubic-bezier transitions (90-180ms)
- **Accessibility**: WCAG 2.1 Level AA compliant

### Core Components

#### BluetoothManager
Handles all BLE connectivity:
- Device scanning and discovery
- Connection management
- Heart rate data parsing
- Persistent event loop

#### HeartSyncMonitor
Main application class:
- UI setup and layout
- Real-time data processing
- Waveform rendering
- Control state management

#### Signal Processing Chain
```
Raw HR → +HR Offset → EMA Smoothing → Source Select → Wet/Dry Calc → +W/D Offset
```

## 📱 VST3 Plugin (Experimental)

The project includes experimental VST3 plugin code in `HeartSyncVST3/`:
- Real-time audio parameter modulation
- OSC sender for external communication
- JUCE framework integration

See `HeartSyncVST3/README.md` for build instructions.

## 🎨 Design Philosophy

> "Make it look like it's from a parallel universe where scientific technology is 10 years ahead, but keep the soul of medical precision and trustworthiness."

### Key Principles
- **Symmetry**: Perfect 8pt grid alignment
- **Luminosity**: High-contrast glowing text
- **Precision**: Tabular numerals, tight kerning
- **Clarity**: Medical-grade readability
- **Trust**: IEC 60601-1-8 compliant alarms

## 📚 Documentation

- [Transformation Guide](NEXTGEN_TRANSFORMATION_GUIDE.md) - Complete visual transformation roadmap
- [Project Structure](PROJECT_STRUCTURE.md) - Codebase organization
- [Setup Guide](docs/setup_guide.md) - Detailed installation instructions
- [Device Description](docs/device_description.md) - Hardware compatibility

## 🤝 Contributing

Contributions are welcome! Please read our contributing guidelines before submitting PRs.

### Development Setup

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 🛡️ Standards Compliance

- **WCAG 2.1 Level AA**: Accessibility standards
- **IEC 60601-1-8**: Medical alarm systems
- **IEC 62366-1**: Human factors engineering
- **IEC 60601-1**: Medical device safety

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Polar Electro for excellent BLE heart rate monitors
- The Python Bleak library for cross-platform BLE support
- JUCE framework for audio plugin development

## 📞 Support

For issues, questions, or suggestions:
- Open an issue on GitHub
- Check existing documentation
- Review the transformation guide

---

**Made with ❤️ and ⚡ quantum plasma technology**

*HeartSync Professional - Where Biology Meets Technology*
