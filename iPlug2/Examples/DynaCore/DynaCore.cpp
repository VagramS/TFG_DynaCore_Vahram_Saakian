// DynaCore.cpp
// Main plugin file — sets up parameters, builds the UI, and does all DSP.
// Presets are in DynaCorePresets.h, UI controls in DynaCoreControls.h.
//
// Author: Vahram Saakian, UCM TFG 2025-2026

#include "DynaCore.h"
#include "IPlug_include_in_plug_src.h"
#include "DynaCorePresets.h"
#include "DynaCoreControls.h"
#include <cmath>
#include <algorithm>

namespace
{
  // moves value slowly toward 0 or 1 (for smooth bypass fade)
  inline void RampToward(double& ramp, double target, double rate)
  {
    if (ramp < target)
      ramp = std::min(target, ramp + rate);
    else
      ramp = std::max(target, ramp - rate);
  }
}

// reset all params to defaults and clear preset selection
void DynaCore::ApplyDefaultPresetFromUI()
{
  ApplyDefaultPresetParams([this](int pIdx, double plain)
  {
    if (auto* p = GetParam(pIdx))
      p->Set(plain);  // set directly, no DAW undo
  });

  // no preset selected anymore
  gSelectedGroupGlobal  = EPresetGroup::None;
  gSelectedPresetGlobal = -1;
  gSelectedPresetName   = kPresetName_None;

  if (GetUI())
    GetUI()->SetAllControlsDirty();  // redraw everything
}

