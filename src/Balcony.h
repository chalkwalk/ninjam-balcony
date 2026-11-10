#pragma once

#include "Archive.h"
#include "Listeners.h"
#include "Mixer.h"
#include "OpusStream.h"
#include "WebServer.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

// The balcony: the room's sound and talk, out to every listener (DESIGN §2-§3,
// §6, §9.3).
//
// One thread runs it: `run` owns the web server, the clock, the mix and the
// encode. Whatever feeds it -- the replay of an archive, or the room through
// the client -- hands intervals over from its own thread with `deliver`, and
// the room's talk with `roomChat`; both are queued and taken on the loop.
//
// THE GRID IS OUR OWN (DESIGN §6). Intervals delivered during one interval of
// the bot's clock are played in the next, from its downbeat, as every Ninjam
// client plays every other. Interval phase between clients is free, so
// following nobody's grid costs nothing audible.

namespace balcony {

class Balcony {
public:
  struct Config {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string staticRoot;  // serve the page too (local runs only)
    MixerConfig mixer;
    int bitrate = 96000;
    size_t maxPendingBytes = 512 * 1024;  // a listener this far behind is dropped
    // Said to the room through `toRoom`, and to the listeners.
    std::function<void(const std::string &)> toRoom;
  };

  explicit Balcony(Config c);
  ~Balcony();

  bool listen(std::string *error);
  int port() const { return server_.port(); }

  // From any thread. One channel's whole interval, at 48 kHz, for the next
  // downbeat. `bpm`/`bpi` are the tempo it was played at.
  void deliver(ChannelAudio audio, int bpm, int bpi);
  // From any thread: a whole interval at once, waiting while the previous one
  // has not been taken yet. For replay, which must not run ahead.
  void deliverInterval(ArchiveInterval interval, const std::atomic<bool> &stop);
  // From any thread: what a player said in the room.
  void roomChat(const std::string &from, const std::string &text);

  // Run until `stop`; then tell every listener the stream has ended.
  void run(const std::atomic<bool> &stop, const std::string &closingWhy = "server-closed");

  // For tests: how many blocks have gone out, and to how many listeners.
  long blocksSent() const { return blocks_; }

private:
  struct Pending {
    int bpm = 0, bpi = 0;
    std::map<ChannelKey, ChannelAudio> channels;
  };
  struct Player {
    int id = 0;
    ChannelKey key;
    std::string channelName;
    int lastInterval = 0;  // the interval it was last heard in
    bool inStream = true;
  };

  void onOpen(int id);
  void onText(int id, const std::string &text);
  void onClose(int id);
  std::string status() const;

  void takeInbox();
  void boundary();
  void produceBlock();
  void mixSpan(float *l, float *r, int n, std::vector<ChannelMeter> &meters);
  void broadcastText(const std::string &json);
  void sendRoom(int id);
  void chatToListeners(const ChatLine &line);
  std::string roomJson() const;
  std::set<std::string> playerNames() const;

  Config cfg_;
  WebServer server_;
  Listeners listeners_;
  Mixer mixer_;
  OpusStream opus_;

  // Inbox, filled from other threads.
  mutable std::mutex inboxMutex_;
  std::condition_variable inboxTaken_;
  Pending inbox_;
  bool inboxWhole_ = false;  // a whole interval is waiting (replay)
  std::vector<std::pair<std::string, std::string>> inboxChat_;

  // The loop's own state.
  Pending current_;
  int samples_ = 0;  // length of the current interval
  int pos_ = 0;      // position in it
  int intervalIndex_ = 0;
  bool started_ = false;
  int bpm_ = 0, bpi_ = 0;
  long blocks_ = 0;
  uint32_t samplePos_ = 0;
  std::map<ChannelKey, Player> players_;
  int nextChannelId_ = 1;
  std::set<int> welcomed_;
};

// The meter byte: 0..240 for -60..0 dBFS in 0.25 dB steps (DESIGN §9.3).
uint8_t meterByte(float db);

}  // namespace balcony
