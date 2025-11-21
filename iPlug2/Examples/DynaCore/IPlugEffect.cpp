#include "IPlugEffect.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <cstdio> // snprintf
#include <cmath>
#include <string>

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

// ==== global state for presets page ====
// Last opened group (purely for UX)
static EPresetGroup gLastPresetGroup = EPresetGroup::Vocals;

// Single globally selected preset: group + index (-1 = none)
static EPresetGroup gSelectedGroupGlobal  = EPresetGroup::None;
static int          gSelectedPresetGlobal = -1;

// Human-readable name of selected preset (for any future use / debug)
static std::string  gSelectedPresetName   = kPresetName_None;

// ---------- Reset main parameters to defaults (triggered from UI) ----------
void IPlugEffect::ApplyDefaultPresetFromUI()
{
  // Global bypass
  GetParam(kBypass)->Set(0.0);

  // Compressor bypass
  GetParam(kCompBypass)->Set(1.0);

  // All modulation modules bypassed
  GetParam(kTremBypass)->Set(1.0);
  GetParam(kPanBypass)->Set(1.0);
  GetParam(kPitchBypass)->Set(1.0);
  GetParam(kPhaserBypass)->Set(1.0);

  // Main gain
  GetParam(kGain)->Set(0.0);

  // Tremolo / Pan / Pitch / Phaser values (all = 0)
  GetParam(kTremRate)->Set(0.0);
  GetParam(kTremDepth)->Set(0.0);

  GetParam(kPanRate)->Set(0.0);
  GetParam(kPanDepth)->Set(0.0);

  GetParam(kPitchRate)->Set(0.0);
  GetParam(kPitchDepth)->Set(0.0);

  GetParam(kPhaserRate)->Set(0.0);
  GetParam(kPhaserDepth)->Set(0.0);

  // Compressor defaults — ALL = 0
  GetParam(kCompMix)->Set(0.0);
  GetParam(kCompThreshold)->Set(0.0);
  GetParam(kCompRatio)->Set(0.0);
  GetParam(kCompGain)->Set(0.0);
  GetParam(kCompAttack)->Set(0.0);
  GetParam(kCompRelease)->Set(0.0);

  // Master / output defaults
  GetParam(kMasterIntensity)->Set(0.0);
  GetParam(kOutputLevel)->Set(0.0);

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

  void Draw(IGraphics& g) override
  {
    // Overlay background
    g.DrawBitmap(mPageBitmap, mRECT);

    // "Revert to Default" base bitmap (button is a separate control on top)
    const IRECT revertRect(413.f, 536.f, 413.f + 130.f, 536.f + 36.f);
    g.DrawBitmap(mRevertBmp, revertRect);

    // Group rects (used for hover visuals and hit-testing)
    const IRECT vocalsRect(19.f, 104.f, 285.f, 173.f);
    const IRECT padsRect  (19.f, 174.f, 285.f, 241.f);
    const IRECT drumsRect (19.f, 242.f, 285.f, 311.f);
    const IRECT expRect   (19.f, 312.f, 285.f, 380.f);

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
      const int needed = GetPresetCountForGroup(mSelectedGroup);

      for (int i = 0; i < needed && i < 4; ++i)
        g.DrawBitmap(mDividerBmp, IRECT(X, Ylist[i], X + W, Ylist[i] + H));
    }

    // ----- PRESET POINTER BULLETS (right side list) -----
    {
      const int presetCount = GetPresetCountForGroup(mSelectedGroup);
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
      const int presetCount = GetPresetCountForGroup(mSelectedGroup);
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
      const int presetCount = GetPresetCountForGroup(mSelectedGroup);

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
      const int count = GetPresetCountForGroup(mSelectedGroup);
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
    const IRECT vocalsRect(19.f, 104.f, 285.f, 173.f);
    const IRECT padsRect  (19.f, 174.f, 285.f, 241.f);
    const IRECT drumsRect (19.f, 242.f, 285.f, 311.f);
    const IRECT expRect   (19.f, 312.f, 285.f, 380.f);

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
      const int count = GetPresetCountForGroup(mSelectedGroup);
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

        // safe placeholder: no DSP changes yet
        ApplyPresetPlaceholder(mSelectedGroup, clickedPreset);

        SetDirty(false);
        if (auto* ui = GetUI()) ui->SetAllControlsDirty(); // update label on the main page
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
  int GetPresetCountForGroup(EPresetGroup g) const
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

  void ApplyPresetPlaceholder(EPresetGroup /*g*/, int /*idx*/)
  {
    // Stub for future preset application.
    // Intentionally empty to avoid side effects until real DSP is wired.
  }

  void UpdateHover(float x, float y)
  {
    // Group rects
    const IRECT vocalsRect(19.f, 104.f, 285.f, 173.f);
    const IRECT padsRect  (19.f, 174.f, 285.f, 241.f);
    const IRECT drumsRect (19.f, 242.f, 285.f, 311.f);
    const IRECT expRect   (19.f, 312.f, 285.f, 380.f);

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

    const int count = GetPresetCountForGroup(mSelectedGroup);

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
      // Bool parameters (normalized 0/1)
      dlg->SendParameterValueFromUI(kBypass,       0.0);
      dlg->SendParameterValueFromUI(kCompBypass,   1.0);
      dlg->SendParameterValueFromUI(kTremBypass,   1.0);
      dlg->SendParameterValueFromUI(kPanBypass,    1.0);
      dlg->SendParameterValueFromUI(kPitchBypass,  1.0);
      dlg->SendParameterValueFromUI(kPhaserBypass, 1.0);

      auto sendNorm = [dlg](int idx, double val, double minV, double maxV)
      {
        const double denom = (maxV - minV);
        dlg->SendParameterValueFromUI(idx, denom == 0.0 ? 0.0 : (val - minV) / denom);
      };

      // Modulation / main — all zeros, lower bounds = 0
      sendNorm(kGain,            0.0,   0.0, 100.0);

      sendNorm(kTremRate,        0.0,   0.0, 20.0);
      sendNorm(kTremDepth,       0.0,   0.0, 100.0);

      sendNorm(kPanRate,         0.0,   0.0, 20.0);
      sendNorm(kPanDepth,        0.0,   0.0, 100.0);

      sendNorm(kPitchRate,       0.0,   0.0, 10.0);
      sendNorm(kPitchDepth,      0.0,   0.0, 100.0);

      sendNorm(kPhaserRate,      0.0,   0.0, 20.0);
      sendNorm(kPhaserDepth,     0.0,   0.0, 100.0);

      // Compressor — all zeros
      sendNorm(kCompMix,         0.0,   0.0, 100.0);
      sendNorm(kCompThreshold,   0.0,  -60.0,  0.0);
      sendNorm(kCompRatio,       0.0,   0.0, 20.0);
      sendNorm(kCompGain,        0.0,  -24.0, 24.0);
      sendNorm(kCompAttack,      0.0,   0.0, 100.0);
      sendNorm(kCompRelease,     0.0,   0.0, 1000.0);

      // Master / output
      sendNorm(kMasterIntensity, 0.0,   0.0, 100.0);
      sendNorm(kOutputLevel,     0.0,  -24.0, 24.0);
    }

    // Reset globally selected preset
    gSelectedGroupGlobal  = EPresetGroup::None;
    gSelectedPresetGlobal = -1;
    gSelectedPresetName   = kPresetName_None;
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
  , mOff(offBitmap), mOn(onBitmap)
  , mHoverColor(hoverColor), mCornerRadius(cornerRadius)
  {}

  void Draw(IGraphics& g) override
  {
    const bool bypass = (GetValue() >= 0.5f); // 1 = bypass ON
    g.DrawBitmap(bypass ? mOff : mOn, mRECT);

    if (GetMouseIsOver())
      DrawHoverOverlay(g, mRECT, mHoverColor, mCornerRadius);
  }

  void OnMouseDown(float, float, const IMouseMod&) override
  {
    SetValue(GetValue() < 0.5f ? 1.f : 0.f);
    SetDirty(false);
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
  IBitmap mOff, mOn;
  IColor  mHoverColor;
  float   mCornerRadius = 0.f;
};

// ========== Knob with optional hover overlay and HAND cursor ==========
class HoverKnobRotaterControl : public IBKnobRotaterControl
{
public:
  HoverKnobRotaterControl(const IRECT& bounds,
                          const IBitmap& knobBmp,
                          int paramIdx,
                          const IColor& hoverColor,
                          float cornerRadius,
                          bool drawOverlay)
  : IBKnobRotaterControl(bounds, knobBmp, paramIdx)
  , mHoverColor(hoverColor)
  , mCornerRadius(cornerRadius)
  , mDrawOverlay(drawOverlay)
  {}

  void Draw(IGraphics& g) override
  {
    IBKnobRotaterControl::Draw(g);
    if (mDrawOverlay && (GetMouseIsOver() || mDragging))
      DrawHoverOverlay(g, mRECT, mHoverColor, mCornerRadius);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    IBKnobRotaterControl::OnMouseOver(x, y, mod);
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::HAND);
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    IBKnobRotaterControl::OnMouseOut();
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::ARROW);
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    mDragging = true;
    IBKnobRotaterControl::OnMouseDown(x, y, mod);
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::HAND);
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    mDragging = false;
    IBKnobRotaterControl::OnMouseUp(x, y, mod);
    if (auto* ui = GetUI()) ui->SetMouseCursor(ECursor::ARROW);
    SetDirty(false);
  }

