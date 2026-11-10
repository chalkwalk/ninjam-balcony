#pragma once

#include "Archive.h"

#include <chalkwalk/dsp/Compressor.h>
#include <chalkwalk/dsp/OnePole.h>
#include <chalkwalk/dsp/PeakLimiter.h>
#include <chalkwalk/dsp/SlidingLoudness.h>
#include <chalkwalk/dsp/SmallRoom.h>
#include <chalkwalk/dsp/Svf.h>
#include <chalkwalk/dsp/TiltEq.h>

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
//
// `MixerConfig::mastered` selects the second chain (DESIGN §6.1): a rumble
// filter, a rider with a long and a short view, and a soft then a hard limit on
// each channel; a little automatic spread; a quiet shared room; and on the sum
// a slower rider of the same kind, a slow tilt toward a usual balance, a glue
// compressor, and a soft then a hard limit. Off, the chain above is unchanged.

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
  // The master's own window and boost cap; 0 and below mean "as the channels".
  // A long window and a small cap let the whole band's dynamics through (a
  // quiet passage stays quiet) while the channels are still evened out:
  // option (b) of the period's question Q1, for listening, not yet a default.
  double masterWindowSeconds = 0.0;
  double masterMaxBoostDb = -1.0;
  double limiterLookaheadMs = 5.0;
  double limiterReleaseMs = 50.0;
  double limiterCeilingDb = -1.0;

  // ---- The mastered chain (DESIGN §6.1, §9.4). Off: everything above. ----
  bool mastered = false;
  // Each channel. The rumble filter: 0 Hz is none; order 1 is 6 dB/octave
  // (one pole), 2 is 12 dB/octave (Butterworth).
  double highPassHz = 35.0;
  int highPassOrder = 2;
  // The rider's two views of one meter: the long one sets the level; the short
  // one may only cut, quickly, when it reads more than `shortLeadLu` above the
  // long one (someone suddenly much louder). Back up is always slow.
  double longWindowSeconds = 300.0;
  double shortWindowSeconds = 30.0;
  double shortLeadLu = 3.0;
  double slowSlewDbPerSecond = 0.25;
  double fastCutDbPerSecond = 2.0;
  // A rider's first minute moves faster, so a new player arrives at level.
  double settleSeconds = 60.0;
  double settleSlewDbPerSecond = 1.0;
  // Less playing than this in the short view: the short view is not trusted.
  double restingSeconds = 3.0;
  double channelBoostDb = 6.0;
  double channelCutDb = 12.0;
  // Soft (a fast compressor on peaks) and hard (lookahead) limits, set above
  // the channel target, so they catch one player's spikes before the sum
  // has to turn everyone down for them.
  double channelSoftOverDb = 14.0;
  double channelHardOverDb = 18.0;
  // Placement: a small spread by order of arrival, on top of the player's own
  // pan; nothing is decided from what a player sends (DESIGN §6.1).
  bool autoPan = true;
  double panGlidePerSecond = 0.02;
  double presenceSeconds = 30.0;
  // The shared room: short, dark and quiet. At or below -90 dB it is off.
  double roomLevelDb = -16.0;
  double roomDecaySeconds = 0.5;
  // Unmasking (experimental): where two players crowd one band, the one for
  // whom it is not home band dips there, by at most this, over seconds.
  bool unmask = false;
  double unmaskMaxDipDb = 2.0;
  double unmaskGlideDbPerSecond = 0.2;
  // The master.
  double masteredTargetLufs = -13.0;  // the glue takes about 1 dB after: out near -14
  double masterLongWindowSeconds = 300.0;
  double masterShortWindowSeconds = 30.0;
  double masterSlowSlewDbPerSecond = 0.1;
  double masterFastCutDbPerSecond = 1.0;
  double masterBoostDb = 10.0;
  double masterCutDb = 12.0;
  // The tilt: the sum's low/high balance about the pivot, over about 30 s,
  // compared with a usual one; `tiltStrength` of the difference, at most
  // `tiltMaxDb`, moving at most `tiltSlewDbPerSecond`. Strength 0 is off.
  double tiltPivotHz = 700.0;
  double tiltBalanceDb = 5.2;  // the median of the 12 archived jams (DESIGN §9.4)
  double tiltStrength = 0.5;
  double tiltMaxDb = 1.5;
  double tiltSlewDbPerSecond = 0.02;
  // Glue, then the soft limit, then the brick wall.
  double glueThresholdDb = -18.0;
  double glueRatio = 2.0;
  double softLimitDb = -4.0;
  double masteredCeilingDb = -1.5;  // sample peak, with room for the encoder's overshoot
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
  int latencySamples() const;

  // The mastered chain's state, for the render tool and the tests.
  double channelPan(const ChannelKey &key) const;  // the automatic part only
  double unmaskDipDb(const ChannelKey &key, int band) const;
  static constexpr int kBands = 4;
  struct Report {
    double tiltDb = 0.0;             // now
    double balanceDb = 0.0;          // the sum's low/high balance, ~30 s
    double meanBalanceDb = 0.0;      // over everything so far, energy-weighted
    double glueDb = 0.0, softDb = 0.0;  // gain reduction now
    double glueMeanDb = 0.0;         // mean reduction over the blocks with sound
    double softShare = 0.0;          // share of those blocks with > 1 dB of soft limiting
    double brickShare = 0.0;         // ... and of brick-wall limiting
    double roomUnderDryDb = 0.0;     // the room's level below the dry sum
  };
  Report report() const;

