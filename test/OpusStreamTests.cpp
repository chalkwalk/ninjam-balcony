#include "OpusStream.h"

#include <chalkwalk/dsp/Pi.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <opus.h>
#include <vector>

using namespace balcony;

TEST_CASE("a tone survives the encode at about its level", "[opus]") {
  OpusStream enc;
  REQUIRE(enc.ok());
  int err = 0;
  OpusDecoder *dec = opus_decoder_create(48000, 2, &err);
  REQUIRE(err == OPUS_OK);

  std::vector<float> l(OpusStream::kFrameSamples), r(OpusStream::kFrameSamples);
  std::vector<float> out(OpusStream::kFrameSamples * 2);
  std::vector<uint8_t> packet;
  double sumIn = 0.0, sumOut = 0.0;
  size_t bytes = 0;
  long t = 0;
  const int frames = 250;  // 5 s
  for (int f = 0; f < frames; ++f) {
    for (int i = 0; i < OpusStream::kFrameSamples; ++i, ++t)
      l[(size_t)i] = r[(size_t)i] =
          (float)(0.3 * std::sin(2.0 * chalkwalk::dsp::kPi * 440.0 * (double)t / 48000.0));
    REQUIRE(enc.encode(l.data(), r.data(), packet));
    bytes += packet.size();
    const int n = opus_decode_float(dec, packet.data(), (opus_int32)packet.size(),
                                    out.data(), OpusStream::kFrameSamples, 0);
    REQUIRE(n == OpusStream::kFrameSamples);
    if (f >= 50) {  // past the codec's start-up
      for (int i = 0; i < n; ++i) {
        sumIn += (double)l[(size_t)i] * l[(size_t)i];
        sumOut += (double)out[(size_t)i * 2] * out[(size_t)i * 2];
      }
    }
  }
  opus_decoder_destroy(dec);
  CHECK(10.0 * std::log10(sumOut / sumIn) == Catch::Approx(0.0).margin(0.5));
  const double kbps = bytes * 8.0 / (frames * 0.02) / 1000.0;
  CHECK(kbps < 130.0);  // about the 96 kbit/s asked for, VBR
}
