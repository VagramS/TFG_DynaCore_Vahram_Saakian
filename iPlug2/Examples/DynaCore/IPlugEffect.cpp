#include "IPlugEffect.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <cstdio> // for snprintf

// Preset groups used by presets page
enum class EPresetGroup
{
  None,
  Vocals,
  Pads,
  Drums,
  Experimental
};

// Last selected preset group (persists between openings)
static EPresetGroup gLastPresetGroup = EPresetGroup::Vocals;

// Reset all main parameters to default values (called from presets UI)
void IPlugEffect::ApplyDefaultPresetFromUI()
{
  // Global bypass
  GetParam(kBypass)->Set(0.0);

  // Bypass compressor
  GetParam(kCompBypass)->Set(1.0);

  // Bypass all modulation modules
  GetParam(kTremBypass)->Set(1.0);
  GetParam(kPanBypass)->Set(1.0);
  GetParam(kPitchBypass)->Set(1.0);
  GetParam(kPhaserBypass)->Set(1.0);

  // Tremolo / Pan / Pitch / Phaser gains and depths
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

  // Redraw UI
  if (GetUI())
    GetUI()->SetAllControlsDirty();
}


// Full-screen presets overlay for the plugin, closes when clicking outside active area
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
                     const IBitmap& revertBmp)
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
  {
    mIgnoreMouse = false;

    // Initialize with last selected group when opening presets page
    mSelectedGroup = gLastPresetGroup;
  }

  EPresetGroup GetSelectedGroup() const { return mSelectedGroup; }

  void Draw(IGraphics& g) override
  {
    // Background
    g.DrawBitmap(mPageBitmap, mRECT);

    // Revert button
    const IRECT revertRect(423.f, 537.f, 423.f + 122.f, 537.f + 28.f);
    g.DrawBitmap(mRevertBmp, revertRect);

    // Selected group highlight
    switch (mSelectedGroup)
    {
      case EPresetGroup::Vocals:
        g.DrawBitmap(mVocalsSelectBmp, IRECT(14.f, 98.f, 14.f + 276.f, 98.f + 79.f));
        break;
      case EPresetGroup::Pads:
        g.DrawBitmap(mPadsSelectBmp,   IRECT(14.f, 168.f, 14.f + 276.f, 168.f + 77.f));
        break;
      case EPresetGroup::Drums:
        g.DrawBitmap(mDrumsSelectBmp,  IRECT(14.f, 236.f, 14.f + 276.f, 236.f + 79.f));
        break;
      case EPresetGroup::Experimental:
        g.DrawBitmap(mExpSelectBmp,    IRECT(14.f, 306.f, 14.f + 276.f, 306.f + 78.f));
        break;
      case EPresetGroup::None:
      default:
        break;
    }

    // Labels for groups
    // Vocals: Size (82 x 19)  X: 112, Y: 130
    g.DrawBitmap(mVocalsLabelBmp,
                 IRECT(112.f, 130.f, 112.f + 82.f, 130.f + 19.f));

    // Pads: Size (111 x 21)  X: 101, Y: 199
    g.DrawBitmap(mPadsLabelBmp,
                 IRECT(97.f, 199.f, 97.f + 111.f, 199.f + 21.f));

    // Drums: Size (209 x 21) X: 52, Y: 269
    g.DrawBitmap(mDrumsLabelBmp,
                 IRECT(48.f, 268.f, 48.f + 209.f, 268.f + 21.f));

    // Experimental: Size (189 x 21) X: 62, Y: 338
    g.DrawBitmap(mExpLabelBmp,
                 IRECT(58.f, 338.f, 58.f + 189.f, 338.f + 21.f));

    // Arrow on top of highlight + labels
    IRECT arrowRect;
    switch (mSelectedGroup)
    {
      case EPresetGroup::Vocals:
        arrowRect = IRECT(267.f, 130.f, 267.f + 14.f, 131.f + 19.f);
        break;
      case EPresetGroup::Pads:
        arrowRect = IRECT(267.f, 199.f, 267.f + 14.f, 200.f + 19.f);
        break;
      case EPresetGroup::Drums:
        arrowRect = IRECT(267.f, 268.f, 267.f + 14.f, 269.f + 19.f);
        break;
      case EPresetGroup::Experimental:
        arrowRect = IRECT(267.f, 338.f, 267.f + 14.f, 339.f + 19.f);
        break;
      case EPresetGroup::None:
      default:
        return;
    }

    g.DrawBitmap(mArrowBmp, arrowRect);
  }


  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    // Area where the overlay stays open
    const IRECT active(19.f, 104.f, 554.f, 578.f);

    // Click outside active area -> close overlay
    if (!active.Contains(x, y))
    {
      if (IGraphics* ui = GetUI())
        ui->RemoveControl(this);
      return;
    }

    // "Revert to default" button area
    const IRECT revertRect(423.f, 537.f, 423.f + 122.f, 537.f + 28.f);

    if (revertRect.Contains(x, y))
    {
      ApplyDefaultPreset(); // set default parameters

      if (IGraphics* ui = GetUI())
        ui->RemoveControl(this); // close overlay

      return;
    }

    // Group areas for selecting preset group
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
      mSelectedGroup = newGroup;
      gLastPresetGroup = newGroup; // remember last selected group
      SetDirty(false);
    }
  }

