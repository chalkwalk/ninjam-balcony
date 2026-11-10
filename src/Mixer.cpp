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
      2, cfg_.sampleRate, cfg_.windowSeconds);
  limiter_.prepare(cfg_.sampleRate, cfg_.limiterLookaheadMs,
                   cfg_.limiterReleaseMs, cfg_.limiterCeilingDb);
}

std::pair<double, double> Mixer::ride(Rider &r, double targetLufs, int n) {
  const double before = r.gainDb;
  double target = before;
  if (r.meter->gatedSeconds() >= cfg_.minGatedSeconds) {
    const double lufs = r.meter->lufs();
    const double boost = lufs < cfg_.boostFloorLufs ? 0.0 : cfg_.maxBoostDb;
    target = std::clamp(targetLufs - lufs, -cfg_.maxCutDb, boost);
  }
  const double step = cfg_.slewDbPerSecond * n / cfg_.sampleRate;
  r.gainDb = before + std::clamp(target - before, -step, step);
  return {toLinear(before), toLinear(r.gainDb)};
}

void Mixer::process(const std::vector<ChannelInput> &inputs, int n, float *outL,
                    float *outR, std::vector<ChannelMeter> *meters) {
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
    const auto [g0, g1] = ride(rider, cfg_.channelTargetLufs, n);

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
  const auto [m0, m1] = ride(master_, cfg_.masterTargetLufs, n);
  for (int i = 0; i < n; ++i) {
    const double g = m0 + (m1 - m0) * (double)(i + 1) / n;
    outL[i] = (float)(outL[i] * g);
    outR[i] = (float)(outR[i] * g);
  }
  limiter_.process(outL, outR, n);
}

double Mixer::channelGainDb(const ChannelKey &key) const {
  auto it = channels_.find(key);
  return it == channels_.end() ? 0.0 : it->second.gainDb;
}

double Mixer::channelLufs(const ChannelKey &key) const {
  auto it = channels_.find(key);
  return it == channels_.end() ? chalkwalk::dsp::measure::kSilenceLufs
                               : it->second.meter->lufs();
}

}  // namespace balcony
