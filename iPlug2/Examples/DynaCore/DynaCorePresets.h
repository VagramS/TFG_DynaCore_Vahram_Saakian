// DynaCorePresets.h
// Preset system: groups, names, values, carousel navigation, defaults.
// Also has rate-knob helpers used by presets and controls.
// This file is included by DynaCore.cpp, not compiled on its own.
//
// Author: Vahram Saakian, UCM TFG 2025-2026

#pragma once

#include "DynaCore.h"
#include <cmath>
#include <algorithm>
#include <string>

// ============================================================
//  Rate knob helpers (used by presets and controls)
// ============================================================

// table of "nice" Hz values the rate knob snaps to
static constexpr double kRateStepsHz[] =
{
  0.00,  // = stopped
  0.10, 0.12, 0.16, 0.18, 0.20, 0.22, 0.25, 0.28, 0.30, 0.35, 0.40, 0.45, 0.50,
  0.60, 0.65, 0.80, 0.90, 1.00, 1.60, 2.00, 3.00, 5.00, 6.00
};

static double SnapRateHz(double hz)
{
  if (hz <= 0.0) return 0.0;

  double best = kRateStepsHz[0];
  double bestDist = std::fabs(hz - best);

  for (double s : kRateStepsHz)
  {
    const double d = std::fabs(hz - s);
    if (d < bestDist)
    {
      bestDist = d;
      best = s;
    }
  }
  return best;
}

// returns true if this param is one of the four rate knobs
static inline bool IsRateParamIdx(int idx)
{
  return idx == kTremRate || idx == kPanRate || idx == kPitchRate || idx == kPhaserRate;
}

// minimum Hz before the rate snaps to 0 (below this it's inaudible)
static inline double RateDeadZoneHz(int idx)
{
  if (!IsRateParamIdx(idx)) return 0.0;
  return (idx == kPitchRate) ? 0.01 : 0.02;
}

// if rate is too low, snap to 0 so the LFO just stops
static inline double ApplyRateDeadZone(int idx, double hz)
{
  if (!IsRateParamIdx(idx))
    return hz;

  const double dz = RateDeadZoneHz(idx);
  if (hz > 0.0 && hz < dz)
    return 0.0;

  return hz;
}

// ============================================================
//  BPM-synced rate divisions
//  Knob position picks one of these note values; Hz comes from
//  host tempo: freq = (bpm / 60) * freqMultiplier.
// ============================================================

struct SyncDivision
{
  const char* label;
  double      freqMultiplier;  // freq relative to bpm/60 (quarter-note rate)
};

// sorted slowest -> fastest (so the knob sweeps from slow to fast)
static constexpr SyncDivision kSyncDivisions[] =
{
  { "1/1",   0.25 },          // whole note
  { "1/2d",  1.0 / 3.0 },     // dotted half
  { "1/2",   0.5 },           // half
  { "1/4d",  2.0 / 3.0 },     // dotted quarter
  { "1/2t",  0.75 },          // half triplet
  { "1/4",   1.0 },           // quarter
  { "1/8d",  4.0 / 3.0 },     // dotted eighth
  { "1/4t",  1.5 },           // quarter triplet
  { "1/8",   2.0 },           // eighth
  { "1/16d", 8.0 / 3.0 },     // dotted sixteenth
  { "1/8t",  3.0 },           // eighth triplet
  { "1/16",  4.0 },           // sixteenth
  { "1/16t", 6.0 },           // sixteenth triplet
  { "1/32",  8.0 }            // thirty-second
};

static constexpr int kSyncDivCount =
  sizeof(kSyncDivisions) / sizeof(kSyncDivisions[0]);

// returns the matching sync param idx for a rate param, or -1 if none
static inline int RateSyncParamIdxForRate(int rateIdx)
{
  switch (rateIdx)
  {
    case kTremRate:   return kTremRateSync;
    case kPanRate:    return kPanRateSync;
    case kPitchRate:  return kPitchRateSync;
    case kPhaserRate: return kPhaserRateSync;
    default:          return -1;
  }
}

// knob position 0..1 -> nearest division index
static inline int NormToSyncDivIdx(double norm)
{
  if (norm <= 0.0) return 0;
  if (norm >= 1.0) return kSyncDivCount - 1;
  return static_cast<int>(std::round(norm * (kSyncDivCount - 1)));
}

