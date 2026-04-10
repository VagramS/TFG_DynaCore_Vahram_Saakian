// DynaCore.cpp
// Main implementation file — DSP, UI controls, preset system.
//
// Author: Vahram Saakian, UCM TFG 2025-2026

#include "DynaCore.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <cstdio> // snprintf
#include <cmath>
#include <algorithm> // std::clamp
#include <string>
#include "wdlstring.h"

// draws a semi-transparent tint over a rect (used for hover effects)
namespace
{
  inline void DrawHoverOverlay(IGraphics& g,
                               const IRECT& bounds,
                               const IColor& color,
                               float cornerRadius)
  {
    if (cornerRadius > 0.f)
      g.FillRoundRect(color, bounds, cornerRadius);
    else
      g.FillRect(color, bounds);
  }
}

// LFO rate knobs are continuous but we snap to a fixed set of clean values.
// 0.00 = stopped, anything below the dead zone threshold also snaps to 0.
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

// checks if a param index is one of the four Rate knobs
static inline bool IsRateParamIdx(int idx)
{
  return idx == kTremRate || idx == kPanRate || idx == kPitchRate || idx == kPhaserRate;
}

// tiny Hz values near zero are indistinguishable from stopped, so snap them to 0
static inline double RateDeadZoneHz(int idx)
{
  switch (idx)
  {
    case kTremRate:   return 0.02;
    case kPanRate:    return 0.02;
    case kPitchRate:  return 0.01;
    case kPhaserRate: return 0.02;
    default:          return 0.0;
  }
}

static inline double ApplyRateDeadZone(int idx, double hz)
{
  if (!IsRateParamIdx(idx))
    return hz;

  const double dz = RateDeadZoneHz(idx);
  if (hz > 0.0 && hz < dz)
    return 0.0;

  return hz;
}

// auto-bypass a module when Rate or Depth hits zero — only checks the module that just changed
static void AutoGateModulesFromParams(IEditorDelegate* dlg, int changedParamIdx)
{
  if (!dlg) return;

  // guard against re-entry: setting bypass fires OnParamChange which calls this again
  static bool sInAutoGate = false;
  if (sInAutoGate) return;
  sInAutoGate = true;

  auto GetPlain = [dlg](int pIdx) -> double
  {
    if (IParam* p = dlg->GetParam(pIdx))
      return p->Value();
    return 0.0;
  };

  auto IsRateOn = [dlg](int rateIdx) -> bool
  {
    if (IParam* p = dlg->GetParam(rateIdx))
    {
      const double hz = ApplyRateDeadZone(rateIdx, p->Value());
      return hz > 0.0;
    }
    return false;
  };

  auto IsDepthOn = [dlg](int depthIdx) -> bool
  {
    if (IParam* p = dlg->GetParam(depthIdx))
      return p->Value() > 0.0;
    return false;
  };

  auto SetBypass = [dlg](int bypassIdx, bool bypass)
  {
    // 1.0 = bypassed, 0.0 = active
    const double target = bypass ? 1.0 : 0.0;

    if (IParam* bp = dlg->GetParam(bypassIdx))
    {
      if (std::fabs(bp->GetNormalized() - target) > 1e-9)
        dlg->SendParameterValueFromUI(bypassIdx, target);
    }
  };

  struct ModGate { int rateIdx; int depthIdx; int bypassIdx; };

  static const ModGate kMods[] =
  {
    { kTremRate,   kTremDepth,   kTremBypass   },
    { kPanRate,    kPanDepth,    kPanBypass    },
    { kPitchRate,  kPitchDepth,  kPitchBypass  },
    { kPhaserRate, kPhaserDepth, kPhaserBypass }
  };

  for (const auto& m : kMods)
  {
    if (changedParamIdx != m.rateIdx && changedParamIdx != m.depthIdx)
      continue;

    const bool on = IsRateOn(m.rateIdx) && IsDepthOn(m.depthIdx);
    SetBypass(m.bypassIdx, !on);
  }

  // Compressor: only auto-gate when Mix itself changes
  if (changedParamIdx == kCompMix)
  {
    const bool compOn = GetPlain(kCompMix) > 0.0;
    SetBypass(kCompBypass, !compOn);
  }

  sInAutoGate = false;
}

class RevertButtonControl; // forward declaration

// Preset groups
enum class EPresetGroup
{
  None,
  Vocals,
  Pads,
  Drums,
  Experimental
};

// Preset names
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

// Helper: preset name by group and index (nullptr if none)
static const char* GetPresetName(EPresetGroup g, int idx)
{
  switch (g)
  {
    case EPresetGroup::Vocals:
      if (idx >= 0 && idx < 4) return kPresets_Vocals[idx];
      break;
    case EPresetGroup::Pads:
      if (idx >= 0 && idx < 4) return kPresets_Pads[idx];
      break;
    case EPresetGroup::Drums:
      if (idx >= 0 && idx < 3) return kPresets_Drums[idx];
      break;
    case EPresetGroup::Experimental:
      if (idx >= 0 && idx < 2) return kPresets_Exp[idx];
      break;
    default: break;
  }
  return nullptr;
}

static int GetPresetCountGlobal(EPresetGroup g)
{
  switch (g)
  {
    case EPresetGroup::Vocals:       return 4;
    case EPresetGroup::Pads:         return 4;
    case EPresetGroup::Drums:        return 3;
    case EPresetGroup::Experimental: return 2;
    default:                         return 0;
  }
}

static EPresetGroup kPresetCarouselOrder[] =
{
  EPresetGroup::Vocals,
  EPresetGroup::Pads,
  EPresetGroup::Drums,
  EPresetGroup::Experimental
};

static int GetGroupOrderIndex(EPresetGroup g)
{
  for (int i = 0; i < (int) (sizeof(kPresetCarouselOrder) / sizeof(kPresetCarouselOrder[0])); i++)
    if (kPresetCarouselOrder[i] == g) return i;

  return 0; // fallback
}

static EPresetGroup NextGroup(EPresetGroup g)
{
  const int n = (int) (sizeof(kPresetCarouselOrder) / sizeof(kPresetCarouselOrder[0]));
  int i = GetGroupOrderIndex(g);
  i = (i + 1) % n;
  return kPresetCarouselOrder[i];
}

static EPresetGroup PrevGroup(EPresetGroup g)
{
  const int n = (int) (sizeof(kPresetCarouselOrder) / sizeof(kPresetCarouselOrder[0]));
  int i = GetGroupOrderIndex(g);
  i = (i - 1) % n;
  if (i < 0) i += n;
  return kPresetCarouselOrder[i];
}

// Step carousel by dir (+1 / -1), wrapping across groups
static void StepPresetCarousel(EPresetGroup& g, int& idx, int dir)
{
  if (dir == 0) return;

  // If nothing selected yet, start from the beginning/end of the carousel
  if (g == EPresetGroup::None || idx < 0)
  {
    g = (dir > 0) ? kPresetCarouselOrder[0] : kPresetCarouselOrder[(sizeof(kPresetCarouselOrder)/sizeof(kPresetCarouselOrder[0])) - 1];
    const int cnt = GetPresetCountGlobal(g);
    idx = (dir > 0) ? 0 : (cnt - 1);
    return;
  }

  const int cnt = GetPresetCountGlobal(g);
  if (cnt <= 0)
  {
    // If current group has no presets, jump to next/prev group until we find a non-empty one
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

  idx += dir;

  if (idx >= cnt)
  {
    // Move to next group, first preset
    g = NextGroup(g);
    idx = 0;
  }
  else if (idx < 0)
  {
    // Move to previous group, last preset
    g = PrevGroup(g);
    const int c2 = GetPresetCountGlobal(g);
    idx = (c2 > 0) ? (c2 - 1) : 0;
  }
}


// All parameter values for one preset
struct PresetValues
{
  double tremBypass, tremRate, tremDepth;
  double panBypass,  panRate,  panDepth;
  double pitchBypass,pitchRate,pitchDepth;
  double phaserBypass,phaserRate,phaserDepth;

  double compBypass, compMix, compThreshold, compRatio, compGain, compAttack, compRelease;

  double width;           // stereo width 0-200%
  double masterIntensity;
  double outputLevel;
};

// Preset value tables — columns: trem | pan | pitch | phaser | comp | width | masterInt | output

static const PresetValues kPresetVals_Vocals[4] =
{
  // 1) Cold Whisper 14 — intimate, breathy vocal with gentle tremolo shimmer + tight compression
  { 0,2.80,15,   1,0,0,      0,0.08,4,    1,0,0,       0,85,-18,4.0,3.0,5,80,     100, 55, 0.0 },

  // 2) Blade Mono Focus — aggressive mono vocal, heavy compression, phaser edge
  { 1,0,0,       1,0,0,      1,0,0,       0,1.20,45,   0,100,-12,8.0,6.0,2,60,    40, 80, 1.5 },

  // 3) Spectral Glide — ethereal vocal, wide stereo pan + pitch drift chorus
  { 1,0,0,       0,0.15,70,  0,0.30,18,   0,0.40,30,   0,40,-28,2.0,0.0,20,300,   150, 65,-1.0 },

  // 4) Ritual Double — rhythmic vocal effect, pulsing tremolo + pan + compression
  { 0,4.50,40,   0,2.25,55,  1,0,0,       1,0,0,       0,75,-15,5.5,4.0,3,100,    120, 70, 0.5 }
};

static const PresetValues kPresetVals_Pads[4] =
{
  // 1) Cryostasis Pad — frozen, almost static pad with deep phaser sweep + heavy compression
  { 1,0,0,       1,0,0,      0,0.05,8,    0,0.06,80,   0,30,-35,2.0,-2.0,40,600,  160, 40,-3.0 },

  // 2) Nocturne Pulse — slow breathing tremolo + wide pan for ambient pads
  { 0,0.25,60,   0,0.12,85,  1,0,0,       1,0,0,       0,50,-30,1.8,0.0,30,500,   180, 45,-2.0 },

  // 3) Moon Tides — gentle stereo movement with subtle pitch drift detune
  { 1,0,0,       0,0.08,50,  0,0.12,12,   0,0.15,35,   0,20,-32,1.5,-1.0,35,700,  140, 35,-2.5 },

  // 4) Glass Cathedral — shimmering phaser + fast subtle tremolo sparkle
  { 0,7.00,12,   1,0,0,      1,0,0,       0,0.30,65,   0,45,-25,2.5,1.0,15,350,   100, 55,-1.5 }
};

static const PresetValues kPresetVals_Drums[3] =
{
  // 1) Iron March — punchy, aggressive drums with hard compression + fast phaser grit
  { 1,0,0,       1,0,0,      1,0,0,       0,3.50,25,   0,100,-10,12.0,8.0,0.5,40, 100, 90, 3.0 },

  // 2) Ghost Hats — hi-hat/perc focused, fast tremolo gate + extreme pan movement
  { 0,8.00,70,   0,5.00,90,  1,0,0,       1,0,0,       0,60,-20,3.0,0.0,1,50,     80, 50,-1.0 },

  // 3) Submerge Kit — underwater drum effect, heavy pitch drift + deep phaser + squashed compression
  { 0,0.60,30,   1,0,0,      0,0.40,35,   0,0.20,70,   0,80,-14,6.0,4.0,5,150,    60, 75, 1.0 }
};

static const PresetValues kPresetVals_Exp[2] =
{
  // 1) Event Horizon — extreme pitch drift + deep phaser, no tremolo/pan, cinematic drone
  { 1,0,0,       1,0,0,      0,0.08,50,   0,0.04,95,   0,30,-40,1.5,-3.0,50,800,  200, 25,-4.0 },

  // 2) Time Shear — all modules active, chaotic modulation for glitch/experimental
  { 0,3.70,55,   0,1.80,75,  0,0.60,25,   0,2.50,50,   0,70,-12,7.0,5.0,1,60,     130, 95, 2.0 }
};

static const PresetValues* GetPresetValues(EPresetGroup g, int idx)
{
  switch (g)
  {
    case EPresetGroup::Vocals:       if (idx>=0 && idx<4) return &kPresetVals_Vocals[idx]; break;
    case EPresetGroup::Pads:         if (idx>=0 && idx<4) return &kPresetVals_Pads[idx]; break;
    case EPresetGroup::Drums:        if (idx>=0 && idx<3) return &kPresetVals_Drums[idx]; break;
    case EPresetGroup::Experimental: if (idx>=0 && idx<2) return &kPresetVals_Exp[idx]; break;
    default: break;
  }
  return nullptr;
}

// Apply a preset via SendParameterValueFromUI (so the host sees changes like user input)
static void ApplyPresetToParams(IEditorDelegate* dlg, EPresetGroup g, int idx)
{
  const PresetValues* pv = GetPresetValues(g, idx);
  if (!dlg || !pv) return;

  auto SendPlain = [dlg](int pIdx, double plain)
  {
    if (IParam* p = dlg->GetParam(pIdx))
    {
      if (IsRateParamIdx(pIdx))
        plain = SnapRateHz(plain);

      const double clamped = std::clamp(plain, p->GetMin(), p->GetMax());
      dlg->SendParameterValueFromUI(pIdx, p->ToNormalized(clamped));
    }
  };

  auto SetBypass = [dlg](int pIdx, bool bypass)
  {
    dlg->SendParameterValueFromUI(pIdx, bypass ? 1.0 : 0.0); // 1=OFF, 0=ON
  };

  // Plugin ON
  dlg->SendParameterValueFromUI(kBypass, 0.0);

  // Trem
  {
    const bool enable = (pv->tremBypass < 0.5) || (pv->tremRate > 1e-9) || (pv->tremDepth > 1e-9);
    SetBypass(kTremBypass, !enable);
    SendPlain(kTremRate,  pv->tremRate);
    SendPlain(kTremDepth, pv->tremDepth);
  }

  // Pan
  {
    const bool enable = (pv->panBypass < 0.5) || (pv->panRate > 1e-9) || (pv->panDepth > 1e-9);
    SetBypass(kPanBypass, !enable);
    SendPlain(kPanRate,  pv->panRate);
    SendPlain(kPanDepth, pv->panDepth);
  }

  // Pitch
  {
    const bool enable = (pv->pitchBypass < 0.5) || (pv->pitchRate > 1e-9) || (pv->pitchDepth > 1e-9);
    SetBypass(kPitchBypass, !enable);
    SendPlain(kPitchRate,  pv->pitchRate);
    SendPlain(kPitchDepth, pv->pitchDepth);
  }

  // Phaser
  {
    const bool enable = (pv->phaserBypass < 0.5) || (pv->phaserRate > 1e-9) || (pv->phaserDepth > 1e-9);
    SetBypass(kPhaserBypass, !enable);
    SendPlain(kPhaserRate,  pv->phaserRate);
    SendPlain(kPhaserDepth, pv->phaserDepth);
  }

  // Compressor
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

  // Mastering
  SendPlain(kWidth,            pv->width);

  // Master / Output
  SendPlain(kMasterIntensity, pv->masterIntensity);
  SendPlain(kOutputLevel,     pv->outputLevel);
}

// Global preset state — UI thread only
static EPresetGroup gLastPresetGroup      = EPresetGroup::Vocals;
static EPresetGroup gSelectedGroupGlobal  = EPresetGroup::None;
static int          gSelectedPresetGlobal = -1;
static std::string  gSelectedPresetName   = kPresetName_None;

// Default preset values — used on first open and for "Revert to Default"
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

    // 4 modulation modules: OFF by default, with musically useful starting values
    { kTremRate,   4.0  }, { kTremDepth,   50.0 },   // 4 Hz, medium depth
    { kPanRate,    0.5  }, { kPanDepth,    60.0 },   // slow wide pan
    { kPitchRate,  0.3  }, { kPitchDepth,  40.0 },   // gentle vibrato/chorus
    { kPhaserRate, 0.8  }, { kPhaserDepth, 70.0 },   // moderate phaser sweep

    // Compressor: punchy parallel compression, lets transients through
    { kCompMix,        65.0  },   // parallel blend — adds punch without over-squashing
    { kCompThreshold, -18.0  },   // moderate threshold, catches peaks on most material
    { kCompRatio,       3.0  },   // 3:1 — controls dynamics without killing them
    { kCompGain,        3.0  },   // makeup gain to compensate
    { kCompAttack,     15.0  },   // 15ms — lets initial transient punch through
    { kCompRelease,   200.0  },   // 200ms — musical, tracks natural decay

    { kWidth,           110.0 },  // slightly wider than mono — open, transparent
    { kMasterIntensity, 100.0 },  // full effect by default
    { kOutputLevel,    -1.0   }   // -1 dB headroom to prevent clipping
  };

  template <typename Setter>
  inline void ApplyDefaultPresetParams(Setter&& set)
  {
    for (const auto& p : kDefaultPresetParams)
      set(p.idx, p.value);
  }
}

