// DynaCore.h
// Main plugin class and parameter list.
//
// Processing order:
//   Input -> [mono fold] -> Compressor -> Tremolo -> Pan -> Pitch Drift
//         -> Phaser -> Master Intensity -> Output Gain -> Width -> Bypass -> Output
//
// Author: Vahram Saakian, UCM TFG 2025-2026

#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "Smoothers.h"
#include <cmath>
#include <cstring>

// iPlug2 requires at least 1 preset slot even if we handle presets ourselves
constexpr int kNumPresets = 1;

// Parameter IDs — must match the order in the constructor
enum EParams
{
  kGain = 0,         // output gain (dB)

  // --- Tremolo (volume modulation with LFO) ---
  kTremBypass,       // 1 = bypassed, 0 = active
  kTremRate,         // LFO speed in Hz
  kTremDepth,        // how strong the effect is (0-100%)

  // --- Pan Motion (stereo panning with LFO) ---
  kPanBypass,        // 1 = bypassed, 0 = active
  kPanRate,          // LFO speed in Hz
  kPanDepth,         // panning amount (0-100%)

  // --- Pitch Drift (modulated delay = chorus/vibrato) ---
  kPitchBypass,      // 1 = bypassed, 0 = active
  kPitchRate,        // LFO speed in Hz
  kPitchDepth,       // modulation amount (0-100%)

  // --- Phaser (6 allpass stages + feedback) ---
  kPhaserBypass,     // 1 = bypassed, 0 = active
  kPhaserRate,       // sweep speed in Hz
  kPhaserDepth,      // wet/dry (0-100%)

  // --- Compressor (peak follower, parallel mix) ---
  kCompMix,          // parallel blend (0-100%)
  kCompThreshold,    // threshold in dB
  kCompRatio,        // ratio (1:1 to 20:1)
  kCompGain,         // makeup gain in dB
  kCompAttack,       // attack in ms
  kCompRelease,      // release in ms
  kCompBypass,       // 0 = active, 1 = bypassed

  // --- Mastering ---
  kWidth,            // stereo width: 0% = mono, 100% = normal, 200% = wide

  // --- Master / Output ---
  kMasterIntensity,  // global wet/dry for all modulation effects (0-100%)
  kOutputLevel,      // output volume in dB (-20 to +20)
  kBypass,           // global bypass: 0 = processing, 1 = dry signal

  // --- Toggles ---
  kCompAutoGain,     // auto makeup gain on/off
  kTremWaveform,     // tremolo shape: 0 = sine, 1 = square
  kPanWaveform,      // pan shape: 0 = sine, 1 = square

  // --- Rate sync toggles (Hz vs musical division relative to host BPM) ---
  kTremRateSync,     // 0 = Hz, 1 = synced to BPM
  kPanRateSync,      // 0 = Hz, 1 = synced to BPM
  kPitchRateSync,    // 0 = Hz, 1 = synced to BPM
  kPhaserRateSync,   // 0 = Hz, 1 = synced to BPM

  kNumParams         // total number of parameters
};

using namespace iplug;
using namespace igraphics;

// plugin class
class DynaCore final : public Plugin
{
public:
  DynaCore(const InstanceInfo& info);

#if IPLUG_DSP
  // main audio processing — host calls this for each block of samples
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;

  // runs when sample rate changes or playback resets
  void OnReset() override;
#endif

  // reset all parameters to default values (used by the Revert button)
  void ApplyDefaultPresetFromUI();

  // getters for the output level meter (UI reads these every frame)
  double   GetOutputLevelDB()    const { return std::max(mOutputLevelDBL, mOutputLevelDBR); }
  double   GetOutputLevelDBL()   const { return mOutputLevelDBL; }
  double   GetOutputLevelDBR()   const { return mOutputLevelDBR; }
  uint64_t GetMeterUpdateCount() const { return mMeterUpdateCount; }

  // getters for the GR meter (compressor gain reduction)
  double   GetGainReductionL()  const { return mGainReductionL; }
  double   GetGainReductionR()  const { return mGainReductionR; }
  uint64_t GetGRUpdateCount()   const { return mGRUpdateCount; }

private:
  // output meter values (DSP writes, UI reads)
  double   mOutputLevelDBL   = -100.0;
  double   mOutputLevelDBR   = -100.0;
  double   mOutputSmoothedL  = -100.0;  // internal smoother state
  double   mOutputSmoothedR  = -100.0;
  uint64_t mMeterUpdateCount = 0;

  // GR meter values
  double   mGainReductionL  = 0.0;
  double   mGainReductionR  = 0.0;
  uint64_t mGRUpdateCount   = 0;

  double mSampleRate = 44100.0;

  // one smoother per parameter — prevents clicks when values change (5ms transition)
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

  // LFO phases (0 to 1, wraps around)
  double mTremPhase   = 0.0;
  double mPanPhase    = 0.0;
  double mPitchPhase  = 0.0;
  double mPhaserPhase = 0.0;

  // pitch drift delay buffer (~93ms at 44.1kHz)
  static constexpr int kPitchDelayBufSize = 4096;
  double mPitchDelayBuf[2][kPitchDelayBufSize] = {};
  int    mPitchDelayWriteIdx = 0;

  // phaser: 6 allpass filter states + feedback for each channel
  double mAllpassState[2][6] = {};
  double mPhaserFeedbackL = 0.0;
  double mPhaserFeedbackR = 0.0;

  // compressor peak envelope per channel
  double mCompEnv[2] = {};

  // bypass ramps (0=off, 1=on) — fades over ~10ms so there are no clicks
  double mTremBypassRamp   = 0.0;
  double mPanBypassRamp    = 0.0;
  double mPitchBypassRamp  = 0.0;
  double mPhaserBypassRamp = 0.0;
};
