#pragma once

#include "IPlug_include_in_plug_hdr.h"

const int kNumPresets = 1;


enum EParams
{
  kGain = 0,
  kCompBypass,
  kBypass,

  kTremBypass,
  kTremRate,
  kTremDepth,

  kPanBypass,
  kPanRate,
  kPanDepth,

  kPitchBypass,
  kPitchRate,
  kPitchDepth,

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

  // --- MASTER / OUTPUT ---
  kMasterIntensity,
  kOutputLevel,

  kNumParams
};

using namespace iplug;
using namespace igraphics;

class IPlugEffect final : public Plugin
{
public:
  IPlugEffect(const InstanceInfo& info);

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif
};
