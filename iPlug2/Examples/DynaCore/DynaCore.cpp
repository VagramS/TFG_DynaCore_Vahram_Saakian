#include "DynaCore.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <cstdio> // snprintf
#include <cmath>
#include <algorithm> // std::clamp
#include <string>
#include "wdlstring.h"

// ---------- helper for hover overlay ----------
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

// ----------------- Rate snapping (Hz) -----------------
static const double kRateStepsHz[] =
{
  0.00,  // = unplugged/stop
  0.10, 0.12, 0.16, 0.18, 0.20, 0.22, 0.25, 0.28, 0.30, 0.35, 0.40, 0.45, 0.50,
  0.60, 0.65, 0.80, 0.90, 1.00, 1.60, 2.00, 3.00, 5.00, 6.00
};

static double SnapRateHz(double hz)
{
  // Guard against invalid input.
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

// ---------------- Rate helpers (must be declared before AutoGateModulesFromParams) ----------------
static inline bool IsRateParamIdx(int idx)
{
  return idx == kTremRate || idx == kPanRate || idx == kPitchRate || idx == kPhaserRate;
}

// Dead zone: if 0 < Hz < threshold => treat as 0.0
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

static void AutoGateModulesFromParams(IEditorDelegate* dlg, int changedParamIdx)
{
  if (!dlg) return;

  // Prevent recursion when we change bypass params from inside this function
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
    // 1.0 = bypass (OFF), 0.0 = active (ON)
    const double target = bypass ? 1.0 : 0.0;

    if (IParam* bp = dlg->GetParam(bypassIdx))
    {
      // Avoid spamming if already correct
      if (std::fabs(bp->GetNormalized() - target) > 1e-9)
        dlg->SendParameterValueFromUI(bypassIdx, target);
    }
  };

  // ---- Mod modules: ON only if BOTH Rate and Depth are non-zero ----
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
    const bool on = IsRateOn(m.rateIdx) && IsDepthOn(m.depthIdx);
    SetBypass(m.bypassIdx, !on);
  }

  // Compressor: ON only if Mix > 0
  const bool compOn = GetPlain(kCompMix) > 0.0;
  SetBypass(kCompBypass, !compOn);

  sInAutoGate = false;
}

// Forward declaration
class RevertButtonControl;

// Preset groups
enum class EPresetGroup
{
  None,
  Vocals,
  Pads,
  Drums,
  Experimental
};

// ==== PRESET NAMES (by group/index) ====
// Index = cell index in the right list (1..4 on the UI, 0..3 in code)
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

static void StepPresetCarousel(EPresetGroup& g, int& idx, int dir)
{
  // dir: +1 next, -1 previous
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
    const int c2 = GetPresetCountGlobal(g);
    idx = (c2 > 0) ? 0 : 0;
  }
  else if (idx < 0)
  {
    // Move to previous group, last preset
    g = PrevGroup(g);
    const int c2 = GetPresetCountGlobal(g);
    idx = (c2 > 0) ? (c2 - 1) : 0;
  }
}


struct PresetValues
{
  double tremBypass, tremRate, tremDepth;
  double panBypass,  panRate,  panDepth;
  double pitchBypass,pitchRate,pitchDepth;
  double phaserBypass,phaserRate,phaserDepth;

  double compBypass, compMix, compThreshold, compRatio, compGain, compAttack, compRelease;

  double masterIntensity;
  double outputLevel;
};

// ---- PRESET VALUE TABLES ----
static const PresetValues kPresetVals_Vocals[4] =
{
  // 1) Cold Whisper 14
  { 0,0.35,8,   0,0.18,16,  0,0.12,3,   0,0.10,6,   0,65,-22,3.2,1.5,8,140,  12,-0.5 },

  // 2) Blade Mono Focus
  { 1,0,0,      0,0.10,4,   1,0,0,      0,0.45,10,  0,80,-18,4.5,2.0,6,110,  15,-1.0 },

  // 3) Spectral Glide
  { 1,0,0,      0,0.30,22,  0,0.22,7,   0,0.25,14,  0,55,-20,2.5,1.0,12,220, 18,-0.8 },

  // 4) Ritual Double
  { 0,1.00,10,  0,0.50,28,  0,0.35,4,   1,0,0,      0,70,-24,3.8,2.5,7,160,  20,-1.2 }
};