void DynaCore::ApplyDefaultPresetFromUI()
{
  ApplyDefaultPresetParams([this](int pIdx, double plain)
  {
    if (auto* p = GetParam(pIdx))
      p->Set(plain);
  });

  // Update text on the main page
  gSelectedGroupGlobal  = EPresetGroup::None;
  gSelectedPresetGlobal = -1;
  gSelectedPresetName   = kPresetName_None;

  if (GetUI())
    GetUI()->SetAllControlsDirty();
}


// Full-screen preset selection overlay (left = category tabs, right = preset names)
class PresetsPageControl : public IControl
{
public:
  PresetsPageControl(const IRECT& bounds,
                     const IBitmap& pageBitmap,
                     const IBitmap& vocalsSelectBmp,
                     const IBitmap& padsSelectBmp,
                     const IBitmap& drumsSelectBmp,
                     const IBitmap& expSelectBmp,
                     const IBitmap& vocalsLabelBmp,
                     const IBitmap& padsLabelBmp,
                     const IBitmap& drumsLabelBmp,
                     const IBitmap& expLabelBmp,
                     const IBitmap& arrowBmp,
                     const IBitmap& revertBmp,
                     const IBitmap& dividerBmp,
                     const IBitmap& presetFirstSelectBmp,
                     const IBitmap& presetRestSelectBmp,
                     const IBitmap& presetPointerBmp)
  : IControl(bounds)
  , mPageBitmap(pageBitmap)
  , mVocalsSelectBmp(vocalsSelectBmp)
  , mPadsSelectBmp(padsSelectBmp)
  , mDrmsSelectBmp(drumsSelectBmp)
  , mExpSelectBmp(expSelectBmp)
  , mVocalsLabelBmp(vocalsLabelBmp)
  , mPadsLabelBmp(padsLabelBmp)
  , mDrumsLabelBmp(drumsLabelBmp)
  , mExpLabelBmp(expLabelBmp)
  , mArrowBmp(arrowBmp)
  , mRevertBmp(revertBmp)
  , mDividerBmp(dividerBmp)
  , mPresetFirstSelectBmp(presetFirstSelectBmp)
  , mPresetRestSelectBmp(presetRestSelectBmp)
  , mPresetPointerBmp(presetPointerBmp)
  {
    mIgnoreMouse = false;

    // Open on the last viewed group, and display selection only if it belongs to this group
    mSelectedGroup = gLastPresetGroup;
    mSelectedPresetIndex = (gSelectedGroupGlobal == mSelectedGroup) ? gSelectedPresetGlobal : -1;
  }

  EPresetGroup GetSelectedGroup() const { return mSelectedGroup; }

  void SetRevertButton(IControl* button) { mRevertButton = button; }

static IRECT GroupRect(EPresetGroup g)
{
  switch (g)
  {
    case EPresetGroup::Vocals:       return IRECT(19.f, 104.f, 285.f, 173.f);
    case EPresetGroup::Pads:         return IRECT(19.f, 174.f, 285.f, 241.f);
    case EPresetGroup::Drums:        return IRECT(19.f, 242.f, 285.f, 311.f);
    case EPresetGroup::Experimental: return IRECT(19.f, 312.f, 285.f, 380.f);
    case EPresetGroup::None: default: return IRECT();
  }
}