private:
  // Normalize value to [0; 1]
  static double Normalize(double value, double minV, double maxV)
  {
    return (value - minV) / (maxV - minV);
  }

  // Apply default preset state and send normalized parameter values to DSP
  void ApplyDefaultPreset()
  {
    if (auto* dlg = GetDelegate())
    {
      // ===== Bypass toggles (Bool, normalized 0/1) =====
      dlg->SendParameterValueFromUI(kBypass,       0.0); // global bypass off
      dlg->SendParameterValueFromUI(kCompBypass,   1.0); // compressor bypass on
      dlg->SendParameterValueFromUI(kTremBypass,   1.0);
      dlg->SendParameterValueFromUI(kPanBypass,    1.0);
      dlg->SendParameterValueFromUI(kPitchBypass,  1.0);
      dlg->SendParameterValueFromUI(kPhaserBypass, 1.0);

      // Small helper lambda for normalized sending
      auto sendNorm = [dlg](int paramIdx,
                            double value,
                            double minV,
                            double maxV)
      {
        const double norm = Normalize(value, minV, maxV);
        dlg->SendParameterValueFromUI(paramIdx, norm);
      };

      // ===== Other parameters (use their units) =====
      // See InitDouble(...) ranges

      sendNorm(kGain,            0.0,   0.0, 100.0);

      sendNorm(kTremRate,        0.0,   0.1, 20.0);
      sendNorm(kTremDepth,       0.0,   0.0, 100.0);

      sendNorm(kPanRate,         0.0,   0.1, 20.0);
      sendNorm(kPanDepth,        0.0,   0.0, 100.0);

      sendNorm(kPitchRate,       0.0,   0.1, 10.0);
      sendNorm(kPitchDepth,      0.0,   0.0, 100.0);

      sendNorm(kPhaserRate,      0.0,   0.1, 20.0);
      sendNorm(kPhaserDepth,     0.0,   0.0, 100.0);

      // COMP
      sendNorm(kCompMix,         0.0,   0.0, 100.0);
      sendNorm(kCompThreshold,  -24.0, -60.0,  0.0);
      sendNorm(kCompRatio,       4.0,   1.0, 20.0);
      sendNorm(kCompGain,        0.0, -24.0, 24.0);
      sendNorm(kCompAttack,     10.0,   0.1, 100.0);
      sendNorm(kCompRelease,   100.0,   5.0, 1000.0);

      // MASTER / OUTPUT
      sendNorm(kMasterIntensity, 0.0,   0.0, 100.0);
      sendNorm(kOutputLevel,     0.0, -24.0, 24.0);
    }

    // Reset selected group after applying defaults (no highlight)
    mSelectedGroup = EPresetGroup::None;
  }

    IBitmap mPageBitmap;
    IBitmap mVocalsSelectBmp;
    IBitmap mPadsSelectBmp;
    IBitmap mDrumsSelectBmp;
    IBitmap mExpSelectBmp;

    IBitmap mVocalsLabelBmp;
    IBitmap mPadsLabelBmp;
    IBitmap mDrumsLabelBmp;
    IBitmap mExpLabelBmp;

    IBitmap mArrowBmp;
    IBitmap mRevertBmp;

    EPresetGroup mSelectedGroup = EPresetGroup::Vocals;
};