// division index -> normalized knob position
static inline double SyncDivIdxToNorm(int idx)
{
  if (kSyncDivCount <= 1) return 0.0;
  if (idx < 0) idx = 0;
  if (idx > kSyncDivCount - 1) idx = kSyncDivCount - 1;
  return static_cast<double>(idx) / static_cast<double>(kSyncDivCount - 1);
}

// Hz value for a division at the given tempo
static inline double SyncDivFreqHz(double bpm, int idx)
{
  if (idx < 0) idx = 0;
  if (idx > kSyncDivCount - 1) idx = kSyncDivCount - 1;
  return (bpm / 60.0) * kSyncDivisions[idx].freqMultiplier;
}

// label for a division (e.g. "1/8d") — never returns null
static inline const char* SyncDivLabel(int idx)
{
  if (idx < 0) idx = 0;
  if (idx > kSyncDivCount - 1) idx = kSyncDivCount - 1;
  return kSyncDivisions[idx].label;
}

// ============================================================
//  Preset groups, names, and navigation
// ============================================================

// categories for presets
enum class EPresetGroup
{
  None,
  Vocals,
  Pads,
  Drums,
  Experimental
};

// preset names for each group
static const char* kPresetName_None = "Default/None";

static const char* kPresets_Vocals[4] = {
  "COLD WHISPER 14",   // idx 0
  "BLADE MONO FOCUS",  // idx 1
  "SPECTRAL GLIDE",    // idx 2
  "RITUAL DOUBLE"      // idx 3
};

static const char* kPresets_Pads[4] = {
  "CRYOSTASIS PAD",    // idx 0
  "NOCTURNE PULSE",    // idx 1
  "MOON TIDES",        // idx 2
  "GLASS CATHEDRAL"    // idx 3
};

static const char* kPresets_Drums[3] = {
  "IRON MARCH",        // idx 0
  "GHOST HATS",        // idx 1
  "SUBMERGE KIT"       // idx 2
};

static const char* kPresets_Exp[2] = {
  "EVENT HORIZON",     // idx 0
  "TIME SHEAR"         // idx 1
};

// get preset name by group and index (returns nullptr if not found)
static const char* GetPresetName(EPresetGroup g, int idx)
{
  static const char* const* tables[] = { nullptr, kPresets_Vocals, kPresets_Pads, kPresets_Drums, kPresets_Exp };
  static constexpr int counts[] = { 0, 4, 4, 3, 2 };
  const int gi = static_cast<int>(g);
  if (gi < 1 || gi > 4) return nullptr;
  return (idx >= 0 && idx < counts[gi]) ? tables[gi][idx] : nullptr;
}

// how many presets are in a group
static int GetPresetCountGlobal(EPresetGroup g)
{
  static constexpr int counts[] = { 0, 4, 4, 3, 2 };
  const int gi = static_cast<int>(g);
  return (gi >= 1 && gi <= 4) ? counts[gi] : 0;
}

// the order groups cycle in when you press prev/next
static EPresetGroup kPresetCarouselOrder[] =
{
  EPresetGroup::Vocals,
  EPresetGroup::Pads,
  EPresetGroup::Drums,
  EPresetGroup::Experimental
};
static constexpr int kNumPresetGroups = sizeof(kPresetCarouselOrder) / sizeof(kPresetCarouselOrder[0]);

// find where a group sits in the carousel order
static int GetGroupOrderIndex(EPresetGroup g)
{
  for (int i = 0; i < kNumPresetGroups; i++)
    if (kPresetCarouselOrder[i] == g) return i;

  return 0;
}

// next group (wraps around at the end)
static EPresetGroup NextGroup(EPresetGroup g)
{
  int i = (GetGroupOrderIndex(g) + 1) % kNumPresetGroups;
  return kPresetCarouselOrder[i];
}

// previous group (wraps around at the start)
static EPresetGroup PrevGroup(EPresetGroup g)
{
  int i = GetGroupOrderIndex(g) - 1;
  if (i < 0) i += kNumPresetGroups;
  return kPresetCarouselOrder[i];
}