static const PresetValues kPresetVals_Pads[4] =
{
  // 1) Cryostasis Pad
  { 0,0.18,12,  0,0.12,18,  1,0,0,      0,0.20,24,  0,35,-26,2.0,0.0,20,300, 10,-1.5 },

  // 2) Nocturne Pulse
  { 0,0.40,22,  1,0,0,      1,0,0,      0,0.35,18,  0,30,-28,2.2,0.0,18,380, 14,-1.0 },

  // 3) Moon Tides
  { 1,0,0,      0,0.16,35,  0,0.10,5,   0,0.28,20,  0,25,-24,1.8,0.0,25,450, 12,-1.0 },

  // 4) Glass Cathedral
  { 1,0,0,      0,0.22,14,  1,0,0,      0,0.60,26,  0,40,-22,2.8,0.5,15,260, 16,-0.8 }
};

static const PresetValues kPresetVals_Drums[3] =
{
  // 1) Iron March
  { 0,2.00,18,  1,0,0,      1,0,0,      0,0.90,10,  0,70,-16,5.0,1.0,4,120,  22,-1.5 },

  // 2) Ghost Hats
  { 0,6.00,12,  0,3.00,40,  1,0,0,      0,5.00,18,  0,35,-18,2.4,0.0,3,90,   15,-0.8 },

  // 3) Submerge Kit
  { 0,0.90,24,  1,0,0,      0,0.35,2,   0,0.65,28,  0,60,-22,3.5,1.5,8,180,  18,-1.2 }
};

static const PresetValues kPresetVals_Exp[2] =
{
  // 1) Event Horizon
  { 1,0,0,      1,0,0,      0,0.20,9,   0,0.12,40,  0,45,-26,3.0,0.0,12,300, 24,-1.5 },

  // 2) Time Shear
  { 0,1.60,10,  0,0.80,55,  0,0.45,6,   1,0,0,      0,50,-20,2.8,0.5,10,200, 20,-1.0 }
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

  // Master / Output
  SendPlain(kMasterIntensity, pv->masterIntensity);
  SendPlain(kOutputLevel,     pv->outputLevel);
}

// ==== Global state for presets page ====
// Last opened group (purely for UX)
static EPresetGroup gLastPresetGroup = EPresetGroup::Vocals;

// Single globally selected preset: group + index (-1 = none)
static EPresetGroup gSelectedGroupGlobal  = EPresetGroup::None;
static int          gSelectedPresetGlobal = -1;

// Human-readable name of selected preset (for any future use / debug)
static std::string  gSelectedPresetName   = kPresetName_None;


// ---------- Reset main parameters to defaults (triggered from UI) ----------
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

    { kTremRate,        0.0   }, { kTremDepth,     0.0   },
    { kPanRate,         0.0   }, { kPanDepth,      0.0   },
    { kPitchRate,       0.0   }, { kPitchDepth,    0.0   },
    { kPhaserRate,      0.0   }, { kPhaserDepth,   0.0   },

    { kCompMix,         100.0 },
    { kCompThreshold,   -25.0 },
    { kCompRatio,       2.0   },
    { kCompGain,        0.0   },
    { kCompAttack,      10.0   },
    { kCompRelease,     120.0   },

    { kMasterIntensity, 100.0 },
    { kOutputLevel,     0.0   }
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


// =================== PRESETS OVERLAY ===================

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

// ---------- Revert To Default (programmatic hover + hand) ----------
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

// ---------- "SELECT PRESET" button that opens the overlay ----------
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


// ========== Toggle button with hover overlay ==========
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

//============================================================
// Background arc for BIG knob (drawn UNDER the rotating cap)
//============================================================
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


// ========== Knob with optional hover overlay, HAND cursor ==========
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

// ring rect is the control bounds (optionally padded)
const IRECT arcR = (mArcPad != 0.f) ? mRECT.GetPadded(mArcPad) : mRECT;

const float cx = arcR.MW();
const float cy = arcR.MH();
const float radius = 0.5f * std::min(arcR.W(), arcR.H()) - (mArcThickness * 0.5f);

// IMPORTANT:
// In this project build, IGraphics::PathArc expects angles in DEGREES (not radians).
// Passing radians makes the arc so small it looks like a dot.
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
// ========== Value arc for big knob background ring (non-interactive) ==========
class KnobValueArcControl : public IControl
{
public:
  KnobValueArcControl(const IRECT& bounds,
                      int paramIdx,
                      IColor arcColor = IColor(255, 0x50, 0x62, 0x74), // #506274
                      float arcThickness = 2.f,
                      float arcStartDeg = 225.f,
                      float arcSweepDeg = 270.f)
  : IControl(bounds, paramIdx)
  , mArcColor(arcColor)
  , mArcThickness(arcThickness)
  , mArcStartDeg(arcStartDeg)
  , mArcSweepDeg(arcSweepDeg)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    double norm = GetValue();
    if (const IParam* p = GetParam())
      norm = p->GetNormalized();

    const float t = (float) norm;

    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float radius = 0.5f * std::min(mRECT.W(), mRECT.H()) - (mArcThickness * 0.5f);