  void Draw(IGraphics& g) override
  {
    // Overlay background
    g.DrawBitmap(mPageBitmap, mRECT);

    // "Revert to Default" base bitmap (button is a separate control on top)
    const IRECT revertRect(413.f, 536.f, 413.f + 130.f, 536.f + 36.f);
    g.DrawBitmap(mRevertBmp, revertRect);

    // Group rects (used for hover visuals and hit-testing)
    const IRECT vocalsRect = GroupRect(EPresetGroup::Vocals);
    const IRECT padsRect   = GroupRect(EPresetGroup::Pads);
    const IRECT drumsRect  = GroupRect(EPresetGroup::Drums);
    const IRECT expRect    = GroupRect(EPresetGroup::Experimental);

    // Group hover overlays
    const IColor groupHoverColor(20, 184, 184, 184);
    constexpr float groupCornerRadius = 5.f;

    switch (mHoverGroup)
    {
      case EPresetGroup::Vocals:       DrawHoverOverlay(g, vocalsRect, groupHoverColor, groupCornerRadius); break;
      case EPresetGroup::Pads:         DrawHoverOverlay(g, padsRect,    groupHoverColor, groupCornerRadius); break;
      case EPresetGroup::Drums:        DrawHoverOverlay(g, drumsRect,   groupHoverColor, groupCornerRadius); break;
      case EPresetGroup::Experimental: DrawHoverOverlay(g, expRect,     groupHoverColor, groupCornerRadius); break;
      case EPresetGroup::None: default: break;
    }

    // Selected group highlight bitmaps
    switch (mSelectedGroup)
    {
      case EPresetGroup::Vocals:       g.DrawBitmap(mVocalsSelectBmp, IRECT(14.f,  98.f, 14.f + 276.f,  98.f + 79.f)); break;
      case EPresetGroup::Pads:         g.DrawBitmap(mPadsSelectBmp,   IRECT(14.f, 168.f, 14.f + 276.f, 168.f + 77.f)); break;
      case EPresetGroup::Drums:        g.DrawBitmap(mDrmsSelectBmp,   IRECT(14.f, 236.f, 14.f + 276.f, 236.f + 79.f)); break;
      case EPresetGroup::Experimental: g.DrawBitmap(mExpSelectBmp,    IRECT(14.f, 306.f, 14.f + 276.f, 306.f + 78.f)); break;
      case EPresetGroup::None: default: break;
    }

    // Group labels
    g.DrawBitmap(mVocalsLabelBmp, IRECT(112.f,130.f, 194.f,149.f));
    g.DrawBitmap(mPadsLabelBmp,   IRECT( 97.f,199.f, 208.f,220.f));
    g.DrawBitmap(mDrumsLabelBmp,  IRECT( 48.f,268.f, 257.f,289.f));
    g.DrawBitmap(mExpLabelBmp,    IRECT( 58.f,338.f, 247.f,359.f));

    // Dividers per selected group
    {
      const float X = 287.f;
      const float W = 266.f;
      const float H = 1.f;
      const float Ylist[4] = {154.f, 205.f, 256.f, 307.f};
      const int needed = GetPresetCountGlobal(mSelectedGroup);

      for (int i = 0; i < needed && i < 4; ++i)
        g.DrawBitmap(mDividerBmp, IRECT(X, Ylist[i], X + W, Ylist[i] + H));
    }

    // ----- PRESET POINTER BULLETS (right side list) -----
    {
      const int presetCount = GetPresetCountGlobal(mSelectedGroup);
      const float bulletX = 311.f;
      const float bulletW = 6.f;
      const float bulletH = 6.f;
      const float bulletY[4] = {127.f, 178.f, 229.f, 280.f};

      for (int i = 0; i < presetCount && i < 4; ++i)
      {
        const IRECT r(bulletX, bulletY[i], bulletX + bulletW, bulletY[i] + bulletH);
        g.DrawBitmap(mPresetPointerBmp, r);
      }
    }

    // ----- PRESET NAMES (Font: Inter-Medium, Size 15, Color #CAD8E6) -----
    {
      const int presetCount = GetPresetCountGlobal(mSelectedGroup);
      const float textX = 330.f;
      const float textY[4] = {130.5f, 181.5f, 232.5f, 283.5f};
      const IColor nameColor(255, 202, 216, 230);
      IText nameText(15.f, nameColor, "Inter-Semi-Bold", EAlign::Near, EVAlign::Middle);

      for (int i = 0; i < presetCount && i < 4; ++i)
      {
        const char* nm = GetPresetName(mSelectedGroup, i);
        if (!nm) continue;
        IRECT tr(textX, textY[i] - 9.f, textX + 220.f, textY[i] + 9.f);
        g.DrawText(nameText, nm, tr);
      }
    }

    // Preset hover overlay
    {
      const int presetCount = GetPresetCountGlobal(mSelectedGroup);

      if (mHoverPresetIndex >= 0 && mHoverPresetIndex < presetCount)
      {
        const float presetX = 287.f;
        const float presetW = 266.f;
        const float presetH = 50.f;
        const float presetY[4] = { 104.f, 155.f, 206.f, 257.f };

        const int idx = mHoverPresetIndex;
        IRECT presetRect(presetX, presetY[idx], presetX + presetW, presetY[idx] + presetH);
        DrawHoverOverlay(g, presetRect, IColor(20,184,184,184), 5.f);
      }
    }

    // Selected preset highlight (within current group)
    if (mSelectedPresetIndex >= 0)
    {
      const int count = GetPresetCountGlobal(mSelectedGroup);
      if (mSelectedPresetIndex < count)
      {
        const float presetX = 283.f;
        const float presetW = 276.f;

        const float firstY = 99.f;
        const float firstH = 65.f;
        const float restH  = 60.f;
        const float restY[3] = {149.f, 200.f, 251.f};

        if (mSelectedPresetIndex == 0)
        {
          g.DrawBitmap(mPresetFirstSelectBmp, IRECT(presetX, firstY, presetX + presetW, firstY + firstH));
        }
        else if (mSelectedPresetIndex >= 1 && mSelectedPresetIndex <= 3)
        {
          const int idx = mSelectedPresetIndex - 1;
          g.DrawBitmap(mPresetRestSelectBmp, IRECT(presetX, restY[idx], presetX + presetW, restY[idx] + restH));
        }
      }
    }

    // Arrow near current selection
    IRECT arrowRect;
    switch (mSelectedGroup)
    {
      case EPresetGroup::Vocals:       arrowRect = IRECT(267.f,130.f, 281.f,149.f); break;
      case EPresetGroup::Pads:         arrowRect = IRECT(267.f,199.f, 281.f,218.f); break;
      case EPresetGroup::Drums:        arrowRect = IRECT(267.f,268.f, 281.f,287.f); break;
      case EPresetGroup::Experimental: arrowRect = IRECT(267.f,338.f, 281.f,357.f); break;
      case EPresetGroup::None: default: return;
    }
    g.DrawBitmap(mArrowBmp, arrowRect);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    // Active overlay area (click outside closes overlay and revert button)
    const IRECT active(19.f, 104.f, 554.f, 578.f);

    if (!active.Contains(x, y))
    {
      if (IGraphics* ui = GetUI())
      {
        ui->SetMouseCursor(ECursor::ARROW);
        if (mRevertButton)
        {
          ui->RemoveControl(mRevertButton);
          mRevertButton = nullptr;
        }
        ui->RemoveControl(this);
      }
      return;
    }

    // --- Group selection (left side) ---
    const IRECT vocalsRect = GroupRect(EPresetGroup::Vocals);
    const IRECT padsRect   = GroupRect(EPresetGroup::Pads);
    const IRECT drumsRect  = GroupRect(EPresetGroup::Drums);
    const IRECT expRect    = GroupRect(EPresetGroup::Experimental);

    EPresetGroup newGroup = mSelectedGroup;

    if      (vocalsRect.Contains(x, y)) newGroup = EPresetGroup::Vocals;
    else if (padsRect.Contains(x,   y)) newGroup = EPresetGroup::Pads;
    else if (drumsRect.Contains(x,  y)) newGroup = EPresetGroup::Drums;
    else if (expRect.Contains(x,    y)) newGroup = EPresetGroup::Experimental;

    if (newGroup != mSelectedGroup)
    {
      mSelectedGroup   = newGroup;
      gLastPresetGroup = newGroup;

      // show selection only if it belongs to this group
      mSelectedPresetIndex = (gSelectedGroupGlobal == mSelectedGroup) ? gSelectedPresetGlobal : -1;

      mHoverPresetIndex  = -1;
      SetDirty(false);
      return;
    }

    // --- Preset selection (right side) ---
    const float presetX = 287.f;
    const float presetW = 276.f;

    const float firstY = 104.f;
    const float firstH = 51.f;

    const float restH  = 51.f;
    const float restY[3] = {155.f, 206.f, 257.f};

    IRECT presetFirstRect(presetX, firstY, presetX + presetW, firstY + firstH);
    IRECT presetRect1(presetX, restY[0], presetX + presetW, restY[0] + restH);
    IRECT presetRect2(presetX, restY[1], presetX + presetW, restY[1] + restH);
    IRECT presetRect3(presetX, restY[2], presetX + presetW, restY[2] + restH);

    int clickedPreset = -1;

    if      (presetFirstRect.Contains(x, y)) clickedPreset = 0;
    else if (presetRect1.Contains(x,    y)) clickedPreset = 1;
    else if (presetRect2.Contains(x,    y)) clickedPreset = 2;
    else if (presetRect3.Contains(x,    y)) clickedPreset = 3;

    if (clickedPreset >= 0)
    {
      const int count = GetPresetCountGlobal(mSelectedGroup);
      if (clickedPreset < count)
      {
        // update local and GLOBAL selection (single selection across all groups)
        mSelectedPresetIndex  = clickedPreset;
        gSelectedGroupGlobal  = mSelectedGroup;
        gSelectedPresetGlobal = clickedPreset;

        // update global readable name
        if (const char* nm = GetPresetName(mSelectedGroup, clickedPreset))
          gSelectedPresetName = nm;
        else
          gSelectedPresetName = kPresetName_None;

        // Apply preset to parameters
        ApplyPresetToParams(GetDelegate(), mSelectedGroup, clickedPreset);

        SetDirty(false);

        if (auto* ui = GetUI())
        {
          ui->SetAllControlsDirty(); // refresh label on the main page

          // --- Close the window after select a preset ---
          ui->SetMouseCursor(ECursor::ARROW);

          if (mRevertButton)
          {
            ui->RemoveControl(mRevertButton);
            mRevertButton = nullptr;
          }

          ui->RemoveControl(this);
          return;
        }
      }
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    IControl::OnMouseOver(x, y, mod);
    UpdateHover(x, y);
  }

  void OnMouseOut() override
  {
    IControl::OnMouseOut();

    if (mHoverGroup != EPresetGroup::None || mHoverPresetIndex != -1)
    {
      mHoverGroup = EPresetGroup::None;
      mHoverPresetIndex = -1;
      SetDirty(false);
    }

    if (auto* ui = GetUI())
      ui->SetMouseCursor(ECursor::ARROW);
  }

private:
  void UpdateHover(float x, float y)
  {
    // Group rects
    const IRECT vocalsRect = GroupRect(EPresetGroup::Vocals);
    const IRECT padsRect   = GroupRect(EPresetGroup::Pads);
    const IRECT drumsRect  = GroupRect(EPresetGroup::Drums);
    const IRECT expRect    = GroupRect(EPresetGroup::Experimental);

    EPresetGroup newHoverGroup = EPresetGroup::None;

    if      (vocalsRect.Contains(x, y)) newHoverGroup = EPresetGroup::Vocals;
    else if (padsRect.Contains(x,   y)) newHoverGroup = EPresetGroup::Pads;
    else if (drumsRect.Contains(x,  y)) newHoverGroup = EPresetGroup::Drums;
    else if (expRect.Contains(x,    y)) newHoverGroup = EPresetGroup::Experimental;

    // Preset rects (only for existing presets)
    const float presetX = 287.f;
    const float presetW = 276.f;

    const float firstY = 104.f;
    const float firstH = 51.f;

    const float restH  = 51.f;
    const float restY[3] = {155.f, 206.f, 257.f};

    IRECT presetFirstRect(presetX, firstY, presetX + presetW, firstY + firstH);
    IRECT presetRect1(presetX, restY[0], presetX + presetW, restY[0] + restH);
    IRECT presetRect2(presetX, restY[1], presetX + presetW, restY[1] + restH);
    IRECT presetRect3(presetX, restY[2], presetX + presetW, restY[2] + restH);

    const int count = GetPresetCountGlobal(mSelectedGroup);

    int newHoverPreset = -1;
    if (count > 0 && presetFirstRect.Contains(x, y))      newHoverPreset = 0;
    else if (count > 1 && presetRect1.Contains(x, y))     newHoverPreset = 1;
    else if (count > 2 && presetRect2.Contains(x, y))     newHoverPreset = 2;
    else if (count > 3 && presetRect3.Contains(x, y))     newHoverPreset = 3;

    bool changed = false;

    if (newHoverGroup != mHoverGroup)       { mHoverGroup = newHoverGroup; changed = true; }
    if (newHoverPreset != mHoverPresetIndex){ mHoverPresetIndex = newHoverPreset; changed = true; }

    if (changed) SetDirty(false);

    if (auto* ui = GetUI())
      ui->SetMouseCursor((newHoverGroup != EPresetGroup::None || newHoverPreset != -1) ? ECursor::HAND
                                                                                       : ECursor::ARROW);
  }

  IBitmap mPageBitmap;
  IBitmap mVocalsSelectBmp, mPadsSelectBmp, mDrmsSelectBmp, mExpSelectBmp;
  IBitmap mVocalsLabelBmp,  mPadsLabelBmp,  mDrumsLabelBmp,  mExpLabelBmp;
  IBitmap mArrowBmp, mRevertBmp;
  IBitmap mDividerBmp;
  IBitmap mPresetFirstSelectBmp;
  IBitmap mPresetRestSelectBmp;
  IBitmap mPresetPointerBmp;

  EPresetGroup mSelectedGroup = EPresetGroup::Vocals;
  EPresetGroup mHoverGroup    = EPresetGroup::None;
  int          mSelectedPresetIndex = -1; // only if this group == gSelectedGroupGlobal
  int          mHoverPresetIndex    = -1; // -1 means none hovered
  IControl*    mRevertButton  = nullptr;
};

// Hit area over "Revert to Default" bitmap — applies defaults and closes overlay on click
class RevertButtonControl : public IControl
{
public:
  RevertButtonControl(const IRECT& bounds, PresetsPageControl* overlayToClose)
  : IControl(bounds), mOverlay(overlayToClose)
  {
    mIgnoreMouse = false;
  }

  void Draw(IGraphics& g) override
  {
    if (GetMouseIsOver())
    {
      IColor overlay(20, 184, 184, 184);
      DrawHoverOverlay(g, mRECT, overlay, 5.f);
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    IControl::OnMouseOver(x, y, mod);
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::HAND);
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    IControl::OnMouseOut();
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::ARROW);
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    ApplyDefaultPreset();

    if (IGraphics* ui = GetUI())
    {
      if (mOverlay) ui->RemoveControl(mOverlay);
      ui->RemoveControl(this);
      ui->SetAllControlsDirty(); // update label on the main page
    }
  }

private:
void ApplyDefaultPreset()
{
  if (auto* dlg = GetDelegate())
  {
    ApplyDefaultPresetParams([dlg](int pIdx, double plain)
    {
      if (IParam* p = dlg->GetParam(pIdx))
      {
        const double clamped = std::clamp(plain, p->GetMin(), p->GetMax());
        dlg->SendParameterValueFromUI(pIdx, p->ToNormalized(clamped));
      }
    });
  }

  gSelectedGroupGlobal  = EPresetGroup::None;
  gSelectedPresetGlobal = -1;
  gSelectedPresetName   = kPresetName_None;

  if (auto* ui = GetUI())
    ui->SetAllControlsDirty();
}



PresetsPageControl* mOverlay = nullptr;

};

// "SELECT PRESET" button — opens PresetsPageControl overlay on click
class SelectPresetControl : public IControl
{
public:
  SelectPresetControl(const IRECT& bounds,
                      const IBitmap& buttonBitmap,
                      const IBitmap& pageBitmap,
                      const IBitmap& vocalsSelectBmp,
                      const IBitmap& padsSelectBmp,
                      const IBitmap& drumsSelectBmp,
                      const IBitmap& expSelectBmp,
                      const IBitmap& vocalsLabelBmp,
                      const IBitmap& padsLabelBmp,
                      const IBitmap& drumsLabelBmp,
                      const IBitmap& expLabelBmp,
                      const IBitmap& arrowBmp,
                      const IBitmap& revertBmp,
                      const IBitmap& dividerBmp,
                      const IBitmap& presetFirstSelectBmp,
                      const IBitmap& presetRestSelectBmp,
                      const IBitmap& presetPointerBmp)
  : IControl(bounds)
  , mButtonBitmap(buttonBitmap)
  , mPageBitmap(pageBitmap)
  , mVocalsSelectBmp(vocalsSelectBmp)
  , mPadsSelectBmp(padsSelectBmp)
  , mDrmsSelectBmp(drumsSelectBmp)
  , mExpSelectBmp(expSelectBmp)
  , mVocalsLabelBmp(vocalsLabelBmp)
  , mPadsLabelBmp(padsLabelBmp)
  , mDrumsLabelBmp(drumsLabelBmp)
  , mExpLabelBmp(expLabelBmp)
  , mArrowBmp(arrowBmp)
  , mRevertBmp(revertBmp)
  , mDividerBmp(dividerBmp)
  , mPresetFirstSelectBmp(presetFirstSelectBmp)
  , mPresetRestSelectBmp(presetRestSelectBmp)
  , mPresetPointerBmp(presetPointerBmp)
  {}

  void Draw(IGraphics& g) override
  {
    g.DrawBitmap(mButtonBitmap, mRECT);

    if (GetMouseIsOver())
    {
      IColor overlay(30, 101, 101, 101);
      DrawHoverOverlay(g, mRECT, overlay, 5.f);
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    IControl::OnMouseOver(x, y, mod);
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::HAND);
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    IControl::OnMouseOut();
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::ARROW);
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (IGraphics* ui = GetUI())
    {
      IRECT fullRect(0.f, 0.f, (float)PLUG_WIDTH, (float)PLUG_HEIGHT);

      auto* overlay = new PresetsPageControl(fullRect,
                                             mPageBitmap,
                                             mVocalsSelectBmp,
                                             mPadsSelectBmp,
                                             mDrmsSelectBmp,
                                             mExpSelectBmp,
                                             mVocalsLabelBmp,
                                             mPadsLabelBmp,
                                             mDrumsLabelBmp,
                                             mExpLabelBmp,
                                             mArrowBmp,
                                             mRevertBmp,
                                             mDividerBmp,
                                             mPresetFirstSelectBmp,
                                             mPresetRestSelectBmp,
                                             mPresetPointerBmp);
      ui->AttachControl(overlay);

      const IRECT revertRect(419.f, 538.f, 541.f, 566.f);
      auto* revertButton = new RevertButtonControl(revertRect, overlay);
      ui->AttachControl(revertButton);

      overlay->SetRevertButton(revertButton);
    }
    SetDirty(false);
  }

