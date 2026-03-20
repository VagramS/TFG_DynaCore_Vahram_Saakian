# TFG — DynaCore

> **Trabajo Fin de Grado** — Universidad Complutense de Madrid (UCM)
> Grado en Ingeniería Informática · 2025-2026
> Author: **Vahram Saakian** · vahramsa@ucm.es
> Director: Miguel Gómez-Zamalloa Gil, Jaime Sánchez Hernández

---

## About

**DynaCore** is a multi-effect audio plugin developed as a TFG at UCM.
It runs compression, modulation and stereo processing in a fixed signal chain, with a custom UI, 13 factory presets, and per-sample smoothing on all parameters.

Built with [iPlug2](https://github.com/iPlug2/iPlug2) in C++. Distributed as **VST3** and **AU** for macOS, and **VST3** for Windows.

→ **[Plugin documentation, build & install guide](iPlug2/Examples/DynaCore/README.md)**

---

## Downloads

Pre-built installers are available on the [Releases](https://github.com/VagramS/TFG_DynaCore_Vahram_Saakian/releases) page:

| Platform | File | Formats |
|----------|------|---------|
| **macOS** | `DynaCore-1.0.0-macOS.pkg` | VST3 + AU |
| **Windows** | `DynaCore-1.0.0-Windows-Setup.exe` | VST3 |

The Windows installer is built automatically by GitHub Actions on every tagged release.

---

## UI Design

Designed in Figma before any code was written:

🎨 [TFG — DynaCore Design — Vahram Saakian](https://www.figma.com/design/Em3jdV60MSLZxlxcE9jMQJ/TFG--DynaCore-Design--Vahram-Saakian?node-id=0-1)

---

## Repository structure

```
TFG_DynaCore_Vahram_Saakian/          ← repo root (iPlug2 SDK)
├── .github/
│   └── workflows/
│       └── build-windows.yml         ← CI: builds Windows VST3 + installer
├── iPlug2/
│   ├── Build/                        ← prebuilt SDK libs (macOS)
│   ├── Dependencies/                 ← VST3 SDK, IGraphics libs, etc.
│   ├── Examples/
│   │   └── DynaCore/                 ← plugin source ← start here
│   │       ├── DynaCore.cpp          ← all DSP, UI, presets (~2700 lines)
│   │       ├── DynaCore.h            ← class declaration + param enum
│   │       ├── config.h              ← plugin metadata
│   │       ├── projects/
│   │       │   ├── DynaCore-macOS.xcodeproj
│   │       │   ├── img/              ← UI background images
│   │       │   └── fonts/
│   │       ├── resources/
│   │       │   ├── img/              ← UI bitmaps (@1x / @2x / @3x)
│   │       │   ├── fonts/            ← Inter typeface
│   │       │   └── *.plist
│   │       └── installer/
│   │           ├── makedist-mac.sh   ← builds macOS .pkg
│   │           └── DynaCore-win.iss  ← Windows installer (Inno Setup)
│   ├── IGraphics/                    ← rendering engine
│   ├── IPlug/                        ← plugin API abstraction
│   └── Scripts/                      ← iPlug2 build utilities
├── .gitignore
├── LICENSE
└── README.md                         ← this file
```

---

## License

Copyright 2025-2026 Vahram Saakian — Universidad Complutense de Madrid.
Part of a university thesis (TFG). All rights reserved.