    const float start = mArcStartDeg;
    const float end   = mArcStartDeg + mArcSweepDeg;
    const float cur   = start + t * (end - start);

    // IMPORTANT: In this project PathArc expects DEGREES (not radians).
    g.PathClear();
    g.PathArc(cx, cy, radius, start, cur);
    g.PathStroke(mArcColor, mArcThickness);
  }

    private:
      IColor mArcColor;
      float  mArcThickness = 2.f;
      float  mArcStartDeg = 225.f;
      float  mArcSweepDeg = 270.f;
  };

};


// =================== Output level text ===================
class OutputLevelTextControl : public IControl
{
public:
  OutputLevelTextControl(const IRECT& bounds, DynaCore* plugin)
  : IControl(bounds), mPlugin(plugin)
  { mIgnoreMouse = true; }

  void Draw(IGraphics& g) override
  {
    if (!mPlugin) return;

    const double db = mPlugin->GetOutputLevelDB();

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", db);

    // Shadow glow behind text
    IColor shadowColor(10, 255, 255, 255);
    IText shadowText(17.f, shadowColor, "Inter-Semi-Bold",
                     EAlign::Center, EVAlign::Middle);

    for (int dy = -1; dy <= 1; ++dy)
    {
      for (int dx = -1; dx <= 1; ++dx)
      {
        if (dx == 0 && dy == 0) continue;
        IRECT r(mRECT.L + dx, mRECT.T + dy, mRECT.R + dx, mRECT.B + dy);
        g.DrawText(shadowText, buf, r);
      }
    }

    // Main text
    IText text(17.f, COLOR_WHITE, "Inter-Semi-Bold",
               EAlign::Center, EVAlign::Middle);
    g.DrawText(text, buf, mRECT);

    SetDirty(false);
  }

private:
  DynaCore* mPlugin = nullptr;
};

// =================== Value text under BIG knobs ===================
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

    IColor color(255, 53, 66, 80);
    IText text(17.f, color, "Inter-Regular", EAlign::Center, EVAlign::Middle);

    g.DrawText(text, buf, mRECT);
    SetDirty(false);
  }

private:
  int mParamIdx = -1;
};


// =================== Value text under MID knobs ===================
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



// =================== Preset title text (X:45, Y:77) ===================
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

    // Color ABABAB, font size 10, using Inter-Medium (loaded in layout)
    IColor color(255, 171, 171, 171);
    IText  itext(10.f, color, "Inter-Medium", EAlign::Near, EVAlign::Middle);

    g.DrawText(itext, text, mRECT);
    SetDirty(false);
  }
};

