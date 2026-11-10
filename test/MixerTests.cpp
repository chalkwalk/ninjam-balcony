#include "Mixer.h"

#include <chalkwalk/dsp/Loudness.h>
#include <chalkwalk/dsp/Pi.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace balcony;

namespace {

constexpr double kRate = 48000.0;
constexpr int kBlock = 2880;

struct Tone {
  double hz, amp;
  long t = 0;
  void fill(std::vector<float> &v) {
    for (auto &s : v)
      s = (float)(amp * std::sin(2.0 * chalkwalk::dsp::kPi * hz * (double)(t++) / kRate));
  }
};

}  // namespace

TEST_CASE("two players 10 dB apart come out at one level", "[mixer]") {
  Mixer mixer;
  ChannelKey loud{"loud", 0}, quiet{"quiet@1.2.3.x", 0};
  Tone a{440.0, 0.1}, b{660.0, 0.0316};
  std::vector<float> la(kBlock), lb(kBlock), outL(kBlock), outR(kBlock);
  // Long enough for the rider to arrive at 0.5 dB/s from 0 to about +8 dB.
  for (int i = 0; i < (int)(150 * kRate / kBlock); ++i) {
    a.fill(la);
    b.fill(lb);
    mixer.process({{&loud, la.data(), nullptr, 0.0f, true},
                   {&quiet, lb.data(), nullptr, 0.0f, true}},
                  kBlock, outL.data(), outR.data());
  }
  const double heardLoud = mixer.channelLufs(loud) + mixer.channelGainDb(loud);
  const double heardQuiet = mixer.channelLufs(quiet) + mixer.channelGainDb(quiet);
  CHECK(heardLoud == Catch::Approx(-23.0).margin(0.5));
  CHECK(heardQuiet == Catch::Approx(-23.0).margin(0.5));
}

TEST_CASE("a player who has barely played is not ridden", "[mixer]") {
  Mixer mixer;
  ChannelKey k{"new", 0};
  Tone t{440.0, 0.01};
  std::vector<float> in(kBlock), outL(kBlock), outR(kBlock);
  for (int i = 0; i < (int)(5 * kRate / kBlock); ++i) {
    t.fill(in);
    mixer.process({{&k, in.data(), nullptr, 0.0f, true}}, kBlock, outL.data(), outR.data());
  }
  CHECK(mixer.channelGainDb(k) == 0.0);
}

TEST_CASE("the rider never moves faster than its slew", "[mixer]") {
  Mixer mixer;
  ChannelKey k{"p", 0};
  Tone t{440.0, 0.9};
  std::vector<float> in(kBlock), outL(kBlock), outR(kBlock);
  double last = 0.0, worst = 0.0;
  for (int i = 0; i < (int)(40 * kRate / kBlock); ++i) {
    t.fill(in);
    mixer.process({{&k, in.data(), nullptr, 0.0f, true}}, kBlock, outL.data(), outR.data());
    worst = std::max(worst, std::fabs(mixer.channelGainDb(k) - last));
    last = mixer.channelGainDb(k);
  }
  CHECK(worst <= MixerConfig{}.slewDbPerSecond * kBlock / kRate + 1e-9);
  CHECK(last < -5.0);  // and it did move
}

TEST_CASE("the mix never passes the ceiling and settles at the listening level",
          "[mixer]") {
  Mixer mixer;
  ChannelKey a{"a", 0}, b{"b", 1};
  Tone ta{220.0, 0.95}, tb{330.0, 0.95};
  std::vector<float> la(kBlock), lb(kBlock), outL(kBlock), outR(kBlock), tailL, tailR;
  const float ceiling = (float)std::pow(10.0, -1.0 / 20.0);
  float worst = 0.0f;
  const int blocks = (int)(240 * kRate / kBlock);
  for (int i = 0; i < blocks; ++i) {
    ta.fill(la);
    tb.fill(lb);
    mixer.process({{&a, la.data(), nullptr, -0.5f, true}, {&b, lb.data(), nullptr, 0.5f, true}},
                  kBlock, outL.data(), outR.data());
    for (int s = 0; s < kBlock; ++s)
      worst = std::max(worst, std::max(std::fabs(outL[(size_t)s]), std::fabs(outR[(size_t)s])));
    if (i >= blocks - (int)(20 * kRate / kBlock)) {
      tailL.insert(tailL.end(), outL.begin(), outL.end());
      tailR.insert(tailR.end(), outR.begin(), outR.end());
    }
  }
  CHECK(worst <= ceiling);
  const double lufs = chalkwalk::dsp::measure::integratedLufs(
      tailL.data(), tailR.data(), (int)tailL.size(), kRate);
  CHECK(lufs == Catch::Approx(-16.0).margin(1.0));
}

TEST_CASE("a player left out of the stream is not heard but is still measured",
          "[mixer]") {
  Mixer mixer;
  ChannelKey k{"shy", 0};
  Tone t{440.0, 0.5};
  std::vector<float> in(kBlock), outL(kBlock), outR(kBlock);
  std::vector<ChannelMeter> meters;
  for (int i = 0; i < 50; ++i) {
    t.fill(in);
    mixer.process({{&k, in.data(), nullptr, 0.0f, false}}, kBlock, outL.data(),
                  outR.data(), &meters);
  }
  float out = 0.0f;
  for (float s : outL)
    out = std::max(out, std::fabs(s));
  CHECK(out == 0.0f);
  REQUIRE(meters.size() == 1);
  CHECK(meters[0].peakDb == -120.0f);
}

TEST_CASE("gain is clamped: a channel far too loud is cut by at most 12 dB",
          "[mixer]") {
  Mixer mixer;
  ChannelKey k{"blaring", 0};
  Tone t{440.0, 0.5};
  std::vector<float> in(kBlock), outL(kBlock), outR(kBlock);
  for (int i = 0; i < (int)(90 * kRate / kBlock); ++i) {
    t.fill(in);
    mixer.process({{&k, in.data(), nullptr, 0.0f, true}}, kBlock, outL.data(), outR.data());
  }
  CHECK(mixer.channelGainDb(k) == Catch::Approx(-12.0).margin(1e-9));
}

TEST_CASE("boost is clamped lower than cut, and noise is never lifted", "[mixer]") {
  Mixer mixer;
  ChannelKey quiet{"quiet", 0}, hiss{"hiss", 0};
  Tone q{440.0, 0.01}, h{440.0, 0.0006};  // about -43 and -65 LUFS
  std::vector<float> lq(kBlock), lh(kBlock), outL(kBlock), outR(kBlock);
  for (int i = 0; i < (int)(60 * kRate / kBlock); ++i) {
    q.fill(lq);
    h.fill(lh);
    mixer.process({{&quiet, lq.data(), nullptr, 0.0f, true}, {&hiss, lh.data(), nullptr, 0.0f, true}},
                  kBlock, outL.data(), outR.data());
  }
  CHECK(mixer.channelGainDb(quiet) == Catch::Approx(9.0).margin(1e-9));
  CHECK(mixer.channelGainDb(hiss) == 0.0);
}