DynaCore::DynaCore(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // --- Main gain ---
  GetParam(kGain)->InitDouble("Gain", 0.0, 0.0, 100.0, 0.01, "%");

  // Tremolo
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
  GetParam(kCompThreshold)->InitDouble("Comp Threshold", -25.0, -32.0,   0.0, 0.1, "dB");
  GetParam(kCompRatio)->InitDouble    ("Comp Ratio",     2.0,   1.0,  20.0, 0.1, ":1");
  GetParam(kCompGain)->InitDouble     ("Comp Gain",      0.0, -20.0,  30.0, 0.1, "dB");
  GetParam(kCompAttack)->InitDouble   ("Comp Attack",    10.0,   0.0, 100.0, 0.1, "ms");
  GetParam(kCompRelease)->InitDouble  ("Comp Release",   120.0,   0.0, 1000.0, 1.0, "ms");
  GetParam(kCompBypass)->InitBool     ("Comp Bypass",    false); // false = active

  // Mastering
  GetParam(kWidth)->InitDouble("Width", 100.0, 0.0, 200.0, 0.1, "%");

  // Master / Output
  GetParam(kBypass)->InitBool("Bypass", false);
  GetParam(kMasterIntensity)->InitDouble("Master Intensity", 100.0, 0.0, 100.0, 1.0, "%");
  GetParam(kOutputLevel)->InitDouble("Output Level", 0.0, -20.0, 20.0, 0.1, "dB");

  // Toggles
  GetParam(kCompAutoGain)->InitBool("Comp Auto Gain", false);   // off = manual gain
  GetParam(kTremWaveform)->InitBool("Trem Waveform", false);    // off = sine
  GetParam(kPanWaveform)->InitBool("Pan Waveform", false);      // off = sine

  // Rate sync (BPM): off = knob is Hz, on = knob picks a musical division
  GetParam(kTremRateSync)->InitBool  ("Trem Rate Sync",   false);  // off = Hz
  GetParam(kPanRateSync)->InitBool   ("Pan Rate Sync",    false);  // off = Hz
  GetParam(kPitchRateSync)->InitBool ("Pitch Rate Sync",  false);  // off = Hz
  GetParam(kPhaserRateSync)->InitBool("Phaser Rate Sync", false);  // off = Hz

  // set all params to defaults so the plugin starts in a known state
  ApplyDefaultPresetParams([this](int pIdx, double plain)
  {
    if (auto* p = GetParam(pIdx))
      p->Set(plain);
  });

  // no preset on first open
  gSelectedGroupGlobal  = EPresetGroup::None;
  gSelectedPresetGlobal = -1;
  gSelectedPresetName   = kPresetName_None;

#if IPLUG_EDITOR
  // plugin window (1000x600, 60fps)
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  // build UI layout — runs once when editor opens
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->EnableMouseOver(true);           // for hover effects
    pGraphics->EnableTooltips(true);            // tooltip hints on hover
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_BLACK);
    pGraphics->LoadFont("Inter-Regular",   INTER_REGULAR_FN);    // fonts for labels
    pGraphics->LoadFont("Inter-Semi-Bold", INTER_SEMI_BOLD_FN);
    pGraphics->LoadFont("Inter-Medium",    INTER_MEDIUM_FN);

    // background image
    IBitmap bg = pGraphics->LoadBitmap(MAIN_BACKGROUND_FN, 1);
    const IRECT bounds = pGraphics->GetBounds();
    pGraphics->AttachControl(new IBitmapControl(bounds, bg));

    const IColor hoverColorButtons  = IColor(23, 184, 184, 184);
    const IColor hoverColorModules  = IColor(23, 14,  14,  14);
    const IColor hoverColorKnobs    = IColor(18, 184, 184, 184);

    // COMPRESSOR ON/OFF
    IRECT compBtnRect(459.f, 447.f, 480.f, 467.f);
    IBitmap bmpOff = pGraphics->LoadBitmap(COMP_OFF_FN, 1);
    IBitmap bmpOn  = pGraphics->LoadBitmap(COMP_ON_FN,  1);
    pGraphics->AttachControl(new HoverButtonWithOverlay(
      compBtnRect, kCompBypass, bmpOff, bmpOn, hoverColorButtons, 5.f))
      ->SetTooltip("Compressor On/Off — turns the dynamic range processing on or off.");

    // BYPASS (global)
    IRECT bypassBtnRect(836.f, 74.f, 898.f, 94.f);
    IBitmap bmpBypOff = pGraphics->LoadBitmap(BYPASS_OFF_FN, 1);
    IBitmap bmpBypOn  = pGraphics->LoadBitmap(BYPASS_ON_FN,  1);
    pGraphics->AttachControl(new HoverButtonWithOverlay(
      bypassBtnRect, kBypass, bmpBypOn, bmpBypOff, hoverColorButtons, 3.f))
      ->SetTooltip("Global Bypass — routes the dry input straight to the output, skipping all processing.");

    // 4 MODULE TOGGLES
    IBitmap modOff = pGraphics->LoadBitmap(MODULE_OFF_FN, 1);
    IBitmap modOn  = pGraphics->LoadBitmap(MODULE_ON_FN,  1);

    pGraphics->AttachControl(new HoverButtonWithOverlay(
      IRECT(126.f,390.f,145.f,408.f), kTremBypass,  modOff, modOn, hoverColorModules, 5.f))
      ->SetTooltip("Tremolo On/Off — turns the periodic volume modulation on or off.");
    pGraphics->AttachControl(new HoverButtonWithOverlay(
      IRECT(344.f,390.f,363.f,408.f), kPanBypass,   modOff, modOn, hoverColorModules, 5.f))
      ->SetTooltip("Pan Motion On/Off — turns the automatic left/right movement on or off.");
    pGraphics->AttachControl(new HoverButtonWithOverlay(
      IRECT(560.f,390.f,579.f,408.f), kPitchBypass, modOff, modOn, hoverColorModules, 5.f))
      ->SetTooltip("Pitch Drift On/Off — turns the chorus-style detuning on or off.");
    pGraphics->AttachControl(new HoverButtonWithOverlay(
      IRECT(776.f,390.f,795.f,408.f), kPhaserBypass,modOff, modOn, hoverColorModules, 5.f))
      ->SetTooltip("Phaser On/Off — turns the moving-notch sweep effect on or off.");

    // WAVEFORM SWITCHES (sine/square) — same size as bypass, 10px gap above
    pGraphics->AttachControl(new WaveformToggleControl(
      IRECT(126.f, 362.f, 145.f, 380.f), kTremWaveform))
      ->SetTooltip("Tremolo Waveform — switches the LFO shape between smooth sine and choppy square.");
    pGraphics->AttachControl(new WaveformToggleControl(
      IRECT(344.f, 362.f, 363.f, 380.f), kPanWaveform))
      ->SetTooltip("Pan Waveform — switches the LFO shape between smooth sine and choppy square.");

    // RATE SYNC TOGGLES (Hz <-> BPM) — above each Rate knob
    pGraphics->AttachControl(new RateSyncToggleControl(
      IRECT(69.f, 296.f, 91.f, 310.f), kTremRateSync, kTremRate))
      ->SetTooltip("Tremolo Rate Sync — switches the Rate knob between free Hz and musical divisions tied to the DAW tempo.");
    pGraphics->AttachControl(new RateSyncToggleControl(
      IRECT(285.f, 296.f, 307.f, 310.f), kPanRateSync, kPanRate))
      ->SetTooltip("Pan Rate Sync — switches the Rate knob between free Hz and musical divisions tied to the DAW tempo.");
    pGraphics->AttachControl(new RateSyncToggleControl(
      IRECT(501.f, 296.f, 523.f, 310.f), kPitchRateSync, kPitchRate))
      ->SetTooltip("Pitch Drift Rate Sync — switches the Rate knob between free Hz and musical divisions tied to the DAW tempo.");
    pGraphics->AttachControl(new RateSyncToggleControl(
      IRECT(717.f, 296.f, 739.f, 310.f), kPhaserRateSync, kPhaserRate))
      ->SetTooltip("Phaser Rate Sync — switches the Rate knob between free Hz and musical divisions tied to the DAW tempo.");

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
      presetPointerBmp))
      ->SetTooltip("Select Preset — opens the preset browser to pick a factory preset from the four categories (Vocals, Pads, Drums, Experimental).");

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
      pGraphics->AttachControl(new PresetStepButtonControl(prevR, prevPresetBmp, -1))
        ->SetTooltip("Previous Preset — loads the previous preset in the carousel, wrapping across categories.");
      pGraphics->AttachControl(new PresetStepButtonControl(nextR, nextPresetBmp, +1))
        ->SetTooltip("Next Preset — loads the next preset in the carousel, wrapping across categories.");
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
    auto AttachBigKnobWithArc = [&](const IRECT& knobRect, int paramIdx, const char* tip)
    {
      const IRECT ringRect = knobRect.GetPadded(6.4f);
      pGraphics->AttachControl(new KnobValueArcControl(
        ringRect, paramIdx, IColor(210, 0x50, 0x62, 0x74), 2.f, 225.f, 270.f));
      pGraphics->AttachControl(new HoverKnobRotaterControl(
        knobRect, bigKnob, paramIdx, hoverColorKnobs, 10.f, false))
        ->SetTooltip(tip);
    };

    auto AttachMidKnobWithArc = [&](const IRECT& knobRect, int paramIdx, const char* tip)
    {
      const IRECT ringRect = knobRect.GetPadded(5.7f);
      pGraphics->AttachControl(new KnobValueArcControl(
        ringRect, paramIdx, IColor(245, 171, 171, 171), 2.f, 222.f, 270.f));
      pGraphics->AttachControl(new HoverKnobRotaterControl(
        knobRect, midKnob, paramIdx, hoverColorKnobs, 10.f, true))
        ->SetTooltip(tip);
    };

    auto AttachSmallKnobWithArc = [&](const IRECT& knobRect, int paramIdx, const char* tip)
    {
      const IRECT ringRect = knobRect.GetPadded(4.9f);
      pGraphics->AttachControl(new KnobValueArcControl(
        ringRect, paramIdx, IColor(245, 171, 171, 171), 2.f, 223.f, 270.f));
      pGraphics->AttachControl(new HoverKnobRotaterControl(
        knobRect, smallKnob, paramIdx, hoverColorKnobs, 10.f, true))
        ->SetTooltip(tip);
    };

    AttachBigKnobWithArc(tremRateRect,   kTremRate,    "Tremolo Rate (Hz) — controls how fast the volume pulses.");
    AttachBigKnobWithArc(tremDepthRect,  kTremDepth,   "Tremolo Depth (%) — controls how much the volume drops on each pulse.");
    AttachBigKnobWithArc(panRateRect,    kPanRate,     "Pan Motion Rate (Hz) — controls how fast the sound moves between left and right.");
    AttachBigKnobWithArc(panDepthRect,   kPanDepth,    "Pan Motion Depth (%) — controls how far the sound moves toward each side.");
    AttachBigKnobWithArc(pitchRateRect,  kPitchRate,   "Pitch Drift Rate (Hz) — controls how fast the pitch wobbles.");
    AttachBigKnobWithArc(pitchDepthRect, kPitchDepth,  "Pitch Drift Depth (%) — controls how strong the detuning is.");
    AttachBigKnobWithArc(phaserRateRect, kPhaserRate,  "Phaser Rate (Hz) — controls how fast the notches move through the spectrum.");
    AttachBigKnobWithArc(phaserDepthRect,kPhaserDepth, "Phaser Depth (%) — controls how much of the phased signal is mixed with the dry.");

    // MID / SMALL knobs with hover overlay enabled
    const float kMidW = 19.f,  kMidH = 19.f;
    const float kSmW  = 14.f,  kSmH  = 14.f;

    // COMP: Mix / Threshold / Ratio / Gain (mid)
    IRECT compMixRect     ( 47.f,   512.f,  47.f + kMidW,  512.f + kMidH);
    IRECT compThreshRect  (127.5f, 512.f, 127.5f + kMidW, 512.f + kMidH);
    IRECT compRatioRect   (203.5f, 512.f, 203.5f + kMidW, 512.f + kMidH);
    IRECT compGainRect    (274.5f, 513.f, 274.5f + kMidW, 513.f + kMidH);

    AttachMidKnobWithArc(compMixRect, kCompMix,         "Compressor Mix (%) — blends the compressed signal with the dry input (parallel compression).");
    AttachMidKnobWithArc(compThreshRect, kCompThreshold,"Compressor Threshold (dB) — sets the level above which compression kicks in.");
    AttachMidKnobWithArc(compRatioRect, kCompRatio,     "Compressor Ratio — sets how strongly the signal is reduced above the threshold.");
    AttachMidKnobWithArc(compGainRect, kCompGain,       "Compressor Gain (dB) — boosts the output to compensate for the level lost during compression.");

    // AUTO-GAIN BUTTON — small "Auto" above the Gain knob
    pGraphics->AttachControl(new SmallTextToggleControl(
      IRECT(289.f, 481.f, 313.f, 493.f), kCompAutoGain, "Auto", this))
      ->SetTooltip("Auto Gain — automatically tracks the real average gain reduction and compensates it in real time.");

    // COMP: Attack / Release (small)
    IRECT compAttackRect  (351.2f, 492.2f, 351.2f + kSmW, 492.2f + kSmH);
    IRECT compReleaseRect (351.2f, 540.2f, 351.2f + kSmW, 540.2f + kSmH);

    AttachSmallKnobWithArc(compAttackRect, kCompAttack,   "Compressor Attack (ms) — how fast the compressor reacts when the signal goes above the threshold.");
    AttachSmallKnobWithArc(compReleaseRect, kCompRelease, "Compressor Release (ms) — how fast the compressor stops compressing when the signal drops back below the threshold.");

    // MASTERING: Width (mid) — in MASTERING section
    IRECT widthRect(532.5f, 512.f, 532.5f + kMidW, 512.f + kMidH);
    AttachMidKnobWithArc(widthRect, kWidth,             "Stereo Width — controls how wide the stereo image is (0% = mono, 100% = normal, 200% = wide).");

    // MASTERING: Intensity (mid)
    IRECT masterIntRect(617.5f, 512.f, 617.5f + kMidW, 512.f + kMidH);
    AttachMidKnobWithArc(masterIntRect, kMasterIntensity,"Master Intensity (%) — blends between only the compressed signal and the full effect chain.");

    // OUTPUT: Level (mid)
    IRECT outLevelRect(954.5f, 527.2f, 954.5f + kMidW, 527.2f + kMidH);
    AttachMidKnobWithArc(outLevelRect, kOutputLevel,    "Output Level (dB) — adjusts the final output volume of the plugin.");

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
    pGraphics->AttachControl(new GRMeterControl(grMeterRect, this))
      ->SetTooltip("Gain Reduction Meter (dB) — shows how many dB the compressor is currently cutting from the signal.");

    // Stereo output level meter (two bars + dB readout)
    IRECT meterRect(930.f, 72.f, 978.f, 525.f);
    pGraphics->AttachControl(new StereoLevelMeterControl(meterRect, this))
      ->SetTooltip("Output Level Meter (dB) — shows the peak output level of the plugin.");
  };