// =================== PLUGIN CONSTRUCTOR ===================

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


  // Master / Output
  GetParam(kBypass)->InitBool("Bypass", false);
  GetParam(kMasterIntensity)->InitDouble("Master Intensity", 100.0, 0.0, 100.0, 1.0, "%");
  GetParam(kOutputLevel)->InitDouble("Output Level", 0.0, -20.0, 20.0, 0.1, "dB");

  // On load — all modules bypassed, Default/None active
  GetParam(kCompBypass)->Set(0.0); // 0 = active, 1 = bypass
  GetParam(kTremBypass)->Set(1.0);
  GetParam(kPanBypass)->Set(1.0);
  GetParam(kPitchBypass)->Set(1.0);
  GetParam(kPhaserBypass)->Set(1.0);

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
    pGraphics->LoadFont("Inter-Regular",  INTER_REGULAR_FN);
    pGraphics->LoadFont("Inter-Semi-Bold",  INTER_SEMI_BOLD_FN);
    pGraphics->LoadFont("Inter-Medium",  INTER_MEDIUM_FN);

    // BACKGROUND
    IBitmap bg = pGraphics->LoadBitmap(MAIN_BACKGROUND_FN, 1);
    const IRECT bounds = pGraphics->GetBounds();
    pGraphics->AttachControl(new IBitmapControl(bounds, bg));

    // HOVER COLORS
    const IColor hoverColorButtons  = IColor(23, 184, 184, 184);
    const IColor hoverColorModules  = IColor(23, 14,  14,  14);
    const IColor hoverColorKnobs    = IColor(18, 184, 184, 184); // mid/small

    // COMPRESSOR ON/OFF
    IRECT compBtnRect(371.f, 447.f, 392.f, 467.f);
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

    IBitmap vocalsLabelBmp  = pGraphics->LoadBitmap(PRESET_VOCALS_LABLE_FN, 1);
    IBitmap padsLabelBmp    = pGraphics->LoadBitmap(PRESET_PADS_LABLE_FN,   1);
    IBitmap drumsLabelBmp   = pGraphics->LoadBitmap(PRESET_DRUMS_LABLE_FN,   1);
    IBitmap expLabelBmp     = pGraphics->LoadBitmap(PRESET_EXP_LABLE_FN,    1);

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


    // BIG knobs — HAND cursor only, no hover overlay
    // BIG knobs — background ring arc + rotating cap (HAND cursor only, no hover overlay)
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
        knobRect, midKnob, paramIdx, hoverColorKnobs, 10.f, false));
    };

    auto AttachSmallKnobWithArc = [&](const IRECT& knobRect, int paramIdx)
    {
      const IRECT ringRect = knobRect.GetPadded(4.9f);

      pGraphics->AttachControl(new KnobValueArcControl(
        ringRect, paramIdx, IColor(245, 171, 171, 171), 2.f, 223.f, 270.f));

      pGraphics->AttachControl(new HoverKnobRotaterControl(
        knobRect, smallKnob, paramIdx, hoverColorKnobs, 10.f, false));
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

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compMixRect, midKnob, kCompMix, hoverColorKnobs, 10.f, true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compThreshRect, midKnob, kCompThreshold, hoverColorKnobs, 10.f, true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compRatioRect, midKnob, kCompRatio, hoverColorKnobs, 10.f, true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compGainRect, midKnob, kCompGain, hoverColorKnobs, 10.f, true));
    
    AttachMidKnobWithArc(compMixRect, kCompMix);
    AttachMidKnobWithArc(compThreshRect, kCompThreshold);
    AttachMidKnobWithArc(compRatioRect, kCompRatio);
    AttachMidKnobWithArc(compGainRect, kCompGain);
    

    // COMP: Attack / Release (small)
    IRECT compAttackRect  (351.2f, 492.2f, 351.2f + kSmW, 492.2f + kSmH);
    IRECT compReleaseRect (351.2f, 540.2f, 351.2f + kSmW, 540.2f + kSmH);

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compAttackRect, smallKnob, kCompAttack,  hoverColorKnobs, 10.f, true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compReleaseRect, smallKnob, kCompRelease, hoverColorKnobs, 10.f, true));
    
    AttachSmallKnobWithArc(compAttackRect, kCompAttack);
    AttachSmallKnobWithArc(compReleaseRect, kCompRelease);

    // MASTER: Intensity (mid)
    IRECT masterIntRect(459.6f, 512.8f, 459.6f + kMidW, 512.8f + kMidH);
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      masterIntRect, midKnob, kMasterIntensity, hoverColorKnobs, 10.f, true));
    
    AttachMidKnobWithArc(masterIntRect, kMasterIntensity);

    // OUTPUT: Level (mid)
    IRECT outLevelRect(954.5f, 527.2f, 954.5f + kMidW, 527.2f + kMidH);
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      outLevelRect, midKnob, kOutputLevel, hoverColorKnobs, 10.f, true));
    
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

      // X: 56/137/213/284/469, Y: 565
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(56.f, midLabelY), kCompMix));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(137.f, midLabelY), kCompThreshold));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(213.f, midLabelY), kCompRatio));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(284.f, midLabelY), kCompGain));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(469.f, midLabelY), kMasterIntensity));
      pGraphics->AttachControl(
        new MidKnobValueTextControl(MakeMidRect(963.f, midLabelY + 14), kOutputLevel));
    }

    // Output level numeric display
    IRECT outTextRect(937.8f, 481.f, 990.4f, 521.f);
    pGraphics->AttachControl(new OutputLevelTextControl(outTextRect, this));
  };
#endif
}

// =================== DSP ===================
#if IPLUG_DSP
void DynaCore::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const bool bypass   = GetParam(kBypass)->Bool();
  const double outDB  = GetParam(kOutputLevel)->Value();
  const double outAmp = std::pow(10.0, outDB / 20.0);
  const int nChans    = NOutChansConnected();

  double sumSquares = 0.0;
  int sampleCount   = 0;

  for (int s = 0; s < nFrames; ++s)
  {
    for (int c = 0; c < nChans; ++c)
    {
      const sample inS  = inputs[c][s];

      // TODO: add processing chain later
      const sample proc = inS;

      const sample outS = bypass ? inS : (sample)(proc * outAmp);
      outputs[c][s] = outS;

      const double d = (double)outS;
      sumSquares += d * d;
      ++sampleCount;
    }
  }

  if (sampleCount > 0)
  {
    if (sumSquares <= 0.0)
      mOutputLevelDB = 0.0;
    else
    {
      const double rms = std::sqrt(sumSquares / (double)sampleCount);
      mOutputLevelDB = 20.0 * std::log10(rms);
    }
  }
}
#endif