// step to next or previous preset — jumps across groups when needed
static void StepPresetCarousel(EPresetGroup& g, int& idx, int dir)
{
  if (dir == 0) return;

  // nothing selected yet — go to first or last preset
  if (g == EPresetGroup::None || idx < 0)
  {
    g = (dir > 0) ? kPresetCarouselOrder[0] : kPresetCarouselOrder[kNumPresetGroups - 1];
    const int cnt = GetPresetCountGlobal(g);
    idx = (dir > 0) ? 0 : (cnt - 1);  // first or last in that group
    return;
  }

  const int cnt = GetPresetCountGlobal(g);
  if (cnt <= 0)
  {
    // group is empty — skip ahead until we find one with presets
    for (int guard = 0; guard < 8; guard++)
    {
      g = (dir > 0) ? NextGroup(g) : PrevGroup(g);
      const int c2 = GetPresetCountGlobal(g);
      if (c2 > 0)
      {
        idx = (dir > 0) ? 0 : (c2 - 1);
        return;
      }
    }
    return;
  }

  idx += dir;  // move one step

  if (idx >= cnt)
  {
    g = NextGroup(g);  // went past the last — jump to next group
    idx = 0;           // start from first
  }
  else if (idx < 0)
  {
    g = PrevGroup(g);  // went before the first — jump to previous group
    const int c2 = GetPresetCountGlobal(g);
    idx = (c2 > 0) ? (c2 - 1) : 0;  // land on last preset there
  }
}

// all the knob values that make up one preset
struct PresetValues
{
  double tremBypass, tremRate, tremDepth;
  double panBypass,  panRate,  panDepth;
  double pitchBypass, pitchRate, pitchDepth;
  double phaserBypass, phaserRate, phaserDepth;

  double compBypass, compMix, compThreshold, compRatio, compGain, compAttack, compRelease;

  double width;           // stereo width (0-200%)
  double masterIntensity; // global wet/dry
  double outputLevel;     // output gain in dB
};

// preset value tables — order: trem | pan | pitch | phaser | comp | width | masterInt | output

static const PresetValues kPresetVals_Vocals[4] =
{
  // 1) Cold Whisper 14 — soft vocal, gentle tremolo shimmer + tight compression
  { 0,2.80,15,   1,0,0,      0,0.08,4,    1,0,0,       0,85,-18,4.0,3.0,5,80,     100, 55, 0.0 },

  // 2) Blade Mono Focus — aggressive mono vocal, heavy comp, phaser edge
  { 1,0,0,       1,0,0,      1,0,0,       0,1.20,45,   0,100,-12,8.0,6.0,2,60,    40, 80, 1.5 },

  // 3) Spectral Glide — airy vocal, wide stereo pan + pitch drift chorus
  { 1,0,0,       0,0.15,70,  0,0.30,18,   0,0.40,30,   0,40,-28,2.0,0.0,20,300,   150, 65,-1.0 },

  // 4) Ritual Double — rhythmic vocal, pulsing tremolo + pan + compression
  { 0,4.50,40,   0,2.25,55,  1,0,0,       1,0,0,       0,75,-15,5.5,4.0,3,100,    120, 70, 0.5 }
};

static const PresetValues kPresetVals_Pads[4] =
{
  // 1) Cryostasis Pad — frozen drone, phaser-only (no pitch, no trem, no pan)
  { 1,0,0,       1,0,0,      1,0,0,       0,0.04,95,   0,40,-28,2.5,-2.0,50,800,  180, 45,-3.0 },

  // 2) Nocturne Pulse — slow breathing tremolo + wide pan, ambient feel
  { 0,0.25,60,   0,0.12,85,  1,0,0,       1,0,0,       0,50,-30,1.8,0.0,30,500,   180, 45,-2.0 },

  // 3) Moon Tides — lush stereo chorus, pan + pitch drift (no phaser)
  { 1,0,0,       0,0.10,70,  0,0.18,20,   1,0,0,       0,25,-28,1.8,-1.0,30,650,  130, 50,-2.0 },

  // 4) Glass Cathedral — shimmering phaser + fast subtle tremolo
  { 0,7.00,12,   1,0,0,      1,0,0,       0,0.30,65,   0,45,-25,2.5,1.0,15,350,   100, 55,-1.5 }
};