private:
  IBitmap mButtonBitmap, mPageBitmap;
  IBitmap mVocalsSelectBmp, mPadsSelectBmp, mDrmsSelectBmp, mExpSelectBmp;
  IBitmap mVocalsLabelBmp,  mPadsLabelBmp,  mDrumsLabelBmp,  mExpLabelBmp;
  IBitmap mArrowBmp, mRevertBmp;
  IBitmap mDividerBmp;
  IBitmap mPresetFirstSelectBmp;
  IBitmap mPresetRestSelectBmp;
  IBitmap mPresetPointerBmp;
};

// Prev/next arrow buttons — step through all presets across groups (dir: +1/-1)
class PresetStepButtonControl : public IControl
{
public:
  PresetStepButtonControl(const IRECT& bounds, const IBitmap& bmp, int dir)
  : IControl(bounds)
  , mBmp(bmp)
  , mDir(dir)
  {}

  void Draw(IGraphics& g) override
  {
    g.DrawBitmap(mBmp, mRECT);

    if (GetMouseIsOver())
    {
      // Same hover as SelectPresetControl
      IColor overlay(30, 101, 101, 101);
      DrawHoverOverlay(g, mRECT, overlay, 5.f);
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    IControl::OnMouseOver(x, y, mod);
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::HAND);
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    IControl::OnMouseOut();
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::ARROW);
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    auto* dlg = GetDelegate();
    auto* ui  = GetUI();
    if (!dlg || !ui) return;

    EPresetGroup g = gSelectedGroupGlobal;
    int idx = gSelectedPresetGlobal;

    // Step across ALL presets (carousel across groups)
    StepPresetCarousel(g, idx, mDir);

    // Update globals
    gSelectedGroupGlobal  = g;
    gSelectedPresetGlobal = idx;

    if (const char* nm = GetPresetName(g, idx))
      gSelectedPresetName = nm;
    else
      gSelectedPresetName = kPresetName_None;

    // Apply preset
    ApplyPresetToParams(dlg, g, idx);

    // Refresh UI
    ui->SetAllControlsDirty();
    SetDirty(false);
  }

private:
  IBitmap mBmp;
  int mDir = 1; // +1 next, -1 previous
};


// Bitmap toggle button with hover tint (1.0 = bypassed/OFF, 0.0 = active/ON)
class HoverButtonWithOverlay : public IControl
{
public:
  HoverButtonWithOverlay(const IRECT& bounds,
                         int paramIdx,
                         const IBitmap& offBitmap,
                         const IBitmap& onBitmap,
                         const IColor& hoverColor,
                         float cornerRadius)
  : IControl(bounds, paramIdx)
  , mParamIdx(paramIdx)
  , mOff(offBitmap), mOn(onBitmap)
  , mHoverColor(hoverColor), mCornerRadius(cornerRadius)
  {}


  void Draw(IGraphics& g) override
  {
    if (const IParam* p = GetParam())
      SetValue(p->GetNormalized());

    const bool bypass = (GetValue() >= 0.5f); // 1 = bypass ON (module disabled)
    g.DrawBitmap(bypass ? mOff : mOn, mRECT);

    if (GetMouseIsOver())
      DrawHoverOverlay(g, mRECT, mHoverColor, mCornerRadius);
  }

  void OnMouseDown(float, float, const IMouseMod&) override
  {
  // Read the live parameter value to avoid UI desync.
    double cur = GetValue();
    if (const IParam* p = GetParam())
      cur = p->GetNormalized();

    // toggle bypass: 1=bypass (off), 0=on
    const double newNorm = (cur >= 0.5) ? 0.0 : 1.0;

    SetValue(newNorm);

    if (auto* dlg = GetDelegate())
      dlg->SendParameterValueFromUI(mParamIdx, newNorm); // Sends the change to the processor.

    SetDirty(false); // Redraw only; do not resend the value.
  }


  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    IControl::OnMouseOver(x, y, mod);
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::HAND);
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    IControl::OnMouseOut();
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::ARROW);
    SetDirty(false);
  }

private:
    int mParamIdx = -1;
    IBitmap mOff, mOn;
    IColor  mHoverColor;
    float   mCornerRadius = 0.f;
};

// Value-tracking arc ring drawn behind a knob (mouse-transparent)
class KnobValueArcControl : public IControl
{
public:
  KnobValueArcControl(const IRECT& bounds,
                      int paramIdx,
                      IColor color = IColor(255, 84, 97, 114),
                      float thickness = 2.f,
                      float startDeg = 225.f,
                      float sweepDeg = 270.f)
  : IControl(bounds, paramIdx)
  , mColor(color)
  , mThickness(thickness)
  , mStartDeg(startDeg)
  , mSweepDeg(sweepDeg)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    const IParam* p = GetParam();
    if (!p) return;

    const float norm = (float) p->GetNormalized();

    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float radius = 0.5f * std::min(mRECT.W(), mRECT.H()) - (mThickness * 0.5f);
    float end = mStartDeg + norm * mSweepDeg;

  // Tiny overshoot at 100% to visually close the gap on the ring.
  // Usually 0.6–1.2 degrees is enough.
    if (norm >= 0.999f)
      end = mStartDeg + mSweepDeg + 1.5f;

    g.DrawArc(mColor, cx, cy, radius, mStartDeg, end, nullptr, mThickness);
  }

private:
  IColor mColor;
  float  mThickness;
  float  mStartDeg;
  float  mSweepDeg;
};


// Knob with hover tint, HAND cursor, rate dead-zone enforcement, and auto-gate on change
class HoverKnobRotaterControl : public IBKnobRotaterControl
{
public:
  HoverKnobRotaterControl(const IRECT& bounds,
                          const IBitmap& knobBmp,
                          int paramIdx,
                          const IColor& hoverColor,
                          float cornerRadius,
                          bool drawOverlay,
                          bool drawValueArc = false,
                          float arcPad = 0.f,
                          float knobInnerPad = 0.f,
                          float arcThickness = 2.f,
                          IColor arcColor = IColor(255, 0x50, 0x62, 0x74),
                          float arcStartDeg = 225.f,
                          float arcSweepDeg = 270.f)
  : IBKnobRotaterControl(bounds, knobBmp, paramIdx)
  , mParamIdxLocal(paramIdx)
  , mHoverColor(hoverColor)
  , mCornerRadius(cornerRadius)
  , mDrawOverlay(drawOverlay)
  , mDrawValueArc(drawValueArc)
  , mArcPad(arcPad)
  , mKnobInnerPad(knobInnerPad)
  , mArcThickness(arcThickness)
  , mArcColor(arcColor)
  , mArcStartDeg(arcStartDeg)
  , mArcSweepDeg(arcSweepDeg)
  {}

  void Draw(IGraphics& g) override
  {
    constexpr double kStart = 0.0;
    constexpr double kEnd   = 270.0;

    // Use the param's normalized value to draw, so preset changes always update knob position
    double norm = GetValue();
    if (const IParam* p = GetParam())
      norm = p->GetNormalized();

    const double angle = kStart + norm * (kEnd - kStart);

    // 1) draw the knob bitmap
    const IRECT knobR = (mKnobInnerPad != 0.f) ? mRECT.GetPadded(-mKnobInnerPad) : mRECT;
    g.DrawRotatedBitmap(mBitmap, knobR.MW(), knobR.MH(), angle, &mBlend);

    // 2) draw value arc ON TOP (so it won't be hidden if your knob bitmap contains the ring)
    if (mDrawValueArc)
    {
      const float t = (float) norm;

      // Ring rect is the control bounds (optionally padded)
      const IRECT arcR = (mArcPad != 0.f) ? mRECT.GetPadded(mArcPad) : mRECT;

      const float cx     = arcR.MW();
      const float cy     = arcR.MH();
      const float radius = 0.5f * std::min(arcR.W(), arcR.H()) - (mArcThickness * 0.5f);

      // NOTE: IGraphics::PathArc expects angles in DEGREES (not radians).
      const float start = mArcStartDeg;
      const float end   = mArcStartDeg + (mArcSweepDeg * t);

      if (t > 0.f)
      {
        g.PathClear();
        g.PathArc(cx, cy, radius, start, end);
        g.PathStroke(mArcColor, mArcThickness);
      }
    }

    // 3) hover overlay (optional)
    if (mDrawOverlay && GetMouseIsOver())
      DrawHoverOverlay(g, mRECT, mHoverColor, mCornerRadius);
  }

  void OnMouseOver(float x, float y, const IMouseMod& m) override
  {
    IBKnobRotaterControl::OnMouseOver(x, y, m);
    if (auto* ui = GetUI())
      ui->SetMouseCursor(ECursor::HAND);
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    IBKnobRotaterControl::OnMouseOut();
    if (auto* ui = GetUI())
      ui->SetMouseCursor(ECursor::ARROW);
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    // Sync control value with actual param value before drag starts,
    // so the knob doesn't jump after a preset change
    if (const IParam* p = GetParam())
      SetValue(p->GetNormalized());

    IBKnobRotaterControl::OnMouseDown(x, y, mod);
    if (auto* ui = GetUI())
      ui->SetMouseCursor(ECursor::HAND);
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    IBKnobRotaterControl::OnMouseUp(x, y, mod);
    if (auto* ui = GetUI())
      ui->SetMouseCursor(ECursor::ARROW);
    SetDirty(false);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    static constexpr float kDragTightness = 0.45f; // knob tightness
    
    IBKnobRotaterControl::OnMouseDrag(x, y, dX, dY * kDragTightness, mod);

    EnforceRateDeadZone();

    if (auto* dlg = GetDelegate())
      AutoGateModulesFromParams(dlg, mParamIdxLocal);

    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();

    SetDirty(true);
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    IBKnobRotaterControl::OnMouseWheel(x, y, mod, d);
    
    EnforceRateDeadZone();

    if (auto* dlg = GetDelegate())
      AutoGateModulesFromParams(dlg, mParamIdxLocal);

    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();

    SetDirty(true);
  }

private:
  void EnforceRateDeadZone()
  {
    if (!IsRateParamIdx(mParamIdxLocal))
      return;

    auto* dlg = GetDelegate();
    const IParam* p = GetParam();
    if (!dlg || !p)
      return;

    // Convert current normalized -> plain Hz, apply dead zone, then push back if needed
    const double hz = p->FromNormalized(GetValue());
    const double fixedHz = ApplyRateDeadZone(mParamIdxLocal, hz);

    if (fixedHz == hz)
      return;

    const double fixedNorm = p->ToNormalized(fixedHz);
    SetValue(fixedNorm);
    dlg->SendParameterValueFromUI(mParamIdxLocal, fixedNorm);

    // Redraw immediately
    SetDirty(false);
    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();
  }

private:
  int    mParamIdxLocal = -1;
  IColor mHoverColor;
  float  mCornerRadius = 0.f;
  bool   mDrawOverlay  = true;

  // Value arc (big knobs only)
  bool   mDrawValueArc = false;
  float  mArcPad = 0.f;
  float  mArcThickness = 2.f;
  float  mKnobInnerPad = 0.f;
  IColor mArcColor = IColor(255, 0x50, 0x62, 0x74);
  float  mArcStartDeg = 225.f;
  float  mArcSweepDeg = 270.f;
};


// Stereo output level bars + dB readout (green→yellow→red, fast attack / slow release)
class StereoLevelMeterControl : public IControl
{
public:
  StereoLevelMeterControl(const IRECT& bounds, DynaCore* plugin)
  : IControl(bounds), mPlugin(plugin)
  { mIgnoreMouse = true; }

