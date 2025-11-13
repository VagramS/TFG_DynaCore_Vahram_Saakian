#include "IPlugEffect.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <cstdio> // snprintf
#include <cmath>

// ---------- small helper to draw hover overlay ----------
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

// Persist last selected group
static EPresetGroup gLastPresetGroup = EPresetGroup::Vocals;

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

  // Tremolo / Pan / Pitch / Phaser values
  GetParam(kGain)->Set(0.0);

  GetParam(kTremRate)->Set(0.0);
  GetParam(kTremDepth)->Set(0.0);

  GetParam(kPanRate)->Set(0.0);
  GetParam(kPanDepth)->Set(0.0);

  GetParam(kPitchRate)->Set(0.0);
  GetParam(kPitchDepth)->Set(0.0);

  GetParam(kPhaserRate)->Set(0.0);
  GetParam(kPhaserDepth)->Set(0.0);

  // Compressor defaults
  GetParam(kCompMix)->Set(100.0);
  GetParam(kCompThreshold)->Set(-24.0);
  GetParam(kCompRatio)->Set(4.0);
  GetParam(kCompGain)->Set(0.0);
  GetParam(kCompAttack)->Set(10.0);
  GetParam(kCompRelease)->Set(100.0);

  // Master / output defaults
  GetParam(kMasterIntensity)->Set(0.0);
  GetParam(kOutputLevel)->Set(0.0);

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
                     const IBitmap& dividerBmp)         // <— NEW
  : IControl(bounds)
  , mPageBitmap(pageBitmap)
  , mVocalsSelectBmp(vocalsSelectBmp)
  , mPadsSelectBmp(padsSelectBmp)
  , mDrumsSelectBmp(drumsSelectBmp)
  , mExpSelectBmp(expSelectBmp)
  , mVocalsLabelBmp(vocalsLabelBmp)
  , mPadsLabelBmp(padsLabelBmp)
  , mDrumsLabelBmp(drumsLabelBmp)
  , mExpLabelBmp(expLabelBmp)
  , mArrowBmp(arrowBmp)
  , mRevertBmp(revertBmp)
  , mDividerBmp(dividerBmp)       // <— NEW
  {
    mIgnoreMouse = false;
    mSelectedGroup = gLastPresetGroup;
  }

  EPresetGroup GetSelectedGroup() const { return mSelectedGroup; }

  // Link overlay with the "Revert to Default" button instance
  void SetRevertButton(IControl* button) { mRevertButton = button; }

  void Draw(IGraphics& g) override
  {
    // Overlay background
    g.DrawBitmap(mPageBitmap, mRECT);

    // "Revert to Default" base bitmap (button is a separate control on top)
    const IRECT revertRect(413.f, 536.f, 413.f + 130.f, 536.f + 36.f);
    g.DrawBitmap(mRevertBmp, revertRect);

    // Group rects (used for both hover visuals and hit-testing)
    const IRECT vocalsRect(19.f, 104.f, 285.f, 173.f);
    const IRECT padsRect  (19.f, 174.f, 285.f, 241.f);
    const IRECT drumsRect (19.f, 242.f, 285.f, 311.f);
    const IRECT expRect   (19.f, 312.f, 285.f, 380.f);

    // Hover overlays for groups
    const IColor groupHoverColor(20, 184, 184, 184);
    constexpr float groupCornerRadius = 5.f;

    switch (mHoverGroup)
    {
      case EPresetGroup::Vocals:       DrawHoverOverlay(g, vocalsRect, groupHoverColor, groupCornerRadius); break;
      case EPresetGroup::Pads:         DrawHoverOverlay(g, padsRect,   groupHoverColor, groupCornerRadius); break;
      case EPresetGroup::Drums:        DrawHoverOverlay(g, drumsRect,  groupHoverColor, groupCornerRadius); break;
      case EPresetGroup::Experimental: DrawHoverOverlay(g, expRect,    groupHoverColor, groupCornerRadius); break;
      case EPresetGroup::None: default: break;
    }

    // Selected group highlight bitmaps
    switch (mSelectedGroup)
    {
      case EPresetGroup::Vocals:       g.DrawBitmap(mVocalsSelectBmp, IRECT(14.f, 98.f, 14.f + 276.f, 98.f + 79.f));  break;
      case EPresetGroup::Pads:         g.DrawBitmap(mPadsSelectBmp,   IRECT(14.f,168.f, 14.f + 276.f, 168.f + 77.f)); break;
      case EPresetGroup::Drums:        g.DrawBitmap(mDrumsSelectBmp,  IRECT(14.f,236.f, 14.f + 276.f, 236.f + 79.f)); break;
      case EPresetGroup::Experimental: g.DrawBitmap(mExpSelectBmp,    IRECT(14.f,306.f, 14.f + 276.f, 306.f + 78.f)); break;
      case EPresetGroup::None: default: break;
    }

    // Labels
    g.DrawBitmap(mVocalsLabelBmp, IRECT(112.f,130.f, 194.f,149.f));
    g.DrawBitmap(mPadsLabelBmp,   IRECT( 97.f,199.f, 208.f,220.f));
    g.DrawBitmap(mDrumsLabelBmp,  IRECT( 48.f,268.f, 257.f,289.f));
    g.DrawBitmap(mExpLabelBmp,    IRECT( 58.f,338.f, 247.f,359.f));

    // Dividers per selected group (positions and count)
    {
      // Fixed positions and size
      const float X = 287.f;
      const float W = 266.f;
      const float H = 1.f;
      const float Ylist[4] = {160.f, 211.f, 262.f, 313.f};

      int needed = 0;
      switch (mSelectedGroup)
      {
        case EPresetGroup::Vocals:       needed = 4; break;
        case EPresetGroup::Pads:         needed = 4; break;
        case EPresetGroup::Drums:        needed = 3; break;
        case EPresetGroup::Experimental: needed = 2; break;
        default:                         needed = 0; break;
      }

      for (int i = 0; i < needed; ++i)
      {
        const IRECT r(X, Ylist[i], X + W, Ylist[i] + H);
        g.DrawBitmap(mDividerBmp, r);
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
    // Active overlay area (click outside closes the overlay and revert button)
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

    // Choose group
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
      SetDirty(false);
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

    if (mHoverGroup != EPresetGroup::None)
    {
      mHoverGroup = EPresetGroup::None;
      SetDirty(false);
    }

    if (auto* ui = GetUI())
      ui->SetMouseCursor(ECursor::ARROW);
  }

private:
  void UpdateHover(float x, float y)
  {
    const IRECT vocalsRect(19.f, 104.f, 285.f, 173.f);
    const IRECT padsRect  (19.f, 174.f, 285.f, 241.f);
    const IRECT drumsRect (19.f, 242.f, 285.f, 311.f);
    const IRECT expRect   (19.f, 312.f, 285.f, 380.f);

    EPresetGroup newHover = EPresetGroup::None;

    if      (vocalsRect.Contains(x, y)) newHover = EPresetGroup::Vocals;
    else if (padsRect.Contains(x,   y)) newHover = EPresetGroup::Pads;
    else if (drumsRect.Contains(x,  y)) newHover = EPresetGroup::Drums;
    else if (expRect.Contains(x,    y)) newHover = EPresetGroup::Experimental;

    if (newHover != mHoverGroup)
    {
      mHoverGroup = newHover;
      SetDirty(false);
    }

    if (auto* ui = GetUI())
      ui->SetMouseCursor(newHover != EPresetGroup::None ? ECursor::HAND
                                                        : ECursor::ARROW);
  }

  IBitmap mPageBitmap;
  IBitmap mVocalsSelectBmp, mPadsSelectBmp, mDrumsSelectBmp, mExpSelectBmp;
  IBitmap mVocalsLabelBmp,  mPadsLabelBmp,  mDrumsLabelBmp,  mExpLabelBmp;
  IBitmap mArrowBmp, mRevertBmp;
  IBitmap mDividerBmp;  // <— NEW

  EPresetGroup mSelectedGroup = EPresetGroup::Vocals;
  EPresetGroup mHoverGroup    = EPresetGroup::None;
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
    }
  }