#endif
}

#if IPLUG_DSP

// ---------------------------------------------------------------------------
// DSP helpers — used inside ProcessBlock
// ---------------------------------------------------------------------------
namespace
{
  constexpr double kPi    = 3.14159265358979323846;
  constexpr double kTwoPi = 6.283185307179586476925286766559;
  constexpr double kSqrt2 = 1.4142135623730951;  // for constant-power panning

  // constant for dB to linear conversion
  static const double kLn10Over20 = std::log(10.0) / 20.0;

  // single allpass filter stage — phaser uses 6 of these in series
  // it shifts the phase of the signal without changing volume,
  // and when mixed with dry signal, creates moving notches
  inline double AllpassProcess(double input, double coeff, double& state)
  {
    const double v = input - coeff * state;    // remove feedback from last sample
    const double out = coeff * v + state;      // combine with stored value
    state = v;                                 // store for next sample
    return out;
  }

  // turns frequency (Hz) into allpass coefficient
  // the LFO changes this every sample, which moves the notches
  inline double AllpassCoeff(double fc, double sampleRate)
  {
    const double t = std::tan(kPi * fc / sampleRate);  // Hz to filter domain
    return (t - 1.0) / (t + 1.0);                     // coefficient in range -1..+1
  }

  // reads from the delay buffer with linear interpolation
  // because the read position is not always a whole number,
  // we blend between two neighbor samples for a smooth result
  inline double DelayRead(const double* buf, int bufSize, int writeIdx, double delaySamples)
  {
    double readPos = static_cast<double>(writeIdx) - delaySamples;  // calculate read position
    if (readPos < 0.0) readPos += static_cast<double>(bufSize);     // wrap around if negative

    int idx0 = static_cast<int>(readPos);     // integer part (sample before)
    int idx1 = idx0 + 1;                      // next sample
    if (idx0 >= bufSize) idx0 -= bufSize;     // keep in bounds
    if (idx1 >= bufSize) idx1 -= bufSize;

    const double frac = readPos - idx0;                       // fractional part (0..1)
    return buf[idx0] + frac * (buf[idx1] - buf[idx0]);        // linear interpolation
  }

