#include "Mixer.h"

#include <chalkwalk/dsp/Loudness.h>
#include <chalkwalk/dsp/Pi.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

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

// The first chain, which the tests below the mastered ones were written for;
// `either` runs a rule that must hold in both chains against each.
MixerConfig plain() {
  MixerConfig c;
  c.mastered = false;
  return c;
}
MixerConfig either() {
  MixerConfig c;
  c.mastered = GENERATE(false, true);
  return c;
}

}  // namespace

TEST_CASE("two players 10 dB apart come out at one level", "[mixer]") {
  Mixer mixer(plain());
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
  Mixer mixer(either());
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
  Mixer mixer(plain());
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
  Mixer mixer(plain());
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
  Mixer mixer(either());
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
  Mixer mixer(either());
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
  const MixerConfig c = either();
  Mixer mixer(c);
  ChannelKey quiet{"quiet", 0}, hiss{"hiss", 0};
  Tone q{440.0, 0.01}, h{440.0, 0.0006};  // about -43 and -65 LUFS
  std::vector<float> lq(kBlock), lh(kBlock), outL(kBlock), outR(kBlock);
  for (int i = 0; i < (int)(60 * kRate / kBlock); ++i) {
    q.fill(lq);
    h.fill(lh);
    mixer.process({{&quiet, lq.data(), nullptr, 0.0f, true}, {&hiss, lh.data(), nullptr, 0.0f, true}},
                  kBlock, outL.data(), outR.data());
  }
  CHECK(mixer.channelGainDb(quiet) ==
        Catch::Approx(c.mastered ? c.channelBoostDb : c.maxBoostDb).margin(1e-9));
  CHECK(mixer.channelGainDb(hiss) == 0.0);
}

// ---- The mastered chain (DESIGN §6.1) ----

namespace {

MixerConfig mastered() {
  MixerConfig c;
  c.mastered = true;
  return c;
}

// Feeds tones per channel for `seconds`; `amps` may change between calls.
struct Scene {
  Mixer mixer;
  std::vector<ChannelKey> keys;
  std::vector<std::vector<Tone>> tones;  // per channel, summed
  std::vector<float> pans;
  explicit Scene(const MixerConfig &c) : mixer(c) {}
  void add(const std::string &user, std::vector<Tone> t, float pan = 0.0f) {
    keys.push_back({user, 0});
    tones.push_back(std::move(t));
    pans.push_back(pan);
  }
  float peak = 0.0f;
  void run(double seconds, const std::vector<bool> &active = {}) {
    std::vector<std::vector<float>> in(keys.size(), std::vector<float>(kBlock));
    std::vector<float> tmp(kBlock), outL(kBlock), outR(kBlock);
    for (int b = 0; b < (int)(seconds * kRate / kBlock); ++b) {
      std::vector<ChannelInput> inputs;
      for (size_t k = 0; k < keys.size(); ++k) {
        std::fill(in[k].begin(), in[k].end(), 0.0f);
        for (auto &t : tones[k]) {
          t.fill(tmp);
          for (int i = 0; i < kBlock; ++i)
            in[k][(size_t)i] += tmp[(size_t)i];
        }
        if (active.empty() || active[k])
          inputs.push_back({&keys[k], in[k].data(), nullptr, pans[k], true});
      }
      mixer.process(inputs, kBlock, outL.data(), outR.data());
      for (int i = 0; i < kBlock; ++i)
        peak = std::max(peak, std::max(std::fabs(outL[(size_t)i]), std::fabs(outR[(size_t)i])));
    }
  }
};

}  // namespace

TEST_CASE("mastered: two players 6 dB apart come out at one level", "[mixer][mastered]") {
  // 6 dB: the mastered chain lifts at most +6 dB (it cuts up to 12).
  Scene s(mastered());
  s.add("loud", {{440.0, 0.1}});
  s.add("quiet", {{660.0, 0.05}});
  s.run(150.0);
  for (const auto &k : s.keys)
    CHECK(s.mixer.channelLufs(k) + s.mixer.channelGainDb(k) == Catch::Approx(-23.0).margin(0.5));
  CHECK(s.peak <= std::pow(10.0f, -1.5f / 20.0f) + 1e-6f);
}

TEST_CASE("mastered: someone suddenly much louder is caught quickly, then slowly",
          "[mixer][mastered]") {
  Scene s(mastered());
  s.add("p", {{440.0, 0.05}});
  s.run(120.0);
  const double settled = s.mixer.channelGainDb(s.keys[0]);
  s.tones[0][0].amp = 0.25;  // +14 dB
  s.run(8.0);
  const double after = s.mixer.channelGainDb(s.keys[0]);
  // The old single view, at 0.5 dB/s, could not have moved more than 4 dB.
  CHECK(settled - after > 5.0);
}