private:
  static double Normalize(double v, double minV, double maxV)
  {
    return (v - minV) / (maxV - minV);
  }

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
        dlg->SendParameterValueFromUI(idx, Normalize(val, minV, maxV));
      };

      // Modulation / main
      sendNorm(kGain,            0.0,   0.0, 100.0);

      sendNorm(kTremRate,        0.0,   0.1, 20.0);
      sendNorm(kTremDepth,       0.0,   0.0, 100.0);

      sendNorm(kPanRate,         0.0,   0.1, 20.0);
      sendNorm(kPanDepth,        0.0,   0.0, 100.0);

      sendNorm(kPitchRate,       0.0,   0.1, 10.0);
      sendNorm(kPitchDepth,      0.0,   0.0, 100.0);

      sendNorm(kPhaserRate,      0.0,   0.1, 20.0);
      sendNorm(kPhaserDepth,     0.0,   0.0, 100.0);

      // Compressor
      sendNorm(kCompMix,         0.0,   0.0, 100.0);
      sendNorm(kCompThreshold,  -24.0, -60.0,  0.0);
      sendNorm(kCompRatio,       4.0,   1.0, 20.0);
      sendNorm(kCompGain,        0.0, -24.0, 24.0);
      sendNorm(kCompAttack,     10.0,   0.1, 100.0);
      sendNorm(kCompRelease,   100.0,   5.0, 1000.0);

      // Master / output
      sendNorm(kMasterIntensity, 0.0,   0.0, 100.0);
      sendNorm(kOutputLevel,     0.0, -24.0, 24.0);
    }
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
                      const IBitmap& dividerBmp) // <— NEW
  : IControl(bounds)
  , mButtonBitmap(buttonBitmap)
  , mPageBitmap(pageBitmap)
  , mVocalsSelectBmp(vocalsSelectBmp)
  , mPadsSelectBmp(padsSelectBmp)
  , mDrumsSelectBmp(drumsSelectBmp)
  , mExpSelectBmp(expSelectBmp)
  , mVocalsLabelBmp(vocalsLabelBmp)
  , mPadsLabelBmp(padsLabelBmp)
  , mDrumsLabelBmp(drumsLabelBmp)
  , mExpLabelBmp(expLabelBmp)
  , mArrowBmp(arrowBmp)
  , mRevertBmp(revertBmp)
  , mDividerBmp(dividerBmp)    // <— NEW
  {}

  void Draw(IGraphics& g) override
  {
    // Base bitmap
    g.DrawBitmap(mButtonBitmap, mRECT);

    // Hover overlay
    if (GetMouseIsOver())
    {
      IColor overlay(25, 101, 101, 101);
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

      // 1) overlay
      auto* overlay = new PresetsPageControl(fullRect,
                                             mPageBitmap,
                                             mVocalsSelectBmp,
                                             mPadsSelectBmp,
                                             mDrumsSelectBmp,
                                             mExpSelectBmp,
                                             mVocalsLabelBmp,
                                             mPadsLabelBmp,
                                             mDrumsLabelBmp,
                                             mExpLabelBmp,
                                             mArrowBmp,
                                             mRevertBmp,
                                             mDividerBmp); // <— pass divider
      ui->AttachControl(overlay);

      // 2) "Revert to Default" button paired with overlay
      const IRECT revertRect(419.f, 538.f, 541.f, 566.f);
      auto* revertButton = new RevertButtonControl(revertRect, overlay);
      ui->AttachControl(revertButton);

      overlay->SetRevertButton(revertButton);
    }
    SetDirty(false);
  }

