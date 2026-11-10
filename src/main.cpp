// balcony: the listener bot (README.md, DESIGN.md).
//
//   balcony --replay <archive dir> [--listen 127.0.0.1:8080] [--page web/]
//
// --replay plays a session archive as if it were the room (DESIGN §9.7):
// everything from the mix to the page, with no server. --page serves the page
// as well, for running locally without Caddy.

#include "Archive.h"
#include "Balcony.h"
#include "Version.h"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

using namespace balcony;

namespace {
std::atomic<bool> g_stop{false};
void onSignal(int) { g_stop = true; }

void usage(const char *argv0) {
  std::fprintf(stderr,
               "%s %s\nusage: %s --replay <archive dir> [--listen host:port] [--page dir] "
               "[--loop]\n",
               kBotName, version(), argv0);
}
}  // namespace

int main(int argc, char **argv) {
  std::string replay, page, listen = "127.0.0.1:8080";
  bool loop = false;
  for (int i = 1; i < argc; ++i) {
    auto arg = [&](const char *f, std::string &v) {
      if (std::strcmp(argv[i], f) == 0 && i + 1 < argc) {
        v = argv[++i];
        return true;
      }
      return false;
    };
    if (arg("--replay", replay) || arg("--page", page) || arg("--listen", listen))
      continue;
    if (std::strcmp(argv[i], "--loop") == 0) {
      loop = true;
      continue;
    }
    usage(argv[0]);
    return 2;
  }
  if (replay.empty()) {
    usage(argv[0]);
    return 2;
  }

  Balcony::Config cfg;
  const size_t colon = listen.rfind(':');
  cfg.host = listen.substr(0, colon);
  cfg.port = std::atoi(listen.substr(colon + 1).c_str());
  cfg.staticRoot = page;
  cfg.toRoom = [](const std::string &line) { std::printf("[to room] %s\n", line.c_str()); };

  ArchiveReader reader(replay);
  if (!reader.ok()) {
    std::fprintf(stderr, "%s\n", reader.error().c_str());
    return 1;
  }
  Balcony balcony(cfg);
  std::string err;
  if (!balcony.listen(&err)) {
    std::fprintf(stderr, "cannot listen on %s: %s\n", listen.c_str(), err.c_str());
    return 1;
  }
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);
  std::printf("replaying %d intervals from %s on http://%s/\n", reader.intervalCount(),
              replay.c_str(), listen.c_str());
  std::fflush(stdout);

  std::thread feeder([&] {
    ArchiveInterval iv;
    while (!g_stop) {
      if (!reader.next(iv)) {
        if (!loop)
          break;
        reader.rewind();
        continue;
      }
      balcony.deliverInterval(std::move(iv), g_stop);
    }
  });
  balcony.run(g_stop);
  g_stop = true;
  feeder.join();
  return 0;
}