  void Draw(IGraphics& g) override
  {
    if (!mPlugin) return;

    // Detect if DSP stopped updating (playback stopped)
    uint64_t currentCount = mPlugin->GetMeterUpdateCount();
    if (currentCount != mLastUpdateCount) {
      mStaleFrames = 0;
      mLastUpdateCount = currentCount;
    } else {
      mStaleFrames++;
    }
    // Only consider stale after 10+ consecutive Draw calls with no DSP update (~170ms)
    bool dspStale = (mStaleFrames > 10);
    bool bypassed = mPlugin->GetParam(kBypass)->Bool();
    bool forceZero = dspStale || bypassed;

    const double rawDbL = forceZero ? -100.0 : mPlugin->GetOutputLevelDBL();
    const double rawDbR = forceZero ? -100.0 : mPlugin->GetOutputLevelDBR();

    // Keep raw dB for text display (unclamped, can be > 0)
    double rawMaxDB = std::max(rawDbL, rawDbR);

    auto dbToNorm = [](double db) -> float {
      double n = (db - (-54.0)) / (0.0 - (-54.0));
      if (n < 0.0) n = 0.0;
      if (n > 1.0) n = 1.0;
      return static_cast<float>(n);
    };

    float targetL = dbToNorm(rawDbL);
    float targetR = dbToNorm(rawDbR);

    // Visual smoothing — fast rise, slow fall (same speed for stop/bypass/normal decay)
    auto smoothVal = [](float current, float target) -> float {
      float diff = target - current;
      float rate = (diff > 0.f) ? 0.18f : 0.07f; // fast attack, slow release always
      return current + diff * rate;
    };
    mVisualL = smoothVal(mVisualL, targetL);
    mVisualR = smoothVal(mVisualR, targetR);
    if (mVisualL < 0.002f) mVisualL = 0.f;
    if (mVisualR < 0.002f) mVisualR = 0.f;

    float normL = mVisualL;
    float normR = mVisualR;

    // Bar coordinates (-0.1px left from previous)
    constexpr float kBarTop = 78.f, kBarBottom = 466.f;
    constexpr float kLeftBarL = 955.6f, kLeftBarR = 963.6f;
    constexpr float kRightBarL = 965.6f, kRightBarR = 973.6f;
    constexpr float kBarH = kBarBottom - kBarTop;

    // Pixel-by-pixel rendering — monolithic bars
    auto drawBar = [&](float barL, float barR, float norm) {
      if (norm <= 0.001f) return;
      float fillH = kBarH * norm;
      float fillTop = kBarBottom - fillH;
      int yStart = static_cast<int>(fillTop);
      int yEnd   = static_cast<int>(kBarBottom);
      for (int py = yStart; py < yEnd; py++) {
        float pos = (kBarBottom - static_cast<float>(py)) / kBarH;
        int r, gr, b;
        if (pos < 0.55f) {
          float t = pos / 0.55f;
          r = int(76 + t * (180 - 76));
          gr = int(209 + t * (220 - 209));
          b = int(55 + t * (20 - 55));
        } else if (pos < 0.85f) {
          float t = (pos - 0.55f) / 0.30f;
          r = int(180 + t * (241 - 180));
          gr = int(220 + t * (196 - 220));
          b = int(20 + t * (15 - 20));
        } else {
          float t = (pos - 0.85f) / 0.15f;
          r = int(241 + t * (237 - 241));
          gr = int(196 + t * (67 - 196));
          b = int(15 + t * (55 - 15));
        }
        g.FillRect(IColor(255, r, gr, b),
                   IRECT(barL, static_cast<float>(py), barR, static_cast<float>(py + 1)));
      }
    };

    drawBar(kLeftBarL, kLeftBarR, normL);
    drawBar(kRightBarL, kRightBarR, normR);

    // Numeric dB display — uses raw dB (unclamped, shows > 0dB correctly)
    double targetTextDB = (rawMaxDB > -54.0) ? rawMaxDB : -100.0;
    // Extra slow smoothing for stable text readout
    if (targetTextDB > mTextDB)
      mTextDB += (targetTextDB - mTextDB) * 0.030; // slow attack
    else
      mTextDB += (targetTextDB - mTextDB) * 0.016; // very slow release

    char buf[32];
    float maxVis = std::max(mVisualL, mVisualR);
    if (maxVis < 0.001f && mTextDB < -53.0) {
      std::snprintf(buf, sizeof(buf), " -inf");
    } else {
      // Snap tiny negative values to 0.0 to prevent "-0.0" display artefact
      double displayDB = (mTextDB > -0.05 && mTextDB < 0.0) ? 0.0 : mTextDB;
      std::snprintf(buf, sizeof(buf), "%5.1f", displayDB); // "  0.0", " -3.2", "-12.4"
    }

    IRECT textRect(mRECT.L + 5.5f, mRECT.B - 46.f, mRECT.R + 9.5f, mRECT.B - 28.f);
    g.DrawText(mDbText, buf, textRect);

    SetDirty(false);
  }

private:
  DynaCore* mPlugin = nullptr;
  float mVisualL = 0.f;
  float mVisualR = 0.f;
  double mTextDB = -100.0; // smoothed dB for text display
  const IText mDbText{14.f, COLOR_WHITE, "Inter-Semi-Bold", EAlign::Center, EVAlign::Middle};
  uint64_t mLastUpdateCount = 0;
  int mStaleFrames = 100;
};

// GR meter bars — top-to-bottom (0 dB at top, 24 dB at bottom), effective GR after mix
class GRMeterControl : public IControl
{
public:
  GRMeterControl(const IRECT& bounds, DynaCore* plugin)
  : IControl(bounds), mPlugin(plugin)
  { mIgnoreMouse = true; }

  void Draw(IGraphics& g) override
  {
    if (!mPlugin) return;

    // Detect if DSP stopped updating
    uint64_t currentCount = mPlugin->GetGRUpdateCount();
    if (currentCount != mLastUpdateCount) {
      mStaleFrames = 0;
      mLastUpdateCount = currentCount;
    } else {
      mStaleFrames++;
    }
    bool dspStale = (mStaleFrames > 10);
    bool bypassed = mPlugin->GetParam(kBypass)->Bool();
    bool compBypassed = mPlugin->GetParam(kCompBypass)->Bool();
    bool forceZero = dspStale || bypassed || compBypassed;

    double grL = forceZero ? 0.0 : mPlugin->GetGainReductionL();
    double grR = forceZero ? 0.0 : mPlugin->GetGainReductionR();

    // Map GR dB to 0..1 (0dB=0, 24dB=1)
    float targetL = static_cast<float>(std::min(grL / 24.0, 1.0));
    float targetR = static_cast<float>(std::min(grR / 24.0, 1.0));
    if (targetL < 0.f) targetL = 0.f;
    if (targetR < 0.f) targetR = 0.f;

    // Visual smoothing — fast attack, slow release
    auto smoothVal = [](float current, float target) -> float {
      float diff = target - current;
      float rate = (diff > 0.f) ? 0.18f : 0.07f;
      return current + diff * rate;
    };
    mVisualL = smoothVal(mVisualL, targetL);
    mVisualR = smoothVal(mVisualR, targetR);
    if (mVisualL < 0.002f) mVisualL = 0.f;
    if (mVisualR < 0.002f) mVisualR = 0.f;
    // Snap to full when sustained near maximum — asymptotic smoothing never reaches 1.0 exactly
    if (mVisualL > 0.96f && targetL >= 0.999f) mVisualL = 1.0f;
    if (mVisualR > 0.96f && targetR >= 0.999f) mVisualR = 1.0f;

    // Bar coordinates — 10px wide bars inside GR rail slots
    // kBarBottom slightly past the 24dB mark so max compression fills the slot
    constexpr float kBarTop    = 499.f, kBarBottom = 559.f;
    constexpr float kLeftBarL  = 416.f, kLeftBarR  = 426.f;  // 10px, center 421
    constexpr float kRightBarL = 452.f, kRightBarR = 462.f;  // 10px, center 457
    constexpr float kBarH      = kBarBottom - kBarTop;       // 60px total rail height
    constexpr float cr         = 5.f;                        // = half bar width

    // Monolithic capsule bars — full opacity, rounded top and bottom.
    // Skip below 2*cr to avoid the NanoVG circle/blob artefact.
    auto drawBar = [&](float barL, float barR, float norm) {
      if (norm <= 0.001f) return;
      float fillH = kBarH * norm;
      if (fillH <= 2.f * cr) return;
      g.FillRoundRect(IColor(255, 211, 222, 242),
                      IRECT(barL, kBarTop, barR, kBarTop + fillH), cr);
    };

    drawBar(kLeftBarL, kLeftBarR, mVisualL);
    drawBar(kRightBarL, kRightBarR, mVisualR);

    SetDirty(false);
  }

private:
  DynaCore* mPlugin = nullptr;
  float mVisualL = 0.f;
  float mVisualR = 0.f;
  uint64_t mLastUpdateCount = 0;
  int mStaleFrames = 100;
};

// Value label under big Rate/Depth knobs (rate → "X.XXHz", depth → "X%")
class BigKnobValueTextControl : public IControl
{
public:
  BigKnobValueTextControl(const IRECT& bounds, int paramIdx)
  : IControl(bounds)
  , mParamIdx(paramIdx)
  {
    mIgnoreMouse = true;
    
    // Widen only Rate labels so "20.00Hz" fits
    if (IsRateParamIdx(mParamIdx))
    {
      const float extra = bounds.W() * 0.15f;
      mRECT.L -= extra * 0.5f;
      mRECT.R += extra * 0.5f;
    }
  }

  void Draw(IGraphics& g) override
  {
    auto* dlg = GetDelegate();
    if (!dlg || mParamIdx < 0 || mParamIdx >= dlg->NParams())
      return;

    IParam* p = dlg->GetParam(mParamIdx);
    if (!p)
      return;

    char buf[32];

    if (IsRateParamIdx(mParamIdx))
    {
      double hz = p->Value();
      hz = ApplyRateDeadZone(mParamIdx, hz);
      std::snprintf(buf, sizeof(buf), "%.2fHz", hz);
    }
    else
    {
      // Depth knobs are 0..100, show as percent
      const double percent = p->Value();
      std::snprintf(buf, sizeof(buf), "%.0f%%", percent);
    }

    g.DrawText(mValueText, buf, mRECT);
    SetDirty(false);
  }

private:
  int mParamIdx = -1;
  const IText mValueText{17.f, IColor(255, 53, 66, 80), "Inter-Regular", EAlign::Center, EVAlign::Middle};
};


// Value label under mid/small knobs — format varies per param (dB, ratio, Hz, %)
class MidKnobValueTextControl : public IControl
{
public:
  // IMPORTANT: Must match pGraphics->LoadFont("...", ...)
  static constexpr const char* kValueFont = "Inter-Regular";

  MidKnobValueTextControl(const IRECT& bounds, int paramIdx)
  : MidKnobValueTextControl(bounds, paramIdx,
                            IText(12.f, IColor(255, 230, 230, 230), kValueFont))
  {}

  MidKnobValueTextControl(const IRECT& bounds, int paramIdx, const IText& text)
  : IControl(bounds, paramIdx)
  , mText(text)
  , mParamIdx(paramIdx)
  {}

  void Draw(IGraphics& g) override
  {
    const IParam* p = GetParam();
    if (!p) return;

    WDL_String s;

    // Special formatting for compressor parameters
    if (mParamIdx == kCompThreshold)
    {
      // Display threshold in dB
      s.SetFormatted(64, "%.1f dB", p->Value());
    }
    else if (mParamIdx == kCompGain)
    {
      // Display makeup gain in dB
      s.SetFormatted(64, "%.1f dB", p->Value());
    }
    else if (mParamIdx == kCompRatio)
    {
      // Display ratio as X:1
      const double r = p->Value();

      // If nearly an integer, show without decimals
      const double ri = std::round(r);
      if (std::fabs(r - ri) < 1e-6)
        s.SetFormatted(64, "%.0f:1", ri);
      else
        s.SetFormatted(64, "%.1f:1", r);
    }
    else if (IsRateParamIdx(mParamIdx))
    {
      // Display rate in Hz
      s.SetFormatted(64, "%.2f Hz", p->Value());
    }
    else if (mParamIdx == kOutputLevel)
    {
      // Display output gain in dB (center = 0 dB)
      const double db = p->Value();

      if (std::fabs(db) < 1e-6)
        s.SetFormatted(64, "0.0 dB");
      else
        s.SetFormatted(64, "%+.1f dB", db); // + for positive values
    }
    else if (mParamIdx == kWidth)
    {
      // Display width directly as percent (0-200%)
      s.SetFormatted(64, "%.0f%%", p->Value());
    }
    else
    {
      // Default display: percent based on normalized value
      const int pct = (int) std::lround(p->GetNormalized() * 100.0);
      s.SetFormatted(64, "%d%%", pct);
    }

    g.DrawText(mText, s.Get(), mRECT);
  }
  
private:
  IText mText;
  int   mParamIdx = -1;
};



// Displays the active preset name top-left (or "SELECT PRESET" if none)
class PresetNameTextControl : public IControl
{
public:
  PresetNameTextControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    const bool hasPreset = (gSelectedPresetGlobal >= 0);
    const char* text = hasPreset ? gSelectedPresetName.c_str() : "SELECT PRESET";

    g.DrawText(mPresetText, text, mRECT);
    SetDirty(false);
  }

private:
  const IText mPresetText{10.f, IColor(255, 171, 171, 171), "Inter-Medium", EAlign::Near, EVAlign::Middle};
};