private:
  struct Rider {
    std::unique_ptr<chalkwalk::dsp::measure::SlidingLoudness> meter;
    double gainDb = 0.0;
    double ageSeconds = 0.0;
  };
  struct Views {
    double target, longSeconds, shortSeconds, boost, cut, slow, fast;
  };
  struct Strip {
    Rider rider;
    chalkwalk::dsp::Svf hp2[2];
    chalkwalk::dsp::OnePole hp1[2];
    chalkwalk::dsp::Compressor soft;
    chalkwalk::dsp::PeakLimiter hard;
    chalkwalk::dsp::Svf measure[kBands];
    chalkwalk::dsp::Svf bell[kBands][2];
    double bandEnergy[kBands] = {0, 0, 0, 0};
    double dipDb[kBands] = {0, 0, 0, 0};
    std::vector<float> l, r;
    long lastSeen = -1;
  };
  struct Player {
    long rank = 0;
    long lastSeen = -1;
    double pan = 0.0;
  };

  void processMastered(const std::vector<ChannelInput> &inputs, int n, float *outL,
                       float *outR, std::vector<ChannelMeter> *meters);
  std::pair<double, double> rideTwo(Rider &r, const Views &v, int n);
  void place(const std::vector<ChannelInput> &inputs, int n);
  void unmaskTargets(int n);
  Strip &strip(const ChannelKey &key);

  // Move a rider's gain toward its target by at most the slew for `n`
  // samples; returns the linear gains at the start and end of the block.
  std::pair<double, double> ride(Rider &r, double targetLufs, double maxBoostDb, int n);

  MixerConfig cfg_;
  std::map<ChannelKey, Rider> channels_;
  Rider master_;
  chalkwalk::dsp::PeakLimiter limiter_;
  std::vector<float> scratchL_, scratchR_;

  // The mastered chain.
  std::map<ChannelKey, Strip> strips_;
  std::map<std::string, Player> players_;
  long nextRank_ = 0;
  long clock_ = 0;  // samples mixed so far
  chalkwalk::dsp::SmallRoom room_;
  std::vector<float> wetL_, wetR_;
  double roomGain_ = 0.0;
  chalkwalk::dsp::OnePole balanceSplit_;
  double balanceLow_ = 0.0, balanceHigh_ = 0.0;      // smoothed
  double balanceLowAll_ = 0.0, balanceHighAll_ = 0.0;
  chalkwalk::dsp::TiltEq tilt_;
  double tiltDb_ = 0.0;
  chalkwalk::dsp::Compressor glue_, softLimit_;
  double glueSum_ = 0.0;
  long soundBlocks_ = 0, softBlocks_ = 0, brickBlocks_ = 0;
  double drySum_ = 0.0, wetSum_ = 0.0;
  ChannelKey owner_[kBands];
  bool hasOwner_[kBands] = {false, false, false, false};
};

}  // namespace balcony
