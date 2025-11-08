#include "IPlugEffect.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"


// Окно пресетов на весь плагин, закрывается кликом вне заданной области
class PresetsPageControl : public IControl
{
public:
  PresetsPageControl(const IRECT& bounds, const IBitmap& pageBitmap)
  : IControl(bounds)
  , mPageBitmap(pageBitmap)
  {
    mIgnoreMouse = false;
  }

  void Draw(IGraphics& g) override
  {
    g.DrawBitmap(mPageBitmap, mRECT);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    // "активная зона", где клик НЕ закрывает окно
    const IRECT active(19.f, 104.f, 554.f, 578.f);

    if (!active.Contains(x, y))
    {
      if (IGraphics* ui = GetUI())
      {
        ui->RemoveControl(this);  // контрол удалили
      }
      return;                     // ВАЖНО: больше не трогаем this
    }

    // внутри активной зоны просто ничего не делаем.
    SetDirty(false);
  }

private:
  IBitmap mPageBitmap;
};


// Кнопка "SELECT PRESET", открывает окно пресетов
class SelectPresetControl : public IControl
{
public:
  SelectPresetControl(const IRECT& bounds,
                      const IBitmap& buttonBitmap,
                      const IBitmap& pageBitmap)
  : IControl(bounds)
  , mButtonBitmap(buttonBitmap)
  , mPageBitmap(pageBitmap)
  {}

  void Draw(IGraphics& g) override
  {
    g.DrawBitmap(mButtonBitmap, mRECT);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (IGraphics* ui = GetUI())
    {
      // полный размер плагина
      IRECT fullRect(0.f, 0.f,
                     static_cast<float>(PLUG_WIDTH),
                     static_cast<float>(PLUG_HEIGHT));

      ui->AttachControl(new PresetsPageControl(fullRect, mPageBitmap));
    }

    SetDirty(false);
  }

private:
  IBitmap mButtonBitmap;
  IBitmap mPageBitmap;
};


// КНОПКА-ТУМБЛЕР ON/OFF
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
    const bool bypass = (GetValue() >= 0.5f); // true = байпасс включен
    const IBitmap& bmp = bypass ? mOff : mOn; // bypass -> OFF, !bypass -> ON
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

// Крутилка на базе IVKnobControl, с PNG-капом и прогресс-кольцом
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
                  "",          // без подписи
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