DynaCore::DynaCore(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // --- MAIN GAIN ---
  GetParam(kGain)->InitDouble("Gain", 0.0, 0.0, 100.0, 0.01, "%");

  // Tremolo (zero start/min values)
  GetParam(kTremRate)->InitDouble ("Trem Rate",  0.0, 0.0, 20.0, 0.01, "Hz");
  GetParam(kTremDepth)->InitDouble("Trem Depth", 0.0, 0.0, 100.0, 1.0,  "%");
  GetParam(kTremBypass)->InitBool ("Trem Bypass", true);

  // Pan
  GetParam(kPanRate)->InitDouble  ("Pan Rate",   0.0, 0.0, 10.0, 0.01, "Hz");
  GetParam(kPanDepth)->InitDouble ("Pan Depth",  0.0, 0.0, 100.0, 1.0,  "%");
  GetParam(kPanBypass)->InitBool  ("Pan Bypass", true);

  // Pitch
  GetParam(kPitchRate)->InitDouble ("Pitch Rate",  0.0, 0.0, 12.0, 0.01, "Hz");
  GetParam(kPitchDepth)->InitDouble("Pitch Depth", 0.0, 0.0, 100.0, 1.0,  "%");
  GetParam(kPitchBypass)->InitBool ("Pitch Bypass", true);

  // Phaser
  GetParam(kPhaserRate)->InitDouble ("Phaser Rate",  0.0, 0.0, 10.0, 0.01, "Hz");
  GetParam(kPhaserDepth)->InitDouble("Phaser Depth", 0.0, 0.0, 100.0, 1.0,  "%");
  GetParam(kPhaserBypass)->InitBool ("Phaser Bypass", true);

  // Compressor
  GetParam(kCompMix)->InitDouble      ("Comp Mix",       100.0,   0.0, 100.0, 1.0, "%");
  GetParam(kCompThreshold)->InitDouble("Comp Threshold", -25.0, -50.0,   0.0, 0.1, "dB");
  GetParam(kCompRatio)->InitDouble    ("Comp Ratio",     2.0,   1.0,  20.0, 0.1, ":1");
  GetParam(kCompGain)->InitDouble     ("Comp Gain",      0.0, -20.0,  20.0, 0.1, "dB");
  GetParam(kCompAttack)->InitDouble   ("Comp Attack",    10.0,   0.0, 100.0, 0.1, "ms");
  GetParam(kCompRelease)->InitDouble  ("Comp Release",   120.0,   0.0, 1000.0, 1.0, "ms");
  GetParam(kCompBypass)->InitBool     ("Comp Bypass",    false); // false = ACTIVE (not bypassed)

  // Mastering
  GetParam(kWidth)->InitDouble("Width", 100.0, 0.0, 200.0, 0.1, "%");

  // Master / Output
  GetParam(kBypass)->InitBool("Bypass", false);
  GetParam(kMasterIntensity)->InitDouble("Master Intensity", 100.0, 0.0, 100.0, 1.0, "%");
  GetParam(kOutputLevel)->InitDouble("Output Level", 0.0, -20.0, 20.0, 0.1, "dB");

  // init params to defaults so the plugin opens in a known state
  ApplyDefaultPresetParams([this](int pIdx, double plain)
  {
    if (auto* p = GetParam(pIdx))
      p->Set(plain);
  });

  gSelectedGroupGlobal  = EPresetGroup::None;
  gSelectedPresetGlobal = -1;
  gSelectedPresetName   = kPresetName_None;

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->EnableMouseOver(true);
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_BLACK);
    pGraphics->LoadFont("Inter-Regular",   INTER_REGULAR_FN);
    pGraphics->LoadFont("Inter-Semi-Bold", INTER_SEMI_BOLD_FN);
    pGraphics->LoadFont("Inter-Medium",    INTER_MEDIUM_FN);

    IBitmap bg = pGraphics->LoadBitmap(MAIN_BACKGROUND_FN, 1);
    const IRECT bounds = pGraphics->GetBounds();
    pGraphics->AttachControl(new IBitmapControl(bounds, bg));

    const IColor hoverColorButtons  = IColor(23, 184, 184, 184);
    const IColor hoverColorModules  = IColor(23, 14,  14,  14);
    const IColor hoverColorKnobs    = IColor(18, 184, 184, 184);

    // COMPRESSOR ON/OFF (moved to new position near cyan indicator)
    IRECT compBtnRect(459.f, 447.f, 480.f, 467.f);
    IBitmap bmpOff = pGraphics->LoadBitmap(COMP_OFF_FN, 1);
    IBitmap bmpOn  = pGraphics->LoadBitmap(COMP_ON_FN,  1);
    pGraphics->AttachControl(new HoverButtonWithOverlay(
      compBtnRect, kCompBypass, bmpOff, bmpOn, hoverColorButtons, 5.f));

    // BYPASS (global)
    IRECT bypassBtnRect(836.f, 74.f, 898.f, 94.f);
    IBitmap bmpBypOff = pGraphics->LoadBitmap(BYPASS_OFF_FN, 1);
    IBitmap bmpBypOn  = pGraphics->LoadBitmap(BYPASS_ON_FN,  1);
    pGraphics->AttachControl(new HoverButtonWithOverlay(
      bypassBtnRect, kBypass, bmpBypOn, bmpBypOff, hoverColorButtons, 3.f));

    // 4 MODULE TOGGLES
    IBitmap modOff = pGraphics->LoadBitmap(MODULE_OFF_FN, 1);
    IBitmap modOn  = pGraphics->LoadBitmap(MODULE_ON_FN,  1);

    pGraphics->AttachControl(new HoverButtonWithOverlay(
      IRECT(126.f,390.f,145.f,408.f), kTremBypass,  modOff, modOn, hoverColorModules, 5.f));
    pGraphics->AttachControl(new HoverButtonWithOverlay(
      IRECT(344.f,390.f,363.f,408.f), kPanBypass,   modOff, modOn, hoverColorModules, 5.f));
    pGraphics->AttachControl(new HoverButtonWithOverlay(
      IRECT(560.f,390.f,579.f,408.f), kPitchBypass, modOff, modOn, hoverColorModules, 5.f));
    pGraphics->AttachControl(new HoverButtonWithOverlay(
      IRECT(776.f,390.f,795.f,408.f), kPhaserBypass,modOff, modOn, hoverColorModules, 5.f));

    // SELECT PRESET + OVERLAY
    IBitmap presetBtnBmp   = pGraphics->LoadBitmap(SELECT_PRESET_FN, 1);
    IBitmap presetsPageBmp = pGraphics->LoadBitmap(PRESETS_PAGE_FN,  1);

    IBitmap vocalsSelectBmp = pGraphics->LoadBitmap(PRESET_GROUP_VOCALS_SELECT_FN, 1);
    IBitmap padsSelectBmp   = pGraphics->LoadBitmap(PRESET_GROUP_PADS_SELECT_FN,   1);
    IBitmap drumsSelectBmp  = pGraphics->LoadBitmap(PRESET_GROUP_DRUMS_SELECT_FN,  1);
    IBitmap expSelectBmp    = pGraphics->LoadBitmap(PRESET_GROUP_EXP_SELECT_FN,    1);

    IBitmap vocalsLabelBmp  = pGraphics->LoadBitmap(PRESET_VOCALS_LABEL_FN, 1);
    IBitmap padsLabelBmp    = pGraphics->LoadBitmap(PRESET_PADS_LABEL_FN,   1);
    IBitmap drumsLabelBmp   = pGraphics->LoadBitmap(PRESET_DRUMS_LABEL_FN,   1);
    IBitmap expLabelBmp     = pGraphics->LoadBitmap(PRESET_EXP_LABEL_FN,    1);

    IBitmap arrowBmp        = pGraphics->LoadBitmap(PRESET_GROUP_SELECT_ARROW_FN, 1);
    IBitmap revertBmp       = pGraphics->LoadBitmap(REVERT_TO_DEFAULT_FN,        1);

    IBitmap dividerBmp      = pGraphics->LoadBitmap(PRESET_DIVIDER_FN, 1);

    IBitmap presetFirstSelectBmp = pGraphics->LoadBitmap(PRESET_FROM_GROUP_SELECT_FIRST_FN, 1);
    IBitmap presetRestSelectBmp  = pGraphics->LoadBitmap(PRESET_FROM_GROUP_SELECT_REST_FN,  1);

    IBitmap presetPointerBmp = pGraphics->LoadBitmap(PRESET_POINTER_FN, 1);
    
    IBitmap nextPresetBmp = pGraphics->LoadBitmap(NEXT_PRESET_FN, 1);
    IBitmap prevPresetBmp = pGraphics->LoadBitmap(PREVIOUS_PRESET_FN, 1);

    IRECT preset_bounds(27.f, 71.f, 272.f, 97.f);
    pGraphics->AttachControl(new SelectPresetControl(
      preset_bounds,
      presetBtnBmp,
      presetsPageBmp,
      vocalsSelectBmp,
      padsSelectBmp,
      drumsSelectBmp,
      expSelectBmp,
      vocalsLabelBmp,
      padsLabelBmp,
      drumsLabelBmp,
      expLabelBmp,
      arrowBmp,
      revertBmp,
      dividerBmp,
      presetFirstSelectBmp,
      presetRestSelectBmp,
      presetPointerBmp));

    // ===== Preset Name text at X:55, Y:85 =====
    {
      const float x = 54.f, y = 85.f;
      IRECT nameRect(x, y - 6.f, x + 220.f, y + 6.f);
      pGraphics->AttachControl(new PresetNameTextControl(nameRect));
    }
    
    // Position by top-left (X,Y). Size from bitmap.
    {
      const float prevX = 277.f, prevY = 73.f;
      const float nextX = 301.f, nextY = 73.f;

      IRECT prevR(prevX, prevY, prevX + (float)prevPresetBmp.W(), prevY + (float)prevPresetBmp.H());
      IRECT nextR(nextX, nextY, nextX + (float)nextPresetBmp.W(), nextY + (float)nextPresetBmp.H());

      // dir: -1 = previous, +1 = next
      pGraphics->AttachControl(new PresetStepButtonControl(prevR, prevPresetBmp, -1));
      pGraphics->AttachControl(new PresetStepButtonControl(nextR, nextPresetBmp, +1));
    }

    // ===== KNOBS =====
    IBitmap bigKnob   = pGraphics->LoadBitmap(BIG_KNOB_FN,   1);
    IBitmap midKnob   = pGraphics->LoadBitmap(MID_KNOB_FN,   1);
    IBitmap smallKnob = pGraphics->LoadBitmap(SMALL_KNOB_FN, 1);

    const float kBigW = 29.f, kBigH = 29.f;

    // Tremolo
    IRECT tremRateRect  (65.5f,  325.5f, 65.5f  + kBigW, 325.5f + kBigH);
    IRECT tremDepthRect (180.5f, 325.5f, 180.5f + kBigW, 325.5f + kBigH);

    // Pan Motion
    IRECT panRateRect   (281.5f, 325.5f, 281.5f + kBigW, 325.5f + kBigH);
    IRECT panDepthRect  (398.5f, 325.5f, 398.5f + kBigW, 325.5f + kBigH);

    // Pitch Drift
    IRECT pitchRateRect (497.5f, 325.5f, 497.5f + kBigW, 325.5f + kBigH);
    IRECT pitchDepthRect(614.5f, 325.5f, 614.5f + kBigW, 325.5f + kBigH);

    // Phaser
    IRECT phaserRateRect (713.5f, 325.5f, 713.5f + kBigW, 325.5f + kBigH);
    IRECT phaserDepthRect(830.5f, 325.5f, 830.5f + kBigW, 325.5f + kBigH);
    
    // ------------------------------------------------------
    //  Values under large Rate/Depth knobs (percentages)
    // ------------------------------------------------------
    const float labelY = 400.f;

    auto MakeLabelRect = [](float cx, float cy)
    {
      const float halfW = 24.f;
      const float halfH = 8.f;
      return IRECT(cx - halfW, cy - halfH, cx + halfW, cy + halfH);
    };

    // Tremolo
    pGraphics->AttachControl(
      new BigKnobValueTextControl(MakeLabelRect(80.f,  labelY), kTremRate));
    pGraphics->AttachControl(
      new BigKnobValueTextControl(MakeLabelRect(197.f, labelY), kTremDepth));

    // Pan
    pGraphics->AttachControl(
      new BigKnobValueTextControl(MakeLabelRect(296.f, labelY), kPanRate));
    pGraphics->AttachControl(
      new BigKnobValueTextControl(MakeLabelRect(415.f, labelY), kPanDepth));

    // Pitch
    pGraphics->AttachControl(
      new BigKnobValueTextControl(MakeLabelRect(512.f, labelY), kPitchRate));
    pGraphics->AttachControl(
      new BigKnobValueTextControl(MakeLabelRect(632.f, labelY), kPitchDepth));

    // Phaser
    pGraphics->AttachControl(
      new BigKnobValueTextControl(MakeLabelRect(728.f, labelY), kPhaserRate));
    pGraphics->AttachControl(
      new BigKnobValueTextControl(MakeLabelRect(847.f, labelY), kPhaserDepth));


    // Big knobs — arc ring + rotating cap, no hover overlay
    auto AttachBigKnobWithArc = [&](const IRECT& knobRect, int paramIdx)
    {
      const IRECT ringRect = knobRect.GetPadded(6.4f);
      pGraphics->AttachControl(new KnobValueArcControl(
        ringRect, paramIdx, IColor(210, 0x50, 0x62, 0x74), 2.f, 225.f, 270.f));
      pGraphics->AttachControl(new HoverKnobRotaterControl(
        knobRect, bigKnob, paramIdx, hoverColorKnobs, 10.f, false));
    };
    
    auto AttachMidKnobWithArc = [&](const IRECT& knobRect, int paramIdx)
    {
      const IRECT ringRect = knobRect.GetPadded(5.7f);
      pGraphics->AttachControl(new KnobValueArcControl(
        ringRect, paramIdx, IColor(245, 171, 171, 171), 2.f, 222.f, 270.f));
      pGraphics->AttachControl(new HoverKnobRotaterControl(
        knobRect, midKnob, paramIdx, hoverColorKnobs, 10.f, true));
    };

    auto AttachSmallKnobWithArc = [&](const IRECT& knobRect, int paramIdx)
    {
      const IRECT ringRect = knobRect.GetPadded(4.9f);
      pGraphics->AttachControl(new KnobValueArcControl(
        ringRect, paramIdx, IColor(245, 171, 171, 171), 2.f, 223.f, 270.f));
      pGraphics->AttachControl(new HoverKnobRotaterControl(
        knobRect, smallKnob, paramIdx, hoverColorKnobs, 10.f, true));
    };

    AttachBigKnobWithArc(tremRateRect,   kTremRate);
    AttachBigKnobWithArc(tremDepthRect,  kTremDepth);
    AttachBigKnobWithArc(panRateRect,    kPanRate);
    AttachBigKnobWithArc(panDepthRect,   kPanDepth);
    AttachBigKnobWithArc(pitchRateRect,  kPitchRate);
    AttachBigKnobWithArc(pitchDepthRect, kPitchDepth);
    AttachBigKnobWithArc(phaserRateRect, kPhaserRate);
    AttachBigKnobWithArc(phaserDepthRect,kPhaserDepth);

    // MID / SMALL knobs with hover overlay enabled
    const float kMidW = 19.f,  kMidH = 19.f;
    const float kSmW  = 14.f,  kSmH  = 14.f;

    // COMP: Mix / Threshold / Ratio / Gain (mid)
    IRECT compMixRect     ( 47.f,   512.f,  47.f + kMidW,  512.f + kMidH);
    IRECT compThreshRect  (127.5f, 512.f, 127.5f + kMidW, 512.f + kMidH);
    IRECT compRatioRect   (203.5f, 512.f, 203.5f + kMidW, 512.f + kMidH);
    IRECT compGainRect    (274.5f, 513.f, 274.5f + kMidW, 513.f + kMidH);

    AttachMidKnobWithArc(compMixRect, kCompMix);
    AttachMidKnobWithArc(compThreshRect, kCompThreshold);
    AttachMidKnobWithArc(compRatioRect, kCompRatio);
    AttachMidKnobWithArc(compGainRect, kCompGain);

    // COMP: Attack / Release (small)
    IRECT compAttackRect  (351.2f, 492.2f, 351.2f + kSmW, 492.2f + kSmH);
    IRECT compReleaseRect (351.2f, 540.2f, 351.2f + kSmW, 540.2f + kSmH);

    AttachSmallKnobWithArc(compAttackRect, kCompAttack);
    AttachSmallKnobWithArc(compReleaseRect, kCompRelease);

    // MASTERING: Width (mid) — in MASTERING section
    IRECT widthRect(532.5f, 512.f, 532.5f + kMidW, 512.f + kMidH);
    AttachMidKnobWithArc(widthRect, kWidth);

    // MASTERING: Intensity (mid) — moved to MASTERING section
    IRECT masterIntRect(617.5f, 512.f, 617.5f + kMidW, 512.f + kMidH);
    AttachMidKnobWithArc(masterIntRect, kMasterIntensity);

    // OUTPUT: Level (mid)
    IRECT outLevelRect(954.5f, 527.2f, 954.5f + kMidW, 527.2f + kMidH);
    AttachMidKnobWithArc(outLevelRect, kOutputLevel);

    // ====== Value labels for MID knobs (COMP + MASTER) ======
    {
      const float midLabelY = 565.f;

      auto MakeMidRect = [](float cx, float cy)
      {
        const float halfW = 22.f;
        const float halfH = 7.f;
        return IRECT(cx - halfW, cy - halfH, cx + halfW, cy + halfH);
      };

      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(56.f, midLabelY), kCompMix));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(137.f, midLabelY), kCompThreshold));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(213.f, midLabelY), kCompRatio));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(284.f, midLabelY), kCompGain));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(542.f, midLabelY), kWidth));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(627.f, midLabelY), kMasterIntensity));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(963.f, midLabelY + 14), kOutputLevel));
    }

    // GR (Gain Reduction) meter in compression section
    IRECT grMeterRect(412.f, 491.f, 472.f, 572.f);  // generous margins: bars at x=416..462, y=499..559
    pGraphics->AttachControl(new GRMeterControl(grMeterRect, this));

    // Stereo output level meter (two bars + dB readout)
    IRECT meterRect(930.f, 72.f, 978.f, 525.f);
    pGraphics->AttachControl(new StereoLevelMeterControl(meterRect, this));
  };
