// DynaCoreControls.h
// All custom UI controls — buttons, knobs, meters, labels, preset overlay.
// Included from DynaCore.cpp, not compiled on its own.
//
// Author: Vahram Saakian, UCM TFG 2025-2026

#pragma once

#include "DynaCorePresets.h"
#include "DynaCore.h"
#include "IControls.h"
#include <cstdio>
#include <cmath>
#include "wdlstring.h"

// ============================================================
//  UI utilities
// ============================================================

// draws a tinted overlay on a rect (for hover effect)
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

  // smoothing for meter bars — rises fast, falls slow
  inline float MeterSmooth(float current, float target)
  {
    float diff = target - current;
    float rate = (diff > 0.f) ? 0.18f : 0.07f;
    return current + diff * rate;
  }
}

// checks if DSP stopped updating (playback paused)
struct StaleDetector
{
  uint64_t lastCount = 0;
  int frames = 100;

  bool Update(uint64_t count)
  {
    if (count != lastCount) { frames = 0; lastCount = count; }
    else { frames++; }
    return frames > 10;
  }
};

// base class for controls that show hand cursor on hover
class HandCursorControl : public IControl
{
public:
  using IControl::IControl;

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
};

// ============================================================
//  Auto-gate logic (turns off module when rate+depth are both 0)
// ============================================================

// turns off a module when both Rate and Depth are zero
// static guard prevents infinite loop (setting bypass triggers this again)
static void AutoGateModulesFromParams(IEditorDelegate* dlg, int changedParamIdx)
{
  if (!dlg) return;

  // guard to prevent this function calling itself
  static bool sInAutoGate = false;
  if (sInAutoGate) return;
  sInAutoGate = true;

  // get the raw param value
  auto GetPlain = [dlg](int pIdx) -> double
  {
    if (IParam* p = dlg->GetParam(pIdx))
      return p->Value();
    return 0.0;
  };

  // check if rate is above the dead zone (or sync mode is on, which always
  // produces a non-zero rate)
  auto IsRateOn = [dlg](int rateIdx) -> bool
  {
    const int syncIdx = RateSyncParamIdxForRate(rateIdx);
    if (syncIdx >= 0)
      if (IParam* sp = dlg->GetParam(syncIdx))
        if (sp->Bool()) return true;  // sync mode always plays at some Hz

    if (IParam* p = dlg->GetParam(rateIdx))
    {
      const double hz = ApplyRateDeadZone(rateIdx, p->Value());
      return hz > 0.0;
    }
    return false;
  };

  // check if depth is not zero
  auto IsDepthOn = [dlg](int depthIdx) -> bool
  {
    if (IParam* p = dlg->GetParam(depthIdx))
      return p->Value() > 0.0;
    return false;
  };

  // set bypass only if it changed
  auto SetBypass = [dlg](int bypassIdx, bool bypass)
  {
    const double target = bypass ? 1.0 : 0.0;  // 1.0 = bypassed, 0.0 = active

    if (IParam* bp = dlg->GetParam(bypassIdx))
    {
      if (std::fabs(bp->GetNormalized() - target) > 1e-9)  // only send if different
        dlg->SendParameterValueFromUI(bypassIdx, target);
    }
  };

  // maps each module's rate, depth, and bypass param indices
  struct ModGate { int rateIdx; int depthIdx; int bypassIdx; };

  static const ModGate kMods[] =
  {
    { kTremRate,   kTremDepth,   kTremBypass   },
    { kPanRate,    kPanDepth,    kPanBypass    },
    { kPitchRate,  kPitchDepth,  kPitchBypass  },
    { kPhaserRate, kPhaserDepth, kPhaserBypass }
  };

  // if the changed param is a rate or depth, check both for that module
  for (const auto& m : kMods)
  {
    if (changedParamIdx != m.rateIdx && changedParamIdx != m.depthIdx)
      continue;  // not this module, skip

    // module is ON only if both rate and depth are not zero
    const bool on = IsRateOn(m.rateIdx) && IsDepthOn(m.depthIdx);
    SetBypass(m.bypassIdx, !on);  // bypass = !on
  }

  // compressor: bypass when Mix is zero
  if (changedParamIdx == kCompMix)
  {
    const bool compOn = GetPlain(kCompMix) > 0.0;
    SetBypass(kCompBypass, !compOn);
  }

  sInAutoGate = false;  // done, release guard
}

class RevertButtonControl; // forward declaration

// ============================================================
//  UI control classes
// ============================================================