IPlugEffect::IPlugEffect(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
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


    // фон
        IBitmap bg = pGraphics->LoadBitmap(MAIN_BACKGROUND_FN, 1);
        const IRECT bounds = pGraphics->GetBounds();
        pGraphics->AttachControl(new IBitmapControl(bounds, bg));

        // КНОПКА COMP (как было)
        const float x_c = 371.f;
        const float y_c = 447.f;
        const float w_c = 21.f;
        const float h_c = 20.f;
        IRECT compBtnRect(x_c, y_c, x_c + w_c, y_c + h_c);

        IBitmap bmpOff = pGraphics->LoadBitmap(COMP_OFF_FN, 1);
        IBitmap bmpOn  = pGraphics->LoadBitmap(COMP_ON_FN, 1);

        pGraphics->AttachControl(new CompBypassButton(
          compBtnRect, kCompBypass, bmpOff, bmpOn));
      
        // ==== BYPASS (общий) ====
        const float x_b = 836.f;
        const float y_b = 74.f;
        const float w_b = 62.f;
        const float h_b = 20.f;
        IRECT bypassBtnRect(x_b, y_b, x_b + w_b, y_b + h_b);

        IBitmap bmpBypOff = pGraphics->LoadBitmap(BYPASS_OFF_FN, 1);
        IBitmap bmpBypOn  = pGraphics->LoadBitmap(BYPASS_ON_FN, 1);

        // ВАЖНО: для общего bypass меняем порядок, чтобы:
        // value = false (0.0)  -> серая (OFF) картинка
        // value = true  (1.0)  -> яркая (ON) картинка
        pGraphics->AttachControl(new CompBypassButton(
          bypassBtnRect, kBypass, bmpBypOn, bmpBypOff));

        // ==== 4 МОДУЛЯ ====
        // общие bitmaps для модульных кнопок
        IBitmap modOff = pGraphics->LoadBitmap(MODULE_OFF_FN, 1);
        IBitmap modOn  = pGraphics->LoadBitmap(MODULE_ON_FN, 1);

        const float w_m = 19.f;
        const float h_m = 18.f;

        // Tremolo
        IRECT tremRect(126.f, 390.f, 127.f + w_m, 390.f + h_m);
        pGraphics->AttachControl(new CompBypassButton(
          tremRect, kTremBypass, modOff, modOn));

        // Pan Motion
        IRECT panRect(344.f, 390.f, 344.f + w_m, 390.f + h_m);
        pGraphics->AttachControl(new CompBypassButton(
          panRect, kPanBypass, modOff, modOn));

        // Pitch Drift
        IRECT pitchRect(560.f, 390.f, 560.f + w_m, 390.f + h_m);
        pGraphics->AttachControl(new CompBypassButton(
          pitchRect, kPitchBypass, modOff, modOn));

        // Phaser
        IRECT phaserRect(776.f, 390.f, 776.f + w_m, 390.f + h_m);
        pGraphics->AttachControl(new CompBypassButton(
          phaserRect, kPhaserBypass, modOff, modOn));
        // =================

        // Select preset
        const float x_p = 27.f;
        const float y_p = 71.f;
        const float w_p = 245.f;
        const float h_p = 26.f;

        IBitmap presetBtnBmp   = pGraphics->LoadBitmap(SELECT_PRESET_FN, 1);
        IBitmap presetsPageBmp = pGraphics->LoadBitmap(PRESETS_PAGE_FN, 1);

        IRECT preset_bounds(x_p, y_p, x_p + w_p, y_p + h_p);
        pGraphics->AttachControl(new SelectPresetControl(
          preset_bounds, presetBtnBmp, presetsPageBmp));
            
    
    // ===== BIG KNOB (вращаемый cap) =====
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
          pitchDepthRect, kPitchDepth, bigKnob, knobStyle,kKnobMinAngle, kKnobMaxAngle));

        // --- Phaser ---
        IRECT phaserRateRect (713.f, 332.f, 713.f + kKnobW, 332.f + kKnobH);
        IRECT phaserDepthRect(830.f, 332.f, 830.f + kKnobW, 332.f + kKnobH);

        pGraphics->AttachControl(new BitmapIVKnob(
          phaserRateRect, kPhaserRate, bigKnob,knobStyle, kKnobMinAngle, kKnobMaxAngle));

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

        IRECT compMixRect      (47.f, 513.f,  47.f + kMidKnobW,   512.f + kMidKnobH);
        IRECT compThreshRect   (130.f, 513.f, 129.5f + kMidKnobW,   512.f + kMidKnobH);
        IRECT compRatioRect    (204.5f, 513.f, 204.5f + kMidKnobW,   512.f + kMidKnobH);
        IRECT compGainRect     (275.f, 513.f, 274.5f + kMidKnobW,   512.f + kMidKnobH);
    
        pGraphics->AttachControl(new BitmapIVKnob(
          compMixRect, kCompMix, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));
        pGraphics->AttachControl(new BitmapIVKnob(
          compThreshRect, kCompThreshold, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));
        pGraphics->AttachControl(new BitmapIVKnob(
          compRatioRect, kCompRatio, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));
        pGraphics->AttachControl(new BitmapIVKnob(
          compGainRect, kCompGain, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

        // --- COMP: Attack / Release (маленькие) ---

        IRECT compAttackRect  (351.f, 493.f, 351.5f + kSmallKnobW, 492.f + kSmallKnobH);
        IRECT compReleaseRect (351.f, 541.f, 351.5f + kSmallKnobW, 540.f + kSmallKnobH);

        pGraphics->AttachControl(new BitmapIVKnob(
          compAttackRect, kCompAttack, smallKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));
        pGraphics->AttachControl(new BitmapIVKnob(
          compReleaseRect, kCompRelease, smallKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

        // --- MASTERING: Intensity (mid_knob) ---

        IRECT masterIntRect(460.5f, 513.f, 460.f + kMidKnobW, 512.5f + kMidKnobH);
        pGraphics->AttachControl(new BitmapIVKnob(
          masterIntRect, kMasterIntensity, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

        // --- OUTPUT: Level (mid_knob) ---
        // можно где-то справа внизу рядом с твоим текущим output-колоном

        IRECT outLevelRect(950.5f, 544.5f, 950.5f + kMidKnobW, 544.5f + kMidKnobH);
        pGraphics->AttachControl(new BitmapIVKnob(
          outLevelRect, kOutputLevel, midKnob, knobStyle, kKnobMinAngle, kKnobMaxAngle));

        
   };
#endif
}

#if IPLUG_DSP
#include <cmath>

void IPlugEffect::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const bool bypass = GetParam(kBypass)->Bool();            // true = плагин выключен

  // предположим, что kOutputLevel у тебя в dB (-24 .. +24, как выше)
  const double outGainDB = GetParam(kOutputLevel)->Value(); // dB
  const double outGain   = std::pow(10.0, outGainDB / 20.0); // dB -> линейный

  const int nChans = NOutChansConnected();

  for (int s = 0; s < nFrames; s++)
  {
    for (int c = 0; c < nChans; c++)
    {
      const sample inSample = inputs[c][s];

      // TODO: сюда позже вставишь обработку модулей/компа
      const sample processed = inSample;

      // если bypass включен – отдать сухой сигнал без гейна
      // если выключен – применяем обработку + выходной гейн
      outputs[c][s] = bypass ? inSample : static_cast<sample>(processed * outGain);
    }
  }
}
#endif