#endif
}

#if IPLUG_DSP

// ---------------------------------------------------------------------------
// DSP constants and inline helpers
// ---------------------------------------------------------------------------
namespace
{
  constexpr double kPi    = 3.14159265358979323846;
  constexpr double kTwoPi = 6.283185307179586476925286766559;
  constexpr double kSqrt2 = 1.4142135623730951;         ///< √2 for constant-power panning

  /// exp(x * kLn10Over20) ≡ pow(10, x/20) — avoids expensive pow() per sample.
  static const double kLn10Over20 = std::log(10.0) / 20.0;

  /**
   * @brief First-order allpass filter in lattice form.
   *
   * Implements:  v[n] = x[n] − a·v[n−1]  ;  y[n] = a·v[n] + v[n−1]
   *
   * @param input  Current input sample.
   * @param coeff  Allpass coefficient (from AllpassCoeff).
   * @param state  Filter state variable (updated in-place).
   * @return Filtered output sample.
   */
  inline double AllpassProcess(double input, double coeff, double& state)
  {
    const double v = input - coeff * state;
    const double out = coeff * v + state;
    state = v;
    return out;
  }

  /**
   * @brief Compute first-order allpass coefficient for a given cutoff.
   *
   * Formula: a = (tan(π·fc/fs) − 1) / (tan(π·fc/fs) + 1)
   *
   * @param fc         Cutoff frequency in Hz.
   * @param sampleRate Host sample rate in Hz.
   * @return Allpass coefficient in (−1, 1).
   */
  inline double AllpassCoeff(double fc, double sampleRate)
  {
    const double t = std::tan(kPi * fc / sampleRate);
    return (t - 1.0) / (t + 1.0);
  }

  /**
   * @brief Read from a circular delay buffer with fractional-sample interpolation.
   *
   * Uses linear interpolation between adjacent samples for sub-sample accuracy.
   *
   * @param buf          Pointer to the circular buffer.
   * @param bufSize      Length of the buffer in samples.
   * @param writeIdx     Current write position.
   * @param delaySamples Desired delay in fractional samples.
   * @return Interpolated output sample.
   */
  inline double DelayRead(const double* buf, int bufSize, int writeIdx, double delaySamples)
  {
    double readPos = static_cast<double>(writeIdx) - delaySamples;
    if (readPos < 0.0) readPos += static_cast<double>(bufSize);

    int idx0 = static_cast<int>(readPos);
    int idx1 = idx0 + 1;
    if (idx0 >= bufSize) idx0 -= bufSize;
    if (idx1 >= bufSize) idx1 -= bufSize;

    const double frac = readPos - idx0;
    return buf[idx0] + frac * (buf[idx1] - buf[idx0]);
  }

  /**
   * @brief Compute gain reduction in dB from a peak-envelope value.
   *
   * Used inside the compressor section of ProcessBlock.  If the envelope
   * exceeds the threshold, the excess is reduced by (1 − 1/ratio).
   *
   * @param env        Peak envelope level (linear amplitude).
   * @param threshDB   Compressor threshold in dB.
   * @param ratio      Compressor ratio (≥ 1.0).
   * @return Gain reduction in dB (≥ 0).
   */
  inline double ComputeGR(double env, double threshDB, double ratio)
  {
    const double envDB = (env > 1e-10) ? 20.0 * std::log10(env) : -200.0;
    if (envDB > threshDB)
      return (envDB - threshDB) * (1.0 - 1.0 / ratio);
    return 0.0;
  }

  /**
   * @brief Compute effective gain reduction accounting for parallel mix.
   *
   * effectiveGR = −20·log10((1−mix) + mix·10^(−rawGR/20))
   *
   * @param rawGR     Raw GR in dB from the compressor.
   * @param compMix   Parallel mix fraction (0.0 … 1.0).
   * @return Effective GR in dB (≥ 0).
   */
  inline double EffectiveGR(double rawGR, double compMix)
  {
    if (rawGR <= 0.0) return 0.0;
    const double mixedGain = (1.0 - compMix) + compMix * std::exp(-rawGR * kLn10Over20);
    return -20.0 * std::log10(std::max(mixedGain, 1e-10));
  }
} // namespace

/// @see DynaCore::OnReset (DynaCore.h)
void DynaCore::OnReset()
{
  mSampleRate = GetSampleRate();

  mSmoothTremRate.SetSmoothTime(5.0, mSampleRate);
  mSmoothTremDepth.SetSmoothTime(5.0, mSampleRate);
  mSmoothPanRate.SetSmoothTime(5.0, mSampleRate);
  mSmoothPanDepth.SetSmoothTime(5.0, mSampleRate);
  mSmoothPitchRate.SetSmoothTime(5.0, mSampleRate);
  mSmoothPitchDepth.SetSmoothTime(5.0, mSampleRate);
  mSmoothPhaserRate.SetSmoothTime(5.0, mSampleRate);
  mSmoothPhaserDepth.SetSmoothTime(5.0, mSampleRate);
  mSmoothMasterInt.SetSmoothTime(5.0, mSampleRate);
  mSmoothOutGain.SetSmoothTime(5.0, mSampleRate);
  mSmoothBypass.SetSmoothTime(5.0, mSampleRate);
  mSmoothCompThresh.SetSmoothTime(5.0, mSampleRate);
  mSmoothCompRatio.SetSmoothTime(5.0, mSampleRate);
  mSmoothCompGain.SetSmoothTime(5.0, mSampleRate);
  mSmoothCompMix.SetSmoothTime(5.0, mSampleRate);
  mSmoothWidth.SetSmoothTime(5.0, mSampleRate);

  mTremPhase = mPanPhase = mPitchPhase = mPhaserPhase = 0.0;

  std::memset(mPitchDelayBuf, 0, sizeof(mPitchDelayBuf));
  mPitchDelayWriteIdx = 0;

  std::memset(mAllpassState, 0, sizeof(mAllpassState));

  mCompEnv[0] = mCompEnv[1] = 0.0;

  mOutputLevelDBL = mOutputLevelDBR = -100.0;
  mOutputSmoothedL = mOutputSmoothedR = -100.0;
}

