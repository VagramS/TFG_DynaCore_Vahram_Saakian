#pragma once

#include "IPlug_include_in_plug_hdr.h"

const int kNumPresets = 1;

// Parameter indices (shared by UI and DSP)
enum EParams
{
  kGain = 0,

  // Tremolo
  kTremBypass,
  kTremRate,
  kTremDepth,

  // Pan motion
  kPanBypass,
  kPanRate,
  kPanDepth,

  // Pitch drift
  kPitchBypass,
  kPitchRate,
  kPitchDepth,

  // Phaser
  kPhaserBypass,
  kPhaserRate,
  kPhaserDepth,

  // Compressor
  kCompMix,
  kCompThreshold,
  kCompRatio,
  kCompGain,
  kCompAttack,
  kCompRelease,
  kCompBypass,

  // Master / output
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

  // Reset parameters from the presets UI ("Revert to default")
  void ApplyDefaultPresetFromUI();

  // Last measured output level in dB for the UI meter
  double GetOutputLevelDB() const { return mOutputLevelDB; }

private:
  // Output level for the numeric meter
  double mOutputLevelDB = 0.0;

  // Phase accumulator for optional internal test tone
  double mTestPhase = 0.0;

  // When true, plugin outputs a test tone instead of input
  bool mUseTestTone = false;
};