static const PresetValues kPresetVals_Drums[3] =
{
  // 1) Iron March — punchy drums, hard comp + fast phaser grit
  { 1,0,0,       1,0,0,      1,0,0,       0,3.50,25,   0,100,-10,12.0,8.0,0.5,40, 100, 90, 3.0 },

  // 2) Ghost Hats — hi-hats/perc, fast tremolo gate + extreme pan
  { 0,8.00,70,   0,5.00,90,  1,0,0,       1,0,0,       0,60,-20,3.0,0.0,1,50,     80, 50,-1.0 },

  // 3) Submerge Kit — underwater drum feel, heavy pitch drift + deep phaser + squashed comp
  { 0,0.60,30,   1,0,0,      0,0.40,35,   0,0.20,70,   0,80,-14,6.0,4.0,5,150,    60, 75, 1.0 }
};

static const PresetValues kPresetVals_Exp[2] =
{
  // 1) Event Horizon — extreme pitch drift + deep phaser, cinematic drone
  { 1,0,0,       1,0,0,      0,0.08,50,   0,0.04,95,   0,30,-30,1.5,-3.0,50,800,  200, 25,-4.0 },

  // 2) Time Shear — everything on, chaotic modulation for glitch/experimental stuff
  { 0,3.70,55,   0,1.80,75,  0,0.60,25,   0,2.50,50,   0,70,-12,7.0,5.0,1,60,     130, 95, 2.0 }
};

// get the values struct for a preset by group + index
static const PresetValues* GetPresetValues(EPresetGroup g, int idx)
{
  static const PresetValues* tables[] = { nullptr, kPresetVals_Vocals, kPresetVals_Pads, kPresetVals_Drums, kPresetVals_Exp };
  const int gi = static_cast<int>(g);
  if (gi < 1 || gi > 4) return nullptr;
  const int count = GetPresetCountGlobal(g);
  return (idx >= 0 && idx < count) ? &tables[gi][idx] : nullptr;
}

// send all preset values to the host like the user turned each knob
// (this way automation and undo still work properly)
static void ApplyPresetToParams(IEditorDelegate* dlg, EPresetGroup g, int idx)
{
  const PresetValues* pv = GetPresetValues(g, idx);
  if (!dlg || !pv) return;

  // send a plain value (Hz, %, dB) — snaps rates, clamps, normalises, then sends
  auto SendPlain = [dlg](int pIdx, double plain)
  {
    if (IParam* p = dlg->GetParam(pIdx))
    {
      if (IsRateParamIdx(pIdx))
        plain = SnapRateHz(plain);  // snap to a nice Hz value

      const double clamped = std::clamp(plain, p->GetMin(), p->GetMax());  // stay in valid range
      dlg->SendParameterValueFromUI(pIdx, p->ToNormalized(clamped));       // convert to 0..1 and send
    }
  };

  // set bypass (1.0 = off/bypassed, 0.0 = on/active)
  auto SetBypass = [dlg](int pIdx, bool bypass)
  {
    dlg->SendParameterValueFromUI(pIdx, bypass ? 1.0 : 0.0);
  };

  // make sure plugin is on
  dlg->SendParameterValueFromUI(kBypass, 0.0);

  // modulation modules — turn on if the preset uses them
  {
    struct ModInfo { double bypass, rate, depth; int bypassIdx, rateIdx, depthIdx; };
    const ModInfo mods[] = {
      { pv->tremBypass,   pv->tremRate,   pv->tremDepth,   kTremBypass,   kTremRate,   kTremDepth },
      { pv->panBypass,    pv->panRate,    pv->panDepth,    kPanBypass,    kPanRate,    kPanDepth },
      { pv->pitchBypass,  pv->pitchRate,  pv->pitchDepth,  kPitchBypass,  kPitchRate,  kPitchDepth },
      { pv->phaserBypass, pv->phaserRate, pv->phaserDepth, kPhaserBypass, kPhaserRate, kPhaserDepth },
    };
    for (const auto& m : mods)
    {
      const bool enable = (m.bypass < 0.5) || (m.rate > 1e-9) || (m.depth > 1e-9);
      SetBypass(m.bypassIdx, !enable);
      SendPlain(m.rateIdx, m.rate);
      SendPlain(m.depthIdx, m.depth);
    }
  }

  // compressor — turn on if preset uses it
  {
    const bool enable = (pv->compBypass < 0.5) || (pv->compMix > 1e-9);
    SetBypass(kCompBypass, !enable);

    SendPlain(kCompMix,       pv->compMix);
    SendPlain(kCompThreshold, pv->compThreshold);
    SendPlain(kCompRatio,     pv->compRatio);
    SendPlain(kCompGain,      pv->compGain);
    SendPlain(kCompAttack,    pv->compAttack);
    SendPlain(kCompRelease,   pv->compRelease);
  }

  // mastering
  SendPlain(kWidth,            pv->width);

  // master / output
  SendPlain(kMasterIntensity, pv->masterIntensity);
  SendPlain(kOutputLevel,     pv->outputLevel);

  // reset toggles to defaults when loading a preset
  dlg->SendParameterValueFromUI(kCompAutoGain, 0.0);  // manual gain mode
  dlg->SendParameterValueFromUI(kTremWaveform, 0.0);  // sine wave
  dlg->SendParameterValueFromUI(kPanWaveform,  0.0);  // sine wave

  // presets store rates as Hz, so make sure all sync toggles start off
  dlg->SendParameterValueFromUI(kTremRateSync,   0.0);
  dlg->SendParameterValueFromUI(kPanRateSync,    0.0);
  dlg->SendParameterValueFromUI(kPitchRateSync,  0.0);
  dlg->SendParameterValueFromUI(kPhaserRateSync, 0.0);
}

