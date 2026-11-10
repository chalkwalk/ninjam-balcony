// balcony-fake-player: a stand-in player for testing against a LOCAL ninjamsrv.
//
//   balcony-fake-player <host> <port> <name> [--tone HZ] [--hear] [--say TEXT]
//                       [--seconds N] [--pan P]
//
// Joins anonymously; with --tone it publishes one channel and sends a tone
// every interval; with --hear it subscribes to everyone and prints each
// interval it receives (who, channel, samples, RMS); --say sends one line
// after joining. Prints the room's chat. Never point it at a public room.

#include "SocketClient.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_stop{false};
void onSignal(int) { g_stop = true; }

struct Player : BotClient::Listener {
  BotClient::SocketClient &client;
  double hz = 0;
  std::string say;
  std::atomic<int> bpm{0}, bpi{0};
  std::atomic<bool> gone{false};
  explicit Player(BotClient::SocketClient &c) : client(c) {}

  void onConnected() override {
    std::printf("connected as %s\n", client.serverUsername().c_str());
    if (!say.empty())
      client.sendChat(say);
  }
  void onDisconnected(const std::string &why) override {
    std::printf("disconnected: %s\n", why.c_str());
    gone = true;
  }
  void onServerConfig(int b, int i) override {
    std::printf("config %d bpm %d bpi\n", b, i);
    bpm = b;
    bpi = i;
  }
  void onRoomMembershipChange(const std::string &u, bool joined) override {
    std::printf("%s %s\n", joined ? "join" : "part", u.c_str());
  }
  void onChatMessage(const std::string &type, const std::string &u,
                     const std::string &text) override {
    if (type == "MSG" || type == "PRIVMSG")
      std::printf("%s <%s> %s\n", type.c_str(), u.c_str(), text.c_str());
    std::fflush(stdout);
  }
  void onIntervalReceived(const std::string &u, int ch, const float *l, const float *r,
                          int n) override {
    double sum = 0;
    for (int i = 0; i < n; ++i)
      sum += (double)l[i] * l[i];
    std::printf("interval %s ch%d %d samples%s rms %.3f\n", u.c_str(), ch, n,
                r ? "" : " mono", n ? std::sqrt(sum / n) : 0.0);
    std::fflush(stdout);
  }
};
} // namespace

int main(int argc, char **argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: %s <host> <port> <name> [--tone HZ] [--hear] "
                         "[--say TEXT] [--seconds N]\n", argv[0]);
    return 2;
  }
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);
  BotClient::SocketClient client;
  Player p(client);
  bool hear = false;
  double seconds = 0;
  for (int i = 4; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--tone" && i + 1 < argc) p.hz = std::atof(argv[++i]);
    else if (a == "--hear") hear = true;
    else if (a == "--say" && i + 1 < argc) p.say = argv[++i];
    else if (a == "--seconds" && i + 1 < argc) seconds = std::atof(argv[++i]);
  }
  const double rate = 48000;
  client.addListener(&p);
  client.setSampleRate(rate);
  client.setChannels(p.hz > 0 ? std::vector<std::string>{"tone"} : std::vector<std::string>{});
  client.setDefaultRecvEnabled(hear);
  client.connect(argv[1], std::atoi(argv[2]), std::string("anonymous:") + argv[3], "");

  // Send one interval of tone per interval, on our own clock: a stand-in, not
  // a client, so it does not care about the grid's phase.
  const auto start = std::chrono::steady_clock::now();
  double phase = 0;
  auto next = start;
  while (!g_stop && !p.gone) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const auto now = std::chrono::steady_clock::now();
    if (seconds > 0 && now - start > std::chrono::duration<double>(seconds))
      break;
    if (p.hz <= 0 || !client.isConnected() || p.bpm <= 0 || now < next)
      continue;
    const double len = 60.0 * p.bpi / p.bpm;
    const int n = (int)std::lround(len * rate);
    std::vector<float> l((size_t)n), r((size_t)n);
    for (int i = 0; i < n; ++i) {
      const float s = 0.2f * (float)std::sin(phase);
      phase += 2 * M_PI * p.hz / rate;
      l[(size_t)i] = s;
      r[(size_t)i] = s;
    }
    client.transmit(l.data(), r.data(), n);
    next = (next == start ? now : next) + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                                std::chrono::duration<double>(len));
  }
  client.disconnect();
  return 0;
}