private:
  IBitmap mButtonBitmap, mPageBitmap;
  IBitmap mVocalsSelectBmp, mPadsSelectBmp, mDrumsSelectBmp, mExpSelectBmp;
  IBitmap mVocalsLabelBmp,  mPadsLabelBmp,  mDrumsLabelBmp,  mExpLabelBmp;
  IBitmap mArrowBmp, mRevertBmp;
  IBitmap mDividerBmp; // <— NEW
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
// drawOverlay=false -> no shading (used for BIG knobs)
// drawOverlay=true  -> shading drawn (used for MID/SMALL knobs)
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

    // Soft glow behind the main text
    IColor shadowColor(10, 255, 255, 255);
    IText shadowText(18.f, shadowColor, "Roboto-Regular",
                     EAlign::Center, EVAlign::Middle);

    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
      {
        if (dx || dy)
          g.DrawText(shadowText, buf, IRECT(mRECT.L + dx, mRECT.T + dy,
                                            mRECT.R + dx, mRECT.B + dy));
      }

    // Main text
    IText text(18.f, COLOR_WHITE, "Roboto-Regular",
               EAlign::Center, EVAlign::Middle);
    g.DrawText(text, buf, mRECT);

    SetDirty(false);
  }

private:
  IPlugEffect* mPlugin = nullptr;
};

// =================== PLUGIN CONSTRUCTOR ===================

