#include "Balcony.h"

#include "Json.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>

namespace balcony {

namespace {
constexpr int kBlock = 2880;  // 60 ms at 48 kHz: three Opus frames
constexpr int kRate = 48000;
constexpr int kForgetAfterIntervals = 8;  // a player resting this long leaves the picture

long long unixNow() { return (long long)std::time(nullptr); }

std::string chatJson(const ChatLine &l) {
  return "{\"t\":\"chat\",\"from\":" + json::quote(l.from) +
         ",\"text\":" + json::quote(l.text) + ",\"kind\":" + json::quote(l.kind) +
         ",\"at\":" + std::to_string(l.at) + "}";
}
}  // namespace

uint8_t meterByte(float db) {
  const float v = std::round((db + 60.0f) * 4.0f);
  return (uint8_t)std::clamp(v, 0.0f, 240.0f);
}

Balcony::Balcony(Config c)
    : cfg_(std::move(c)),
      server_(WebServer::Handlers{
          [this] { return status(); }, [this](int id) { onOpen(id); },
          [this](int id, const std::string &t) { onText(id, t); },
          [this](int id) { onClose(id); }}),
      mixer_(cfg_.mixer),
      opus_(cfg_.bitrate) {
  open_ = cfg_.openAtStart;
  if (!cfg_.staticRoot.empty())
    server_.setStaticRoot(cfg_.staticRoot);
}

Balcony::~Balcony() = default;

bool Balcony::listen(std::string *error) { return server_.listen(cfg_.host, cfg_.port, error); }

void Balcony::deliver(ChannelAudio audio, int bpm, int bpi) {
  std::lock_guard<std::mutex> g(inboxMutex_);
  inbox_.bpm = bpm;
  inbox_.bpi = bpi;
  inbox_.channels[audio.key] = std::move(audio);
}

void Balcony::deliverInterval(ArchiveInterval interval, const std::atomic<bool> &stop) {
  std::unique_lock<std::mutex> g(inboxMutex_);
  while (inboxWhole_ && !stop)
    inboxTaken_.wait_for(g, std::chrono::milliseconds(100));
  if (stop)
    return;
  inbox_.bpm = interval.bpm;
  inbox_.bpi = interval.bpi;
  inbox_.channels.clear();
  for (auto &c : interval.channels)
    inbox_.channels[c.key] = std::move(c);
  inboxWhole_ = true;
}

void Balcony::roomChat(const std::string &from, const std::string &text) {
  std::lock_guard<std::mutex> g(inboxMutex_);
  inboxChat_.emplace_back(from, text);
}

void Balcony::setTempo(int bpm, int bpi) {
  std::lock_guard<std::mutex> g(inboxMutex_);
  inbox_.bpm = bpm;
  inbox_.bpi = bpi;
}

void Balcony::setRoster(std::vector<RosterChannel> roster) {
  std::lock_guard<std::mutex> g(inboxMutex_);
  roster_ = std::move(roster);
  rosterPending_ = true;
}

void Balcony::post(std::function<void()> f) {
  std::lock_guard<std::mutex> g(inboxMutex_);
  inboxDo_.push_back(std::move(f));
}

void Balcony::kick(const std::string &name) {
  post([this, name] {
    const int id = listeners_.kick(name);
    if (id < 0) {
      if (cfg_.toRoom)
        cfg_.toRoom("nobody called " + name + " is in the balcony");
      return;
    }
    server_.sendText(id, "{\"t\":\"closing\",\"why\":\"kicked\"}");
    server_.close(id, 1008, "removed by the room");
    if (cfg_.toRoom)
      cfg_.toRoom(name + " was removed from the balcony for this jam");
  });
}

void Balcony::setQuiet(bool quiet) {
  post([this, quiet] {
    if (quiet == quiet_)
      return;
    quiet_ = quiet;
    const std::string text = quiet ? "The room has paused web chat. You can still listen."
                                   : "The room has turned web chat back on.";
    for (int id : welcomed_)
      server_.sendText(id, "{\"t\":\"notice\",\"text\":" + json::quote(text) + "}");
    if (cfg_.toRoom)
      cfg_.toRoom(quiet ? "web chat paused" : "web chat back on");
  });
}

void Balcony::applyRoster(const std::vector<RosterChannel> &roster) {
  live_ = true;
  std::map<ChannelKey, Player> next;
  for (const auto &rc : roster) {
    auto id = ids_.find(rc.key);
    if (id == ids_.end())
      id = ids_.emplace(rc.key, nextChannelId_++).first;
    Player p;
    p.id = id->second;
    p.key = rc.key;
    p.channelName = rc.name;
    p.inStream = rc.inStream;
    p.pan = rc.pan;
    p.lastInterval = intervalIndex_;
    next.emplace(rc.key, p);
  }
  players_.swap(next);
  for (int id : welcomed_)
    sendRoom(id);
}

std::set<std::string> Balcony::playerNames() const {
  std::set<std::string> s;
  for (const auto &kv : players_)
    s.insert(displayName(kv.first.user));
  return s;
}

std::string Balcony::status() const {
  if (!open_)
    return "{\"open\":false}";
  return "{\"open\":true,\"bpm\":" + std::to_string(bpm_) + ",\"bpi\":" +
         std::to_string(bpi_) + ",\"players\":" + std::to_string(playerNames().size()) +
         ",\"listeners\":" + std::to_string(listeners_.size()) + "}";
}

std::string Balcony::roomJson() const {
  // Players with their channels, in name order; then the listeners.
  std::map<std::string, std::vector<const Player *>> byName;
  for (const auto &kv : players_)
    byName[displayName(kv.first.user)].push_back(&kv.second);
  std::string s = "{\"t\":\"room\",\"players\":[";
  bool firstP = true;
  for (const auto &p : byName) {
    s += (firstP ? "" : ",") + std::string("{\"name\":") + json::quote(p.first) +
         ",\"channels\":[";
    firstP = false;
    bool firstC = true;
    for (const Player *c : p.second) {
      s += (firstC ? "" : ",") + std::string("{\"id\":") + std::to_string(c->id) +
           ",\"name\":" + json::quote(c->channelName) +
           ",\"inStream\":" + (c->inStream ? "true" : "false") + "}";
      firstC = false;
    }
    s += "]}";
  }
  s += "],\"listeners\":[";
  bool first = true;
  for (const auto &n : listeners_.names()) {
    s += (first ? "" : ",") + json::quote(n);
    first = false;
  }
  return s + "]}";
}

void Balcony::onOpen(int) {}

void Balcony::onClose(int id) {
  welcomed_.erase(id);
  if (listeners_.has(id)) {
    listeners_.leave(id);
    for (int other : welcomed_)
      sendRoom(other);
  }
}

void Balcony::sendRoom(int id) { server_.sendText(id, roomJson()); }

void Balcony::broadcastText(const std::string &json) {
  for (int id : welcomed_)
    server_.sendText(id, json);
}

void Balcony::chatToListeners(const ChatLine &line) {
  listeners_.remember(line, Clock::now());
  broadcastText(chatJson(line));
}

void Balcony::onText(int id, const std::string &text) {
  std::map<std::string, std::string> m;
  if (!json::parseFlat(text, m))
    return;
  const std::string &t = m["t"];

  if (t == "hello" && !welcomed_.count(id)) {
    const auto r = listeners_.join(id, m["name"], playerNames());
    if (r != Listeners::Join::Ok) {
      const char *why = r == Listeners::Join::NameTaken ? "name-taken"
                        : r == Listeners::Join::Blocked  ? "blocked"
                                                         : "name-invalid";
      server_.sendText(id, std::string("{\"t\":\"refused\",\"why\":\"") + why + "\"}");
      return;  // they may try another name
    }
    welcomed_.insert(id);
    server_.sendText(id, "{\"t\":\"welcome\",\"you\":" + json::quote(m["name"]) +
                             ",\"bpm\":" + std::to_string(bpm_) + ",\"bpi\":" +
                             std::to_string(bpi_) + ",\"sampleRate\":48000}");
    std::string h = "{\"t\":\"history\",\"lines\":[";
    bool first = true;
    for (const auto &l : listeners_.history(Clock::now())) {
      h += (first ? "" : ",") + chatJson(l);
      first = false;
    }
    server_.sendText(id, h + "]}");
    for (int other : welcomed_)
      sendRoom(other);
    return;
  }

  if (t == "say" && welcomed_.count(id) && quiet_) {
    server_.sendText(id, "{\"t\":\"notice\",\"text\":\"The room has paused web chat.\"}");
    return;
  }
  if (t == "say" && welcomed_.count(id)) {
    std::string cleaned, why;
    if (!listeners_.say(id, m["text"], Clock::now(), cleaned, why)) {
      server_.sendText(id, "{\"t\":\"notice\",\"text\":" + json::quote(why) + "}");
      return;
    }
    if (cleaned.empty())
      return;
    const std::string who = listeners_.name(id);
    chatToListeners({who, cleaned, "listener", unixNow()});
    if (cfg_.toRoom)
      cfg_.toRoom("<" + who + "> " + cleaned);
  }
}

void Balcony::takeInbox() {
  std::vector<std::pair<std::string, std::string>> chat;
  std::vector<std::function<void()>> todo;
  std::vector<RosterChannel> roster;
  bool newRoster = false;
  {
    std::lock_guard<std::mutex> g(inboxMutex_);
    chat.swap(inboxChat_);
    todo.swap(inboxDo_);
    if (rosterPending_) {
      roster = roster_;
      newRoster = true;
      rosterPending_ = false;
    }
  }
  if (newRoster)
    applyRoster(roster);
  for (auto &f : todo)
    f();
  for (auto &c : chat)
    chatToListeners({displayName(c.first), c.second, "player", unixNow()});
}

void Balcony::boundary() {
  {
    std::lock_guard<std::mutex> g(inboxMutex_);
    current_ = std::move(inbox_);
    inbox_ = Pending{};
    inboxWhole_ = false;
  }
  inboxTaken_.notify_all();
  ++intervalIndex_;

  bool roomChanged = false;
  if (current_.bpm > 0 && (current_.bpm != bpm_ || current_.bpi != bpi_)) {
    bpm_ = current_.bpm;
    bpi_ = current_.bpi;
    broadcastText("{\"t\":\"config\",\"bpm\":" + std::to_string(bpm_) +
                  ",\"bpi\":" + std::to_string(bpi_) + "}");
  }
  samples_ = bpm_ > 0 ? (int)std::lround((double)kRate * 60.0 * bpi_ / bpm_) : kRate;
  pos_ = 0;

  for (const auto &kv : current_.channels) {
    if (live_)
      break;  // the room says who is here, not the audio
    auto it = players_.find(kv.first);
    if (it == players_.end()) {
      Player p;
      p.id = nextChannelId_++;
      p.key = kv.first;
      p.channelName = kv.second.channelName;
      it = players_.emplace(kv.first, p).first;
      roomChanged = true;
    }
    it->second.lastInterval = intervalIndex_;
  }
  for (auto it = players_.begin(); it != players_.end();) {
    if (!live_ && intervalIndex_ - it->second.lastInterval > kForgetAfterIntervals) {
      it = players_.erase(it);
      roomChanged = true;
    } else {
      ++it;
    }
  }
  if (roomChanged)
    for (int id : welcomed_)
      sendRoom(id);

  // Arrivals and departures, at most one line per interval (DESIGN §3).
  std::vector<std::string> joined, left;
  listeners_.takeChanges(joined, left);
  const std::string line = batchedLine(joined, left, listeners_.size());
  if (!line.empty()) {
    chatToListeners({"balcony", line, "bot", unixNow()});
    if (cfg_.toRoom)
      cfg_.toRoom(line);
  }
}

void Balcony::mixSpan(float *l, float *r, int n, std::vector<ChannelMeter> &meters) {
  std::vector<ChannelInput> inputs;
  for (const auto &kv : current_.channels) {
    const ChannelAudio &a = kv.second;
    if ((int)a.left.size() < pos_ + n)
      continue;
    const auto p = players_.find(kv.first);
    if (live_ && p == players_.end())
      continue;  // gone from the room: not heard from the next interval
    inputs.push_back({&kv.first, a.left.data() + pos_, a.right.data() + pos_,
                      p == players_.end() ? 0.0f : p->second.pan,
                      p == players_.end() || p->second.inStream});
  }
  mixer_.process(inputs, n, l, r, &meters);
}

void Balcony::produceBlock() {
  float l[kBlock], r[kBlock];
  std::vector<ChannelMeter> meters;
  int done = 0;
  while (done < kBlock) {
    if (pos_ >= samples_)
      boundary();
    const int n = std::min(kBlock - done, samples_ - pos_);
    mixSpan(l + done, r + done, n, meters);
    pos_ += n;
    done += n;
  }

  // One encode for everyone, then the same bytes to each listener.
  std::string msg;
  msg += (char)1;
  msg += (char)3;
  for (int k = 0; k < 4; ++k)
    msg += (char)((samplePos_ >> (8 * k)) & 0xFF);
  std::vector<uint8_t> packet;
  for (int f = 0; f < 3; ++f) {
    opus_.encode(l + f * OpusStream::kFrameSamples, r + f * OpusStream::kFrameSamples, packet);
    msg += (char)(packet.size() & 0xFF);
    msg += (char)(packet.size() >> 8);
    msg.append((const char *)packet.data(), packet.size());
  }
  msg += (char)std::min<size_t>(meters.size(), 255);
  for (size_t i = 0; i < meters.size() && i < 255; ++i) {
    const auto p = players_.find(meters[i].key);
    const int id = p == players_.end() ? 0 : p->second.id;
    msg += (char)(id & 0xFF);
    msg += (char)(id >> 8);
    msg += (char)meterByte(meters[i].peakDb);
    msg += (char)meterByte(meters[i].levelDb);
  }
  samplePos_ += kBlock;
  ++blocks_;

  std::vector<int> slow;
  for (int id : welcomed_) {
    if (server_.pendingBytes(id) > cfg_.maxPendingBytes) {
      slow.push_back(id);
      continue;
    }
    server_.sendBinary(id, msg.data(), msg.size());
  }
  for (int id : slow)
    server_.close(id, 1008, "too far behind");
}

void Balcony::run(const std::atomic<bool> &stop, const std::string &closingWhy) {
  using namespace std::chrono;
  auto next = steady_clock::now();
  const auto block = microseconds(1000000LL * kBlock / kRate);
  while (!stop) {
    const auto now = steady_clock::now();
    const int wait = (int)std::max<long long>(0, duration_cast<milliseconds>(next - now).count());
    server_.poll(std::min(wait, 20));
    takeInbox();
    if (!started_) {
      std::lock_guard<std::mutex> g(inboxMutex_);
      started_ = !inbox_.channels.empty() || clockWanted_;
      if (started_) {
        samples_ = 0;  // so the first block takes the first interval at once
        pos_ = 0;
        next = steady_clock::now();
      }
    }
    if (!started_)
      continue;
    // Behind by more than a second (the machine stalled): skip ahead rather
    // than burst a backlog at every listener.
    if (steady_clock::now() - next > seconds(1))
      next = steady_clock::now();
    while (steady_clock::now() >= next) {
      produceBlock();
      next += block;
    }
  }
  broadcastText("{\"t\":\"closing\",\"why\":" + json::quote(closingWhy) + "}");
  for (int id : std::vector<int>(welcomed_.begin(), welcomed_.end()))
    server_.close(id, 1001, "the stream has ended");
  for (int i = 0; i < 20; ++i)
    server_.poll(10);
}

}  // namespace balcony
