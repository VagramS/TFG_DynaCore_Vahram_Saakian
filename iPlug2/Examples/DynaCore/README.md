# DynaCore

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![Platform](https://img.shields.io/badge/platform-macOS-lightgrey)
![Framework](https://img.shields.io/badge/framework-iPlug2-orange)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Formats](https://img.shields.io/badge/formats-VST3%20%7C%20AU-green)
![License](https://img.shields.io/badge/license-Academic%20(UCM%20TFG)-yellow)

**DynaCore** is a multi-effect audio plugin built with iPlug2. It runs compression, modulation and stereo processing in a fixed signal chain, with a custom UI, 13 factory presets, and per-sample smoothing on all parameters.

Developed as a TFG (Trabajo Fin de Grado) at Universidad Complutense de Madrid, 2025-2026.

---

## Signal chain

```
Input -> [Mono Fold] -> Compressor -> Tremolo -> Pan Motion
      -> Pitch Drift -> Phaser -> Master Intensity -> Output Gain
      -> Stereo Width -> Global Bypass -> Output
```

---

## Modules

### Tremolo
Amplitude LFO. Right channel has a ~30° phase offset for stereo movement.
- **Rate** 0–20 Hz | **Depth** 0–100%

### Pan Motion
Constant-power auto-pan LFO.
- **Rate** 0–10 Hz | **Depth** 0–100%

### Pitch Drift
Modulated delay line (chorus style). L uses sin LFO, R uses cos for a 90° spread.
- **Rate** 0–12 Hz | **Depth** 0–100%

### Phaser
6-stage allpass chain with feedback (0.45) and a log sweep from 200 to 4000 Hz.
- **Rate** 0–10 Hz | **Depth** 0–100%

### Compressor
Peak-following compressor with parallel mix.
- **Mix** | **Threshold** –50 to 0 dB | **Ratio** 1:1–20:1
- **Gain** | **Attack** 0–100 ms | **Release** 0–1000 ms

### Mastering
- **Stereo Width** 0–200% (M/S)
- **Master Intensity** 0–100% (modulation wet/dry)

### Output
- **Output Level** –20 to +20 dB
- **Global Bypass**

---

## Features

- 13 factory presets across 4 groups (Vocals, Pads, Drums, Experimental)
- Per-sample parameter smoothing (~5 ms) — no zipper noise on automation
- Rate knobs snap to musically clean Hz values
- Auto-bypass: modules turn off automatically when Rate or Depth hits zero
- ~10 ms bypass ramp so toggling a module never clicks
- Stereo output meter (green/yellow/red) + gain reduction meter for the compressor
- Preset carousel with prev/next arrows and full-screen overlay

---

## Build

1. Open `DynaCore.xcworkspace` in Xcode
2. Pick the **VST3** or **AU** scheme
3. Set configuration to **Release** and build (`Cmd+B`)

**Installer (both formats + .pkg):**

```bash
bash installer/makedist-mac.sh
```

---

## Project structure

```
DynaCore/
├── DynaCore.cpp          # all DSP, UI controls, preset system
├── DynaCore.h            # class declaration, parameter enum
├── config.h              # plugin metadata, resource filenames
├── DynaCore.xcworkspace  # open this in Xcode
├── projects/             # Xcode project + UI images
│   └── img/
├── resources/            # runtime assets
│   ├── fonts/            # Inter typeface
│   ├── img/              # background + UI bitmaps
│   └── *.plist
└── installer/            # .pkg builder scripts
```

---

## Presets

| Group | Presets |
|---|---|
| Vocals | Cold Whisper 14, Blade Mono Focus, Spectral Glide, Ritual Double |
| Pads | Cryostasis Pad, Nocturne Pulse, Moon Tides, Glass Cathedral |
| Drums | Iron March, Ghost Hats, Submerge Kit |
| Experimental | Event Horizon, Time Shear |

---

## Author

**Vahram Saakian** — Universidad Complutense de Madrid
vahramsa@ucm.es

GitHub: [github.com/VagramS/TFG_DynaCore_Vahram_Saakian](https://github.com/VagramS/TFG_DynaCore_Vahram_Saakian)
Figma: [Design file](https://www.figma.com/design/Em3jdV60MSLZxlxcE9jMQJ/TFG--DynaCore-Design--Vahram-Saakian?node-id=0-1)

---

Academic project — UCM TFG 2025-2026.
