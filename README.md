# DynaCore

Multi-effect audio plugin built with [iPlug2](https://github.com/iPlug2/iPlug2).

> TFG — Universidad Complutense de Madrid (UCM)
> Author: Vahram Saakian | vahramsa@ucm.es

---

## What it does

DynaCore chains five processing modules in a fixed order:

| Module | What it does |
|--------|-------------|
| **Compressor** | Peak compressor with parallel mix, adjustable threshold, ratio, attack/release, makeup gain and auto-gain mode (tracks GR in real time) |
| **Tremolo** | Amplitude LFO, ~30° phase offset between L and R for stereo shimmer. Switchable sine/square waveform |
| **Pan Motion** | Constant-power auto-pan LFO. Switchable sine/square waveform |
| **Pitch Drift** | Short modulated delay (chorus style), 90° L/R spread |
| **Phaser** | 6-stage allpass chain with feedback and log-sweep |

Plus:
- **Stereo Width** — M/S processing (0% = mono, 100% = normal, 200% = extra wide)
- **Master Intensity** — blends all modulation on top of the compressed signal
- **Output Level** — final gain
- **Global Bypass** — clean crossfade to dry

---

## Signal chain

```
Input → [Mono Fold] → Compressor → Tremolo → Pan Motion → Pitch Drift
      → Phaser → Master Intensity → Output Gain → Stereo Width
      → Global Bypass → Output
```

---

## Presets

13 factory presets split into four groups:

| Group | Presets |
|-------|---------|
| **Vocals** (4) | Cold Whisper 14, Blade Mono Focus, Spectral Glide, Ritual Double |
| **Pads** (4) | Cryostasis Pad, Nocturne Pulse, Moon Tides, Glass Cathedral |
| **Drums** (3) | Iron March, Ghost Hats, Submerge Kit |
| **Experimental** (2) | Event Horizon, Time Shear |

Presets are stored as plain C structs, no iPlug2 preset machinery used.

---

## Formats

| Format | Install path |
|--------|--------------|
| **VST3** | `/Library/Audio/Plug-Ins/VST3/DynaCore.vst3` |
| **AUv2** | `/Library/Audio/Plug-Ins/Components/DynaCore.component` |

---

## UI Design

Designed in Figma first, then implemented with iPlug2/IGraphics:

🎨 [TFG — DynaCore Design — Vahram Saakian](https://www.figma.com/design/Em3jdV60MSLZxlxcE9jMQJ/TFG--DynaCore-Design--Vahram-Saakian?node-id=0-1)

---

## Project structure

The repo root is the iPlug2 SDK folder. The actual plugin lives under `iPlug2/Examples/DynaCore/`:

```
TFG_DynaCore_Vahram_Saakian/          ← repo root
├── iPlug2/                           ← iPlug2 SDK
│   ├── Build/                        ← prebuilt SDK libs
│   ├── Dependencies/
│   ├── Examples/
│   │   └── DynaCore/                 ← plugin source (this is what matters)
│   │       ├── DynaCore.h            ← class declaration + param enum
│   │       ├── DynaCore.cpp          ← DSP + constructor + UI layout
│   │       ├── DynaCoreControls.h    ← all custom UI controls
│   │       ├── DynaCorePresets.h     ← preset system + default values
│   │       ├── config.h              ← plugin metadata
│   │       ├── DynaCore.xcworkspace  ← open this in Xcode
│   │       ├── projects/
│   │       │   ├── DynaCore-macOS.xcodeproj
│   │       │   ├── DynaCore.icns
│   │       │   ├── img/              ← UI assets (copied here by Xcode)
│   │       │   └── fonts/
│   │       ├── resources/
│   │       │   ├── img/              ← UI bitmaps (@1x/@2x/@3x)
│   │       │   ├── fonts/            ← Inter typeface
│   │       │   └── *.plist
│   │       └── installer/
│   │           ├── makedist-mac.sh   ← builds .pkg installer
│   │           └── DynaCore-win.iss  ← Windows (Inno Setup)
│   ├── IGraphics/
│   ├── IPlug/
│   └── Scripts/
├── LICENSE
└── README.md
```

---

## Build

Requirements: macOS 12+, Xcode 14+, iPlug2 is already in the repo so no extra install needed.

```bash
git clone https://github.com/VagramS/TFG_DynaCore_Vahram_Saakian.git
cd TFG_DynaCore_Vahram_Saakian
open iPlug2/Examples/DynaCore/DynaCore.xcworkspace
```

Pick the **VST3** or **AUv2** scheme, set configuration to **Release**, hit build.

**Or build everything + package installer in one go:**

```bash
cd iPlug2/Examples/DynaCore
bash installer/makedist-mac.sh
# → installer/DynaCore-1.0.0-macOS.pkg
```

---

## License

Copyright 2025-2026 Vahram Saakian — Universidad Complutense de Madrid.
Part of a university thesis (TFG). All rights reserved.