  // calculates how many dB the compressor should reduce
  // formula: GR = (signal - threshold) * (1 - 1/ratio), capped at 30 dB
  inline double ComputeGR(double env, double threshDB, double ratio)
  {
    const double envDB = (env > 1e-10) ? 20.0 * std::log10(env) : -200.0;  // convert envelope to dB
    if (envDB > threshDB)                                                    // only compress above threshold
      return std::min((envDB - threshDB) * (1.0 - 1.0 / ratio), 30.0);  // limit to 30 dB max
    return 0.0;                                           // below threshold = no reduction
  }

  // soft taper for depth knobs: linear up to the knee, then eases to maxOut.
  // low/mid values stay unchanged, only the top of the range is held back
  inline double SoftCap(double x, double knee, double maxOut)
  {
    if (x <= knee) return x;                          // below knee: no change
    const double t = (x - knee) / (1.0 - knee);       // 0..1 in the upper region
    return knee + (maxOut - knee) * (2.0 * t - t * t); // quadratic ease-out toward maxOut
  }

  // adjusts GR value based on the mix knob — for the meter display
  inline double EffectiveGR(double rawGR, double compMix)
  {
    if (rawGR <= 0.0) return 0.0;  // nothing to show
    // scale by mix so the meter shows what you actually hear
    const double mixedGain = (1.0 - compMix) + compMix * std::exp(-rawGR * kLn10Over20);
    return -20.0 * std::log10(std::max(mixedGain, 1e-10));  // convert back to dB
  }
} // namespace

// runs when sample rate changes or playback resets — we clear everything
void DynaCore::OnReset()
{
  mSampleRate = GetSampleRate();  // get the new sample rate from the host

  // update smoothers for the new sample rate
  iplug::LogParamSmooth<double>* smoothers[] = {
    &mSmoothTremRate, &mSmoothTremDepth, &mSmoothPanRate, &mSmoothPanDepth,
    &mSmoothPitchRate, &mSmoothPitchDepth, &mSmoothPhaserRate, &mSmoothPhaserDepth,
    &mSmoothMasterInt, &mSmoothOutGain, &mSmoothBypass,
    &mSmoothCompThresh, &mSmoothCompRatio, &mSmoothCompGain, &mSmoothCompMix,
    &mSmoothWidth
  };
  for (auto* s : smoothers)
    s->SetSmoothTime(5.0, mSampleRate);

  mTremPhase = mPanPhase = mPitchPhase = mPhaserPhase = 0.0;  // LFOs start from zero

  std::memset(mPitchDelayBuf, 0, sizeof(mPitchDelayBuf));  // clear delay buffer
  mPitchDelayWriteIdx = 0;

  std::memset(mAllpassState, 0, sizeof(mAllpassState));  // clear phaser states

  mCompEnv[0] = mCompEnv[1] = 0.0;  // reset compressor
  mAutoGainEnvDB = 0.0;             // reset auto-gain envelope

  mOutputLevelDBL = mOutputLevelDBR = -100.0;    // meters show silence
  mOutputSmoothedL = mOutputSmoothedR = -100.0;
}

