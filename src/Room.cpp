#include "Room.h"

#include "Version.h"

#include <BotLanguage.h>
#include <BotNames.h>
#include <chalkwalk/ninjam/Voting.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>

namespace balcony {

namespace {

constexpr int kRate = 48000;

// Lower case, apostrophes dropped ("don't" -> "dont"), everything else that is
// not a letter or digit a single space, and padded so phrases match on word
// boundaries: " leave me out ".
std::string plain(const std::string &text) {
  std::string out = " ";
  for (unsigned char c : text) {
    if (c == '\'' || c == 0x60)
      continue;
    if (std::isalnum(c) || c >= 0x80)
      out += (char)std::tolower(c);
    else if (out.back() != ' ')
      out += ' ';
  }
  if (out.back() != ' ')
    out += ' ';
  return out;
}

bool hasAny(const std::string &plainText, std::initializer_list<const char *> phrases) {
  for (const char *p : phrases)
    if (plainText.find(std::string(" ") + p + " ") != std::string::npos)
      return true;
  return false;
}

double nowSeconds() {
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

Room::Room(BotClient::Client &client, Balcony &balcony, Config cfg,
           std::function<void(const std::string &)> stop)
    : client_(client), balcony_(balcony), cfg_(std::move(cfg)), stop_(std::move(stop)) {}

std::string Room::joinMessage(const std::string &pageUrl) {
  // DESIGN §9.6.
  return "balcony here: people are listening to this room on the web" +
         (pageUrl.empty() ? std::string() : " at " + pageUrl) +
         ". Say \"balcony, leave me out\" to be left out of the stream, or \"balcony, "
         "leave\" to send the balcony away.";
}

void Room::say(const std::string &text) { client_.sendChat(text); }

void Room::onConnected() {
  me_ = cfg_.selfName ? cfg_.selfName() : std::string(kBotName);
  say(joinMessage(cfg_.pageUrl));
  balcony_.startClock();
  balcony_.setOpen(true);
  refreshRoster();
}

void Room::onDisconnected(const std::string &reason) {
  (void)reason;
  if (stopped_)
    return;
  stopped_ = true;
  balcony_.setOpen(false);
  stop_(sentAway_ ? "sent-away" : "server-closed");
}

void Room::onServerConfig(int bpm, int bpi) {
  bpm_ = bpm;
  bpi_ = bpi;
  balcony_.setTempo(bpm, bpi);
}

void Room::onUserInfoChange() { refreshRoster(); }

void Room::onRoomMembershipChange(const std::string &username, bool joined) {
  // Whoever arrives after us is told too, privately: an audience nobody was
  // told about is anti-goal A4, and the room heard it once already.
  if (joined && username != me_ && !stopped_)
    client_.sendPrivate(username, joinMessage(cfg_.pageUrl));
  // Leaving the room ends a player's opt-out (DESIGN §3): next time they come
  // they are heard, and told so by the join message's own reminder.
  if (!joined && leftOut_.erase(username))
    refreshRoster();
}

void Room::refreshRoster() {
  std::vector<Balcony::RosterChannel> roster;
  for (const auto &peer : client_.peers()) {
    if (peer.username == me_)
      continue;
    const bool in = leftOut_.count(peer.username) == 0;
    for (const auto &c : peer.channels) {
      // Not even received while they are out: what is not heard cannot leak.
      if (c.recvEnabled != in)
        client_.setRecv(peer.username, c.index, in);
      Balcony::RosterChannel rc;
      rc.key = {peer.username, c.index};
      rc.name = c.name;
      rc.pan = c.pan;
      rc.inStream = in;
      channelNames_[rc.key] = c.name;
      roster.push_back(std::move(rc));
    }
  }
  balcony_.setRoster(std::move(roster));
}

void Room::onIntervalReceived(const std::string &username, int channelIndex, const float *left,
                              const float *right, int numSamples) {
  if (leftOut_.count(username) || bpm_ <= 0 || bpi_ <= 0 || numSamples <= 0)
    return;
  // Exactly one interval long at our rate: the resampler runs a few samples
  // short or long of it, and the grid wants whole intervals.
  const int n = (int)std::lround((double)kRate * 60.0 * bpi_ / bpm_);
  ChannelAudio a;
  a.key = {username, channelIndex};
  const auto name = channelNames_.find(a.key);
  a.channelName = name == channelNames_.end() ? std::string() : name->second;
  const int m = std::min(n, numSamples);
  a.left.assign(left, left + m);
  a.right.assign(right ? right : left, (right ? right : left) + m);
  a.left.resize((size_t)n, 0.0f);
  a.right.resize((size_t)n, 0.0f);
  balcony_.deliver(std::move(a), bpm_, bpi_);
}

BotAddress::Room Room::addressRoom() const {
  BotAddress::Room room;
  auto add = [&room](const std::string &username, const std::string &channel) {
    BotAddress::Participant p;
    p.username = username;
    // By the name people type: `alice`, not `alice@1.2.3.x` (jambot's
    // handles come from bot names, which carry no address).
    p.handle = BotNames::handleOf(displayName(username));
    p.channel = channel;
    p.isBot = BotNames::looksLikeBot(username);
    room.participants.push_back(p);
  };
  add(me_, "");
  const auto peers = client_.peers();
  for (const auto &m : client_.members()) {
    if (m.username == me_)
      continue;
    std::string channel;
    for (const auto &p : peers)
      if (p.username == m.username && !p.channels.empty())
        channel = p.channels.front().name;
    add(m.username, channel);
  }
  room.resolveHandles();
  return room;
}

Room::Ask Room::readAsk(const BotAddress::Room &room, const std::string &me,
                        const std::string &sender, const std::string &text, bool isPrivate,
                        BotAddress::Attention &attention) {
  using BotAddress::Address;
  BotAddress::Incoming msg;
  msg.sender = sender;
  msg.text = text;
  msg.isPrivate = isPrivate;
  msg.at = nowSeconds();
  const Address a = BotAddress::classify(room, me, msg, attention);
  // Leaving works as it does for every jambot: "leave" alone, or addressed.
  if (a == Address::PartAll || a == Address::PartMe)
    return Ask::SendAway;
  if (a != Address::Named && a != Address::Private && a != Address::Continuation)
    return Ask::None;

  const std::string rest = plain(BotAddress::withoutAddress(room, me, text));
  // Out before back: "dont stream me" contains "stream me".
  if (hasAny(rest, {"leave me out", "count me out", "take me out", "opt me out", "opt out",
                    "dont stream me", "do not stream me", "stop streaming me", "mute me",
                    "exclude me", "drop me", "skip me", "leave out my", "without me"}))
    return Ask::LeaveMeOut;
  if (hasAny(rest, {"put me back", "count me in", "opt me in", "opt in", "include me",
                    "stream me", "add me back", "unmute me", "back in", "me back"}))
    return Ask::PutMeBack;

  // The ways people actually ask a bot to go, as jambot reads them -- but only
  // asked, not wondered about or forbidden.
  const auto reading = BotLanguage::read(BotAddress::withoutAddress(room, me, text));
  if (reading.intent == BotLanguage::Intent::Leave && !reading.ambiguous && !reading.negated &&
      !reading.proposal && !(reading.question && !reading.request))
    return Ask::SendAway;
  return Ask::None;
}

bool Room::isAdmin(const std::string &username) const {
  // Logged in only: an anonymous name carries @ip and could be anybody's.
  if (username.find('@') != std::string::npos)
    return false;
  return std::find(cfg_.admins.begin(), cfg_.admins.end(), username) != cfg_.admins.end();
}

void Room::onChatMessage(const std::string &type, const std::string &username,
                         const std::string &text) {
  if (stopped_)
    return;
  if (type == "MSG" && username.empty()) {
    considerVote(text);  // "[voting system] ..." comes from the server itself
    return;
  }
  if ((type != "MSG" && type != "PRIVMSG") || username.empty() || username == me_)
    return;
  const std::string who = displayName(username);

  // The room's conversation reaches the listeners, commands and all; private
  // words to the bot do not.
  if (type == "MSG")
    balcony_.roomChat(username, text);

  if (isAdmin(username) && adminCommand(username, text, type == "PRIVMSG"))
    return;

  switch (readAsk(addressRoom(), me_, username, text, type == "PRIVMSG", attention_)) {
  case Ask::LeaveMeOut:
    leftOut_.insert(username);
    refreshRoster();
    say(who + ", you are out of the stream from the next interval; the listeners still see "
              "your chat. Say \"balcony, put me back\" to be heard again.");
    break;
  case Ask::PutMeBack:
    if (leftOut_.erase(username))
      refreshRoster();
    say(who + ", you are in the stream again.");
    break;
  case Ask::SendAway:
    sentAway_ = true;
    stopped_ = true;
    say("balcony leaving: the room asked me to go. The listeners have been told.");
    balcony_.setOpen(false);
    stop_("sent-away");
    break;
  case Ask::None:
    break;
  }
}

// The stock server refuses any `!` line in the room but `!vote` and `!topic`
// ("Unknown !command") and never relays it, so in the room an admin addresses
// the balcony in words; privately, `!kick` and the rest work as written.
bool Room::adminCommand(const std::string &username, const std::string &text, bool isPrivate) {
  std::string rest = text;
  if (!isPrivate) {
    const std::string p = plain(text);
    const std::string me = " " + plain(displayName(me_)).substr(1);
    if (p.rfind(me, 0) != 0)
      return false;  // not addressed to us
    // Drop the leading name as typed ("Balcony," "balcony:" ...).
    const std::string name = displayName(me_);
    size_t i = 0;
    while (i < text.size() && text[i] == ' ')
      ++i;
    rest = text.substr(std::min(text.size(), i + name.size()));
  }
  std::string r = rest;
  while (!r.empty() && (r[0] == ' ' || r[0] == '!' || r[0] == ',' || r[0] == ':'))
    r.erase(0, 1);
  while (!r.empty() && (r.back() == ' ' || r.back() == '.'))
    r.pop_back();
  const std::string lower = plain(r);
  if (lower.rfind(" kick ", 0) == 0 && r.size() > 5) {
    std::string name = r.substr(5);
    while (!name.empty() && name[0] == ' ')
      name.erase(0, 1);
    balcony_.kick(name);
    return true;
  }
  if (lower == " quiet " || lower == " chat off ") {
    balcony_.setQuiet(true);
    return true;
  }
  if (lower == " unquiet " || lower == " chat on ") {
    balcony_.setQuiet(false);
    return true;
  }
  (void)username;
  return false;
}

void Room::considerVote(const std::string &text) {
  // DESIGN §4: back a vote only when the humans alone have carried it, so the
  // outcome is what it would have been without us. Hidden, we are not counted
  // and never vote.
  namespace voting = chalkwalk::ninjam::voting;
  if (cfg_.hidden)
    return;
  const auto line = voting::parseLine(text);
  if (!line.valid)
    return;
  if (line.settled) {
    backing_.clear();
    return;
  }
  if (line.value <= 0 || line.required <= 0)
    return;
  const std::string what = std::string(line.isBpm ? "bpm " : "bpi ") + std::to_string(line.value);
  if (what == backing_)
    return;
  int users = 1, humans = 0;  // we are counted, and not in our own member list
  for (const auto &m : client_.members()) {
    if (m.username == me_)
      continue;
    ++users;
    if (!BotNames::looksLikeBot(m.username))
      ++humans;
  }
  if (humans <= 0)
    return;
  // With the room's threshold known, the humans' own requirement is exact;
  // without it, it is the most it could be, so we err toward not helping --
  // which can leave a vote short that would have carried without us. On our
  // own server the answer is `H` (DESIGN §4), and then none of this runs.
  const int neededAlone = cfg_.voteThreshold > 0
                              ? voting::votesRequired(humans, cfg_.voteThreshold)
                              : voting::mostVotesNeededAlone(humans, users, line.required);
  if (line.votes < neededAlone)
    return;
  if (line.required - line.votes <= 0)
    return;
  backing_ = what;
  say("!vote " + what);
}

}  // namespace balcony
