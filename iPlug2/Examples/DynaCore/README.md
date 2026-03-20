# DynaCore

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows-lightgrey)
![Framework](https://img.shields.io/badge/framework-iPlug2-orange)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Formats](https://img.shields.io/badge/formats-VST3%20%7C%20AU-green)

**DynaCore** is a multi-effect audio plugin built with iPlug2. It chains compression, modulation and stereo processing in a fixed signal chain, with a custom dark-theme UI and 13 factory presets.

← [Back to project root](../../../../README.md)

---

## Signal chain

```
Input → [Mono Fold] → Compressor → Tremolo → Pan Motion
      → Pitch Drift → Phaser → Master Intensity → Output Gain
      → Stereo Width → Global Bypass → Output
```

---

## Modules

### Compressor
Peak-following compressor with parallel mix.
- **Threshold** –50 to 0 dB · **Ratio** 1:1–20:1
- **Attack** 0–100 ms · **Release** 0–1000 ms
- **Makeup Gain** · **Mix** (parallel/NY compression)

### Tremolo
Amplitude LFO. Right channel has a ~30° phase offset for stereo shimmer.
- **Rate** 0–20 Hz · **Depth** 0–100%

### Pan Motion
Constant-power auto-pan LFO. Disabled automatically on mono input.
- **Rate** 0–10 Hz · **Depth** 0–100%

### Pitch Drift
Modulated delay line (chorus style). L uses sin LFO, R uses cos for a 90° spread.
- **Rate** 0–12 Hz · **Depth** 0–100%

### Phaser
6-stage allpass chain with feedback (0.45), log sweep 200–4000 Hz.
- **Rate** 0–10 Hz · **Depth** 0–100%

### Master Intensity
Blends between the compressed signal and the fully modulated signal.

### Stereo Width
M/S processing — 0% = mono, 100% = normal, 200% = extra wide.

---

## Features

- Per-sample parameter smoothing (~5 ms) — no zipper noise on automation
- Rate knobs snap to musically clean Hz values
- Auto-bypass: modules mute when Rate and Depth both hit zero
- ~10 ms bypass ramp — no clicks when toggling modules
- Stereo VU meter (green/yellow/red) + gain-reduction meter for the compressor
- Preset carousel with prev/next arrows and category overlay

---

## Presets

| Group | Presets |
|---|---|
| **Vocals** (4) | Cold Whisper 14, Blade Mono Focus, Spectral Glide, Ritual Double |
| **Pads** (4) | Cryostasis Pad, Nocturne Pulse, Moon Tides, Glass Cathedral |
| **Drums** (3) | Iron March, Ghost Hats, Submerge Kit |
| **Experimental** (2) | Event Horizon, Time Shear |

---

## Install — macOS

Download `DynaCore-1.0.0-macOS.pkg` from the [Releases](https://github.com/VagramS/TFG_DynaCore_Vahram_Saakian/releases) page and run it. You can choose to install VST3, AU, or both.

| Format | Install path |
|--------|-------------|
| **VST3** | `/Library/Audio/Plug-Ins/VST3/DynaCore.vst3` |
| **AUv2** | `/Library/Audio/Plug-Ins/Components/DynaCore.component` |

After installing, rescan plugins in your DAW.

---

## Install — Windows

Download `DynaCore-1.0.0-Windows-Setup.exe` from the [Releases](https://github.com/VagramS/TFG_DynaCore_Vahram_Saakian/releases) page and run it. The installer handles everything automatically.

| Format | Install path |
|--------|-------------|
| **VST3** | `C:\Program Files\Common Files\VST3\DynaCore.vst3` |

After installing, rescan plugins in your DAW.

> AU is macOS-only and is not available on Windows.

---

## Build from source — macOS

Requirements: macOS 11+, Xcode 15+

```bash
# From the repo root:
open iPlug2/Examples/DynaCore/projects/DynaCore-macOS.xcodeproj
```

Pick the **macOS-VST3** or **macOS-AUv2** scheme → **Release** → `Cmd+B`.

**Or build both formats and package the installer in one command:**

```bash
bash installer/makedist-mac.sh
# → installer/DynaCore-1.0.0-macOS.pkg
```

---

## Build from source — Windows

Requirements: Windows 10 / 11 (64-bit), [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++"

1. Clone the repo and open `DynaCore.sln` in Visual Studio 2022
2. Set configuration to **Release**, platform to **x64**
3. Build the **VST3** target (`Ctrl+Shift+B`)
4. The installer can be built by running `installer/DynaCore-win.iss` with [Inno Setup 6](https://jrsoftware.org/isinfo.php)

> The Windows installer is also built automatically by GitHub Actions on every tagged release — see the [Releases](https://github.com/VagramS/TFG_DynaCore_Vahram_Saakian/releases) page.

---

## Project structure

```
DynaCore/
├── DynaCore.cpp          ← all DSP, UI controls, preset system
├── DynaCore.h            ← class declaration, parameter enum
├── config.h              ← plugin metadata, resource filenames
├── projects/
│   ├── DynaCore-macOS.xcodeproj
│   ├── img/              ← UI background images
│   └── fonts/
├── resources/
│   ├── img/              ← bitmaps (@1x / @2x / @3x)
│   ├── fonts/            ← Inter typeface
│   └── *.plist
└── installer/
    ├── makedist-mac.sh   ← macOS .pkg builder
    └── DynaCore-win.iss  ← Windows installer script (Inno Setup)
```

---

**Vahram Saakian** — Universidad Complutense de Madrid
vahramsa@ucm.es · [GitHub](https://github.com/VagramS/TFG_DynaCore_Vahram_Saakian) · [Figma design](https://www.figma.com/design/Em3jdV60MSLZxlxcE9jMQJ/TFG--DynaCore-Design--Vahram-Saakian?node-id=0-1)

Academic project — UCM TFG 2025-2026.