TEST_CASE("mastered: a player who rests is not lifted", "[mixer][mastered]") {
  Scene s(mastered());
  s.add("p", {{440.0, 0.05}});
  s.add("q", {{550.0, 0.05}});
  s.run(120.0);
  const double before = s.mixer.channelGainDb(s.keys[0]);
  s.tones[0][0].amp = 0.0;  // still connected, sending silence
  s.run(60.0);
  CHECK(s.mixer.channelGainDb(s.keys[0]) == Catch::Approx(before).margin(0.1));
}

TEST_CASE("mastered: a small spread by arrival, the player's own pan on top",
          "[mixer][mastered]") {
  Scene s(mastered());
  s.add("first", {{440.0, 0.05}});
  s.add("second", {{550.0, 0.05}});
  s.run(10.0);
  // Two players: +/-5 %, first on the left.
  CHECK(s.mixer.channelPan(s.keys[0]) == Catch::Approx(-0.05).margin(1e-6));
  CHECK(s.mixer.channelPan(s.keys[1]) == Catch::Approx(0.05).margin(1e-6));
  s.add("third", {{660.0, 0.05}});
  s.add("fourth", {{770.0, 0.05}}, 0.5f);
  s.run(10.0);
  // Four: +/-9.5 %, dealt outside in: first far left, second far right, third
  // inner left, fourth inner right.
  const double half = 0.21 - 0.16 * (11.0 / 13.0) * (11.0 / 13.0);
  CHECK(s.mixer.channelPan(s.keys[0]) == Catch::Approx(-half).margin(1e-6));
  CHECK(s.mixer.channelPan(s.keys[1]) == Catch::Approx(half).margin(1e-6));
  CHECK(s.mixer.channelPan(s.keys[2]) == Catch::Approx(-half / 3.0).margin(1e-6));
  CHECK(s.mixer.channelPan(s.keys[3]) == Catch::Approx(half / 3.0).margin(1e-6));
  // Two leave: after the presence time the rest glide back to +/-5 %, slowly.
  s.run(31.0, {true, true, false, false});
  s.run(1.0, {true, true, false, false});
  CHECK(s.mixer.channelPan(s.keys[0]) > -half);  // moving...
  CHECK(s.mixer.channelPan(s.keys[0]) < -0.05);  // ...not jumped
  s.run(5.0, {true, true, false, false});
  CHECK(s.mixer.channelPan(s.keys[0]) == Catch::Approx(-0.05).margin(1e-6));
}

TEST_CASE("mastered: unmasking dips the crowding player, never anyone's home band",
          "[mixer][mastered]") {
  MixerConfig c = mastered();
  c.unmask = true;
  Scene s(c);
  // A bass player, at home in the lows; a keys player at home around 400 Hz
  // with a lot of low end too, crowding the bass.
  s.add("bass", {{110.0, 0.2}});
  s.add("keys", {{400.0, 0.1}, {125.0, 0.09}});
  s.run(120.0);
  const auto &bass = s.keys[0], &keys = s.keys[1];
  // Keys hold about a third of the low band here; a full 2 dB needs an even split.
  CHECK(s.mixer.unmaskDipDb(keys, 0) < -0.5);           // keys make room in the lows
  CHECK(s.mixer.unmaskDipDb(keys, 0) >= -2.0 - 1e-9);   // by at most 2 dB
  CHECK(s.mixer.unmaskDipDb(keys, 1) == 0.0);           // their home band is theirs
  for (int b = 0; b < Mixer::kBands; ++b)
    CHECK(s.mixer.unmaskDipDb(bass, b) > -0.3);         // the bass is left alone
}

TEST_CASE("mastered: the tilt leans against too much low end, a little and slowly",
          "[mixer][mastered]") {
  MixerConfig c = mastered();
  c.tiltBalanceDb = 0.0;
  Scene s(c);
  s.add("rumbly", {{90.0, 0.2}, {3000.0, 0.01}});
  s.run(30.0);
  const double early = s.mixer.report().tiltDb;
  CHECK(early > 0.0);
  CHECK(early <= c.tiltSlewDbPerSecond * 30.0 + 1e-9);
  s.run(120.0);
  CHECK(s.mixer.report().tiltDb == Catch::Approx(c.tiltMaxDb).margin(1e-9));
}