private:
  IColor mHoverColor;
  float  mCornerRadius = 0.f;
  bool   mDragging = false;
  bool   mDrawOverlay = true;
};

// =================== Output level text ===================
class OutputLevelTextControl : public IControl
{
public:
  OutputLevelTextControl(const IRECT& bounds, IPlugEffect* plugin)
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
  IPlugEffect* mPlugin = nullptr;
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

    // Color ABABAB, font size 8, using Inter-Semi-Bold (loaded in layout)
    IColor color(255, 171, 171, 171);
    IText  itext(11.f, color, "Inter-Semi-Bold", EAlign::Near, EVAlign::Middle);

    g.DrawText(itext, text, mRECT);
    SetDirty(false);
  }
};

// =================== PLUGIN CONSTRUCTOR ===================

IPlugEffect::IPlugEffect(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // --- MAIN GAIN ---
  GetParam(kGain)->InitDouble("Gain", 0.0, 0.0, 100.0, 0.01, "%");

  // Tremolo (zero start/min values)
  GetParam(kTremRate)->InitDouble ("Trem Rate",  0.0, 0.0, 20.0, 0.01, "Hz");
  GetParam(kTremDepth)->InitDouble("Trem Depth", 0.0, 0.0, 100.0, 1.0,  "%");
  GetParam(kTremBypass)->InitBool ("Trem Bypass", true);

  // Pan
  GetParam(kPanRate)->InitDouble  ("Pan Rate",   0.0, 0.0, 20.0, 0.01, "Hz");
  GetParam(kPanDepth)->InitDouble ("Pan Depth",  0.0, 0.0, 100.0, 1.0,  "%");
  GetParam(kPanBypass)->InitBool  ("Pan Bypass", true);

  // Pitch
  GetParam(kPitchRate)->InitDouble ("Pitch Rate",  0.0, 0.0, 10.0, 0.01, "Hz");
  GetParam(kPitchDepth)->InitDouble("Pitch Depth", 0.0, 0.0, 100.0, 1.0,  "%");
  GetParam(kPitchBypass)->InitBool ("Pitch Bypass", true);

  // Phaser
  GetParam(kPhaserRate)->InitDouble ("Phaser Rate",  0.0, 0.0, 20.0, 0.01, "Hz");
  GetParam(kPhaserDepth)->InitDouble("Phaser Depth", 0.0, 0.0, 100.0, 1.0,  "%");
  GetParam(kPhaserBypass)->InitBool ("Phaser Bypass", true);

  // Compressor — all zeros and lower bounds = 0 so that 0 is valid
  GetParam(kCompMix)->InitDouble      ("Comp Mix",       0.0,  0.0, 100.0, 1.0,  "%");
  GetParam(kCompThreshold)->InitDouble("Comp Threshold", 0.0, -60.0,  0.0, 0.1,  "dB");
  GetParam(kCompRatio)->InitDouble    ("Comp Ratio",     0.0,  0.0,  20.0, 0.1,  ":1");
  GetParam(kCompGain)->InitDouble     ("Comp Gain",      0.0, -24.0,  24.0, 0.1, "dB");
  GetParam(kCompAttack)->InitDouble   ("Comp Attack",    0.0,  0.0, 100.0, 0.1,  "ms");
  GetParam(kCompRelease)->InitDouble  ("Comp Release",   0.0,  -10.0, 10.0, 1.0,  "ms");
  GetParam(kCompBypass)->InitBool     ("Comp Bypass",    true);

  // Master / Output
  GetParam(kBypass)->InitBool("Bypass", false);
  GetParam(kMasterIntensity)->InitDouble("Master Intensity", 50.0, 0.0, 100.0, 1.0, "%");
  GetParam(kOutputLevel)->InitDouble("Output Level", 0.0, -24.0, 24.0, 0.1, "dB");

  // On load — all modules bypassed, Default/None active
  GetParam(kCompBypass)->Set(1.0);
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
    //pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Inter-Semi-Bold",  INTER_SEMI_BOLD_FN);

    // BACKGROUND
    IBitmap bg = pGraphics->LoadBitmap(MAIN_BACKGROUND_FN, 1);
    const IRECT bounds = pGraphics->GetBounds();
    pGraphics->AttachControl(new IBitmapControl(bounds, bg));

    // Hover colors
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
    IBitmap drumsLabelBmp   = pGraphics->LoadBitmap(PRESET_DRUMS_LABLE_FN,  1);
    IBitmap expLabelBmp     = pGraphics->LoadBitmap(PRESET_EXP_LABLE_FN,    1);

    IBitmap arrowBmp        = pGraphics->LoadBitmap(PRESET_GROUP_SELECT_ARROW_FN, 1);
    IBitmap revertBmp       = pGraphics->LoadBitmap(REVERT_TO_DEFAULT_FN,        1);

    IBitmap dividerBmp      = pGraphics->LoadBitmap(PRESET_DIVIDER_FN, 1);

    IBitmap presetFirstSelectBmp = pGraphics->LoadBitmap(PRESET_FROM_GROUP_SELECT_FIRST_FN, 1);
    IBitmap presetRestSelectBmp  = pGraphics->LoadBitmap(PRESET_FROM_GROUP_SELECT_REST_FN,  1);

    IBitmap presetPointerBmp = pGraphics->LoadBitmap(PRESET_POINTER_FN, 1);

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

    // ===== KNOBS =====
    IBitmap bigKnob   = pGraphics->LoadBitmap(BIG_KNOB_FN,   1);
    IBitmap midKnob   = pGraphics->LoadBitmap(MID_KNOB_FN,   1);
    IBitmap smallKnob = pGraphics->LoadBitmap(SMALL_KNOB_FN, 1);

    const float kBigW = 29.f, kBigH = 29.f;

    // Tremolo
    IRECT tremRateRect  (65.5f,  332.5f, 65.5f  + kBigW, 332.5f + kBigH);
    IRECT tremDepthRect (180.5f, 332.5f, 180.5f + kBigW, 332.5f + kBigH);

    // Pan Motion
    IRECT panRateRect   (281.5f, 332.5f, 281.5f + kBigW, 332.5f + kBigH);
    IRECT panDepthRect  (398.5f, 332.5f, 398.5f + kBigW, 332.5f + kBigH);

    // Pitch Drift
    IRECT pitchRateRect (497.5f, 332.5f, 497.5f + kBigW, 332.5f + kBigH);
    IRECT pitchDepthRect(614.5f, 332.5f, 614.5f + kBigW, 332.5f + kBigH);

    // Phaser
    IRECT phaserRateRect (713.5f, 332.5f, 713.5f + kBigW, 332.5f + kBigH);
    IRECT phaserDepthRect(830.5f, 332.5f, 830.5f + kBigW, 332.5f + kBigH);

    // BIG knobs — HAND cursor only, no hover overlay
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      tremRateRect,  bigKnob, kTremRate,  hoverColorKnobs, 10.f, false));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      tremDepthRect, bigKnob, kTremDepth, hoverColorKnobs, 10.f, false));

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      panRateRect,   bigKnob, kPanRate,   hoverColorKnobs, 10.f, false));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      panDepthRect,  bigKnob, kPanDepth,  hoverColorKnobs, 10.f, false));

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      pitchRateRect,  bigKnob, kPitchRate,  hoverColorKnobs, 10.f, false));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      pitchDepthRect, bigKnob, kPitchDepth, hoverColorKnobs, 10.f, false));

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      phaserRateRect,  bigKnob, kPhaserRate,  hoverColorKnobs, 10.f, false));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      phaserDepthRect, bigKnob, kPhaserDepth, hoverColorKnobs, 10.f, false));

    // MID / SMALL knobs with hover overlay enabled
    const float kMidW = 19.f,  kMidH = 19.f;
    const float kSmW  = 14.f,  kSmH  = 14.f;

    // COMP: Mix / Threshold / Ratio / Gain (mid)
    IRECT compMixRect     ( 47.f,   512.f,  47.f + kMidW,   512.f + kMidH);
    IRECT compThreshRect  (127.5f,  512.f, 127.5f + kMidW, 512.f + kMidH);
    IRECT compRatioRect   (203.5f,  512.f, 203.5f + kMidW, 512.f + kMidH);
    IRECT compGainRect    (274.5f,  513.f, 274.5f + kMidW, 513.f + kMidH);

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compMixRect, midKnob, kCompMix, hoverColorKnobs, 10.f, true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compThreshRect, midKnob, kCompThreshold, hoverColorKnobs, 10.f, true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compRatioRect, midKnob, kCompRatio, hoverColorKnobs, 10.f, true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compGainRect, midKnob, kCompGain, hoverColorKnobs, 10.f, true));

    // COMP: Attack / Release (small)
    IRECT compAttackRect  (351.2f, 492.2f, 351.2f + kSmW, 492.2f + kSmH);
    IRECT compReleaseRect (351.2f, 540.2f, 351.2f + kSmW, 540.2f + kSmH);

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compAttackRect, smallKnob, kCompAttack,  hoverColorKnobs, 10.f, true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compReleaseRect, smallKnob, kCompRelease, hoverColorKnobs, 10.f, true));

    // MASTER: Intensity (mid)
    IRECT masterIntRect(459.6f, 512.8f, 459.6f + kMidW, 512.8f + kMidH);
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      masterIntRect, midKnob, kMasterIntensity, hoverColorKnobs, 10.f, true));

    // OUTPUT: Level (mid)
    IRECT outLevelRect(956.5f, 544.2f, 956.5f + kMidW, 544.2f + kMidH);
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      outLevelRect, midKnob, kOutputLevel, hoverColorKnobs, 10.f, true));

    // Output level numeric display
    IRECT outTextRect(940.5f, 501.f, 990.5f, 521.f);
    pGraphics->AttachControl(new OutputLevelTextControl(outTextRect, this));
  };
#endif
}

// =================== DSP ===================

#if IPLUG_DSP
void IPlugEffect::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
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
