# DynaCore

Multi-effect audio plugin built with [iPlug2](https://github.com/iPlug2/iPlug2).

> TFG — Universidad Complutense de Madrid (UCM)
> Author: Vahram Saakian | vahramsa@ucm.es

> **macOS only.** Currently, DynaCore is only available to macOS users. However, the project code
> and interface are based on iPlug2's cross-platform layers, so no modifications will be necessary.
> Adapting it for Windows would require a standalone installer built with Visual Studio tools
> (.exe or .msi) and testing in Windows hosts such as FL Studio or Reaper. Since DynaCore has been
> developed and tested entirely on macOS, and compiling it for Windows without testing would be
> irresponsible, this task has been left for future work.

---

## What it does

DynaCore chains five processing modules in a fixed order:

| Module | What it does |
|--------|-------------|
| **Compressor** | The compressor reduces the dynamic range of the audio track, attenuating the loudest parts of the recording and boosting the quietest ones with Gain. This helps reduce the variability of the vocals in the mix, so they do not get lost among the other instruments in the song. This compressor was designed using the "feed-forward peak" topology, with modifications that include the parallel mixing of the compressed signal and automatic volume compensation. |
| **Tremolo** | Modulation works only in the downward direction. At the peak of the oscillation, the signal passes through unchanged, since the multiplier equals one, while at the trough, the sound is attenuated by an amount that depends on the module's Depth parameter. Thus, the Tremolo cannot make the resulting signal louder than the original. |
| **Pan Motion** | In Pan Motion, the Depth parameter controls the amplitude of the signal's movement along the pan, not its volume: at a value of 50%, the sound oscillates within a range extending from the center to half the maximum pan width. The signal's position is converted into an angle between 0 and π/2, and after the conversion, the cosine values are used for the left channel and the sine values for the right channel. In this way, no volume is lost when the sound passes through the center of the panning range, as might occur if a linear law were applied. |
| **Pitch Drift** | Pitch Drift records the signal onto a short delay line and plays it back. In this process, the playback point does not remain fixed; instead, the LFO constantly shifts it. In DynaCore, Pitch Drift has a delay of about 20 ms, and the LFO modulates it over a range of about 2 ms. This results in a thicker sound, but one that does not stray far enough from the original for the ear to perceive it as a flat note. |
| **Phaser** | Six all-pass filters allow the input signal to pass through. None of them affect the volume of the frequencies in the spectrum; instead, they merely delay them by different time intervals. As a result, gaps appear in the output sound – not within the filters themselves – when the processed signal is added to the original: certain frequencies no longer align and therefore cancel each other out. The filter frequencies are controlled by the LFO over a range of 200 to 4000 Hz on a logarithmic, rather than linear, scale. |

Additional features:

- **Stereo Width** alters the perceived size of the stereo image by operating not on the left and right channels, but on the Mid/Side representation of the signal: the Mid component carries what both channels have in common, while the Side component carries what distinguishes them. Adjusting the amplitude of only the "Side" channel of a stereo recording can make the soundstage appear wider or narrower, without altering the content of the "Mid" channel.
- **Master Intensity** is a parameter which controls how much to blend the processed sound with the initial sound. Here 0% = "do not blend the modules' processing", and 100% means "interchange the original audio with the processed one".
- **Tempo synchronisation (Hz / BPM).** The LFO speed can be specified in two ways: in Hz, which is an absolute speed independent of the project's tempo; for example, 3 Hz means 3 cycles per second, regardless of what is happening in the song. Synchronized LFO, on the other hand, is specified in musical divisions such as 1/2, 1/4, 1/8 and 1/8t – and the plugin then converts these values to Hz based on the project tempo provided by the host. DynaCore implements both functions, which can be toggled by pressing the button located above the "Rate" parameter.
- **Tooltips** are enabled for all editable interface elements and explain briefly what each control does. For example: "Global Bypass: sends the unprocessed signal directly to the output, bypassing all processing." They are triggered by holding the cursor over the element for 2 seconds.
- **Parameter smoothing.** DynaCore uses logarithmic smoothers (LogParamSmooth) with a smoothing time of 5 ms for each parameter. This resolves the issue of clicks/cracks when enabling or disabling modules or adjusting plugin settings while a track is playing. Each module is also equipped with a bypass button that disables a specific module individually, and they also feature transition smoothing algorithms to prevent any crackling or clicking sounds when turning them on/off.

---

## Signal chain

```
Input → [Mono Fold] → Compressor → Tremolo → Pan Motion → Pitch Drift
      → Phaser → Master Intensity → Output Gain → Stereo Width
      → Global Bypass → Output
```

The order of audio processing is strictly defined to provide a predictable result without requiring
the user to know how the modules interact. The compressor comes first to control the audio dynamics
before applying any modulation effects (Tremolo, Pan Motion, Pitch Drift, and Phaser). Following the
modules is the Master Intensity, which uses the equal-power (sin/cos) law to adjust the intensity of
all modulations. Finally, there is Stereo Width, which adjusts the signal's stereo image. Global
Bypass functions as a smooth crossfade between the original and processed signals.

A different order in the processing chain would not produce a worse result, but simply a different
one: a compressor placed in the chain after the modulation effects would not react to the dynamics
of the voice, but rather to the oscillations of the effect itself, and as a result, would suppress
whatever effect had been used. For this reason, the plugin's processing chain follows established
conventions – first dynamics, then modulation, and finally stereo width – especially when it comes
to vocal processing.

---

## Presets

DynaCore includes 14 factory presets (one of them is the default one) organized into 5 groups:

| Group | Presets |
|-------|---------|
| **Vocals** (4) | Cold Whisper 14, Blade Mono Focus, Spectral Glide, Ritual Double |
| **Keys/Pads** (4) | Cryostasis Pad, Nocturne Pulse, Moon Tides, Glass Cathedral |
| **Drums / Percussion** (3) | Iron March, Ghost Hats, Submerge Kit |
| **Experimental / FX** (2) | Event Horizon, Time Shear |
| **Default/None** (1) | Revert to default |

The groups have been organized based on the type of material rather than the type of effect. A user
looking for processing for a vocal track does not need to know which effects have been applied to
the preset; instead, they need to know what result they will get. For this reason, the presets have
been named in a way that makes it immediately clear what kind of result to expect.

The presets page is not the only way to access presets. Next to the preset selection button there
are two arrow-shaped buttons, "Previous" and "Next", which pass through the whole preset bank one
preset at a time: at the end of a group, they continue into the next group, and after the last
preset they return to the first, that way the bank forms a loop. The "Revert to default" button
resets all settings to their initial configuration.

Applying presets does not write the parameter values directly to the DSP. Instead, each preset
passes through the host exactly as if the parameters were being adjusted manually. This way, the
preset loading process is reflected in the automation tracks and can be undone and saved along with
the project in the DAW.

---

## Scope

DynaCore does not cover all the professional processing steps required for a vocal signal, but only
a portion of them. The plugin covers compression, sound movement and the stereo field, but it does
not offer the ability to work with equalization, reverb, delay, or de-essing. DynaCore is designed
to be used alongside other plugins in processing chains, not to replace them. The plugin fulfills
the functions for which it was designed: balancing vocal dynamics and creating sonic width – all at
a single point – rather than using four or five different tools. Presets are a starting point and
require adjusting the parameters to suit each user's material, rather than being ready-made
solutions.

---

## Formats

| Format | Install path |
|--------|--------------|
| **VST3** | `/Library/Audio/Plug-Ins/VST3/DynaCore.vst3` |
| **AUv2** | `/Library/Audio/Plug-Ins/Components/DynaCore.component` |

VST3 is the most popular plugin format, as it works on macOS, Windows, and Linux, and is compatible
with FL Studio, Cubase, Reaper, Ableton Live, Studio One, and most other DAWs. AU (Audio Units) is
Apple's proprietary plug-in format; since Logic Pro, Apple's DAW, only works with plug-ins in AU
format, this format could not be overlooked without losing a significant part of the potential
audience. Both feature the same sound-processing code and the same interface. In this way, these two
formats cover all the needs of the various DAWs on macOS, except for Pro Tools: it uses its own AAX
format, which is not available to those who do not participate in Avid's developer program.

DynaCore has been compiled as an AU and VST3 plugin, passes validation using the `auval` utility,
loads into the DAW, and functions and displays correctly in Logic Pro 12.0.1, Studio One 7.2, and
Fender Studio Pro 8.

### Known issue

An unsigned .pkg package triggers a warning the first time the installer is opened because it cannot
be signed without an Apple Developer ID (a paid annual subscription) and notarized through Apple's
notary service. This warning can be bypassed via the Privacy & Security settings. This issue is
considered a limitation rather than a bug.

---

## Demos

Screen recordings of the plugin in use, in [`videos_memoria/`](videos_memoria):

| File | What it shows |
|------|---------------|
| `figure-3-2-ritual-double.mp4` | Rhythmic movement on a lead vocal with the RITUAL DOUBLE preset |
| `figure-3-2-spectral-glide.mp4` | Widening a vocal overdub with the SPECTRAL GLIDE preset |
| `figure-4-7-pan-motion.mp4` | Pan Motion moving the signal across the stereo field |
| `figure-5-4-dynacore-review.mp4` | Full walkthrough of the plugin inside a DAW |

---

## UI Design

The plugin's design has been created in Figma before any development or coding of the plugin began.
This approach has made it possible to iterate quickly on the visual design, so that once coding
started, the exact coordinates of every button, knob and element were already known, without wasting
time on their placement.

🎨 [TFG — DynaCore Design — Vahram Saakian](https://www.figma.com/design/Em3jdV60MSLZxlxcE9jMQJ/TFG--DynaCore-Design--Vahram-Saakian?node-id=0-1)

---

## Project structure

The repository root is the iPlug2 SDK folder. The actual plugin lives under `iPlug2/Examples/DynaCore/`:

```
TFG_DynaCore_Vahram_Saakian/          ← repository root
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
│   │           ├── makedist-mac.sh           ← builds .pkg installer
│   │           └── DynaCore-1.0.0-macOS.pkg  ← shipped installer
│   ├── IGraphics/
│   ├── IPlug/
│   └── Scripts/
├── videos_memoria/                   ← demo recordings referenced from the thesis
├── LICENSE
└── README.md
```

---

## Build

Requirements: macOS 12+, Xcode 14+. iPlug2 is already in the repository, so no extra installation is needed.

```bash
git clone https://github.com/VagramS/TFG_DynaCore_Vahram_Saakian.git
cd TFG_DynaCore_Vahram_Saakian
open iPlug2/Examples/DynaCore/DynaCore.xcworkspace
```

Select the **VST3** or **AUv2** scheme, set Configuration to **Release**, then press **Build**.

Alternatively, build everything with the packaging script:

```bash
cd iPlug2/Examples/DynaCore
bash installer/makedist-mac.sh
# → installer/DynaCore-1.0.0-macOS.pkg
```

The script is located at `iPlug2/Examples/DynaCore/installer/makedist-mac.sh` and builds AU and VST3
targets (in Release configuration). It then packages each extension using `pkgbuild` and compiles
them into a single installer named `DynaCore-1.0.0-macOS.pkg` in the
`iPlug2/Examples/DynaCore/installer/` folder.

When the targets are built with `xcodebuild` in the "All macOS" scheme, `prepare_resources-mac.py`
may trigger an "Invalid file" error on a shared plist, because the AU and VST3 targets modify it at
the same time. Building each target separately, or running `xcodebuild` with `-jobs 1`, avoids the
conflict.

### Validation

```bash
auval -v aufx DnCr UCM1
```

Verifies plugin initialization, response to different sample rates, audio processing, parameter
management and the ability to save and restore state.

### Bypassing macOS Gatekeeper

The .pkg package is not signed with an Apple Developer ID, so the first time it is opened macOS
Gatekeeper blocks it. This is a limitation of the distribution and not a defect of the plugin. The
block has to be lifted only once:

1. Open the .pkg installer. Gatekeeper reports that the package cannot be verified; click "Done".
2. Open System Settings → Privacy & Security and scroll down to the "Security" section.
3. Click "Open Anyway" next to the DynaCore-1.0.0-macOS.pkg warning and confirm the choice in the
   window that appears.

---

## Relevant links

Material consulted while building DynaCore. These are working references rather than academic
citations, so they live here instead of in the thesis bibliography.

**iPlug2**
- [Documentation portal](https://iplug2.github.io) — [IControl](https://iplug2.github.io/docs/class_i_control.html), [IGraphics](https://iplug2.github.io/docs/class_i_graphics.html), [IPlugProcessor](https://iplug2.github.io/docs/class_i_plug_processor.html) (`GetTempo`, `GetPPQPos`, `GetTimeSig`)
- Source: [IControl.h](https://github.com/iPlug2/iPlug2/blob/master/IGraphics/IControl.h) (base control, mouse events, tooltips), [IControls.h](https://github.com/iPlug2/iPlug2/blob/master/IGraphics/Controls/IControls.h), [IGraphics.h](https://github.com/iPlug2/iPlug2/blob/master/IGraphics/IGraphics.h) (`DrawArc`, `FillArc`, `FillRect`), [IPopupMenuControl.h](https://github.com/iPlug2/iPlug2/blob/master/IGraphics/Controls/IPopupMenuControl.h) (overlay pattern), [IVMeterControl.h](https://github.com/iPlug2/iPlug2/blob/master/IGraphics/Controls/IVMeterControl.h)
- Examples: [IPlugControls](https://github.com/iPlug2/iPlug2/blob/master/Examples/IPlugControls/IPlugControls.cpp) (widgets demo), [IPlugEffect](https://github.com/iPlug2/iPlug2/blob/master/Examples/IPlugEffect/IPlugEffect.cpp) (starter template)
- Wiki: [Getting started (macOS/iOS)](https://github.com/iPlug2/iPlug2/wiki/01_Getting_started_mac_ios), [Custom Control tutorial](https://github.com/iPlug2/iPlug2/wiki/05_Custom_Control), [Config settings for a plugin](https://github.com/iPlug2/iPlug2/wiki/Config-Settings-for-a-Plugin)
- [Community forum](https://iplug2.discourse.group/) — [custom menu / overlay](https://iplug2.discourse.group/t/custom-menu-for-iplug2/554), [deriving a control from IControl](https://iplug2.discourse.group/t/derive-custom-ui-control-from-icontrol-class/654), [detecting MouseOver](https://iplug2.discourse.group/t/how-to-detect-mouseover-in-general/177), [presets in VST3/App builds](https://iplug2.discourse.group/t/how-to-implement-presets-in-vst3-app-builds/807), [tempo for an LFO](https://iplug2.discourse.group/t/how-to-set-a-tempo-for-a-lfo/460), [preset A/B buttons](https://iplug2.discourse.group/t/preset-manager-a-b-buttons/409), [tooltip on the wrong control](https://iplug2.discourse.group/t/tooltip-being-applied-to-wrong-control/715), [bitmaps in GUI controls](https://iplug2.discourse.group/t/using-bitmap-in-controls-gui/131), [JSON preset storage](https://iplug2.discourse.group/t/using-json-to-store-presets/379)
- Talks by Oliver Larkin: [An Introduction to iPlug2](https://www.youtube.com/watch?v=YT_0TEftO54), [iPlug2, a C++ framework to build plug-ins](https://www.youtube.com/watch?v=eVi-OWFPwO4)

**Other plugin-development forums**
- JUCE Forum: [sync with host tempo](https://forum.juce.com/t/sync-with-hosts-tempo/7415), [tempo sync note divisions](https://forum.juce.com/t/tempo-sync/11618)
- KVR Audio: [auto gain compensation](https://www.kvraudio.com/forum/viewtopic.php?t=448374), [auto make-up gain](https://www.kvraudio.com/forum/viewtopic.php?t=342883), [gain reduction meter](https://www.kvraudio.com/forum/viewtopic.php?t=466611), [iPlug compressor + GR meter](https://www.kvraudio.com/forum/viewtopic.php?t=478625&start=15), [implementing a VU meter](https://www.kvraudio.com/forum/viewtopic.php?t=289885), [host SYNC for LFO/ADSR](https://www.kvraudio.com/forum/viewtopic.php?t=270213), [VstTimeInfo and host BPM](https://www.kvraudio.com/forum/viewtopic.php?t=38645), [tempo-sync in Hz](https://www.kvraudio.com/forum/viewtopic.php?t=120808), [bitmap over background](https://www.kvraudio.com/forum/viewtopic.php?t=66425), [getting into VST UI design](https://www.kvraudio.com/forum/viewtopic.php?t=223173), [UI design tutorials](https://www.kvraudio.com/forum/viewtopic.php?t=541318), [iPlug2 vs JUCE](https://www.kvraudio.com/forum/viewtopic.php?t=565161)

**DSP theory and reference implementations**
- DSPRelated: [delay-line interpolation](https://www.dsprelated.com/freebooks/pasp/Delay_Line_Interpolation.html), [the panning problem](https://www.dsprelated.com/freebooks/sasp/Panning_Problem.html), [virtual analog phasing](https://www.dsprelated.com/freebooks/pasp/Virtual_Analog_Example_Phasing.html), [tremolo in C](https://www.dsprelated.com/showcode/234.php), [VU meter implementation](https://www.dsprelated.com/showthread/comp.dsp/40956-1.php)
- EarLevel Engineering — [a one-pole filter](https://www.earlevel.com/main/2012/12/15/a-one-pole-filter/)
- Smith, S. W. — [The Scientist and Engineer's Guide to Digital Signal Processing](https://www.dspguide.com)
- Held, P. — [CTAGDRC](https://github.com/p-hlp/CTAGDRC), an open-source JUCE compressor

**Audio-effect background**
- Fender — [Pedal Board Primer: Get to Know Tremolo](https://www.fender.com/articles/parts-and-accessories/pedal-board-primer-get-to-know-tremolo)
- Wikipedia — [Phaser (effect)](https://en.wikipedia.org/wiki/Phaser_(effect)), [Panning (audio)](https://en.wikipedia.org/wiki/Panning_(audio)), [Dotted note](https://en.wikipedia.org/wiki/Dotted_note), [Tuplet](https://en.wikipedia.org/wiki/Tuplet)
- Compression explainers — [Aulart](https://www.aulart.com/blog/what-is-compression-and-how-to-use-it/), [LANDR](https://www.landr.com/what-is-compression), [WAV Monopoly](https://wavmonopoly.com/how-to-use-a-compressor/)

**Interface design**
- Finke, M. — [Making Audio Plugins](https://www.martin-finke.de/tags/making_audio_plugins.html), in particular [Part 7: GUI](https://www.martin-finke.de/articles/audio-plugins-007-gui/)
- [How to design an audio plugin GUI in 6 steps](https://www.youtube.com/watch?v=RUeQR_vPgCI) — Voger Design
- [How to Design an Audio Plugin](https://www.youtube.com/watch?v=XRkCktaW2MA) — Figma walkthrough
- [Vital](https://github.com/mtytel/vital) — its categorised preset browser was the reference for DynaCore's own
- [NanoVG](https://github.com/memononen/nanovg) — the vector renderer behind IGraphics

**Tools**
- [Tempo-sync LFO calculator](https://bchillmix.com/pages/tempo-sync-lfo) and [BPM to Hz calculator](https://www.futurephonic.co.uk/pages/bpm-to-hz-lfo-calculator) — used to check the sync divisions
- [Git](https://git-scm.com) · [Logic Pro](https://www.apple.com/logic-pro/)

---

## License

DynaCore is released under the MIT License — see [LICENSE](LICENSE).

Copyright 2025-2026 Vahram Saakian — Universidad Complutense de Madrid.
Developed as a Trabajo de Fin de Grado. iPlug2 is included under its own license.
