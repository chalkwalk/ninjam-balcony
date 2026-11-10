#include "Mixer.h"

#include <chalkwalk/ninjam/ChannelMix.h>

#include <algorithm>
#include <cmath>

namespace balcony {

namespace {
double toLinear(double db) { return std::pow(10.0, db / 20.0); }
float toDb(double linear) {
  return linear > 1e-6 ? (float)(20.0 * std::log10(linear)) : -120.0f;
}
}  // namespace

Mixer::Mixer(const MixerConfig &config) : cfg_(config) {
  master_.meter = std::make_unique<chalkwalk::dsp::measure::SlidingLoudness>(
      2, cfg_.sampleRate,
      cfg_.masterWindowSeconds > 0 ? cfg_.masterWindowSeconds : cfg_.windowSeconds);
  limiter_.prepare(cfg_.sampleRate, cfg_.limiterLookaheadMs,
                   cfg_.limiterReleaseMs, cfg_.limiterCeilingDb);
  if (!cfg_.mastered)
    return;

  // The mastered chain: the sum's meter has the long window, read short too.
  master_.meter = std::make_unique<chalkwalk::dsp::measure::SlidingLoudness>(
      2, cfg_.sampleRate, cfg_.masterLongWindowSeconds);
  limiter_.prepare(cfg_.sampleRate, cfg_.limiterLookaheadMs, cfg_.limiterReleaseMs,
                   cfg_.masteredCeilingDb);
  chalkwalk::dsp::SmallRoom::Settings room;
  room.decaySeconds = cfg_.roomDecaySeconds;
  room_.prepare(cfg_.sampleRate, room);
  roomGain_ = cfg_.roomLevelDb <= -90.0 ? 0.0 : toLinear(cfg_.roomLevelDb);
  balanceSplit_.set(cfg_.tiltPivotHz, cfg_.sampleRate);
  tilt_.prepare(cfg_.sampleRate, cfg_.tiltPivotHz, 50.0);

  chalkwalk::dsp::Compressor::Settings glue;
  glue.thresholdDb = cfg_.glueThresholdDb;
  glue.ratio = cfg_.glueRatio;
  glue.kneeDb = 6.0;
  glue.attackMs = 30.0;
  glue.releaseMs = 300.0;
  glue.autoRelease = true;
  glue.detectorMs = 20.0;
  glue.sidechainHighPassHz = 80.0;  // the bass does not pump the band
  glue_.prepare(cfg_.sampleRate, glue);

  chalkwalk::dsp::Compressor::Settings soft;
  soft.thresholdDb = cfg_.softLimitDb;
  soft.ratio = 8.0;
  soft.kneeDb = 4.0;
  soft.attackMs = 1.0;
  soft.releaseMs = 80.0;
  soft.detectorMs = 0.0;  // peak
  softLimit_.prepare(cfg_.sampleRate, soft);
}

int Mixer::latencySamples() const {
  // Every channel's hard limit has the same lookahead, so the channels stay
  // aligned with each other; the sum adds its own.
  if (!cfg_.mastered)
    return limiter_.latencySamples();
  int channel = 0;
  if (!strips_.empty())
    channel = strips_.begin()->second.hard.latencySamples();
  return channel + limiter_.latencySamples();
}

std::pair<double, double> Mixer::ride(Rider &r, double targetLufs, double maxBoostDb, int n) {
  const double before = r.gainDb;
  double target = before;
  if (r.meter->gatedSeconds() >= cfg_.minGatedSeconds) {
    const double lufs = r.meter->lufs();
    const double boost = lufs < cfg_.boostFloorLufs ? 0.0 : maxBoostDb;
    target = std::clamp(targetLufs - lufs, -cfg_.maxCutDb, boost);
  }
  const double step = cfg_.slewDbPerSecond * n / cfg_.sampleRate;
  r.gainDb = before + std::clamp(target - before, -step, step);
  return {toLinear(before), toLinear(r.gainDb)};
}

void Mixer::process(const std::vector<ChannelInput> &inputs, int n, float *outL,
                    float *outR, std::vector<ChannelMeter> *meters) {
  if (cfg_.mastered) {
    processMastered(inputs, n, outL, outR, meters);
    return;
  }
  std::fill(outL, outL + n, 0.0f);
  std::fill(outR, outR + n, 0.0f);
  scratchL_.resize((size_t)n);
  scratchR_.resize((size_t)n);
  if (meters != nullptr)
    meters->clear();

  for (const auto &in : inputs) {
    auto it = channels_.find(*in.key);
    if (it == channels_.end()) {
      Rider r;
      r.meter = std::make_unique<chalkwalk::dsp::measure::SlidingLoudness>(
          2, cfg_.sampleRate, cfg_.windowSeconds);
      it = channels_.emplace(*in.key, std::move(r)).first;
    }
    Rider &rider = it->second;
    // Measured before the gain, so the rider is not chasing its own output.
    rider.meter->add(in.left, in.right != nullptr ? in.right : in.left, n);
    const auto [g0, g1] = ride(rider, cfg_.channelTargetLufs, cfg_.maxBoostDb, n);

    ChannelMeter m;
    m.key = *in.key;
    if (in.inStream) {
      const auto pan = chalkwalk::ninjam::channelmix::panGains(1.0f, in.pan);
      double peak = 0.0, sumSq = 0.0;
      for (int i = 0; i < n; ++i) {
        const double g = g0 + (g1 - g0) * (double)(i + 1) / n;
        const float l = (float)(in.left[i] * g * pan.left);
        const float r =
            (float)((in.right != nullptr ? in.right[i] : in.left[i]) * g * pan.right);
        outL[i] += l;
        outR[i] += r;
        peak = std::max(peak, (double)std::max(std::fabs(l), std::fabs(r)));
        sumSq += 0.5 * ((double)l * l + (double)r * r);
      }
      m.peakDb = toDb(peak);
      m.levelDb = toDb(std::sqrt(sumSq / std::max(1, n)));
    }
    if (meters != nullptr)
      meters->push_back(m);
  }

  // The sum: measured, ridden toward the listening level, then limited.
  master_.meter->add(outL, outR, n);
  const auto [m0, m1] = ride(master_, cfg_.masterTargetLufs,
                             cfg_.masterMaxBoostDb >= 0 ? cfg_.masterMaxBoostDb : cfg_.maxBoostDb,
                             n);
  for (int i = 0; i < n; ++i) {
    const double g = m0 + (m1 - m0) * (double)(i + 1) / n;
    outL[i] = (float)(outL[i] * g);
    outR[i] = (float)(outR[i] * g);
  }
  limiter_.process(outL, outR, n);
}

double Mixer::channelGainDb(const ChannelKey &key) const {
  if (cfg_.mastered) {
    auto it = strips_.find(key);
    return it == strips_.end() ? 0.0 : it->second.rider.gainDb;
  }
  auto it = channels_.find(key);
  return it == channels_.end() ? 0.0 : it->second.gainDb;
}

double Mixer::channelLufs(const ChannelKey &key) const {
  if (cfg_.mastered) {
    auto it = strips_.find(key);
    return it == strips_.end() ? chalkwalk::dsp::measure::kSilenceLufs
                               : it->second.rider.meter->lufs();
  }
  auto it = channels_.find(key);
  return it == channels_.end() ? chalkwalk::dsp::measure::kSilenceLufs
                               : it->second.meter->lufs();
}

// ---- The mastered chain (DESIGN §6.1) ----

namespace {
// The bands unmasking looks at: wide, and few, so a dip is a broad colour
// change and never a notch (DESIGN §6.1).
constexpr double kBandHz[Mixer::kBands] = {120.0, 400.0, 1500.0, 5000.0};
constexpr double kBandQ = 0.9;
constexpr double kBandSmoothSeconds = 8.0;

// The spread's half-width for `n` players, as a pan fraction: +/-5 % for two,
// growing quickly and then slowly to +/-21 % at fifteen, where the players are
// 3 % apart; past fifteen the width stays and they close up (DESIGN §6.1).
double spreadFor(size_t n) {
  if (n < 2)
    return 0.0;
  if (n >= 15)
    return 0.21;
  const double x = (15.0 - (double)n) / 13.0;
  return (21.0 - 16.0 * x * x) / 100.0;
}
}  // namespace

Mixer::Strip &Mixer::strip(const ChannelKey &key) {
  auto it = strips_.find(key);
  if (it != strips_.end())
    return it->second;
  Strip &s = strips_[key];
  s.rider.meter = std::make_unique<chalkwalk::dsp::measure::SlidingLoudness>(
      2, cfg_.sampleRate, cfg_.longWindowSeconds);
  for (int c = 0; c < 2; ++c) {
    s.hp2[c].set(cfg_.highPassHz > 0 ? cfg_.highPassHz : 1.0, 0.7071, cfg_.sampleRate);
    s.hp1[c].set(cfg_.highPassHz > 0 ? cfg_.highPassHz : 1.0, cfg_.sampleRate);
  }
  chalkwalk::dsp::Compressor::Settings soft;
  // The soft limit's knee is centred on its threshold; start it there.
  soft.thresholdDb = cfg_.channelTargetLufs + cfg_.channelSoftOverDb;
  soft.ratio = 6.0;
  soft.kneeDb = 6.0;
  soft.attackMs = 1.0;
  soft.releaseMs = 100.0;
  soft.detectorMs = 0.0;
  s.soft.prepare(cfg_.sampleRate, soft);
  s.hard.prepare(cfg_.sampleRate, 2.0, 60.0, cfg_.channelTargetLufs + cfg_.channelHardOverDb);
  for (int b = 0; b < kBands; ++b) {
    s.measure[b].set(kBandHz[b], kBandQ, cfg_.sampleRate);
    for (int c = 0; c < 2; ++c)
      s.bell[b][c].set(kBandHz[b], kBandQ, cfg_.sampleRate);
  }
  return s;
}

std::pair<double, double> Mixer::rideTwo(Rider &r, const Views &v, int n) {
  const double dt = n / cfg_.sampleRate;
  const double before = r.gainDb;
  r.ageSeconds += dt;
  double desired = before;
  bool suddenlyLouder = false;
  if (r.meter->gatedSeconds() >= cfg_.minGatedSeconds) {
    const double longLufs = r.meter->lufs();
    const double cap = longLufs < cfg_.boostFloorLufs ? 0.0 : v.boost;
    desired = std::clamp(v.target - longLufs, -v.cut, cap);
    // The short view only cuts, and only when it is well above the long one.
    if (r.meter->gatedSecondsOver(v.shortSeconds) >= cfg_.restingSeconds) {
      const double excess = r.meter->lufsOver(v.shortSeconds) - longLufs - cfg_.shortLeadLu;
      if (excess > 0.0) {
        desired = std::max(-v.cut, desired - excess);
        suddenlyLouder = true;
      }
    }
  }
  double rate = (suddenlyLouder && desired < before) ? v.fast : v.slow;
  if (r.ageSeconds < cfg_.settleSeconds)
    rate = std::max(rate, cfg_.settleSlewDbPerSecond);
  const double step = rate * dt;
  r.gainDb = before + std::clamp(desired - before, -step, step);
  return {toLinear(before), toLinear(r.gainDb)};
}

void Mixer::place(const std::vector<ChannelInput> &inputs, int n) {
  for (const auto &in : inputs) {
    auto it = players_.find(in.key->user);
    if (it == players_.end()) {
      it = players_.emplace(in.key->user, Player{}).first;
      it->second.rank = nextRank_++;
    }
    it->second.lastSeen = clock_;
  }
  // Present: heard within `presenceSeconds`. Ordered by arrival, and dealt to
  // the slots from the outside in, alternating sides: first arrival far left,
  // second far right, third next to the first... so one more player moves
  // everyone a little, never anyone a lot.
  std::vector<Player *> present;
  const long horizon = (long)(cfg_.presenceSeconds * cfg_.sampleRate);
  for (auto &kv : players_)
    if (clock_ - kv.second.lastSeen <= horizon)
      present.push_back(&kv.second);
  std::sort(present.begin(), present.end(),
            [](const Player *a, const Player *b) { return a->rank < b->rank; });
  const size_t count = present.size();
  const double half = spreadFor(count);
  const double step = cfg_.panGlidePerSecond * n / cfg_.sampleRate;
  for (size_t i = 0; i < count; ++i) {
    const size_t slot = (i % 2 == 0) ? i / 2 : count - 1 - i / 2;
    const double target =
        count < 2 ? 0.0 : -half + 2.0 * half * (double)slot / (double)(count - 1);
    Player &p = *present[i];
    p.pan += std::clamp(target - p.pan, -step, step);
  }
}

void Mixer::unmaskTargets(int n) {
  // Each band's owner is the present channel with the largest share of it;
  // a new owner must have half again the old one's share, so ownership does
  // not flip back and forth between two near-equal players.
  std::vector<std::pair<const ChannelKey *, Strip *>> present;
  const long horizon = (long)(cfg_.presenceSeconds * cfg_.sampleRate);
  for (auto &kv : strips_)
    if (clock_ - kv.second.lastSeen <= horizon)
      present.emplace_back(&kv.first, &kv.second);
  const double step = cfg_.unmaskGlideDbPerSecond * n / cfg_.sampleRate;
  for (int b = 0; b < kBands; ++b) {
    double total = 0.0;
    for (auto &p : present)
      total += p.second->bandEnergy[b];
    Strip *owner = nullptr;
    double ownerShare = 0.0;
    if (hasOwner_[b]) {
      auto it = strips_.find(owner_[b]);
      if (it != strips_.end() && clock_ - it->second.lastSeen <= horizon && total > 0.0) {
        owner = &it->second;
        ownerShare = owner->bandEnergy[b] / total;
      }
    }
    for (auto &p : present) {
      const double share = total > 0.0 ? p.second->bandEnergy[b] / total : 0.0;
      if (owner == nullptr || share > 1.5 * ownerShare) {
        owner = p.second;
        ownerShare = share;
        owner_[b] = *p.first;
        hasOwner_[b] = true;
      }
    }
    for (auto &p : present) {
      Strip &s = *p.second;
      double target = 0.0;
      const double share = total > 0.0 ? s.bandEnergy[b] / total : 0.0;
      // A player's home band (where most of their own energy is) is never
      // dipped: unmasking makes room around players, not out of them.
      int home = 0;
      for (int k = 1; k < kBands; ++k)
        if (s.bandEnergy[k] > s.bandEnergy[home])
          home = k;
      if (present.size() >= 2 && &s != owner && b != home && ownerShare >= 0.3)
        target = -cfg_.unmaskMaxDipDb * std::clamp((share - 0.15) / 0.35, 0.0, 1.0);
      s.dipDb[b] += std::clamp(target - s.dipDb[b], -step, step);
    }
  }
}

void Mixer::processMastered(const std::vector<ChannelInput> &inputs, int n, float *outL,
                            float *outR, std::vector<ChannelMeter> *meters) {
  std::fill(outL, outL + n, 0.0f);
  std::fill(outR, outR + n, 0.0f);
  if (meters != nullptr)
    meters->clear();
  if (cfg_.autoPan)
    place(inputs, n);
  if (cfg_.unmask)
    unmaskTargets(n);

  const Views channelViews{cfg_.channelTargetLufs, cfg_.longWindowSeconds,
                           cfg_.shortWindowSeconds, cfg_.channelBoostDb,
                           cfg_.channelCutDb, cfg_.slowSlewDbPerSecond,
                           cfg_.fastCutDbPerSecond};
  const double bandSmooth = 1.0 - std::exp(-(double)n / (cfg_.sampleRate * kBandSmoothSeconds));

  for (const auto &in : inputs) {
    Strip &s = strip(*in.key);
    s.lastSeen = clock_;
    s.l.assign(in.left, in.left + n);
    s.r.assign(in.right != nullptr ? in.right : in.left, (in.right != nullptr ? in.right : in.left) + n);
    float *l = s.l.data(), *r = s.r.data();

    // Rumble filter, then the measurement, so the rider hears what is heard.
    if (cfg_.highPassHz > 0.0) {
      for (int i = 0; i < n; ++i) {
        if (cfg_.highPassOrder >= 2) {
          l[i] = s.hp2[0].process(l[i], chalkwalk::dsp::Svf::HighPass);
          r[i] = s.hp2[1].process(r[i], chalkwalk::dsp::Svf::HighPass);
        } else {
          l[i] = s.hp1[0].highPass(l[i]);
          r[i] = s.hp1[1].highPass(r[i]);
        }
      }
    }
    s.rider.meter->add(l, r, n);
    const auto [g0, g1] = rideTwo(s.rider, channelViews, n);
    for (int i = 0; i < n; ++i) {
      const float g = (float)(g0 + (g1 - g0) * (double)(i + 1) / n);
      l[i] *= g;
      r[i] *= g;
    }

    if (cfg_.unmask) {
      // Measured before the dips, so a dip does not feed its own reason.
      double e[kBands] = {0, 0, 0, 0};
      for (int i = 0; i < n; ++i) {
        const float mid = 0.5f * (l[i] + r[i]);
        for (int b = 0; b < kBands; ++b) {
          const float v = s.measure[b].process(mid, chalkwalk::dsp::Svf::BandPass) * s.measure[b].k;
          e[b] += (double)v * v;
        }
      }
      for (int b = 0; b < kBands; ++b) {
        s.bandEnergy[b] += bandSmooth * (e[b] / n - s.bandEnergy[b]);
        if (s.dipDb[b] > -0.01)
          continue;
        // A bell: the input plus (G - 1) of the normalised band-pass.
        const float depth = (float)(toLinear(s.dipDb[b]) - 1.0);
        for (int c = 0; c < 2; ++c) {
          float *x = c == 0 ? l : r;
          auto &f = s.bell[b][c];
          for (int i = 0; i < n; ++i)
            x[i] += depth * f.k * f.process(x[i], chalkwalk::dsp::Svf::BandPass);
        }
      }
    }

    s.soft.process(l, r, n);
    s.hard.process(l, r, n);

    ChannelMeter m;
    m.key = *in.key;
    if (in.inStream) {
      float pan = in.pan;
      if (cfg_.autoPan)
        pan = (float)std::clamp((double)in.pan + players_[in.key->user].pan, -1.0, 1.0);
      const auto gains = chalkwalk::ninjam::channelmix::panGains(1.0f, pan);
      double peak = 0.0, sumSq = 0.0;
      for (int i = 0; i < n; ++i) {
        const float a = l[i] * gains.left, b = r[i] * gains.right;
        outL[i] += a;
        outR[i] += b;
        peak = std::max(peak, (double)std::max(std::fabs(a), std::fabs(b)));
        sumSq += 0.5 * ((double)a * a + (double)b * b);
      }
      m.peakDb = toDb(peak);
      m.levelDb = toDb(std::sqrt(sumSq / std::max(1, n)));
    }
    if (meters != nullptr)
      meters->push_back(m);
  }

  // The room, from the sum, mixed in quietly.
  if (roomGain_ > 0.0) {
    wetL_.resize((size_t)n);
    wetR_.resize((size_t)n);
    room_.process(outL, outR, wetL_.data(), wetR_.data(), n);
    for (int i = 0; i < n; ++i) {
      const float wl = (float)(wetL_[i] * roomGain_), wr = (float)(wetR_[i] * roomGain_);
      drySum_ += 0.5 * ((double)outL[i] * outL[i] + (double)outR[i] * outR[i]);
      wetSum_ += 0.5 * ((double)wl * wl + (double)wr * wr);
      outL[i] += wl;
      outR[i] += wr;
    }
  }

  // The master rider.
  master_.meter->add(outL, outR, n);
  const Views masterViews{cfg_.masteredTargetLufs, cfg_.masterLongWindowSeconds,
                          cfg_.masterShortWindowSeconds, cfg_.masterBoostDb,
                          cfg_.masterCutDb, cfg_.masterSlowSlewDbPerSecond,
                          cfg_.masterFastCutDbPerSecond};
  const auto [m0, m1] = rideTwo(master_, masterViews, n);
  double blockEnergy = 0.0, low = 0.0, high = 0.0;
  for (int i = 0; i < n; ++i) {
    const double g = m0 + (m1 - m0) * (double)(i + 1) / n;
    outL[i] = (float)(outL[i] * g);
    outR[i] = (float)(outR[i] * g);
    const float mid = 0.5f * (outL[i] + outR[i]);
    const float lo = balanceSplit_.lowPass(mid);
    low += (double)lo * lo;
    high += (double)(mid - lo) * (mid - lo);
    blockEnergy += (double)mid * mid;
  }

  // The tilt: the balance over about 30 s, only from blocks with sound in them.
  const bool sound = blockEnergy / n > 1e-7;  // about -70 dBFS
  if (sound) {
    const double k = 1.0 - std::exp(-(double)n / (cfg_.sampleRate * 30.0));
    balanceLow_ += k * (low - balanceLow_);
    balanceHigh_ += k * (high - balanceHigh_);
    balanceLowAll_ += low;
    balanceHighAll_ += high;
    if (balanceLow_ > 0.0 && balanceHigh_ > 0.0 && cfg_.tiltStrength > 0.0) {
      const double balance = 10.0 * std::log10(balanceLow_ / balanceHigh_);
      // Too much low end reads high; brighten, which is a positive tilt.
      const double want = std::clamp(cfg_.tiltStrength * (balance - cfg_.tiltBalanceDb),
                                     -cfg_.tiltMaxDb, cfg_.tiltMaxDb);
      const double step = cfg_.tiltSlewDbPerSecond * n / cfg_.sampleRate;
      tiltDb_ += std::clamp(want - tiltDb_, -step, step);
      tilt_.setTiltDb(tiltDb_);
    }
  }
  tilt_.process(outL, outR, n);

  glue_.process(outL, outR, n);
  softLimit_.process(outL, outR, n);
  limiter_.process(outL, outR, n);
  if (sound) {
    ++soundBlocks_;
    glueSum_ += glue_.gainReductionDb();
    if (softLimit_.gainReductionDb() < -1.0)
      ++softBlocks_;
    if (limiter_.gainDb() < -1.0)
      ++brickBlocks_;
  }
  clock_ += n;
}

double Mixer::channelPan(const ChannelKey &key) const {
  auto it = players_.find(key.user);
  return it == players_.end() ? 0.0 : it->second.pan;
}

double Mixer::unmaskDipDb(const ChannelKey &key, int band) const {
  auto it = strips_.find(key);
  return it == strips_.end() || band < 0 || band >= kBands ? 0.0 : it->second.dipDb[band];
}

Mixer::Report Mixer::report() const {
  Report r;
  r.tiltDb = tiltDb_;
  if (balanceLow_ > 0.0 && balanceHigh_ > 0.0)
    r.balanceDb = 10.0 * std::log10(balanceLow_ / balanceHigh_);
  if (balanceLowAll_ > 0.0 && balanceHighAll_ > 0.0)
    r.meanBalanceDb = 10.0 * std::log10(balanceLowAll_ / balanceHighAll_);
  r.glueDb = glue_.gainReductionDb();
  r.softDb = softLimit_.gainReductionDb();
  if (soundBlocks_ > 0) {
    r.glueMeanDb = glueSum_ / (double)soundBlocks_;
    r.softShare = (double)softBlocks_ / (double)soundBlocks_;
    r.brickShare = (double)brickBlocks_ / (double)soundBlocks_;
  }
  if (drySum_ > 0.0 && wetSum_ > 0.0)
    r.roomUnderDryDb = 10.0 * std::log10(wetSum_ / drySum_);
  return r;
}

}  // namespace balcony