// global preset state — shared between UI controls, only used on UI thread
static EPresetGroup gLastPresetGroup      = EPresetGroup::Vocals;   // last group tab the user was on
static EPresetGroup gSelectedGroupGlobal  = EPresetGroup::None;     // active preset's group
static int          gSelectedPresetGlobal = -1;                     // active preset index (-1 = none)
static std::string  gSelectedPresetName   = kPresetName_None;       // name shown in the label

// default values — used on first load and when user clicks "Revert"
namespace
{
  struct DefaultParam { int idx; double value; };

  static const DefaultParam kDefaultPresetParams[] =
  {
    { kBypass,          0.0   },

    { kCompBypass,      0.0   },
    { kTremBypass,      1.0   },
    { kPanBypass,       1.0   },
    { kPitchBypass,     1.0   },
    { kPhaserBypass,    1.0   },

    { kGain,            0.0   },

    // 4 mod modules: off by default, but with useful starting values
    { kTremRate,   4.0  }, { kTremDepth,   50.0 },   // 4 Hz, medium depth
    { kPanRate,    0.5  }, { kPanDepth,    60.0 },   // slow wide pan
    { kPitchRate,  0.3  }, { kPitchDepth,  40.0 },   // gentle chorus
    { kPhaserRate, 0.8  }, { kPhaserDepth, 70.0 },   // moderate sweep

    // compressor: punchy parallel setup, keeps transients
    { kCompMix,        65.0  },   // parallel blend
    { kCompThreshold, -18.0  },   // catches peaks on most material
    { kCompRatio,       3.0  },   // 3:1 — controls dynamics without killing them
    { kCompGain,        3.0  },   // makeup gain
    { kCompAttack,     15.0  },   // lets the transient through
    { kCompRelease,   200.0  },   // musical, follows natural decay

    { kWidth,           110.0 },  // a bit wider than mono
    { kMasterIntensity, 100.0 },  // full effect
    { kOutputLevel,    -1.0   },  // -1 dB headroom so it doesn't clip

    { kCompAutoGain,    0.0   },  // manual makeup gain
    { kTremWaveform,    0.0   },  // sine wave
    { kPanWaveform,     0.0   },  // sine wave

    { kTremRateSync,    0.0   },  // free Hz mode
    { kPanRateSync,     0.0   },  // free Hz mode
    { kPitchRateSync,   0.0   },  // free Hz mode
    { kPhaserRateSync,  0.0   }   // free Hz mode
  };

  template <typename Setter>
  inline void ApplyDefaultPresetParams(Setter&& set)
  {
    for (const auto& p : kDefaultPresetParams)
      set(p.idx, p.value);
  }
}