/// @see DynaCore::ProcessBlock (DynaCore.h)
void DynaCore::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  // grab all params once per block, then smooth per-sample inside the loop
  const double bypassTarget = GetParam(kBypass)->Bool() ? 1.0 : 0.0;
  const double outDB     = GetParam(kOutputLevel)->Value();
  const double outAmp    = std::exp(outDB * kLn10Over20);
  const int nChans       = NOutChansConnected();

  const bool tremBypass  = GetParam(kTremBypass)->Bool();
  const double tremRate  = GetParam(kTremRate)->Value();
  const double tremDepth = GetParam(kTremDepth)->Value() / 100.0;

  const bool panBypass   = GetParam(kPanBypass)->Bool();
  const double panRate   = GetParam(kPanRate)->Value();
  const double panDepth  = GetParam(kPanDepth)->Value() / 100.0;

  const bool pitchBypass = GetParam(kPitchBypass)->Bool();
  const double pitchRate = GetParam(kPitchRate)->Value();
  const double pitchDepth= GetParam(kPitchDepth)->Value() / 100.0;

  const bool phaserBypass= GetParam(kPhaserBypass)->Bool();
  const double phaserRate= GetParam(kPhaserRate)->Value();
  const double phaserDepth= GetParam(kPhaserDepth)->Value() / 100.0;

  // Compressor params
  const bool compBypass    = GetParam(kCompBypass)->Bool();
  const double compThresh  = GetParam(kCompThreshold)->Value();  // dB
  const double compRatio   = GetParam(kCompRatio)->Value();      // :1
  const double compGainDB  = GetParam(kCompGain)->Value();       // dB makeup
  const double compMix     = GetParam(kCompMix)->Value() / 100.0;
  const double compAttackMs  = GetParam(kCompAttack)->Value();   // ms
  const double compReleaseMs = GetParam(kCompRelease)->Value();  // ms

  const double masterInt = GetParam(kMasterIntensity)->Value() / 100.0;
  const double widthNorm = GetParam(kWidth)->Value() / 100.0; // 0-200% → 0.0-2.0

  const double sr    = mSampleRate;
  const double invSr = 1.0 / sr;

  // one-pole IIR attack/release coefficients (0.0 = instant response)
  const double compAttackCoeff  = (compAttackMs  > 0.0)
    ? std::exp(-kTwoPi / (compAttackMs  * 0.001 * sr)) : 0.0;
  const double compReleaseCoeff = (compReleaseMs > 0.0)
    ? std::exp(-kTwoPi / (compReleaseMs * 0.001 * sr)) : 0.0;

  constexpr double kPitchCenterDelayMs = 20.0; // static centre delay
  constexpr double kPitchModDepthMs    = 10.0; // ±LFO excursion

  // phaser sweeps 200–4000 Hz on a log scale; precompute log ratio so per-sample exp is cheap
  constexpr double kPhaserMinFreq     = 200.0;
  constexpr double kPhaserMaxFreq     = 4000.0;
  const double phaserFreqRatio        = kPhaserMaxFreq / kPhaserMinFreq;
  const double phaserFreqRatioLog     = std::log(phaserFreqRatio);

  const double bypassRampRate = 1.0 / (0.010 * sr); // ~10ms module bypass fade

  double peakL = 0.0;
  double peakR = 0.0;
  double lastGainReductionL  = 0.0;
  double lastGainReductionR  = 0.0;

  for (int s = 0; s < nFrames; ++s)
  {
    // Smooth params per-sample
    const double sTremRate   = mSmoothTremRate.Process(tremRate);
    const double sTremDepth  = mSmoothTremDepth.Process(tremDepth);
    const double sPanRate    = mSmoothPanRate.Process(panRate);
    const double sPanDepth   = mSmoothPanDepth.Process(panDepth);
    const double sPitchRate  = mSmoothPitchRate.Process(pitchRate);
    const double sPitchDepth = mSmoothPitchDepth.Process(pitchDepth);
    const double sPhaserRate = mSmoothPhaserRate.Process(phaserRate);
    const double sPhaserDepth= mSmoothPhaserDepth.Process(phaserDepth);
    const double sMasterInt  = mSmoothMasterInt.Process(masterInt);
    const double sOutAmp     = mSmoothOutGain.Process(outAmp);

    const double sBypass     = mSmoothBypass.Process(bypassTarget);

    const double sCompThresh = mSmoothCompThresh.Process(compThresh);
    const double sCompRatio  = mSmoothCompRatio.Process(compRatio);
    const double sCompGainDB = mSmoothCompGain.Process(compGainDB);
    const double sCompMix    = mSmoothCompMix.Process(compMix);
    const double sWidth      = mSmoothWidth.Process(widthNorm);

    double dryL = static_cast<double>(inputs[0][s]);
    double dryR = (nChans >= 2) ? static_cast<double>(inputs[1][s]) : dryL;

    double procL = dryL;
    double procR = dryR;

    // width=0: fold to mono before the compressor so both GR bars track the same signal
    if (sWidth < 0.001 && nChans >= 2) {
      const double monoSig = 0.5 * (procL + procR);
      procL = monoSig;
      procR = monoSig;
    }

    // --- Compressor ---
    if (!compBypass)
    {
      const double absL = std::fabs(procL);
      mCompEnv[0] = absL + (absL > mCompEnv[0] ? compAttackCoeff : compReleaseCoeff) * (mCompEnv[0] - absL);

      const double absR = std::fabs(procR);
      mCompEnv[1] = absR + (absR > mCompEnv[1] ? compAttackCoeff : compReleaseCoeff) * (mCompEnv[1] - absR);

      const double grL = ComputeGR(mCompEnv[0], sCompThresh, sCompRatio);
      const double grR = ComputeGR(mCompEnv[1], sCompThresh, sCompRatio);

      const double gainL = std::exp((sCompGainDB - grL) * kLn10Over20);
      const double gainR = std::exp((sCompGainDB - grR) * kLn10Over20);

      procL = procL + sCompMix * (procL * gainL - procL);
      procR = procR + sCompMix * (procR * gainR - procR);

      lastGainReductionL  = EffectiveGR(grL, sCompMix);
      lastGainReductionR  = EffectiveGR(grR, sCompMix);
    }

    // master intensity blends from the compressor output so the comp is always active regardless of the knob
    const double compL = procL;
    const double compR = procR;

    // --- Tremolo (R channel +30° phase offset for stereo shimmer) ---
    {
      mTremPhase += sTremRate * invSr;
      if (mTremPhase >= 1.0) mTremPhase -= 1.0;

      const double tremTarget = tremBypass ? 0.0 : 1.0;
      if (mTremBypassRamp < tremTarget)
        mTremBypassRamp = std::min(tremTarget, mTremBypassRamp + bypassRampRate);
      else
        mTremBypassRamp = std::max(tremTarget, mTremBypassRamp - bypassRampRate);

      if (mTremBypassRamp > 0.0)
      {
        constexpr double kTremStereoOffset = 0.083; // ~30° stereo spread
        double tremLFO_L = std::sin(mTremPhase * kTwoPi);
        double tremLFO_R = std::sin((mTremPhase + kTremStereoOffset) * kTwoPi);
        double tremL = procL * (1.0 - sTremDepth * 0.5 * (1.0 - tremLFO_L));
        double tremR = procR * (1.0 - sTremDepth * 0.5 * (1.0 - tremLFO_R));
        procL += mTremBypassRamp * (tremL - procL);
        procR += mTremBypassRamp * (tremR - procR);
      }
    }

    // --- Pan Motion (constant-power panning LFO) ---
    {
      mPanPhase += sPanRate * invSr;
      if (mPanPhase >= 1.0) mPanPhase -= 1.0;

      const double panTarget = (panBypass || nChans < 2) ? 0.0 : 1.0;
      if (mPanBypassRamp < panTarget)
        mPanBypassRamp = std::min(panTarget, mPanBypassRamp + bypassRampRate);
      else
        mPanBypassRamp = std::max(panTarget, mPanBypassRamp - bypassRampRate);

      if (mPanBypassRamp > 0.0 && nChans >= 2)
      {
        double panLFO = std::sin(mPanPhase * kTwoPi);
        double panPos = panLFO * sPanDepth;                      // −1 … +1
        double angle  = (1.0 + panPos) * 0.25 * kPi;          // 0 … π/2
        double gainL  = std::cos(angle) * kSqrt2;             // constant-power
        double gainR  = std::sin(angle) * kSqrt2;
        double panL   = procL * gainL;
        double panR   = procR * gainR;
        procL += mPanBypassRamp * (panL - procL);
        procR += mPanBypassRamp * (panR - procR);
      }
    }

    // --- Pitch Drift (stereo chorus: L=sin, R=cos LFO for 90° spread) ---
    {
      mPitchDelayBuf[0][mPitchDelayWriteIdx] = procL;
      if (nChans >= 2)
        mPitchDelayBuf[1][mPitchDelayWriteIdx] = procR;

      mPitchPhase += sPitchRate * invSr;
      if (mPitchPhase >= 1.0) mPitchPhase -= 1.0;

      const double pitchTarget = pitchBypass ? 0.0 : 1.0;
      if (mPitchBypassRamp < pitchTarget)
        mPitchBypassRamp = std::min(pitchTarget, mPitchBypassRamp + bypassRampRate);
      else
        mPitchBypassRamp = std::max(pitchTarget, mPitchBypassRamp - bypassRampRate);

      if (mPitchBypassRamp > 0.0)
      {
        double pitchLFO_L = std::sin(mPitchPhase * kTwoPi);
        double pitchLFO_R = std::cos(mPitchPhase * kTwoPi);

        double centerDelay  = kPitchCenterDelayMs * 0.001 * sr;
        double modExcursion = kPitchModDepthMs * 0.001 * sr * sPitchDepth;

        auto clampDelay = [](double d, int bufSize) {
          if (d < 1.0) d = 1.0;
          if (d > static_cast<double>(bufSize - 2)) d = static_cast<double>(bufSize - 2);
          return d;
        };

        double delayL  = clampDelay(centerDelay + pitchLFO_L * modExcursion, kPitchDelayBufSize);
        double delayR  = clampDelay(centerDelay + pitchLFO_R * modExcursion, kPitchDelayBufSize);
        double pitchL  = DelayRead(mPitchDelayBuf[0], kPitchDelayBufSize, mPitchDelayWriteIdx, delayL);
        double pitchR  = (nChans >= 2)
                         ? DelayRead(mPitchDelayBuf[1], kPitchDelayBufSize, mPitchDelayWriteIdx, delayR)
                         : pitchL;
        procL += mPitchBypassRamp * (pitchL - procL);
        procR += mPitchBypassRamp * (pitchR - procR);
      }

      mPitchDelayWriteIdx = (mPitchDelayWriteIdx + 1) % kPitchDelayBufSize;
    }

    // --- Phaser (6-stage allpass + feedback, stereo 90° LFO) ---
    {
      mPhaserPhase += sPhaserRate * invSr;
      if (mPhaserPhase >= 1.0) mPhaserPhase -= 1.0;

      const double phaserTarget = phaserBypass ? 0.0 : 1.0;
      if (mPhaserBypassRamp < phaserTarget)
        mPhaserBypassRamp = std::min(phaserTarget, mPhaserBypassRamp + bypassRampRate);
      else
        mPhaserBypassRamp = std::max(phaserTarget, mPhaserBypassRamp - bypassRampRate);

      // always run the allpass chain even when bypassed — keeps state warm so there's no pop on re-enable
      if (mPhaserBypassRamp > 0.0 || phaserTarget > 0.0)
      {
        constexpr double kPhaserFeedback = 0.45;

        double sweepNormL = 0.5 + 0.5 * std::sin(mPhaserPhase * kTwoPi); // L: sin
        double sweepNormR = 0.5 + 0.5 * std::cos(mPhaserPhase * kTwoPi); // R: cos (90° offset)

        double freqL    = kPhaserMinFreq * std::exp(sweepNormL * phaserFreqRatioLog);
        double freqR    = kPhaserMinFreq * std::exp(sweepNormR * phaserFreqRatioLog);
        double apCoeffL = AllpassCoeff(freqL, sr);
        double apCoeffR = AllpassCoeff(freqR, sr);

        double apInL  = procL + kPhaserFeedback * mPhaserFeedbackL;
        double apOutL = apInL;
        for (int st = 0; st < 6; ++st)
          apOutL = AllpassProcess(apOutL, apCoeffL, mAllpassState[0][st]);
        mPhaserFeedbackL = apOutL;

        double apOutR;
        if (nChans >= 2)
        {
          double apInR = procR + kPhaserFeedback * mPhaserFeedbackR;
          apOutR = apInR;
          for (int st = 0; st < 6; ++st)
            apOutR = AllpassProcess(apOutR, apCoeffR, mAllpassState[1][st]);
          mPhaserFeedbackR = apOutR;
        }
        else
        {
          apOutR = apOutL;
        }

        double phasL = procL + sPhaserDepth * (apOutL - procL);
        double phasR = procR + sPhaserDepth * (apOutR - procR);
        procL += mPhaserBypassRamp * (phasL - procL);
        procR += mPhaserBypassRamp * (phasR - procR);
      }
    }

    // --- Master Intensity (blend: comp output → fully modulated) ---
    double wetL = procL;
    double wetR = procR;
    procL = compL + sMasterInt * (wetL - compL);
    procR = compR + sMasterInt * (wetR - compR);

    procL *= sOutAmp;
    procR *= sOutAmp;

    // --- Stereo Width (M/S) ---
    if (nChans >= 2)
    {
      double mid  = 0.5 * (procL + procR);
      double side = 0.5 * (procL - procR);
      side *= sWidth; // 0=mono, 1=normal, 2=extra wide
      procL = mid + side;
      procR = mid - side;
    }

    // --- Global bypass crossfade ---
    double outL = procL * (1.0 - sBypass) + dryL * sBypass;
    double outR = procR * (1.0 - sBypass) + dryR * sBypass;

    outputs[0][s] = static_cast<sample>(outL);
    if (nChans >= 2)
      outputs[1][s] = static_cast<sample>(outR);

    double absL = std::fabs(outL);
    double absR = std::fabs(outR);
    if (absL > peakL) peakL = absL;
    if (absR > peakR) peakR = absR;
  }

  // output metering: peak per block with smoothing (40 ms attack / 300 ms release)
  {
    double rawDBL = (peakL > 1e-10) ? 20.0 * std::log10(peakL) : -100.0;
    double rawDBR = (peakR > 1e-10) ? 20.0 * std::log10(peakR) : -100.0;

    // raise per-sample coeff to nFrames to get the right block-level smoothing
    constexpr double kMeterAttackMs  = 40.0;
    constexpr double kMeterReleaseMs = 300.0;
    double attSample = std::exp(-1.0 / (kMeterAttackMs  * 0.001 * sr));
    double relSample = std::exp(-1.0 / (kMeterReleaseMs * 0.001 * sr));
    double attC = std::pow(attSample, static_cast<double>(nFrames));
    double relC = std::pow(relSample, static_cast<double>(nFrames));

    double cL = (rawDBL > mOutputSmoothedL) ? attC : relC;
    mOutputSmoothedL = rawDBL + cL * (mOutputSmoothedL - rawDBL);
    // don't snap mOutputSmoothedL here — if you do, it resets to -100 every block
    // while trying to rise, so the meter stays dead after stop/start. clamp only what we report.
    mOutputLevelDBL = (mOutputSmoothedL < -54.0) ? -100.0 : mOutputSmoothedL;

    double cR = (rawDBR > mOutputSmoothedR) ? attC : relC;
    mOutputSmoothedR = rawDBR + cR * (mOutputSmoothedR - rawDBR);
    mOutputLevelDBR = (mOutputSmoothedR < -54.0) ? -100.0 : mOutputSmoothedR;
    mMeterUpdateCount++;
  }

  // push GR values to the UI — last sample of the block is close enough for a visual meter
  mGainReductionL = lastGainReductionL;
  mGainReductionR = lastGainReductionR;
  mGRUpdateCount++;
}
#endif