IPlugEffect::IPlugEffect(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // --- MAIN GAIN ---
  GetParam(kGain)->InitDouble("Gain", 0., 0., 100.0, 0.01, "%");

  // Tremolo
  GetParam(kTremRate)->InitDouble("Trem Rate", 0.0, 0.1, 20.0, 0.01, "Hz");
  GetParam(kTremDepth)->InitDouble("Trem Depth", 0.0, 0.0, 100.0, 1.0, "%");
  GetParam(kTremBypass)->InitBool("Trem Bypass", true);

  // Pan
  GetParam(kPanRate)->InitDouble("Pan Rate", 0.0, 0.1, 20.0, 0.01, "Hz");
  GetParam(kPanDepth)->InitDouble("Pan Depth", 0.0, 0.0, 100.0, 1.0, "%");
  GetParam(kPanBypass)->InitBool("Pan Bypass", true);

  // Pitch
  GetParam(kPitchRate)->InitDouble("Pitch Rate", 0.0, 0.1, 10.0, 0.01, "Hz");
  GetParam(kPitchDepth)->InitDouble("Pitch Depth", 0.0, 0.0, 100.0, 1.0, "%");
  GetParam(kPitchBypass)->InitBool("Pitch Bypass", true);

  // Phaser
  GetParam(kPhaserRate)->InitDouble("Phaser Rate", 0.0, 0.1, 20.0, 0.01, "Hz");
  GetParam(kPhaserDepth)->InitDouble("Phaser Depth", 0.0, 0.0, 100.0, 1.0, "%");
  GetParam(kPhaserBypass)->InitBool("Phaser Bypass", true);

  // Compressor
  GetParam(kCompMix)->InitDouble("Comp Mix", 100.0, 0.0, 100.0, 1.0, "%");
  GetParam(kCompThreshold)->InitDouble("Comp Threshold", -24.0, -60.0, 0.0, 0.1, "dB");
  GetParam(kCompRatio)->InitDouble("Comp Ratio", 4.0, 1.0, 20.0, 0.1, ":1");
  GetParam(kCompGain)->InitDouble("Comp Gain", 0.0, -24.0, 24.0, 0.1, "dB");
  GetParam(kCompAttack)->InitDouble("Comp Attack", 10.0, 0.1, 100.0, 0.1, "ms");
  GetParam(kCompRelease)->InitDouble("Comp Release", 100.0, 5.0, 1000.0, 1.0, "ms");
  GetParam(kCompBypass)->InitBool("Comp Bypass", true);

  // Master / Output
  GetParam(kBypass)->InitBool("Bypass", false);
  GetParam(kMasterIntensity)->InitDouble("Master Intensity", 0.0, 0.0, 100.0, 1.0, "%");
  GetParam(kOutputLevel)->InitDouble("Output Level", 0.0, -24.0, 24.0, 0.1, "dB");

  // On load — everything bypassed
  GetParam(kCompBypass)->Set(1.0);
  GetParam(kTremBypass)->Set(1.0);
  GetParam(kPanBypass)->Set(1.0);
  GetParam(kPitchBypass)->Set(1.0);
  GetParam(kPhaserBypass)->Set(1.0);

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->EnableMouseOver(true);
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_BLACK);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Inter-Medium",  INTER_FN);

    // BACKGROUND
    IBitmap bg = pGraphics->LoadBitmap(MAIN_BACKGROUND_FN, 1);
    const IRECT bounds = pGraphics->GetBounds();
    pGraphics->AttachControl(new IBitmapControl(bounds, bg));

    // Hover colors
    const IColor hoverColorButtons  = IColor(23, 184, 184, 184);
    const IColor hoverColorModules  = IColor(23, 14,  14,  14);
    const IColor hoverColorKnobs    = IColor(18, 184, 184, 184); // used for MID/SMALL

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

    // NEW: divider bitmap for preset list
    IBitmap dividerBmp      = pGraphics->LoadBitmap(PRESET_divider_FN, 1);

    IRECT preset_bounds(27.f, 71.f, 272.f, 97.f);
    pGraphics->AttachControl(new SelectPresetControl(
      preset_bounds, presetBtnBmp, presetsPageBmp,
      vocalsSelectBmp, padsSelectBmp, drumsSelectBmp, expSelectBmp,
      vocalsLabelBmp, padsLabelBmp, drumsLabelBmp, expLabelBmp,
      arrowBmp, revertBmp, dividerBmp)); // pass divider

    // ===== KNOBS =====
    IBitmap bigKnob   = pGraphics->LoadBitmap(BIG_KNOB_FN,   1);
    IBitmap midKnob   = pGraphics->LoadBitmap(MID_KNOB_FN,   1);
    IBitmap smallKnob = pGraphics->LoadBitmap(SMALL_KNOB_FN, 1);

    // BIG knob bounds
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
      tremRateRect,  bigKnob, kTremRate,  hoverColorKnobs, 10.f, /*drawOverlay*/false));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      tremDepthRect, bigKnob, kTremDepth, hoverColorKnobs, 10.f, /*drawOverlay*/false));

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      panRateRect,   bigKnob, kPanRate,   hoverColorKnobs, 10.f, /*drawOverlay*/false));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      panDepthRect,  bigKnob, kPanDepth,  hoverColorKnobs, 10.f, /*drawOverlay*/false));

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      pitchRateRect,  bigKnob, kPitchRate,  hoverColorKnobs, 10.f, /*drawOverlay*/false));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      pitchDepthRect, bigKnob, kPitchDepth, hoverColorKnobs, 10.f, /*drawOverlay*/false));

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      phaserRateRect,  bigKnob, kPhaserRate,  hoverColorKnobs, 10.f, /*drawOverlay*/false));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      phaserDepthRect, bigKnob, kPhaserDepth, hoverColorKnobs, 10.f, /*drawOverlay*/false));

    // MID / SMALL knobs with hover overlay enabled
    const float kMidW = 19.f,  kMidH = 19.f;
    const float kSmW  = 14.f,  kSmH  = 14.f;

    // COMP: Mix / Threshold / Ratio / Gain (mid)
    IRECT compMixRect     ( 47.f,   512.f,  47.f + kMidW, 512.f + kMidH);
    IRECT compThreshRect  (127.5f,  512.f, 127.5f + kMidW, 512.f + kMidH);
    IRECT compRatioRect   (203.5f,  512.f, 203.5f + kMidW, 512.f + kMidH);
    IRECT compGainRect    (274.5f,  513.f, 274.5f + kMidW, 513.f + kMidH);

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compMixRect, midKnob, kCompMix, hoverColorKnobs, 10.f, /*drawOverlay*/true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compThreshRect, midKnob, kCompThreshold, hoverColorKnobs, 10.f, /*drawOverlay*/true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compRatioRect, midKnob, kCompRatio, hoverColorKnobs, 10.f, /*drawOverlay*/true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compGainRect, midKnob, kCompGain, hoverColorKnobs, 10.f, /*drawOverlay*/true));

    // COMP: Attack / Release (small)
    IRECT compAttackRect  (351.2f, 492.2f, 351.2f + kSmW, 492.2f + kSmH);
    IRECT compReleaseRect (351.2f, 540.2f, 351.2f + kSmW, 540.2f + kSmH);

    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compAttackRect, smallKnob, kCompAttack,  hoverColorKnobs, 10.f, /*drawOverlay*/true));
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      compReleaseRect, smallKnob, kCompRelease, hoverColorKnobs, 10.f, /*drawOverlay*/true));

    // MASTER: Intensity (mid)
    IRECT masterIntRect(459.6f, 512.8f, 459.6f + kMidW, 512.8f + kMidH);
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      masterIntRect, midKnob, kMasterIntensity, hoverColorKnobs, 10.f, /*drawOverlay*/true));

    // OUTPUT: Level (mid)
    IRECT outLevelRect(956.5f, 544.2f, 956.5f + kMidW, 544.2f + kMidH);
    pGraphics->AttachControl(new HoverKnobRotaterControl(
      outLevelRect, midKnob, kOutputLevel, hoverColorKnobs, 10.f, /*drawOverlay*/true));

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

      // TODO: add model processing if needed
      const sample proc = inS;

      const sample outS = bypass ? inS : (sample)(proc * outAmp);
      outputs[c][s] = outS;

      const double d = (double)outS;
      sumSquares += d * d;
      ++sampleCount;
    }
  }

  // Block RMS -> dB for UI
  if (sampleCount > 0)
  {
    if (sumSquares <= 0.0)
      mOutputLevelDB = 0.0; // absolute silence -> show 0.0 dB
    else
    {
      const double rms = std::sqrt(sumSquares / (double)sampleCount);
      mOutputLevelDB = 20.0 * std::log10(rms);
    }
  }
}
#endif
