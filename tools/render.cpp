// balcony-render: mix an archived jam as the balcony would, and measure it.
//
//   balcony-render <archive dir> <out.wav> [--seconds N] [--trace]
//                  [--window S] [--slew DBPS] [--max-boost DB] [--max-cut DB]
//                  [--boost-floor LUFS] [--target LUFS]
//
// The archive may be one *.ninjam directory or a directory of them. Prints,
// per channel, its measured loudness and the gain the rider ended on, then the
// mix's integrated loudness and peak, and how long the mix took against how
// long it plays (DESIGN §8: the cost is measured, not estimated).

#include "Archive.h"
#include "Mixer.h"
#include "Wav.h"

#include <chalkwalk/dsp/Loudness.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace balcony;

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <archive dir> <out.wav> [--seconds N]\n", argv[0]);
    return 2;
  }
  double limitSeconds = 0.0;
  bool trace = false;
  MixerConfig cfg;
  for (int i = 3; i < argc; ++i) {
    if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc)
      limitSeconds = std::atof(argv[i + 1]);
    if (std::strcmp(argv[i], "--trace") == 0)
      trace = true;  // each minute: every channel's windowed LUFS and gain
    // Tuning: the rider's knobs (DESIGN §9.4), to render variants for listening.
    auto num = [&](const char *flag, double &v) {
      if (std::strcmp(argv[i], flag) == 0 && i + 1 < argc)
        v = std::atof(argv[i + 1]);
    };
    num("--window", cfg.windowSeconds);
    num("--slew", cfg.slewDbPerSecond);
    num("--max-boost", cfg.maxBoostDb);
    num("--max-cut", cfg.maxCutDb);
    num("--boost-floor", cfg.boostFloorLufs);
    num("--target", cfg.channelTargetLufs);
  }
  double nextTrace = 0.0;

  const double rate = 48000.0;
  ArchiveReader reader(argv[1], rate);
  if (!reader.ok()) {
    std::fprintf(stderr, "%s\n", reader.error().c_str());
    return 1;
  }
  WavWriter wav(argv[2], (int)rate);
  if (!wav.ok()) {
    std::fprintf(stderr, "cannot write %s\n", argv[2]);
    return 1;
  }

  Mixer mixer(cfg);
  std::vector<float> outL, outR, allL, allR;
  std::map<ChannelKey, std::string> names;
  ArchiveInterval iv;
  double decodeSeconds = 0.0, mixSeconds = 0.0, audioSeconds = 0.0;
  const int block = 2880;  // 60 ms, the size the bot mixes in (DESIGN §9.3)
  float peak = 0.0f;

  using clock = std::chrono::steady_clock;
  while (true) {
    auto t0 = clock::now();
    if (!reader.next(iv))
      break;
    auto t1 = clock::now();
    decodeSeconds += std::chrono::duration<double>(t1 - t0).count();

    for (const auto &c : iv.channels)
      names[c.key] = c.channelName;
    outL.assign((size_t)iv.samples, 0.0f);
    outR.assign((size_t)iv.samples, 0.0f);
    for (int pos = 0; pos < iv.samples; pos += block) {
      const int n = std::min(block, iv.samples - pos);
      std::vector<ChannelInput> inputs;
      for (const auto &c : iv.channels)
        inputs.push_back({&c.key, c.left.data() + pos, c.right.data() + pos, 0.0f, true});
      mixer.process(inputs, n, outL.data() + pos, outR.data() + pos);
    }
    mixSeconds += std::chrono::duration<double>(clock::now() - t1).count();

    for (int i = 0; i < iv.samples; ++i)
      peak = std::max(peak, std::max(std::fabs(outL[(size_t)i]), std::fabs(outR[(size_t)i])));
    wav.write(outL.data(), outR.data(), iv.samples);
    allL.insert(allL.end(), outL.begin(), outL.end());
    allR.insert(allR.end(), outR.begin(), outR.end());
    audioSeconds += iv.samples / rate;
    if (trace && audioSeconds >= nextTrace) {
      nextTrace += 60.0;
      std::printf("%6.1f min ", audioSeconds / 60.0);
      for (const auto &kv : names)
        std::printf(" %s %5.1f/%+5.1f", displayName(kv.first.user).substr(0, 8).c_str(),
                    mixer.channelLufs(kv.first), mixer.channelGainDb(kv.first));
      std::printf("  | master %+5.1f\n", mixer.masterGainDb());
    }
    if (limitSeconds > 0.0 && audioSeconds >= limitSeconds)
      break;
  }
  wav.close();

  std::printf("intervals %d, %.1f min of audio; %d log lines skipped, %d clips missing\n",
              reader.intervalCount(), audioSeconds / 60.0, reader.skippedLogLines(),
              reader.missingClips());
  std::printf("%-28s %-20s %10s %10s\n", "player", "channel", "LUFS(win)", "gain dB");
  for (const auto &kv : names)
    std::printf("%-28s %-20s %10.1f %+10.1f\n", displayName(kv.first.user).c_str(),
                kv.second.c_str(), mixer.channelLufs(kv.first),
                mixer.channelGainDb(kv.first));
  const double lufs = chalkwalk::dsp::measure::integratedLufs(
      allL.data(), allR.data(), (int)allL.size(), rate);
  std::printf("mix: integrated %.1f LUFS, peak %.2f dBFS, master gain %+.1f dB\n", lufs,
              20.0 * std::log10(std::max(peak, 1e-9f)), mixer.masterGainDb());
  std::printf("cost: decode+resample %.1f s, mix %.1f s, for %.1f s of audio "
              "(%.2f%% and %.2f%% of one core)\n",
              decodeSeconds, mixSeconds, audioSeconds,
              100.0 * decodeSeconds / audioSeconds, 100.0 * mixSeconds / audioSeconds);
  return 0;
}
