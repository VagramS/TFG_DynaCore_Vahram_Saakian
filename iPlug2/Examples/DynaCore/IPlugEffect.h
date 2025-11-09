#pragma once

#include "IPlug_include_in_plug_hdr.h"

const int kNumPresets = 1;

// Parameter indices (match UI and DSP)
enum EParams
{
  kGain = 0,
  
  // --- Tremolo ---
  kTremBypass,
  kTremRate,
  kTremDepth,
  
  // --- Pan Motion ---
  kPanBypass,
  kPanRate,
  kPanDepth,
  
  // --- Pitch Drift ---
  kPitchBypass,
  kPitchRate,
  kPitchDepth,
  
  // --- Phaser ---
  kPhaserBypass,
  kPhaserRate,
  kPhaserDepth,

  // --- COMP ---
  kCompMix,
  kCompThreshold,
  kCompRatio,
  kCompGain,
  kCompAttack,
  kCompRelease,
  kCompBypass,

  // --- MASTER / OUTPUT ---
  kMasterIntensity,
  kOutputLevel,
  kBypass,
  
  kNumParams
};

using namespace iplug;
using namespace igraphics;

// Main plugin class
class IPlugEffect final : public Plugin
{
public:
  IPlugEffect(const InstanceInfo& info);

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif
  
  // Reset parameters from UI (used by presets page "Revert to default")
  void ApplyDefaultPresetFromUI();

  // Last measured output level in dB for UI meter
  double GetOutputLevelDB() const { return mOutputLevelDB; }

private:
  // Output level for numeric meter
  double mOutputLevelDB = 0.0; // starts as 0.0 dB for UI

  // Internal phase for optional test tone generator
  double mTestPhase = 0.0;     // phase accumulator for test tone

  // Enable/disable internal test tone instead of processing input
  bool mUseTestTone = false;   // when true, plugin outputs a test tone
};