// Fullscreen preset overlay — categories on left, preset names on right
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

    // open on the last viewed group, show selection only if it belongs here
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

  // rect for a preset slot (0-3) on the right side
  static IRECT PresetSlotRect(int idx)
  {
    constexpr float X = 287.f, W = 276.f, H = 51.f;
    constexpr float Y[] = { 104.f, 155.f, 206.f, 257.f };
    if (idx >= 0 && idx <= 3) return IRECT(X, Y[idx], X + W, Y[idx] + H);
    return IRECT();
  }

  void Draw(IGraphics& g) override
  {
    // overlay background
    g.DrawBitmap(mPageBitmap, mRECT);

    // revert button base bitmap (the actual button is a separate control on top)
    const IRECT revertRect(413.f, 536.f, 413.f + 130.f, 536.f + 36.f);
    g.DrawBitmap(mRevertBmp, revertRect);

    // group rects (for hover and click detection)
    const IRECT vocalsRect = GroupRect(EPresetGroup::Vocals);
    const IRECT padsRect   = GroupRect(EPresetGroup::Pads);
    const IRECT drumsRect  = GroupRect(EPresetGroup::Drums);
    const IRECT expRect    = GroupRect(EPresetGroup::Experimental);

    // group hover overlays
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

    // selected group highlight bitmaps
    switch (mSelectedGroup)
    {
      case EPresetGroup::Vocals:       g.DrawBitmap(mVocalsSelectBmp, IRECT(14.f,  98.f, 14.f + 276.f,  98.f + 79.f)); break;
      case EPresetGroup::Pads:         g.DrawBitmap(mPadsSelectBmp,   IRECT(14.f, 168.f, 14.f + 276.f, 168.f + 77.f)); break;
      case EPresetGroup::Drums:        g.DrawBitmap(mDrmsSelectBmp,   IRECT(14.f, 236.f, 14.f + 276.f, 236.f + 79.f)); break;
      case EPresetGroup::Experimental: g.DrawBitmap(mExpSelectBmp,    IRECT(14.f, 306.f, 14.f + 276.f, 306.f + 78.f)); break;
      case EPresetGroup::None: default: break;
    }

    // group labels
    g.DrawBitmap(mVocalsLabelBmp, IRECT(112.f,130.f, 194.f,149.f));
    g.DrawBitmap(mPadsLabelBmp,   IRECT( 97.f,199.f, 208.f,220.f));
    g.DrawBitmap(mDrumsLabelBmp,  IRECT( 48.f,268.f, 257.f,289.f));
    g.DrawBitmap(mExpLabelBmp,    IRECT( 58.f,338.f, 247.f,359.f));

    // dividers for the selected group
    {
      const float X = 287.f;
      const float W = 266.f;
      const float H = 1.f;
      const float Ylist[4] = {154.f, 205.f, 256.f, 307.f};
      const int needed = GetPresetCountGlobal(mSelectedGroup);

      for (int i = 0; i < needed && i < 4; ++i)
        g.DrawBitmap(mDividerBmp, IRECT(X, Ylist[i], X + W, Ylist[i] + H));
    }

    // ----- preset pointer bullets (right side list) -----
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

    // ----- preset names (Inter-Medium, 15px, #CAD8E6) -----
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

    // preset hover overlay
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

    // selected preset highlight (in the current group)
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

    // arrow next to the current selection
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
    // click outside the overlay — close everything
    const IRECT active(19.f, 104.f, 554.f, 578.f);

    if (!active.Contains(x, y))
    {
      if (IGraphics* ui = GetUI())
      {
        ui->SetMouseCursor(ECursor::ARROW);
        if (mRevertButton)
        {
          ui->RemoveControl(mRevertButton);  // remove revert button
          mRevertButton = nullptr;
        }
        ui->RemoveControl(this);  // remove overlay itself
      }
      return;
    }

    // --- group selection (left side tabs) ---
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
      mSelectedGroup   = newGroup;       // switch to the clicked group
      gLastPresetGroup = newGroup;       // remember for next time

      // highlight only if the active preset belongs to this group
      mSelectedPresetIndex = (gSelectedGroupGlobal == mSelectedGroup) ? gSelectedPresetGlobal : -1;

      mHoverPresetIndex  = -1;
      SetDirty(false);
      return;
    }

    // --- preset selection (right side list) ---
    int clickedPreset = -1;
    for (int i = 0; i < 4; ++i)
      if (PresetSlotRect(i).Contains(x, y)) { clickedPreset = i; break; }

    if (clickedPreset >= 0)
    {
      const int count = GetPresetCountGlobal(mSelectedGroup);
      if (clickedPreset < count)
      {
        // save selection globally (only one preset can be active)
        mSelectedPresetIndex  = clickedPreset;
        gSelectedGroupGlobal  = mSelectedGroup;
        gSelectedPresetGlobal = clickedPreset;

        // update the preset name label on main page
        if (const char* nm = GetPresetName(mSelectedGroup, clickedPreset))
          gSelectedPresetName = nm;
        else
          gSelectedPresetName = kPresetName_None;

        // send all preset values to the DAW
        ApplyPresetToParams(GetDelegate(), mSelectedGroup, clickedPreset);

        SetDirty(false);

        if (auto* ui = GetUI())
        {
          ui->SetAllControlsDirty();  // refresh knobs, labels, meters

          // close overlay after selecting a preset
          ui->SetMouseCursor(ECursor::ARROW);

          if (mRevertButton)
          {
            ui->RemoveControl(mRevertButton);
            mRevertButton = nullptr;
          }

          ui->RemoveControl(this);  // remove overlay
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
  // detect what the mouse is hovering over (group tab or preset name)
  void UpdateHover(float x, float y)
  {
    // check group tabs on the left side
    const IRECT vocalsRect = GroupRect(EPresetGroup::Vocals);
    const IRECT padsRect   = GroupRect(EPresetGroup::Pads);
    const IRECT drumsRect  = GroupRect(EPresetGroup::Drums);
    const IRECT expRect    = GroupRect(EPresetGroup::Experimental);

    EPresetGroup newHoverGroup = EPresetGroup::None;

    if      (vocalsRect.Contains(x, y)) newHoverGroup = EPresetGroup::Vocals;
    else if (padsRect.Contains(x,   y)) newHoverGroup = EPresetGroup::Pads;
    else if (drumsRect.Contains(x,  y)) newHoverGroup = EPresetGroup::Drums;
    else if (expRect.Contains(x,    y)) newHoverGroup = EPresetGroup::Experimental;

    // check preset names on the right side
    const int count = GetPresetCountGlobal(mSelectedGroup);

    int newHoverPreset = -1;
    for (int i = 0; i < count && i < 4; ++i)
      if (PresetSlotRect(i).Contains(x, y)) { newHoverPreset = i; break; }

    // only redraw if hover state actually changed
    bool changed = false;

    if (newHoverGroup != mHoverGroup)       { mHoverGroup = newHoverGroup; changed = true; }
    if (newHoverPreset != mHoverPresetIndex){ mHoverPresetIndex = newHoverPreset; changed = true; }

    if (changed) SetDirty(false);

    // show hand cursor when hovering over a clickable area
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

// Revert button — resets to defaults and closes the overlay
class RevertButtonControl : public HandCursorControl
{
public:
  RevertButtonControl(const IRECT& bounds, PresetsPageControl* overlayToClose)
  : HandCursorControl(bounds), mOverlay(overlayToClose)
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

  // click — apply defaults, close overlay
  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    ApplyDefaultPreset();

    if (IGraphics* ui = GetUI())
    {
      if (mOverlay) ui->RemoveControl(mOverlay);  // close preset overlay
      ui->RemoveControl(this);                     // remove this button
      ui->SetAllControlsDirty();                   // refresh preset label
    }
  }

private:
  // send defaults to DAW and clear selection
  void ApplyDefaultPreset()
  {
    if (auto* dlg = GetDelegate())
    {
      // send each value through the UI so undo works
      ApplyDefaultPresetParams([dlg](int pIdx, double plain)
      {
        if (IParam* p = dlg->GetParam(pIdx))
        {
          const double clamped = std::clamp(plain, p->GetMin(), p->GetMax());
          dlg->SendParameterValueFromUI(pIdx, p->ToNormalized(clamped));
        }
      });
    }

    // no preset selected
    gSelectedGroupGlobal  = EPresetGroup::None;
    gSelectedPresetGlobal = -1;
    gSelectedPresetName   = kPresetName_None;

    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();
  }

  PresetsPageControl* mOverlay = nullptr;
};

// "SELECT PRESET" button — opens the preset overlay on click
class SelectPresetControl : public HandCursorControl
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
  : HandCursorControl(bounds)
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

  // click — open the fullscreen preset overlay
  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (IGraphics* ui = GetUI())
    {
      // covers the whole plugin window
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

      // add revert button on top
      const IRECT revertRect(419.f, 538.f, 541.f, 566.f);
      auto* revertButton = new RevertButtonControl(revertRect, overlay);
      ui->AttachControl(revertButton);

      overlay->SetRevertButton(revertButton);  // so overlay can remove it on close
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

// Prev/next arrows — step through presets across all groups
class PresetStepButtonControl : public HandCursorControl
{
public:
  PresetStepButtonControl(const IRECT& bounds, const IBitmap& bmp, int dir)
  : HandCursorControl(bounds)
  , mBmp(bmp)
  , mDir(dir)
  {}

  void Draw(IGraphics& g) override
  {
    g.DrawBitmap(mBmp, mRECT);

    if (GetMouseIsOver())
    {
      // hover tint
      IColor overlay(30, 101, 101, 101);
      DrawHoverOverlay(g, mRECT, overlay, 5.f);
    }
  }

  // click — go to next/previous preset
  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    auto* dlg = GetDelegate();
    auto* ui  = GetUI();
    if (!dlg || !ui) return;

    EPresetGroup g = gSelectedGroupGlobal;
    int idx = gSelectedPresetGlobal;

    StepPresetCarousel(g, idx, mDir);  // move +1 or -1, wraps across groups

    // update global state
    gSelectedGroupGlobal  = g;
    gSelectedPresetGlobal = idx;

    if (const char* nm = GetPresetName(g, idx))
      gSelectedPresetName = nm;
    else
      gSelectedPresetName = kPresetName_None;

    ApplyPresetToParams(dlg, g, idx);  // send values to DAW

    ui->SetAllControlsDirty();  // refresh everything
    SetDirty(false);
  }

private:
  IBitmap mBmp;
  int mDir = 1;  // +1 = next, -1 = previous
};

// Toggle button with hover tint (bitmap-based)
class HoverButtonWithOverlay : public HandCursorControl
{
public:
  HoverButtonWithOverlay(const IRECT& bounds,
                         int paramIdx,
                         const IBitmap& offBitmap,
                         const IBitmap& onBitmap,
                         const IColor& hoverColor,
                         float cornerRadius)
  : HandCursorControl(bounds, paramIdx)
  , mParamIdx(paramIdx)
  , mOff(offBitmap), mOn(onBitmap)
  , mHoverColor(hoverColor), mCornerRadius(cornerRadius)
  {}

  void Draw(IGraphics& g) override
  {
    if (const IParam* p = GetParam())
      SetValue(p->GetNormalized());  // sync with param (could change from preset)

    const bool bypass = (GetValue() >= 0.5f);  // 1 = bypassed
    g.DrawBitmap(bypass ? mOff : mOn, mRECT);  // show correct bitmap

    if (GetMouseIsOver())
      DrawHoverOverlay(g, mRECT, mHoverColor, mCornerRadius);
  }

  // click — toggle on/off
  void OnMouseDown(float, float, const IMouseMod&) override
  {
    double cur = GetValue();
    if (const IParam* p = GetParam())
      cur = p->GetNormalized();  // sync with actual value

    const double newNorm = (cur >= 0.5) ? 0.0 : 1.0;  // flip

    SetValue(newNorm);

    if (auto* dlg = GetDelegate())
      dlg->SendParameterValueFromUI(mParamIdx, newNorm);  // send to DAW

    SetDirty(false);
  }

private:
  int     mParamIdx = -1;
  IBitmap mOff, mOn;
  IColor  mHoverColor;
  float   mCornerRadius = 0.f;
};

// Arc ring behind a knob showing the current value (ignores mouse)
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

  // draw arc from start to current value
  void Draw(IGraphics& g) override
  {
    const IParam* p = GetParam();
    if (!p) return;

    const float norm = (float) p->GetNormalized();

    const float cx = mRECT.MW();     // center
    const float cy = mRECT.MH();
    const float radius = 0.5f * std::min(mRECT.W(), mRECT.H()) - (mThickness * 0.5f);
    float end = mStartDeg + norm * mSweepDeg;  // end angle

    // small overshoot at 100% to close the visual gap
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

// Knob control with hover effect, dead zone for rate knobs, and auto-gate
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
    constexpr double kStart = 0.0;   // knob rotation range
    constexpr double kEnd   = 270.0;

    // read actual param so knob stays in sync after preset load
    double norm = GetValue();
    if (const IParam* p = GetParam())
      norm = p->GetNormalized();  // 0..1

    const double angle = kStart + norm * (kEnd - kStart);  // 0..1 to degrees

    // draw rotated knob
    const IRECT knobR = (mKnobInnerPad != 0.f) ? mRECT.GetPadded(-mKnobInnerPad) : mRECT;
    g.DrawRotatedBitmap(mBitmap, knobR.MW(), knobR.MH(), angle, &mBlend);

    // value arc on top
    if (mDrawValueArc)
    {
      const float t = (float) norm;

      // ring rect (padded if needed)
      const IRECT arcR = (mArcPad != 0.f) ? mRECT.GetPadded(mArcPad) : mRECT;

      const float cx     = arcR.MW();
      const float cy     = arcR.MH();
      const float radius = 0.5f * std::min(arcR.W(), arcR.H()) - (mArcThickness * 0.5f);

      // angles are in degrees
      const float start = mArcStartDeg;
      const float end   = mArcStartDeg + (mArcSweepDeg * t);

      if (t > 0.f)
      {
        g.PathClear();
        g.PathArc(cx, cy, radius, start, end);
        g.PathStroke(mArcColor, mArcThickness);
      }
    }

    // hover overlay
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
    // sync with param before drag so it doesn't jump after preset
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

  // dragging — slower sensitivity, check dead zone, auto-gate
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    static constexpr float kDragTightness = 0.45f;  // less sensitive drag
    IBKnobRotaterControl::OnMouseDrag(x, y, dX, dY * kDragTightness, mod);
    AfterKnobChange();
  }

  // scroll wheel — same as drag
  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    IBKnobRotaterControl::OnMouseWheel(x, y, mod, d);
    AfterKnobChange();
  }

private:
  // returns true if this rate knob is currently in BPM-sync mode
  bool IsInSyncMode() const
  {
    if (!IsRateParamIdx(mParamIdxLocal)) return false;
    auto* dlg = const_cast<HoverKnobRotaterControl*>(this)->GetDelegate();
    if (!dlg) return false;
    const int syncIdx = RateSyncParamIdxForRate(mParamIdxLocal);
    if (syncIdx < 0) return false;
    if (IParam* sp = dlg->GetParam(syncIdx))
      return sp->Bool();
    return false;
  }

  // runs after drag or scroll
  void AfterKnobChange()
  {
    if (IsInSyncMode())
      EnforceRateSyncSnap();   // sync mode: snap to nearest division
    else
      EnforceRateDeadZone();   // free mode: snap to 0 below threshold

    if (auto* dlg = GetDelegate())
      AutoGateModulesFromParams(dlg, mParamIdxLocal);
    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();
    SetDirty(true);
  }

  // snap rate knob to the nearest discrete sync division
  void EnforceRateSyncSnap()
  {
    if (!IsRateParamIdx(mParamIdxLocal)) return;  // not a rate knob

    auto* dlg = GetDelegate();
    if (!dlg) return;

    const double norm   = GetValue();                       // current 0..1
    const int    divIdx = NormToSyncDivIdx(norm);           // pick nearest division
    const double snap   = SyncDivIdxToNorm(divIdx);         // back to 0..1

    if (std::fabs(snap - norm) < 1e-9)
      return;  // already on a division, no change needed

    SetValue(snap);                                       // update knob
    dlg->SendParameterValueFromUI(mParamIdxLocal, snap);  // send to DAW

    SetDirty(false);
    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();  // update value label
  }

  // snap rate to 0 if it's below the dead zone threshold
  void EnforceRateDeadZone()
  {
    if (!IsRateParamIdx(mParamIdxLocal))
      return;  // not a rate knob

    auto* dlg = GetDelegate();
    const IParam* p = GetParam();
    if (!dlg || !p)
      return;

    const double hz = p->FromNormalized(GetValue());       // current Hz
    const double fixedHz = ApplyRateDeadZone(mParamIdxLocal, hz);  // snap if too low

    if (fixedHz == hz)
      return;  // no change

    const double fixedNorm = p->ToNormalized(fixedHz);  // convert back to 0..1
    SetValue(fixedNorm);                                 // update knob
    dlg->SendParameterValueFromUI(mParamIdxLocal, fixedNorm);  // send to DAW

    SetDirty(false);
    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();  // update value label
  }

private:
  int    mParamIdxLocal = -1;
  IColor mHoverColor;
  float  mCornerRadius = 0.f;
  bool   mDrawOverlay  = true;

  // value arc settings
  bool   mDrawValueArc = false;
  float  mArcPad = 0.f;
  float  mArcThickness = 2.f;
  float  mKnobInnerPad = 0.f;
  IColor mArcColor = IColor(255, 0x50, 0x62, 0x74);
  float  mArcStartDeg = 225.f;
  float  mArcSweepDeg = 270.f;
};

// output level meter — color gradient bars + dB text
class StereoLevelMeterControl : public IControl
{
public:
  StereoLevelMeterControl(const IRECT& bounds, DynaCore* plugin)
  : IControl(bounds), mPlugin(plugin)
  { mIgnoreMouse = true; }

  void Draw(IGraphics& g) override
  {
    if (!mPlugin) return;

    // check if DSP is still running
    bool dspStale = mStale.Update(mPlugin->GetMeterUpdateCount());
    bool bypassed = mPlugin->GetParam(kBypass)->Bool();
    bool forceZero = dspStale || bypassed;  // show silence if stopped or bypassed

    const double rawDbL = forceZero ? -100.0 : mPlugin->GetOutputLevelDBL();
    const double rawDbR = forceZero ? -100.0 : mPlugin->GetOutputLevelDBR();

    double rawMaxDB = std::max(rawDbL, rawDbR);  // for text display

    // convert dB to 0..1 for drawing (-54dB = 0, 0dB = 1)
    auto dbToNorm = [](double db) -> float {
      double n = (db - (-54.0)) / (0.0 - (-54.0));
      if (n < 0.0) n = 0.0;
      if (n > 1.0) n = 1.0;
      return static_cast<float>(n);
    };

    float targetL = dbToNorm(rawDbL);
    float targetR = dbToNorm(rawDbR);

    // smooth the values so bars move naturally
    mVisualL = MeterSmooth(mVisualL, targetL);
    mVisualR = MeterSmooth(mVisualR, targetR);
    if (mVisualL < 0.002f) mVisualL = 0.f;  // snap to zero
    if (mVisualR < 0.002f) mVisualR = 0.f;

    float normL = mVisualL;
    float normR = mVisualR;

    // bar coordinates
    constexpr float kBarTop = 78.f, kBarBottom = 466.f;
    constexpr float kLeftBarL = 955.6f, kLeftBarR = 963.6f;
    constexpr float kRightBarL = 965.6f, kRightBarR = 973.6f;
    constexpr float kBarH = kBarBottom - kBarTop;

    // draw meter bar pixel by pixel with color gradient
    auto drawBar = [&](float barL, float barR, float norm) {
      if (norm <= 0.001f) return;
      float fillH = kBarH * norm;         // bar height in pixels
      float fillTop = kBarBottom - fillH;  // start from bottom
      int yStart = static_cast<int>(fillTop);
      int yEnd   = static_cast<int>(kBarBottom);
      for (int py = yStart; py < yEnd; py++) {
        float pos = (kBarBottom - static_cast<float>(py)) / kBarH;  // 0=bottom, 1=top
        int r, gr, b;
        if (pos < 0.55f) {            // green zone
          float t = pos / 0.55f;
          r = int(76 + t * (180 - 76));
          gr = int(209 + t * (220 - 209));
          b = int(55 + t * (20 - 55));
        } else if (pos < 0.85f) {     // yellow zone
          float t = (pos - 0.55f) / 0.30f;
          r = int(180 + t * (241 - 180));
          gr = int(220 + t * (196 - 220));
          b = int(20 + t * (15 - 20));
        } else {                       // red zone
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

    // dB text — extra slow smoothing so it doesn't jump
    double targetTextDB = (rawMaxDB > -54.0) ? rawMaxDB : -100.0;
    if (targetTextDB > mTextDB)
      mTextDB += (targetTextDB - mTextDB) * 0.030;  // slow attack
    else
      mTextDB += (targetTextDB - mTextDB) * 0.016;  // very slow release

    char buf[32];
    float maxVis = std::max(mVisualL, mVisualR);
    if (maxVis < 0.001f && mTextDB < -53.0) {
      std::snprintf(buf, sizeof(buf), " -inf");           // silent
    } else {
      // avoid showing "-0.0"
      double displayDB = (mTextDB > -0.05 && mTextDB < 0.0) ? 0.0 : mTextDB;
      std::snprintf(buf, sizeof(buf), "%5.1f", displayDB);
    }

    IRECT textRect(mRECT.L + 5.5f, mRECT.B - 46.f, mRECT.R + 9.5f, mRECT.B - 28.f);
    g.DrawText(mDbText, buf, textRect);

    SetDirty(false);
  }

private:
  DynaCore* mPlugin = nullptr;
  float mVisualL = 0.f;
  float mVisualR = 0.f;
  double mTextDB = -100.0; // smoothed dB for text
  const IText mDbText{14.f, COLOR_WHITE, "Inter-Semi-Bold", EAlign::Center, EVAlign::Middle};
  StaleDetector mStale;
};

// GR meter — shows how much the compressor is reducing (0-30 dB)
class GRMeterControl : public IControl
{
public:
  GRMeterControl(const IRECT& bounds, DynaCore* plugin)
  : IControl(bounds), mPlugin(plugin)
  { mIgnoreMouse = true; }

  void Draw(IGraphics& g) override
  {
    if (!mPlugin) return;

    // check if DSP is running and compressor is active
    bool dspStale = mStale.Update(mPlugin->GetGRUpdateCount());
    bool bypassed = mPlugin->GetParam(kBypass)->Bool();
    bool compBypassed = mPlugin->GetParam(kCompBypass)->Bool();
    bool forceZero = dspStale || bypassed || compBypassed;  // hide when not compressing

    double grL = forceZero ? 0.0 : mPlugin->GetGainReductionL();  // GR in dB
    double grR = forceZero ? 0.0 : mPlugin->GetGainReductionR();

    // normalize to 0..1 (0 = no reduction, 1 = 30 dB)
    float targetL = static_cast<float>(std::min(grL / 30.0, 1.0));
    float targetR = static_cast<float>(std::min(grR / 30.0, 1.0));
    if (targetL < 0.f) targetL = 0.f;
    if (targetR < 0.f) targetR = 0.f;

    // smooth the bars so they move naturally
    mVisualL = MeterSmooth(mVisualL, targetL);
    mVisualR = MeterSmooth(mVisualR, targetR);
    if (mVisualL < 0.002f) mVisualL = 0.f;   // snap to zero
    if (mVisualR < 0.002f) mVisualR = 0.f;
    // snap to full when close to max (smoother never quite reaches 1.0)
    if (mVisualL > 0.96f && targetL >= 0.999f) mVisualL = 1.0f;
    if (mVisualR > 0.96f && targetR >= 0.999f) mVisualR = 1.0f;

    // bar positions — 10px wide bars inside the GR rail
    constexpr float kBarTop    = 499.f, kBarBottom = 559.f;
    constexpr float kLeftBarL  = 416.f, kLeftBarR  = 426.f;  // left bar
    constexpr float kRightBarL = 452.f, kRightBarR = 462.f;  // right bar
    constexpr float kBarH      = kBarBottom - kBarTop;       // total height
    constexpr float cr         = 5.f;                        // corner radius

    // capsule bar — rounded on both ends, clipped at bottom
    const IColor barColor(255, 211, 222, 242);
    g.PathClipRegion(IRECT(kLeftBarL, kBarTop, kRightBarR, kBarBottom));

    auto drawBar = [&](float barL, float barRight, float norm) {
      if (norm <= 0.001f) return;
      float fillH = std::max(kBarH * norm, 2.f * cr); // min = full pill shape
      float drawBottom = kBarTop + fillH;
      // fade in: opacity ramps up while bar is at minimum pill size
      float fadeT = 2.f * cr / kBarH;
      int alpha = (norm >= fadeT) ? 255 : static_cast<int>(255.f * norm / fadeT);
      IColor col(alpha, barColor.R, barColor.G, barColor.B);
      g.FillRoundRect(col, IRECT(barL, kBarTop, barRight, drawBottom), cr);
    };

    drawBar(kLeftBarL, kLeftBarR, mVisualL);
    drawBar(kRightBarL, kRightBarR, mVisualR);
    g.PathClipRegion();

    SetDirty(false);
  }

private:
  DynaCore* mPlugin = nullptr;
  float mVisualL = 0.f;
  float mVisualR = 0.f;
  StaleDetector mStale;
};

// value label under big knobs (shows Hz or %)
class BigKnobValueTextControl : public IControl
{
public:
  BigKnobValueTextControl(const IRECT& bounds, int paramIdx)
  : IControl(bounds)
  , mParamIdx(paramIdx)
  {
    mIgnoreMouse = true;
    
    // make rate labels wider so "20.00Hz" fits
    if (IsRateParamIdx(mParamIdx))
    {
      const float extra = bounds.W() * 0.15f;
      mRECT.L -= extra * 0.5f;
      mRECT.R += extra * 0.5f;
    }
  }

  // format and draw the value text
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
      // in sync mode the knob picks a musical division — show its label
      bool syncOn = false;
      const int syncIdx = RateSyncParamIdxForRate(mParamIdx);  // matching toggle
      if (syncIdx >= 0)
        if (IParam* sp = dlg->GetParam(syncIdx))
          syncOn = sp->Bool();  // read the toggle state

      if (syncOn)
      {
        const int divIdx = NormToSyncDivIdx(p->GetNormalized());      // pick division
        std::snprintf(buf, sizeof(buf), "%s", SyncDivLabel(divIdx));  // e.g. "1/8d"
      }
      else
      {
        double hz = p->Value();
        hz = ApplyRateDeadZone(mParamIdx, hz);         // snap to 0 if too low
        std::snprintf(buf, sizeof(buf), "%.2fHz", hz);
      }
    }
    else
    {
      const double percent = p->Value();
      std::snprintf(buf, sizeof(buf), "%.0f%%", percent);
    }

    g.DrawText(mValueText, buf, mRECT);
    SetDirty(false);  // redraw every frame
  }

private:
  int mParamIdx = -1;
  const IText mValueText{17.f, IColor(255, 53, 66, 80), "Inter-Regular", EAlign::Center, EVAlign::Middle};
};

// Small text button that lights up when on (used for the auto-gain "Auto" toggle)
// when active, reads the auto-gain value from the DSP and moves the Gain knob to match
class SmallTextToggleControl : public HandCursorControl
{
public:
  // smoothing coefficients (60fps)
  static constexpr double kAttackCoeff  = 0.84;   // ~100ms rise
  static constexpr double kReleaseCoeff = 0.965;   // ~500ms fall

  SmallTextToggleControl(const IRECT& bounds, int paramIdx, const char* label, DynaCore* plug)
  : HandCursorControl(bounds, paramIdx)
  , mParamIdx(paramIdx)
  , mPlug(plug)
  {
    std::strncpy(mLabel, label, sizeof(mLabel) - 1);
    mLabel[sizeof(mLabel) - 1] = '\0';
  }

  void Draw(IGraphics& g) override
  {
    if (const IParam* p = GetParam())
      SetValue(p->GetNormalized());

    const bool on = (GetValue() >= 0.5);

    // when on, update Gain knob to track actual GR
    if (on && mPlug)
    {
      auto* dlg = GetDelegate();
      if (dlg)
      {
        // check if DSP is running
        uint64_t grCount = mPlug->GetGRUpdateCount();
        bool dspRunning = (grCount != mLastGRCount);
        mLastGRCount = grCount;

        // target = the auto-gain value the DSP is applying
        // (already smoothed in DSP, so the knob shows what is being compensated)
        double targetGR = dspRunning ? mPlug->GetAutoGainMakeupDB() : 0.0;

        // light visual smoother — for the soft fall when DSP pauses or is bypassed
        double coeff = (targetGR > mSmoothedGR) ? kAttackCoeff : kReleaseCoeff;
        mSmoothedGR = targetGR + coeff * (mSmoothedGR - targetGR);

        // snap to 0 when close
        if (mSmoothedGR < 0.05) mSmoothedGR = 0.0;

        if (IParam* gainP = dlg->GetParam(kCompGain))
        {
          double clamped = std::clamp(mSmoothedGR, 0.0, gainP->GetMax());
          gainP->Set(clamped);
        }

        // redraw the knob, arc, and label so they track the smoothed GR
        if (auto* ui = GetUI())
          ui->SetAllControlsDirty();
      }
    }

    // background: subtle rounded rect
    const IColor bgOff(60, 80, 80, 80);
    const IColor bgOn(200, 80, 98, 116);
    g.FillRoundRect(on ? bgOn : bgOff, mRECT, 3.f);

    // border
    const IColor borderOff(80, 120, 120, 120);
    const IColor borderOn(220, 100, 120, 140);
    g.DrawRoundRect(on ? borderOn : borderOff, mRECT, 3.f, nullptr, 1.f);

    // text
    const IColor textOff(120, 160, 160, 160);
    const IColor textOn(255, 230, 230, 230);
    IText text(9.f, on ? textOn : textOff, "Inter-Semi-Bold", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, mLabel, mRECT);

    if (GetMouseIsOver())
      DrawHoverOverlay(g, mRECT, IColor(25, 200, 200, 200), 3.f);

    if (on)
      SetDirty(false);  // keep redrawing every frame so the knob tracks GR in real time
  }

  void OnMouseDown(float, float, const IMouseMod&) override
  {
    double cur = GetValue();
    if (const IParam* p = GetParam())
      cur = p->GetNormalized();

    const bool turningOn = (cur < 0.5);
    const double newNorm = turningOn ? 1.0 : 0.0;

    auto* dlg = GetDelegate();
    if (!dlg) { SetDirty(false); return; }

    // Auto must be toggled before we touch the gain knob.
    // SendParameterValueFromUI can cause an immediate redraw, and if Auto is
    // still on during that redraw, the auto-gain block in Draw runs one more
    // time and overwrites whatever value we just wrote into gainP
    if (turningOn)
    {
      // save current gain before enabling — once Auto is on, the next Draw
      // will start writing mSmoothedGR (=0) into gainP
      if (IParam* gainP = dlg->GetParam(kCompGain))
        mSavedGainDB = gainP->Value();
      mSmoothedGR = 0.0;

      SetValue(newNorm);
      dlg->SendParameterValueFromUI(mParamIdx, newNorm);
    }
    else
    {
      // disable Auto first, so any redraw triggered by the gain restore below
      // sees on=false and skips the auto-gain block
      SetValue(newNorm);
      dlg->SendParameterValueFromUI(mParamIdx, newNorm);
      mSmoothedGR = 0.0;

      // restore the gain to what the user had before Auto was on
      if (IParam* gainP = dlg->GetParam(kCompGain))
      {
        gainP->Set(mSavedGainDB);
        dlg->SendParameterValueFromUI(kCompGain, gainP->ToNormalized(mSavedGainDB));
      }
    }

    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();

    SetDirty(false);
  }

private:
  int       mParamIdx = -1;
  char      mLabel[8] = {};
  DynaCore* mPlug = nullptr;
  double    mSavedGainDB = 0.0;   // gain value saved when auto-gain is turned ON
  double    mSmoothedGR  = 0.0;   // smoothed GR for visual display
  uint64_t  mLastGRCount = 0;     // last seen GR update counter (detects DSP pause)
};

// Waveform toggle — draws a tiny sine or square waveform icon
// used for switching LFO shape on Tremolo and Pan
class WaveformToggleControl : public HandCursorControl
{
public:
  WaveformToggleControl(const IRECT& bounds, int paramIdx)
  : HandCursorControl(bounds, paramIdx)
  , mParamIdx(paramIdx)
  {}

  void Draw(IGraphics& g) override
  {
    if (const IParam* p = GetParam())
      SetValue(p->GetNormalized());

    const bool isSquare = (GetValue() >= 0.5);

    // background
    const IColor bgOff(50, 80, 80, 80);
    const IColor bgOn(160, 80, 98, 116);
    g.FillRoundRect(isSquare ? bgOn : bgOff, mRECT, 3.f);

    // border
    const IColor borderCol(80, 130, 130, 130);
    g.DrawRoundRect(borderCol, mRECT, 3.f, nullptr, 1.f);

    // draw waveform icon inside the button
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float hw = mRECT.W() * 0.32f;  // half-width of waveform
    const float hh = mRECT.H() * 0.28f;  // half-height of waveform
    const IColor waveCol(220, 200, 200, 200);

    if (isSquare)
    {
      // draw square wave: _|‾|_
      const float x0 = cx - hw, x1 = cx - hw * 0.33f, x2 = cx + hw * 0.33f, x3 = cx + hw;
      g.DrawLine(waveCol, x0, cy + hh, x1, cy + hh, nullptr, 1.2f);  // low
      g.DrawLine(waveCol, x1, cy + hh, x1, cy - hh, nullptr, 1.2f);  // rise
      g.DrawLine(waveCol, x1, cy - hh, x2, cy - hh, nullptr, 1.2f);  // high
      g.DrawLine(waveCol, x2, cy - hh, x2, cy + hh, nullptr, 1.2f);  // fall
      g.DrawLine(waveCol, x2, cy + hh, x3, cy + hh, nullptr, 1.2f);  // low
    }
    else
    {
      // draw sine wave with short line segments
      const int nSegs = 12;
      for (int i = 0; i < nSegs; ++i)
      {
        float t0 = (float)i / (float)nSegs;
        float t1 = (float)(i + 1) / (float)nSegs;
        float x0 = cx - hw + t0 * 2.f * hw;
        float x1 = cx - hw + t1 * 2.f * hw;
        float y0 = cy - hh * std::sin(t0 * 6.2832f);
        float y1 = cy - hh * std::sin(t1 * 6.2832f);
        g.DrawLine(waveCol, x0, y0, x1, y1, nullptr, 1.2f);
      }
    }

    if (GetMouseIsOver())
      DrawHoverOverlay(g, mRECT, IColor(25, 200, 200, 200), 3.f);
  }

  void OnMouseDown(float, float, const IMouseMod&) override
  {
    double cur = GetValue();
    if (const IParam* p = GetParam())
      cur = p->GetNormalized();

    const double newNorm = (cur >= 0.5) ? 0.0 : 1.0;
    SetValue(newNorm);

    if (auto* dlg = GetDelegate())
      dlg->SendParameterValueFromUI(mParamIdx, newNorm);

    SetDirty(false);
  }

private:
  int mParamIdx = -1;
};

// Rate sync toggle — switches a rate knob between Hz and BPM-synced divisions
// shows "Hz" when in free mode, "BPM" when synced to host tempo
class RateSyncToggleControl : public HandCursorControl
{
public:
  RateSyncToggleControl(const IRECT& bounds, int syncParamIdx, int rateParamIdx)
  : HandCursorControl(bounds, syncParamIdx)
  , mParamIdx(syncParamIdx)
  , mRateIdx(rateParamIdx)
  {}

  void Draw(IGraphics& g) override
  {
    if (const IParam* p = GetParam())
      SetValue(p->GetNormalized());

    const bool isSync = (GetValue() >= 0.5);

    // background — opaque so it stays readable on the light module panel
    const IColor bgOff(220,  55,  65,  80);   // dark blue-gray when in Hz mode
    const IColor bgOn (235, 100, 122, 144);   // brighter purple when synced
    g.FillRoundRect(isSync ? bgOn : bgOff, mRECT, 3.f);

    // border (more visible than the waveform toggle's border)
    const IColor borderCol(180, 145, 150, 160);
    g.DrawRoundRect(borderCol, mRECT, 3.f, nullptr, 1.f);

    // label: "Hz" in free mode, "BPM" in sync mode
    const IColor textCol(255, 235, 235, 240);
    IText text(9.f, textCol, "Inter-Semi-Bold", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, isSync ? "BPM" : "Hz", mRECT);

    if (GetMouseIsOver())
      DrawHoverOverlay(g, mRECT, IColor(25, 200, 200, 200), 3.f);
  }

  // click — flip between Hz and BPM modes
  void OnMouseDown(float, float, const IMouseMod&) override
  {
    double cur = GetValue();
    if (const IParam* p = GetParam())
      cur = p->GetNormalized();  // sync with actual value

    const bool turningOn = (cur < 0.5);
    const double newNorm = turningOn ? 1.0 : 0.0;  // flip

    SetValue(newNorm);

    if (auto* dlg = GetDelegate())
    {
      dlg->SendParameterValueFromUI(mParamIdx, newNorm);  // send to DAW

      // when entering sync mode, snap the rate knob to the nearest division
      // so the displayed label matches a real division step
      if (turningOn && mRateIdx >= 0)
      {
        if (IParam* rp = dlg->GetParam(mRateIdx))
        {
          const double n = rp->GetNormalized();    // current knob position
          const int    d = NormToSyncDivIdx(n);    // nearest division
          const double s = SyncDivIdxToNorm(d);    // its normalized position
          if (std::fabs(s - n) > 1e-9)
            dlg->SendParameterValueFromUI(mRateIdx, s);  // snap the knob
        }
      }
    }

    if (auto* ui = GetUI())
      ui->SetAllControlsDirty();  // refresh rate label and arc

    SetDirty(false);
  }

private:
  int mParamIdx = -1;  // sync toggle param
  int mRateIdx  = -1;  // matching rate param (for snap on enable)
};

// text label under mid-size knobs — shows value in different formats depending on param
class MidKnobValueTextControl : public IControl
{
public:
  // must match the font loaded in pGraphics->LoadFont(...)
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

    // compressor params need special formatting
    if (mParamIdx == kCompThreshold || mParamIdx == kCompGain)
    {
      // show in dB (gain follows GR live when auto-gain is on)
      s.SetFormatted(64, "%.1f dB", p->Value());
    }
    else if (mParamIdx == kCompRatio)
    {
      // show ratio as X:1
      const double r = p->Value();

      // if close to integer, skip the decimals
      const double ri = std::round(r);
      if (std::fabs(r - ri) < 1e-6)
        s.SetFormatted(64, "%.0f:1", ri);
      else
        s.SetFormatted(64, "%.1f:1", r);
    }
    else if (IsRateParamIdx(mParamIdx))
    {
      // show rate in Hz
      s.SetFormatted(64, "%.2f Hz", p->Value());
    }
    else if (mParamIdx == kOutputLevel)
    {
      // show output level in dB
      const double db = p->Value();

      if (std::fabs(db) < 1e-6)
        s.SetFormatted(64, "0.0 dB");
      else
        s.SetFormatted(64, "%+.1f dB", db); // show + sign for positive
    }
    else if (mParamIdx == kWidth)
    {
      // show width as percent (0-200%)
      s.SetFormatted(64, "%.0f%%", p->Value());
    }
    else
    {
      // everything else — just show as percent
      const int pct = (int) std::lround(p->GetNormalized() * 100.0);
      s.SetFormatted(64, "%d%%", pct);
    }

    g.DrawText(mText, s.Get(), mRECT);
  }
  
private:
  IText mText;
  int   mParamIdx = -1;
};

// shows the current preset name in the top-left corner
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