// "SELECT PRESET" button that opens the presets overlay
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
                      const IBitmap& revertBmp)
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
  {}

  void Draw(IGraphics& g) override
  {
    // Draw "Select preset" button on main page
    g.DrawBitmap(mButtonBitmap, mRECT);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    // Open presets overlay filling the whole plugin window
    if (IGraphics* ui = GetUI())
    {
      IRECT fullRect(0.f, 0.f,
                     static_cast<float>(PLUG_WIDTH),
                     static_cast<float>(PLUG_HEIGHT));

      ui->AttachControl(new PresetsPageControl(fullRect,
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
                                               mRevertBmp));
    }

    SetDirty(false);
  }


private:
  IBitmap mButtonBitmap;
  IBitmap mPageBitmap;

  IBitmap mVocalsSelectBmp;
  IBitmap mPadsSelectBmp;
  IBitmap mDrumsSelectBmp;
  IBitmap mExpSelectBmp;

  IBitmap mVocalsLabelBmp;
  IBitmap mPadsLabelBmp;
  IBitmap mDrumsLabelBmp;
  IBitmap mExpLabelBmp;

  IBitmap mArrowBmp;
  IBitmap mRevertBmp;

};



// ON/OFF toggle button (bypass-style)
class CompBypassButton : public IControl
{
public:
  CompBypassButton(const IRECT& bounds, int paramIdx,
                   const IBitmap& offBitmap, const IBitmap& onBitmap)
  : IControl(bounds, paramIdx)
  , mOff(offBitmap)
  , mOn(onBitmap)
  {}

  void Draw(IGraphics& g) override
  {
    const bool bypass = (GetValue() >= 0.5f); // true = bypass is on
    const IBitmap& bmp = bypass ? mOff : mOn; // bypass -> OFF image, !bypass -> ON image
    g.DrawBitmap(bmp, mRECT);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    const float newVal = (GetValue() < 0.5f ? 1.f : 0.f); // 0 -> 1, 1 -> 0
    SetValue(newVal);
    SetDirty(false);
  }

private:
  IBitmap mOff, mOn;
};

// Knob based on IVKnobControl, with bitmap cap
class BitmapIVKnob : public IVKnobControl
{
public:
  BitmapIVKnob(const IRECT& bounds,
               int paramIdx,
               const IBitmap& bitmap,
               const IVStyle& style,
               float minAngleDeg,
               float maxAngleDeg)
  : IVKnobControl(bounds,
                  paramIdx,
                  "",          // no label
                  style,
                  false,       // valueIsEditable
                  false,       // valueInWidget
                  minAngleDeg, // a1
                  maxAngleDeg, // a2
                  minAngleDeg) // anchor
  , mBitmap(bitmap)
  {}

private:
  IBitmap mBitmap;
};


// Text control that displays current output level in dB with a subtle glow
class OutputLevelTextControl : public IControl
{
public:
  OutputLevelTextControl(const IRECT& bounds, IPlugEffect* plugin)
  : IControl(bounds)
  , mPlugin(plugin)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    if (!mPlugin)
      return;

    const double db = mPlugin->GetOutputLevelDB();

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", db);

