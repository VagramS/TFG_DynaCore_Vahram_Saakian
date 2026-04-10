// DynaCore.h
// Plugin class declaration + parameter enum.
//
// Signal chain:
//   Input -> [mono fold] -> Compressor -> Tremolo -> Pan -> Pitch Drift -> Phaser
//         -> Master Intensity -> Output Gain -> Stereo Width -> Global Bypass -> Output
//
// Author: Vahram Saakian, UCM TFG 2025-2026

#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "Smoothers.h"
#include <cmath>
#include <cstring>

// iPlug2 needs at least 1 preset slot even if you manage them yourself
constexpr int kNumPresets = 1;

// Parameter indices — order must match exactly between this enum and
// the parameter registration in the constructor.
enum EParams
{
  kGain = 0,         ///< Output gain (dB), applied after all processing

  // --- Tremolo (amplitude-modulation LFO) ---
  kTremBypass,       ///< 1 = module bypassed, 0 = active
  kTremRate,         ///< LFO frequency in Hz (0.0 = stopped)
  kTremDepth,        ///< Modulation depth 0–100 %

  // --- Pan Motion (stereo panning LFO) ---
  kPanBypass,        ///< 1 = module bypassed, 0 = active
  kPanRate,          ///< LFO frequency in Hz
  kPanDepth,         ///< Panning depth 0–100 %

  // --- Pitch Drift (modulated delay line — chorus / vibrato) ---
  kPitchBypass,      ///< 1 = module bypassed, 0 = active
  kPitchRate,        ///< LFO frequency in Hz
  kPitchDepth,       ///< Modulation depth 0–100 %

  // --- Phaser (6-stage allpass with feedback) ---
  kPhaserBypass,     ///< 1 = module bypassed, 0 = active
  kPhaserRate,       ///< Sweep LFO frequency in Hz
  kPhaserDepth,      ///< Wet/dry blend 0–100 %

  // --- Compressor (peak-following with parallel mix) ---
  kCompMix,          ///< Parallel wet/dry blend 0–100 %
  kCompThreshold,    ///< Threshold in dB (−50 … 0)
  kCompRatio,        ///< Ratio (1:1 … 20:1)
  kCompGain,         ///< Makeup gain in dB
  kCompAttack,       ///< Attack time in ms
  kCompRelease,      ///< Release time in ms
  kCompBypass,       ///< 0 = active, 1 = bypassed

  // --- Mastering ---
  kWidth,            ///< Stereo width via M/S: 0 % = mono, 100 % = normal, 200 % = extra wide

  // --- Master / Output ---
  kMasterIntensity,  ///< Wet/dry blend of ALL modulation (0–100 %). Compressor always active.
  kOutputLevel,      ///< Output gain in dB (−20 … +20)
  kBypass,           ///< Global bypass: 0 = processing, 1 = dry passthrough

  kNumParams         ///< Total number of automatable parameters
};

using namespace iplug;
using namespace igraphics;

// main plugin class
class DynaCore final : public Plugin
{
public:
  DynaCore(const InstanceInfo& info);

#if IPLUG_DSP
  // called by the host for each audio buffer — all DSP happens here
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;

  // called on sample-rate change or transport reset — resets LFO phases, delay buffers, etc.
  void OnReset() override;
#endif

  // resets all params to defaults (called from the "Revert" button in the preset overlay)
  void ApplyDefaultPresetFromUI();

  // output level meter — read by the UI control every frame
  double   GetOutputLevelDB()    const { return std::max(mOutputLevelDBL, mOutputLevelDBR); }
  double   GetOutputLevelDBL()   const { return mOutputLevelDBL; }
  double   GetOutputLevelDBR()   const { return mOutputLevelDBR; }
  uint64_t GetMeterUpdateCount() const { return mMeterUpdateCount; }

  // GR meter — read by the compressor meter control
  double   GetGainReductionL()  const { return mGainReductionL; }
  double   GetGainReductionR()  const { return mGainReductionR; }
  uint64_t GetGRUpdateCount()   const { return mGRUpdateCount; }

private:
  // output level meter state (written by DSP, read by UI)
  double   mOutputLevelDBL   = -100.0;
  double   mOutputLevelDBR   = -100.0;
  double   mOutputSmoothedL  = -100.0;  // internal smoother, never snapped to -100
  double   mOutputSmoothedR  = -100.0;
  uint64_t mMeterUpdateCount = 0;

  // GR meter state
  double   mGainReductionL  = 0.0;
  double   mGainReductionR  = 0.0;
  uint64_t mGRUpdateCount   = 0;

  double mSampleRate = 44100.0;

  // per-sample smoothers for every automatable parameter (~5 ms smoothing time)
  iplug::LogParamSmooth<double> mSmoothTremRate    {5.0, 0.0};
  iplug::LogParamSmooth<double> mSmoothTremDepth   {5.0, 0.0};
  iplug::LogParamSmooth<double> mSmoothPanRate     {5.0, 0.0};
  iplug::LogParamSmooth<double> mSmoothPanDepth    {5.0, 0.0};
  iplug::LogParamSmooth<double> mSmoothPitchRate   {5.0, 0.0};
  iplug::LogParamSmooth<double> mSmoothPitchDepth  {5.0, 0.0};
  iplug::LogParamSmooth<double> mSmoothPhaserRate  {5.0, 0.0};
  iplug::LogParamSmooth<double> mSmoothPhaserDepth {5.0, 0.0};
  iplug::LogParamSmooth<double> mSmoothMasterInt   {5.0, 1.0};
  iplug::LogParamSmooth<double> mSmoothOutGain     {5.0, 1.0};
  iplug::LogParamSmooth<double> mSmoothBypass      {5.0, 0.0};
  iplug::LogParamSmooth<double> mSmoothCompThresh  {5.0, -25.0};
  iplug::LogParamSmooth<double> mSmoothCompRatio   {5.0, 2.0};
  iplug::LogParamSmooth<double> mSmoothCompGain    {5.0, 0.0};
  iplug::LogParamSmooth<double> mSmoothCompMix     {5.0, 1.0};
  iplug::LogParamSmooth<double> mSmoothWidth       {5.0, 1.0};

  // LFO phases, normalised 0–1
  double mTremPhase   = 0.0;
  double mPanPhase    = 0.0;
  double mPitchPhase  = 0.0;
  double mPhaserPhase = 0.0;

  // pitch drift: circular delay buffer (~93 ms at 44.1 kHz)
  static constexpr int kPitchDelayBufSize = 4096;
  double mPitchDelayBuf[2][kPitchDelayBufSize] = {};
  int    mPitchDelayWriteIdx = 0;

  // phaser: 6-stage allpass state + feedback per channel
  double mAllpassState[2][6] = {};
  double mPhaserFeedbackL = 0.0;
  double mPhaserFeedbackR = 0.0;

  // compressor: peak envelope follower per channel
  double mCompEnv[2] = {};

  // per-module bypass ramps: 0 = bypassed, 1 = active; ramps ~10 ms to avoid clicks
  double mTremBypassRamp   = 0.0;
  double mPanBypassRamp    = 0.0;
  double mPitchBypassRamp  = 0.0;
  double mPhaserBypassRamp = 0.0;
};
