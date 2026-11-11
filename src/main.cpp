// balcony: the listener bot (README.md, DESIGN.md).
//
//   balcony --server <host:port> [--user NAME] [--url URL] [--admins a,b]
//           [--hidden] [--listen 127.0.0.1:8080] [--page web/]
//   balcony --replay <archive dir> [--loop] [--listen ...] [--page web/]
//
// --server joins the room (DESIGN §3, §7) and streams it. Without --user it
// joins anonymously as `balcony`; with one, the password is read from the
// BALCONY_PASSWORD environment variable, never the command line (which every
// user of the machine can read). --hidden says the server gives this account
// the `H` privilege, so it is not counted in votes and never votes; otherwise
// --vote-threshold tells it the server's SetVotingThreshold, so it can back a
// vote exactly when the humans alone would have carried it (DESIGN §4).
//
// --listen-only starts with web chat off (DESIGN §5); an admin's `!unquiet`,
// by private message, turns it on.
//
// --replay plays a session archive as if it were the room (DESIGN §9.7):
// everything from the mix to the page, with no server.
//
// --page serves the page as well, for running locally without Caddy.

#include "Archive.h"
#include "Balcony.h"
#include "Room.h"
#include "Version.h"

#include <SocketClient.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

using namespace balcony;

namespace {
std::atomic<bool> g_stop{false};
void onSignal(int) { g_stop = true; }

void usage(const char *argv0) {
  std::fprintf(stderr,
               "%s %s\nusage: %s --server <host:port> [--user NAME] [--url URL] "
               "[--admins a,b] [--hidden | --vote-threshold PCT] [--listen-only] [--listen host:port] [--page dir]\n"
               "       %s --replay <archive dir> [--loop] [--listen host:port] [--page dir]\n",
               kBotName, version(), argv0, argv0);
}

bool splitHostPort(const std::string &s, std::string &host, int &port) {
  const size_t colon = s.rfind(':');
  if (colon == std::string::npos || colon == 0)
    return false;
  host = s.substr(0, colon);
  port = std::atoi(s.substr(colon + 1).c_str());
  return port > 0 && port < 65536;
}
}  // namespace

int main(int argc, char **argv) {
  std::string replay, server, user, url, admins, page, threshold, listen = "127.0.0.1:8080";
  bool loop = false, hidden = false, listenOnly = false;
  for (int i = 1; i < argc; ++i) {
    auto arg = [&](const char *f, std::string &v) {
      if (std::strcmp(argv[i], f) == 0 && i + 1 < argc) {
        v = argv[++i];
        return true;
      }
      return false;
    };
    if (arg("--replay", replay) || arg("--server", server) || arg("--user", user) ||
        arg("--url", url) || arg("--admins", admins) || arg("--page", page) ||
        arg("--listen", listen) || arg("--vote-threshold", threshold))
      continue;
    if (std::strcmp(argv[i], "--loop") == 0) {
      loop = true;
      continue;
    }
    if (std::strcmp(argv[i], "--listen-only") == 0) {
      listenOnly = true;
      continue;
    }
    if (std::strcmp(argv[i], "--hidden") == 0) {
      hidden = true;
      continue;
    }
    usage(argv[0]);
    return 2;
  }
  if (replay.empty() == server.empty()) {
    usage(argv[0]);
    return 2;
  }

  Balcony::Config cfg;
  if (!splitHostPort(listen, cfg.host, cfg.port)) {
    usage(argv[0]);
    return 2;
  }
  cfg.staticRoot = page;
  cfg.openAtStart = !replay.empty();
  cfg.quietAtStart = listenOnly;

  // The room's client, when there is a room. Chat to the room goes through it.
  BotClient::SocketClient client;
  if (!server.empty())
    cfg.toRoom = [&client](const std::string &line) { client.sendChat(line); };
  else
    cfg.toRoom = [](const std::string &line) { std::printf("[to room] %s\n", line.c_str()); };

  Balcony balcony(cfg);
  std::string err;
  if (!balcony.listen(&err)) {
    std::fprintf(stderr, "cannot listen on %s: %s\n", listen.c_str(), err.c_str());
    return 1;
  }
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);

  std::string closingWhy = "server-closed";

  if (!server.empty()) {
    std::string host;
    int port = 0;
    if (!splitHostPort(server, host, port)) {
      usage(argv[0]);
      return 2;
    }
    Room::Config rc;
    rc.pageUrl = url;
    rc.hidden = hidden;
    rc.voteThreshold = std::atoi(threshold.c_str());
    std::stringstream ss(admins);
    for (std::string a; std::getline(ss, a, ',');)
      if (!a.empty())
        rc.admins.push_back(a);
    rc.selfName = [&client] { return client.serverUsername(); };

    // The room's reason is written before `g_stop` is set, and `run` reads it
    // only after seeing `g_stop`: the atomic orders the two.
    std::atomic<bool> roomEnded{false};
    Room room(client, balcony, rc, [&](const std::string &why) {
      closingWhy = why;
      roomEnded = true;
      g_stop = true;
    });
    client.addListener(&room);
    client.setSampleRate(48000);
    client.setChannels({});  // a listener publishes nothing
    client.setDefaultRecvEnabled(true);
    const char *pw = std::getenv("BALCONY_PASSWORD");
    if (user.empty())
      client.connect(host, port, std::string("anonymous:") + kBotName, "");
    else
      client.connect(host, port, user, pw ? pw : "");
    std::printf("joining %s; listeners on http://%s/\n", server.c_str(), listen.c_str());
    std::fflush(stdout);

    balcony.run(g_stop, closingWhy);
    // Stopped from outside (`ninjam-room close`): say so in the room. Sent
    // away, or the server gone: already said, or nobody to say it to.
    if (!roomEnded && client.isConnected())
      client.sendChat("balcony leaving: the stream has ended.");
    client.removeListener(&room);
    client.disconnect();  // sends what is queued, then closes
    std::printf("stopped: %s\n", closingWhy.c_str());
    return 0;
  }

  ArchiveReader reader(replay);
  if (!reader.ok()) {
    std::fprintf(stderr, "%s\n", reader.error().c_str());
    return 1;
  }
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
  balcony.run(g_stop, closingWhy);
  g_stop = true;
  feeder.join();
  return 0;
}