    // Soft glow behind main text
    IColor shadowColor(10, 255, 255, 255);
    IText shadowText(18.f, shadowColor, "Roboto-Regular",
                     EAlign::Center, EVAlign::Middle);

    // Small "cloud" around text, offsets ±1px
    for (int dy = -1; dy <= 1; ++dy)
    {
      for (int dx = -1; dx <= 1; ++dx)
      {
        if (dx == 0 && dy == 0)
          continue;

        IRECT r(mRECT.L + dx, mRECT.T + dy, mRECT.R + dx, mRECT.B + dy);
        g.DrawText(shadowText, buf, r);
      }
    }

    // Main text on top
    IText text(18.f, COLOR_WHITE, "Roboto-Regular",
               EAlign::Center, EVAlign::Middle);

    g.DrawText(text, buf, mRECT);

    SetDirty(false);
  }

private:
  IPlugEffect* mPlugin = nullptr;
};



IPlugEffect::IPlugEffect(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // --- MAIN GAIN ---
  GetParam(kGain)->InitDouble("Gain", 0., 0., 100.0, 0.01, "%");

  // Tremolo: Rate / Depth
  GetParam(kTremRate)->InitDouble("Trem Rate", 0.0, 0.1, 20.0, 0.01, "Hz");
  GetParam(kTremDepth)->InitDouble("Trem Depth", 0.0, 0.0, 100.0, 1.0, "%");
  GetParam(kTremBypass)->InitBool("Trem Bypass", true);

  // Pan Motion
  GetParam(kPanRate)->InitDouble("Pan Rate", 0.0, 0.1, 20.0, 0.01, "Hz");
  GetParam(kPanDepth)->InitDouble("Pan Depth", 0.0, 0.0, 100.0, 1.0, "%");
  GetParam(kPanBypass)->InitBool("Pan Bypass", true);

  // Pitch Drift
  GetParam(kPitchRate)->InitDouble("Pitch Rate", 0.0, 0.1, 10.0, 0.01, "Hz");
  GetParam(kPitchDepth)->InitDouble("Pitch Depth", 0.0, 0.0, 100.0, 1.0, "%");
  GetParam(kPitchBypass)->InitBool("Pitch Bypass", true);

  // Phaser
  GetParam(kPhaserRate)->InitDouble("Phaser Rate", 0.0, 0.1, 20.0, 0.01, "Hz");
  GetParam(kPhaserDepth)->InitDouble("Phaser Depth", 0.0, 0.0, 100.0, 1.0, "%");
  GetParam(kPhaserBypass)->InitBool("Phaser Bypass", true);

  // --- COMPRESSOR ---
  GetParam(kCompMix)->InitDouble("Comp Mix", 100.0, 0.0, 100.0, 1.0, "%");
  GetParam(kCompThreshold)->InitDouble("Comp Threshold", -24.0, -60.0, 0.0, 0.1, "dB");
  GetParam(kCompRatio)->InitDouble("Comp Ratio", 4.0, 1.0, 20.0, 0.1, ":1");
  GetParam(kCompGain)->InitDouble("Comp Gain", 0.0, -24.0, 24.0, 0.1, "dB");

  GetParam(kCompAttack)->InitDouble("Comp Attack", 10.0, 0.1, 100.0, 0.1, "ms");
  GetParam(kCompRelease)->InitDouble("Comp Release", 100.0, 5.0, 1000.0, 1.0, "ms");
  
  GetParam(kCompBypass)->InitBool("Comp Bypass", true);

  // --- MASTER / OUTPUT ---
  GetParam(kBypass)->InitBool("Bypass", false);
  
  GetParam(kMasterIntensity)->InitDouble("Master Intensity", 0.0, 0.0, 100.0, 1.0, "%");
  GetParam(kOutputLevel)->InitDouble("Output Level", 0.0, -24.0, 24.0, 0.1, "dB");
  
#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_BLACK);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Inter-Medium",  INTER_FN);

    // BACKGROUND
    IBitmap bg = pGraphics->LoadBitmap(MAIN_BACKGROUND_FN, 1);
    const IRECT bounds = pGraphics->GetBounds();
    pGraphics->AttachControl(new IBitmapControl(bounds, bg));

    // COMPRESSOR ON/OFF
    const float x_c = 371.f;
    const float y_c = 447.f;
    const float w_c = 21.f;
    const float h_c = 20.f;
    IRECT compBtnRect(x_c, y_c, x_c + w_c, y_c + h_c);

    IBitmap bmpOff = pGraphics->LoadBitmap(COMP_OFF_FN, 1);
    IBitmap bmpOn  = pGraphics->LoadBitmap(COMP_ON_FN, 1);

    pGraphics->AttachControl(new CompBypassButton(
      compBtnRect, kCompBypass, bmpOff, bmpOn));
      
    // ==== BYPASS (general) ====
    const float x_b = 836.f;
    const float y_b = 74.f;
    const float w_b = 62.f;
    const float h_b = 20.f;
    IRECT bypassBtnRect(x_b, y_b, x_b + w_b, y_b + h_b);

    IBitmap bmpBypOff = pGraphics->LoadBitmap(BYPASS_OFF_FN, 1);
    IBitmap bmpBypOn  = pGraphics->LoadBitmap(BYPASS_ON_FN, 1);

    pGraphics->AttachControl(new CompBypassButton(
      bypassBtnRect, kBypass, bmpBypOn, bmpBypOff));

    // ==== 4 MODULE TOGGLES ====
    // Shared bitmaps for module on/off buttons
    IBitmap modOff = pGraphics->LoadBitmap(MODULE_OFF_FN, 1);
    IBitmap modOn  = pGraphics->LoadBitmap(MODULE_ON_FN, 1);

    const float w_m = 19.f;
    const float h_m = 18.f;

    // Tremolo ON/OFF toggle
    IRECT tremRect(126.f, 390.f, 126.f + w_m, 390.f + h_m);
    pGraphics->AttachControl(new CompBypassButton(
      tremRect, kTremBypass, modOff, modOn));

    // Pan Motion ON/OFF toggle
    IRECT panRect(344.f, 390.f, 344.f + w_m, 390.f + h_m);
    pGraphics->AttachControl(new CompBypassButton(
      panRect, kPanBypass, modOff, modOn));

    // Pitch Drift ON/OFF toggle
    IRECT pitchRect(560.f, 390.f, 560.f + w_m, 390.f + h_m);
    pGraphics->AttachControl(new CompBypassButton(
      pitchRect, kPitchBypass, modOff, modOn));

    // Phaser ON/OFF toggle
    IRECT phaserRect(776.f, 390.f, 776.f + w_m, 390.f + h_m);
    pGraphics->AttachControl(new CompBypassButton(
      phaserRect, kPhaserBypass, modOff, modOn));
    // =================

    // Select preset button and presets overlay bitmaps
    const float x_p = 27.f;
    const float y_p = 71.f;
    const float w_p = 245.f;
    const float h_p = 26.f;

    IBitmap presetBtnBmp   = pGraphics->LoadBitmap(SELECT_PRESET_FN, 1);
    IBitmap presetsPageBmp = pGraphics->LoadBitmap(PRESETS_PAGE_FN, 1);

    IBitmap vocalsSelectBmp = pGraphics->LoadBitmap(PRESET_GROUP_VOCALS_SELECT_FN, 1);
    IBitmap padsSelectBmp   = pGraphics->LoadBitmap(PRESET_GROUP_PADS_SELECT_FN,   1);
    IBitmap drumsSelectBmp  = pGraphics->LoadBitmap(PRESET_GROUP_DRUMS_SELECT_FN,  1);
    IBitmap expSelectBmp    = pGraphics->LoadBitmap(PRESET_GROUP_EXP_SELECT_FN,    1);

    IBitmap vocalsLabelBmp  = pGraphics->LoadBitmap(PRESET_VOCALS_LABLE_FN, 1);
    IBitmap padsLabelBmp    = pGraphics->LoadBitmap(PRESET_PADS_LABLE_FN,   1);
    IBitmap drumsLabelBmp   = pGraphics->LoadBitmap(PRESET_DRUMS_LABLE_FN,  1);
    IBitmap expLabelBmp     = pGraphics->LoadBitmap(PRESET_EXP_LABLE_FN,    1);

    IBitmap arrowBmp        = pGraphics->LoadBitmap(PRESET_GROUP_SELECT_ARROW_FN,  1);
    IBitmap revertBmp       = pGraphics->LoadBitmap(REVERT_TO_DEFAULT_FN,          1);


    IRECT preset_bounds(x_p, y_p, x_p + w_p, y_p + h_p);
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
             revertBmp));

    
    // ===== BIG KNOB =====
    IBitmap bigKnob = pGraphics->LoadBitmap(BIG_KNOB_FN, 1);

    const float kKnobMinAngle = -131.f;
    const float kKnobMaxAngle =  135.f;

    const float kKnobW = 30.f;
    const float kKnobH = 30.f;

    IVStyle knobStyle = DEFAULT_STYLE;
    knobStyle.showLabel   = false;
    knobStyle.showValue   = false;
    knobStyle.drawFrame   = false;
    knobStyle.drawShadows = false;


    // --- Tremolo: Rate / Depth ---
    IRECT tremRateRect  (65.f, 332.f, 65.f + kKnobW, 332.f + kKnobH);
    IRECT tremDepthRect (180.f, 332.f, 180.f + kKnobW, 332.f + kKnobH);

    pGraphics->AttachControl(new BitmapIVKnob(
      tremRateRect, kTremRate, bigKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

    pGraphics->AttachControl(new BitmapIVKnob(
      tremDepthRect, kTremDepth, bigKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

    // --- Pan Motion ---
    IRECT panRateRect   (281.f, 332.f, 281.f + kKnobW, 332.f + kKnobH);
    IRECT panDepthRect  (398.f, 332.f, 398.f + kKnobW, 332.f + kKnobH);

    pGraphics->AttachControl(new BitmapIVKnob(
      panRateRect, kPanRate, bigKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

    pGraphics->AttachControl(new BitmapIVKnob(
      panDepthRect, kPanDepth, bigKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

    // --- Pitch Drift ---
    IRECT pitchRateRect (497.f, 332.f, 497.f + kKnobW, 332.f + kKnobH);
    IRECT pitchDepthRect(614.f, 332.f, 614.f + kKnobW, 332.f + kKnobH);

    pGraphics->AttachControl(new BitmapIVKnob(
      pitchRateRect, kPitchRate, bigKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

    pGraphics->AttachControl(new BitmapIVKnob(
      pitchDepthRect, kPitchDepth, bigKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

    // --- Phaser ---
    IRECT phaserRateRect (713.f, 332.f, 713.f + kKnobW, 332.f + kKnobH);
    IRECT phaserDepthRect(830.f, 332.f, 830.f + kKnobW, 332.f + kKnobH);

    pGraphics->AttachControl(new BitmapIVKnob(
      phaserRateRect, kPhaserRate, bigKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

    pGraphics->AttachControl(new BitmapIVKnob(
      phaserDepthRect, kPhaserDepth, bigKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));
        
    
    // ===== MID / SMALL KNOBS =====
    IBitmap midKnob   = pGraphics->LoadBitmap(MID_KNOB_FN, 1);
    IBitmap smallKnob = pGraphics->LoadBitmap(SMALL_KNOB_FN, 1);

    const float kMidKnobW   = 19.f;
    const float kMidKnobH   = 19.f;
    const float kSmallKnobW = 14.f;
    const float kSmallKnobH = 14.f;
    
    // --- COMP: Mix / Threshold / Ratio / Gain ---

    IRECT compMixRect      (47.f, 513.f,   47.f + kMidKnobW,   512.f + kMidKnobH);
    IRECT compThreshRect   (130.f, 513.f, 129.5f + kMidKnobW,  512.f + kMidKnobH);
    IRECT compRatioRect    (204.5f, 513.f, 204.5f + kMidKnobW, 512.f + kMidKnobH);
    IRECT compGainRect     (275.f, 513.f, 274.5f + kMidKnobW,  512.f + kMidKnobH);
    
    pGraphics->AttachControl(new BitmapIVKnob(
      compMixRect, kCompMix, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));
    pGraphics->AttachControl(new BitmapIVKnob(
      compThreshRect, kCompThreshold, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));
    pGraphics->AttachControl(new BitmapIVKnob(
      compRatioRect, kCompRatio, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));
    pGraphics->AttachControl(new BitmapIVKnob(
      compGainRect, kCompGain, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

    // --- COMP: Attack / Release (small knobs) ---

    IRECT compAttackRect  (351.f, 493.f, 351.5f + kSmallKnobW, 492.f + kSmallKnobH);
    IRECT compReleaseRect (351.f, 541.f, 351.5f + kSmallKnobW, 540.f + kSmallKnobH);

    pGraphics->AttachControl(new BitmapIVKnob(
      compAttackRect, kCompAttack, smallKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));
    pGraphics->AttachControl(new BitmapIVKnob(
      compReleaseRect, kCompRelease, smallKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

    // --- MASTERING: Intensity (mid knob) ---

    IRECT masterIntRect(460.5f, 513.f, 460.f + kMidKnobW, 512.5f + kMidKnobH);
    pGraphics->AttachControl(new BitmapIVKnob(
      masterIntRect, kMasterIntensity, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

    // --- OUTPUT: Level (mid knob) ---
    // Placed bottom-right, near the output column

    IRECT outLevelRect(956.5f, 544.5f, 956.5f + kMidKnobW, 544.5f + kMidKnobH);
    pGraphics->AttachControl(new BitmapIVKnob(
      outLevelRect, kOutputLevel, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));
    
    // Output level numeric display: (dB)
    const float w = 50.f;
    const float h = 20.f;
    IRECT outTextRect(940.f, 501.f, 940.f + w, 501.f + h);

    pGraphics->AttachControl(new OutputLevelTextControl(outTextRect, this));
        
  };
#endif
}

#if IPLUG_DSP
#include <cmath>

// Main audio processing: apply output gain and update RMS meter
void IPlugEffect::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const bool bypass = GetParam(kBypass)->Bool();

  const double outGainDB = GetParam(kOutputLevel)->Value();
  const double outGain   = std::pow(10.0, outGainDB / 20.0);

  const int nChans = NOutChansConnected();

  double sumSquares = 0.0;  // energy accumulator for RMS
  int sampleCount   = 0;    // total samples processed

  for (int s = 0; s < nFrames; s++)
  {
    for (int c = 0; c < nChans; c++)
    {
      const sample inSample = inputs[c][s];

      // TODO: ADD MODEL PROCESSING LATER
      const sample processed = inSample;

      const sample outSample =
        bypass ? inSample : static_cast<sample>(processed * outGain);

      outputs[c][s] = outSample;

      const double d = static_cast<double>(outSample);
      sumSquares += d * d;
      sampleCount++;
    }
  }

  // Block RMS -> dB for UI meter
  if (sampleCount > 0)
  {
    if (sumSquares <= 0.0)
    {
      // Absolute silence: show 0.0 dB as requested
      mOutputLevelDB = 0.0;
    }
    else
    {
      const double rms = std::sqrt(sumSquares / static_cast<double>(sampleCount));
      const double db  = 20.0 * std::log10(rms);
      mOutputLevelDB = db;
    }
  }
}

#endif
