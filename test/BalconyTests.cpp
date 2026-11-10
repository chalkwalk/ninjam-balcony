#include "Balcony.h"
#include "TestClient.h"

#include <chalkwalk/dsp/Pi.h>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <opus.h>
#include <thread>

using namespace balcony;
using namespace balcony::test;

namespace {

ArchiveInterval toneInterval(int bpm, int bpi, double amp, long &t) {
  ArchiveInterval iv;
  iv.bpm = bpm;
  iv.bpi = bpi;
  iv.samples = (int)std::lround(48000.0 * 60.0 * bpi / bpm);
  ChannelAudio a;
  a.key = {"jgorbe@1.2.3.x", 0};
  a.channelName = "guitar";
  a.left.resize((size_t)iv.samples);
  for (auto &s : a.left)
    s = (float)(amp * std::sin(2.0 * chalkwalk::dsp::kPi * 440.0 * (double)(t++) / 48000.0));
  a.right = a.left;
  iv.channels.push_back(std::move(a));
  return iv;
}

struct Running {
  std::atomic<bool> stop{false};
  std::vector<std::string> toRoom;
  std::mutex m;
  std::unique_ptr<Balcony> b;
  std::thread loop, feed;

  Running() {
    Balcony::Config c;
    c.port = 0;
    c.toRoom = [this](const std::string &s) {
      std::lock_guard<std::mutex> g(m);
      toRoom.push_back(s);
    };
    b = std::make_unique<Balcony>(c);
    REQUIRE(b->listen(nullptr));
    loop = std::thread([this] { b->run(stop); });
    feed = std::thread([this] {
      long t = 0;
      // 240 bpm, 4 beats: one-second intervals, so a test sees several.
      while (!stop)
        b->deliverInterval(toneInterval(240, 4, 0.3, t), stop);
    });
  }
  ~Running() {
    stop = true;
    feed.join();
    loop.join();
  }
  bool roomHeard(const std::string &needle) {
    std::lock_guard<std::mutex> g(m);
    for (auto &s : toRoom)
      if (s.find(needle) != std::string::npos)
        return true;
    return false;
  }
};

}  // namespace

TEST_CASE("a listener says hello, is welcomed, and hears the room", "[balcony]") {
  Running r;
  Client c;
  REQUIRE(c.open(r.b->port()));
  c.sendText(R"({"t":"hello","name":"Alice"})");
  const std::string welcome = c.recvText();
  CHECK(welcome.find("\"t\":\"welcome\"") != std::string::npos);
  CHECK(welcome.find("\"you\":\"Alice\"") != std::string::npos);

  int err = 0;
  OpusDecoder *dec = opus_decoder_create(48000, 2, &err);
  REQUIRE(err == OPUS_OK);
  std::vector<float> pcm(960 * 2);
  int messages = 0;
  double sum = 0.0;
  long n = 0;
  uint32_t lastPos = 0;
  bool sawRoom = false, sawMeter = false, positionsAdvance = true;
  while (messages < 60) {  // 3.6 s
    uint8_t op;
    std::string p;
    REQUIRE(c.recvFrame(op, p));
    if (op == 0x1) {
      if (p.find("\"t\":\"room\"") != std::string::npos &&
          p.find("\"name\":\"jgorbe\"") != std::string::npos)
        sawRoom = true;
      continue;
    }
    REQUIRE(op == 0x2);
    REQUIRE((uint8_t)p[0] == 1);
    REQUIRE((uint8_t)p[1] == 3);
    uint32_t pos = 0;
    for (int k = 0; k < 4; ++k)
      pos |= (uint32_t)(uint8_t)p[2 + (size_t)k] << (8 * k);
    if (messages > 0 && pos != lastPos + 2880)
      positionsAdvance = false;
    lastPos = pos;
    size_t off = 6;
    for (int f = 0; f < 3; ++f) {
      const size_t len = (uint8_t)p[off] | ((size_t)(uint8_t)p[off + 1] << 8);
      off += 2;
      const int got = opus_decode_float(dec, (const unsigned char *)p.data() + off,
                                        (opus_int32)len, pcm.data(), 960, 0);
      REQUIRE(got == 960);
      off += len;
      if (messages > 20)
        for (int i = 0; i < 960; ++i, ++n)
          sum += (double)pcm[(size_t)i * 2] * pcm[(size_t)i * 2];
    }
    const int meters = (uint8_t)p[off];
    if (meters > 0 && (uint8_t)p[off + 3] > 0)
      sawMeter = true;
    ++messages;
  }
  opus_decoder_destroy(dec);
  CHECK(sawRoom);
  CHECK(sawMeter);
  CHECK(positionsAdvance);
  // Something like the -16 LUFS mix of a steady tone, certainly not silence.
  CHECK(10.0 * std::log10(sum / (double)n) > -40.0);
}

TEST_CASE("names are refused politely, and the room hears listeners as the bot",
          "[balcony]") {
  Running r;
  Client a, b;
  REQUIRE(a.open(r.b->port()));
  REQUIRE(b.open(r.b->port()));
  a.sendText(R"({"t":"hello","name":"Alice"})");
  CHECK(a.recvText().find("welcome") != std::string::npos);
  b.sendText(R"({"t":"hello","name":"alice"})");
  CHECK(b.recvText().find("\"why\":\"name-taken\"") != std::string::npos);
  b.sendText(R"({"t":"hello","name":"Bob"})");
  CHECK(b.recvText().find("welcome") != std::string::npos);

  a.sendText(R"({"t":"say","text":"nice groove"})");
  bool heard = false;
  for (int i = 0; i < 50 && !heard; ++i) {
    std::string t = b.recvText();
    heard = t.find("\"text\":\"nice groove\"") != std::string::npos &&
            t.find("\"from\":\"Alice\"") != std::string::npos;
  }
  CHECK(heard);
  for (int i = 0; i < 100 && !r.roomHeard("<Alice> nice groove"); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  CHECK(r.roomHeard("<Alice> nice groove"));
  // And their arrival was batched into one line at an interval boundary.
  for (int i = 0; i < 300 && !r.roomHeard("joined the balcony"); ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  CHECK(r.roomHeard("Alice and Bob joined the balcony (2 listening)"));
}
