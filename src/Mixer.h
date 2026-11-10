#pragma once

#include "Archive.h"

#include <chalkwalk/dsp/PeakLimiter.h>
#include <chalkwalk/dsp/SlidingLoudness.h>

#include <map>
#include <memory>
#include <vector>

// One mix of the room, for everyone (DESIGN §6, §9.4; PRINCIPLES §3).
//
// Each channel is measured, BEFORE its gain, by BS.1770 over a moving window
// and ridden slowly toward a common level; panned as its player set it; summed;
// the sum is ridden toward the listening level and limited. The rider moves
// only once a channel has played for long enough to be measured, so a quiet
// player is not lifted into noise on their first note.
//
// Gain changes are ramped across the block they happen in, so nothing steps.
// Not real-time: it allocates when a channel first appears.

namespace balcony {

struct MixerConfig {
  double sampleRate = 48000.0;
  double windowSeconds = 120.0;
  double minGatedSeconds = 10.0;
  double channelTargetLufs = -23.0;
  double maxBoostDb = 9.0;
  double maxCutDb = 12.0;
  // Below this the window is more likely noise or bleed than playing: cut it
  // if it is somehow loud, never lift it.
  double boostFloorLufs = -50.0;
  double slewDbPerSecond = 0.5;
  double masterTargetLufs = -16.0;
  double limiterLookaheadMs = 5.0;
  double limiterReleaseMs = 50.0;
  double limiterCeilingDb = -1.0;
};

struct ChannelMeter {
  ChannelKey key;
  float peakDb = -120.0f;   // of what is heard: after gain and pan
  float levelDb = -120.0f;  // rms of what is heard over the block, dBFS
};

struct ChannelInput {
  const ChannelKey *key = nullptr;
  const float *left = nullptr;
  const float *right = nullptr;  // null for mono
  float pan = 0.0f;              // -1 left .. +1 right, as the player set it
  bool inStream = true;          // false: a player who asked to be left out
};

class Mixer {
public:
  explicit Mixer(const MixerConfig &config = {});

  // Mix `n` samples of every channel into `outL`/`outR` (overwritten).
  // A channel absent from `inputs` is silent for the block; its measurement
  // keeps what it had (silence is gated out anyway).
  void process(const std::vector<ChannelInput> &inputs, int n, float *outL,
               float *outR, std::vector<ChannelMeter> *meters = nullptr);

  // What the rider is doing, for the render tool and the tests.
  double channelGainDb(const ChannelKey &key) const;
  double channelLufs(const ChannelKey &key) const;
  double masterGainDb() const { return master_.gainDb; }
  double masterLufs() const { return master_.meter->lufs(); }
  double limiterGainDb() const { return limiter_.gainDb(); }
  int latencySamples() const { return limiter_.latencySamples(); }

private:
  struct Rider {
    std::unique_ptr<chalkwalk::dsp::measure::SlidingLoudness> meter;
    double gainDb = 0.0;
  };

  // Move a rider's gain toward its target by at most the slew for `n`
  // samples; returns the linear gains at the start and end of the block.
  std::pair<double, double> ride(Rider &r, double targetLufs, int n);

  MixerConfig cfg_;
  std::map<ChannelKey, Rider> channels_;
  Rider master_;
  chalkwalk::dsp::PeakLimiter limiter_;
  std::vector<float> scratchL_, scratchR_;
};

}  // namespace balcony