// main processing loop — host gives us nFrames samples, we process them
void DynaCore::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  // --- read all parameter values once per block ---
  const double bypassTarget = GetParam(kBypass)->Bool() ? 1.0 : 0.0;  // bypass on/off
  const double outDB     = GetParam(kOutputLevel)->Value();            // output in dB
  const double outAmp    = std::exp(outDB * kLn10Over20);              // convert dB to linear
  const int nChans       = NOutChansConnected();                       // how many channels

  // turns a rate knob into Hz — uses host BPM in sync mode, raw Hz otherwise
  // result is clamped to the param's max so fast divisions stay in range
  const double hostBPM = GetTempo();  // host tempo (defaults to 120 if not playing)
  auto RateHz = [this, hostBPM](int rateIdx, int syncIdx) -> double
  {
    IParam* p = GetParam(rateIdx);
    if (!p) return 0.0;                            // shouldn't happen, just safety
    const bool sync = GetParam(syncIdx)->Bool();   // is the BPM toggle on?
    if (!sync)
      return p->Value();  // free mode — knob value is Hz
    const int divIdx = NormToSyncDivIdx(p->GetNormalized());  // knob -> division
    const double hz  = SyncDivFreqHz(hostBPM, divIdx);        // division -> Hz at BPM
    return std::min(hz, p->GetMax());  // cap so we don't exceed module's Hz range
  };

  // tremolo
  const bool tremBypass  = GetParam(kTremBypass)->Bool();
  const double tremRate  = RateHz(kTremRate, kTremRateSync);          // Hz
  const double tremDepth = GetParam(kTremDepth)->Value() / 100.0; // normalize to 0..1

  // pan
  const bool panBypass   = GetParam(kPanBypass)->Bool();
  const double panRate   = RateHz(kPanRate, kPanRateSync);             // Hz
  const double panDepth  = GetParam(kPanDepth)->Value() / 100.0;  // normalize to 0..1

  // pitch drift
  const bool pitchBypass = GetParam(kPitchBypass)->Bool();
  const double pitchRate = RateHz(kPitchRate, kPitchRateSync);         // Hz
  // soft cap above 50% so max depth does not wobble the vocal too much
  const double pitchDepth= SoftCap(GetParam(kPitchDepth)->Value() / 100.0, 0.5, 0.65);

  // phaser
  const bool phaserBypass= GetParam(kPhaserBypass)->Bool();
  const double phaserRate= RateHz(kPhaserRate, kPhaserRateSync);       // Hz
  // soft cap above 50% — at max knob there is still ~40% dry signal, so vocal stays clear
  const double phaserDepth= SoftCap(GetParam(kPhaserDepth)->Value() / 100.0, 0.5, 0.60);

  // compressor
  const bool compBypass    = GetParam(kCompBypass)->Bool();
  const double compThresh  = GetParam(kCompThreshold)->Value();  // dB
  const double compRatio   = GetParam(kCompRatio)->Value();      // ratio like 4:1
  const double compGainDB  = GetParam(kCompGain)->Value();       // makeup gain dB
  const double compMix     = GetParam(kCompMix)->Value() / 100.0;  // mix 0..1
  const double compAttackMs  = GetParam(kCompAttack)->Value();   // ms
  const double compReleaseMs = GetParam(kCompRelease)->Value();  // ms

  // toggles
  const bool compAutoGain = GetParam(kCompAutoGain)->Bool();   // auto makeup on/off
  const bool tremSquare   = GetParam(kTremWaveform)->Bool();   // square wave LFO
  const bool panSquare    = GetParam(kPanWaveform)->Bool();    // square wave LFO

  const double masterInt = GetParam(kMasterIntensity)->Value() / 100.0;  // effect intensity
  const double widthNorm = GetParam(kWidth)->Value() / 100.0;  // width 0..2

  const double sr    = mSampleRate;    // sample rate
  const double invSr = 1.0 / sr;      // for advancing LFO phases

  // --- pre-compute coefficients for this block ---

  // convert attack/release time to smoothing coefficient
  // closer to 1.0 = slower reaction, closer to 0.0 = instant
  const double compAttackCoeff  = (compAttackMs  > 0.0)
    ? std::exp(-kTwoPi / (compAttackMs  * 0.001 * sr)) : 0.0;
  const double compReleaseCoeff = (compReleaseMs > 0.0)
    ? std::exp(-kTwoPi / (compReleaseMs * 0.001 * sr)) : 0.0;

  // auto-gain smoothing: ~300ms low-pass on the real GR, so makeup follows the
  // average gain reduction instead of fighting each transient
  const double autoGainCoeff = std::exp(-kTwoPi / (0.3 * sr));

  // pitch drift: 20ms base delay, LFO moves the read position ±2ms
  constexpr double kPitchCenterDelayMs = 20.0;  // center delay
  constexpr double kPitchModDepthMs    = 2.0;   // LFO swing range — small so vocals don't wobble too much

  // phaser sweep range on a log scale (sounds more natural)
  constexpr double kPhaserMinFreq     = 200.0;   // lowest freq
  constexpr double kPhaserMaxFreq     = 4000.0;  // highest freq
  const double phaserFreqRatio        = kPhaserMaxFreq / kPhaserMinFreq;
  const double phaserFreqRatioLog     = std::log(phaserFreqRatio);  // log scale

  const double bypassRampRate = 1.0 / (0.010 * sr); // ~10ms fade speed

  double peakL = 0.0;                 // peak level for output meter
  double peakR = 0.0;
  double lastGainReductionL  = 0.0;   // GR for compressor meter
  double lastGainReductionR  = 0.0;

  // --- per-sample loop ---
  for (int s = 0; s < nFrames; ++s)
  {
    // smooth each parameter so knob changes don't click
    const double sTremRate   = mSmoothTremRate.Process(tremRate);      // smoothed trem rate
    const double sTremDepth  = mSmoothTremDepth.Process(tremDepth);    // smoothed trem depth
    const double sPanRate    = mSmoothPanRate.Process(panRate);        // smoothed pan rate
    const double sPanDepth   = mSmoothPanDepth.Process(panDepth);      // smoothed pan depth
    const double sPitchRate  = mSmoothPitchRate.Process(pitchRate);    // smoothed pitch rate
    const double sPitchDepth = mSmoothPitchDepth.Process(pitchDepth);  // smoothed pitch depth
    const double sPhaserRate = mSmoothPhaserRate.Process(phaserRate);  // smoothed phaser rate
    const double sPhaserDepth= mSmoothPhaserDepth.Process(phaserDepth);// smoothed phaser depth
    const double sMasterInt  = mSmoothMasterInt.Process(masterInt);    // smoothed master intensity
    const double sOutAmp     = mSmoothOutGain.Process(outAmp);         // smoothed output gain

    const double sBypass     = mSmoothBypass.Process(bypassTarget);    // smoothed bypass value

    const double sCompThresh = mSmoothCompThresh.Process(compThresh);  // smoothed threshold
    const double sCompRatio  = mSmoothCompRatio.Process(compRatio);    // smoothed ratio
    const double sCompGainDB = mSmoothCompGain.Process(compGainDB);    // smoothed makeup gain
    const double sCompMix    = mSmoothCompMix.Process(compMix);        // smoothed comp mix
    const double sWidth      = mSmoothWidth.Process(widthNorm);        // smoothed width

    double dryL = static_cast<double>(inputs[0][s]);                      // read left input
    double dryR = (nChans >= 2) ? static_cast<double>(inputs[1][s]) : dryL;  // read right (or copy left)

    double procL = dryL;  // working copy that each effect will modify
    double procR = dryR;

    // fold to mono if width is zero, so both GR bars show the same
    if (sWidth < 0.001 && nChans >= 2) {
      const double monoSig = 0.5 * (procL + procR);  // average both channels
      procL = monoSig;
      procR = monoSig;
    }

    // --- Compressor ---
    // peak follower tracks level, reduces gain when above threshold,
    // then adds makeup gain and mixes with dry signal
    if (!compBypass)
    {
      const double absL = std::fabs(procL);  // absolute value of left
      // peak follower: fast attack, slow release
      mCompEnv[0] = absL + (absL > mCompEnv[0] ? compAttackCoeff : compReleaseCoeff) * (mCompEnv[0] - absL);

      const double absR = std::fabs(procR);  // absolute value of right
      mCompEnv[1] = absR + (absR > mCompEnv[1] ? compAttackCoeff : compReleaseCoeff) * (mCompEnv[1] - absR);

      const double grL = ComputeGR(mCompEnv[0], sCompThresh, sCompRatio);  // gain reduction L
      const double grR = ComputeGR(mCompEnv[1], sCompThresh, sCompRatio);  // gain reduction R

      // auto-gain envelope: slow average of the real GR
      // when the compressor barely works, this stays near 0, so makeup stays near 0 too
      const double avgGR = 0.5 * (grL + grR);
      mAutoGainEnvDB = avgGR + autoGainCoeff * (mAutoGainEnvDB - avgGR);

      // auto on: compensate the averaged GR. auto off: just use the knob value
      const double makeupDB = compAutoGain ? mAutoGainEnvDB : sCompGainDB;

      // apply GR and makeup, convert to linear
      const double gainL = std::exp((makeupDB - grL) * kLn10Over20);  // dB to multiplier
      const double gainR = std::exp((makeupDB - grR) * kLn10Over20);

      // parallel mix: blend compressed with original
      procL = procL + sCompMix * (procL * gainL - procL);  // mix=0 → dry, mix=1 → compressed
      procR = procR + sCompMix * (procR * gainR - procR);

      lastGainReductionL  = EffectiveGR(grL, sCompMix);  // for the GR meter
      lastGainReductionR  = EffectiveGR(grR, sCompMix);
    }

    // save signal after compressor — this is the "dry" for Master Intensity blend
    const double compL = procL;
    const double compR = procR;

    // --- Tremolo (LFO modulates volume) ---
    {
      mTremPhase += sTremRate * invSr;             // advance LFO phase
      if (mTremPhase >= 1.0) mTremPhase -= 1.0;   // wrap around at 1.0

      // fade in/out over ~10ms to avoid clicks
      const double tremTarget = tremBypass ? 0.0 : 1.0;
      RampToward(mTremBypassRamp, tremTarget, bypassRampRate);

      if (mTremBypassRamp > 0.0)
      {
        constexpr double kTremStereoOffset = 0.083; // ~30° offset between L and R
        double tremLFO_L, tremLFO_R;
        if (tremSquare)
        {
          // tanh turns a sine into a smooth square wave (no hard edges)
          constexpr double kSquareSteepness = 15.0;
          tremLFO_L = std::tanh(kSquareSteepness * std::sin(mTremPhase * kTwoPi));       // left LFO
          double rPhase = mTremPhase + kTremStereoOffset;                                // right phase
          if (rPhase >= 1.0) rPhase -= 1.0;                                              // wrap
          tremLFO_R = std::tanh(kSquareSteepness * std::sin(rPhase * kTwoPi));           // right LFO
        }
        else
        {
          tremLFO_L = std::sin(mTremPhase * kTwoPi);                        // sine for L
          tremLFO_R = std::sin((mTremPhase + kTremStereoOffset) * kTwoPi);  // sine for R, shifted 30°
        }
        // apply amplitude modulation: LFO down = volume drops
        double tremL = procL * (1.0 - sTremDepth * 0.5 * (1.0 - tremLFO_L));  // modulate L
        double tremR = procR * (1.0 - sTremDepth * 0.5 * (1.0 - tremLFO_R));  // modulate R
        // blend with bypass ramp
        procL += mTremBypassRamp * (tremL - procL);  // 0=dry, 1=full effect
        procR += mTremBypassRamp * (tremR - procR);
      }
    }

    // --- Pan Motion (moves sound left/right with constant-power pan law) ---
    // cos/sin keeps total volume constant at any position
    {
      mPanPhase += sPanRate * invSr;             // advance LFO
      if (mPanPhase >= 1.0) mPanPhase -= 1.0;   // wrap around

      const double panTarget = (panBypass || nChans < 2) ? 0.0 : 1.0;
      RampToward(mPanBypassRamp, panTarget, bypassRampRate);  // smooth bypass

      if (mPanBypassRamp > 0.0 && nChans >= 2)
      {
        // tanh makes a smooth square wave, or just use sine
        constexpr double kPanSquareSteepness = 15.0;
        double panLFO = panSquare
          ? std::tanh(kPanSquareSteepness * std::sin(mPanPhase * kTwoPi))  // square
          : std::sin(mPanPhase * kTwoPi);                                  // sine
        double panPos = panLFO * sPanDepth;                      // scale by depth knob
        double angle  = (1.0 + panPos) * 0.25 * kPi;            // map to 0..pi/2
        double gainL  = std::cos(angle) * kSqrt2;               // left gain (constant power)
        double gainR  = std::sin(angle) * kSqrt2;               // right gain
        double panL   = procL * gainL;                           // apply to left
        double panR   = procR * gainR;                           // apply to right
        procL += mPanBypassRamp * (panL - procL);                // blend with ramp
        procR += mPanBypassRamp * (panR - procR);
      }
    }

    // --- Pitch Drift (chorus effect using a modulated delay line) ---
    // writes samples into a circular buffer, reads back at a moving position
    // the movement of the read position changes the pitch slightly
    {
      mPitchDelayBuf[0][mPitchDelayWriteIdx] = procL;  // store left sample in buffer
      if (nChans >= 2)
        mPitchDelayBuf[1][mPitchDelayWriteIdx] = procR;  // store right sample

      mPitchPhase += sPitchRate * invSr;             // advance LFO
      if (mPitchPhase >= 1.0) mPitchPhase -= 1.0;   // wrap around

      const double pitchTarget = pitchBypass ? 0.0 : 1.0;
      RampToward(mPitchBypassRamp, pitchTarget, bypassRampRate);  // smooth bypass

      if (mPitchBypassRamp > 0.0)
      {
        double pitchLFO_L = std::sin(mPitchPhase * kTwoPi);  // sine for L
        double pitchLFO_R = std::cos(mPitchPhase * kTwoPi);  // cosine for R (90° stereo offset)

        double centerDelay  = kPitchCenterDelayMs * 0.001 * sr;             // 20ms in samples
        double modExcursion = kPitchModDepthMs * 0.001 * sr * sPitchDepth;  // LFO swing in samples

        // keep delay inside buffer bounds to avoid crashes
        auto clampDelay = [](double d, int bufSize) {
          if (d < 1.0) d = 1.0;                                                // min 1 sample
          if (d > static_cast<double>(bufSize - 2)) d = static_cast<double>(bufSize - 2);  // max limit
          return d;
        };

        double delayL  = clampDelay(centerDelay + pitchLFO_L * modExcursion, kPitchDelayBufSize);  // L delay
        double delayR  = clampDelay(centerDelay + pitchLFO_R * modExcursion, kPitchDelayBufSize);  // R delay
        // read from buffer with interpolation
        double pitchL  = DelayRead(mPitchDelayBuf[0], kPitchDelayBufSize, mPitchDelayWriteIdx, delayL);
        double pitchR  = (nChans >= 2)
                         ? DelayRead(mPitchDelayBuf[1], kPitchDelayBufSize, mPitchDelayWriteIdx, delayR)
                         : pitchL;
        procL += mPitchBypassRamp * (pitchL - procL);  // blend with ramp
        procR += mPitchBypassRamp * (pitchR - procR);
      }

      mPitchDelayWriteIdx = (mPitchDelayWriteIdx + 1) % kPitchDelayBufSize;  // move write position
    }

    // --- Phaser (6 allpass filters + feedback) ---
    // allpass stages shift the phase, and when mixed with dry signal
    // some frequencies cancel out, creating moving notches
    // feedback at 45% makes the effect more noticeable
    {
      mPhaserPhase += sPhaserRate * invSr;             // advance LFO
      if (mPhaserPhase >= 1.0) mPhaserPhase -= 1.0;   // wrap around

      const double phaserTarget = phaserBypass ? 0.0 : 1.0;
      RampToward(mPhaserBypassRamp, phaserTarget, bypassRampRate);  // smooth bypass

      // filters keep running in bypass so they stay warm (no click on re-enable)
      if (mPhaserBypassRamp > 0.0 || phaserTarget > 0.0)
      {
        constexpr double kPhaserFeedback = 0.10;  // feedback amount — kept low so vocal formants are not over-colored

        // LFO gives sweep position 0..1 (sin for L, cos for R = stereo)
        double sweepNormL = 0.5 + 0.5 * std::sin(mPhaserPhase * kTwoPi);  // L sweep
        double sweepNormR = 0.5 + 0.5 * std::cos(mPhaserPhase * kTwoPi);  // R sweep

        // map sweep to frequency on log scale (sounds more natural than linear)
        double freqL    = kPhaserMinFreq * std::exp(sweepNormL * phaserFreqRatioLog);  // L freq
        double freqR    = kPhaserMinFreq * std::exp(sweepNormR * phaserFreqRatioLog);  // R freq
        double apCoeffL = AllpassCoeff(freqL, sr);  // coefficient for L
        double apCoeffR = AllpassCoeff(freqR, sr);  // coefficient for R

        // add feedback from last output back to input
        double apInL  = procL + kPhaserFeedback * mPhaserFeedbackL;  // input + feedback
        double apOutL = apInL;
        for (int st = 0; st < 6; ++st)                    // pass through 6 allpass filters
          apOutL = AllpassProcess(apOutL, apCoeffL, mAllpassState[0][st]);
        mPhaserFeedbackL = apOutL;                         // save for next sample

        // same thing for right channel
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
          apOutR = apOutL;  // mono: just copy left
        }

        // mix allpass output with dry by depth amount
        double phasL = procL + sPhaserDepth * (apOutL - procL);  // blend L
        double phasR = procR + sPhaserDepth * (apOutR - procR);  // blend R
        procL += mPhaserBypassRamp * (phasL - procL);            // apply bypass ramp
        procR += mPhaserBypassRamp * (phasR - procR);
      }
    }

    // --- Master Intensity: blend between comp-only and full effect chain ---
    // equal-power crossfade (sin/cos) instead of linear, so volume does not dip
    // in the middle — phaser and pitch drift shift the phase a bit, and on a
    // linear crossfade those shifts would partially cancel with the dry at 50%
    double wetL = procL;                                     // signal after all effects
    double wetR = procR;
    const double intDryGain = std::cos(sMasterInt * kPi * 0.5);  // 0%=1, 100%=0
    const double intWetGain = std::sin(sMasterInt * kPi * 0.5);  // 0%=0, 100%=1
    procL = intDryGain * compL + intWetGain * wetL;
    procR = intDryGain * compR + intWetGain * wetR;

    procL *= sOutAmp;                                        // apply output gain
    procR *= sOutAmp;

    // --- Stereo Width (Mid/Side) ---
    if (nChans >= 2)
    {
      double mid  = 0.5 * (procL + procR);                  // mid = sum of both channels
      double side = 0.5 * (procL - procR);                  // side = difference
      side *= sWidth;                                        // scale: 0=mono, 1=normal, 2=wide
      procL = mid + side;                                    // recombine to L
      procR = mid - side;                                    // recombine to R
    }

    // --- Global bypass: crossfade processed and dry ---
    double outL = procL * (1.0 - sBypass) + dryL * sBypass;  // 0=processed, 1=dry
    double outR = procR * (1.0 - sBypass) + dryR * sBypass;

    outputs[0][s] = static_cast<sample>(outL);               // write left output
    if (nChans >= 2)
      outputs[1][s] = static_cast<sample>(outR);             // write right output

    // track peak level for the meter
    double absL = std::fabs(outL);                           // absolute left
    double absR = std::fabs(outR);                           // absolute right
    if (absL > peakL) peakL = absL;                          // keep highest
    if (absR > peakR) peakR = absR;
  }

  // --- output meter smoothing ---
  {
    double rawDBL = (peakL > 1e-10) ? 20.0 * std::log10(peakL) : -100.0;  // peak to dB
    double rawDBR = (peakR > 1e-10) ? 20.0 * std::log10(peakR) : -100.0;

    constexpr double kMeterAttackMs  = 40.0;   // rise time
    constexpr double kMeterReleaseMs = 300.0;   // fall time
    double attSample = std::exp(-1.0 / (kMeterAttackMs  * 0.001 * sr));  // attack coeff
    double relSample = std::exp(-1.0 / (kMeterReleaseMs * 0.001 * sr));  // release coeff
    double attC = std::pow(attSample, static_cast<double>(nFrames));     // scale for block
    double relC = std::pow(relSample, static_cast<double>(nFrames));

    double cL = (rawDBL > mOutputSmoothedL) ? attC : relC;              // pick attack or release
    mOutputSmoothedL = rawDBL + cL * (mOutputSmoothedL - rawDBL);        // one-pole smoother
    // only clamp the reported value, not the internal smoother
    mOutputLevelDBL = (mOutputSmoothedL < -54.0) ? -100.0 : mOutputSmoothedL;

    // same for right channel
    double cR = (rawDBR > mOutputSmoothedR) ? attC : relC;
    mOutputSmoothedR = rawDBR + cR * (mOutputSmoothedR - rawDBR);
    mOutputLevelDBR = (mOutputSmoothedR < -54.0) ? -100.0 : mOutputSmoothedR;
    mMeterUpdateCount++;  // UI checks this to know there is new data
  }

  // pass GR values to UI
  mGainReductionL = lastGainReductionL;
  mGainReductionR = lastGainReductionR;
  mGRUpdateCount++;
}
#endif

