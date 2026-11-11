#pragma once

#include "Balcony.h"

#include <BotAddress.h>
#include <BotClient.h>

#include <atomic>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

// The live room, as the balcony hears it (DESIGN §3-§5, §7).
//
// A `BotClient::Listener`: the client's one callback thread calls in, and
// everything is handed to the `Balcony` (which runs on its own thread) through
// its thread-safe calls. Nothing here touches a socket.
//
// What the room can ask of the balcony:
//   - any player: "balcony, leave me out" / "balcony, put me back" (their own
//     channels, until they leave the room); "balcony, leave", or a bare
//     "leave" as every jambot takes it, sends the balcony away for the jam;
//   - an admin (a logged-in name on the list), by private message only:
//     `!kick <listener>`, `!quiet`, `!unquiet`. Tried in the room, the admin
//     is told privately how to send it.
// And on a server where the bot is counted in votes, it backs a tempo vote
// only when the humans alone have already carried it (DESIGN §4).

namespace balcony {

class Room : public BotClient::Listener {
public:
  struct Config {
    std::string pageUrl;               // said in the join message
    std::vector<std::string> admins;   // logged-in names, without @ip
    bool hidden = false;  // the server gives us `H`: we are not counted, so never vote
    int voteThreshold = 0;  // the server's SetVotingThreshold, when known (percent)
    // The name the server knows us by (`balcony@1.2.3.x` when anonymous),
    // asked for when needed: it is only known once connected.
    std::function<std::string()> selfName;
  };

  // `stop` is called once, from the client's thread, with why the stream ends
  // ("sent-away" or "server-closed"); the caller stops the balcony.
  Room(BotClient::Client &client, Balcony &balcony, Config cfg,
       std::function<void(const std::string &why)> stop);

  void onConnected() override;
  void onDisconnected(const std::string &reason) override;
  void onServerConfig(int bpm, int bpi) override;
  void onUserInfoChange() override;
  void onRoomMembershipChange(const std::string &username, bool joined) override;
  void onIntervalReceived(const std::string &username, int channelIndex, const float *left,
                          const float *right, int numSamples) override;
  void onChatMessage(const std::string &type, const std::string &username,
                     const std::string &text) override;

  // What a player's line asks of the balcony, if anything. Exposed for tests.
  enum class Ask { None, LeaveMeOut, PutMeBack, SendAway };
  static Ask readAsk(const BotAddress::Room &room, const std::string &me,
                     const std::string &sender, const std::string &text, bool isPrivate,
                     BotAddress::Attention &attention);

  static std::string joinMessage(const std::string &pageUrl);

private:
  void say(const std::string &text);
  void refreshRoster();
  BotAddress::Room addressRoom() const;
  bool isAdmin(const std::string &username) const;
  void considerVote(const std::string &text);
  bool adminCommand(const std::string &username, const std::string &text, bool isPrivate);

  BotClient::Client &client_;
  Balcony &balcony_;
  Config cfg_;
  std::function<void(const std::string &)> stop_;
  std::string me_;
  int bpm_ = 0, bpi_ = 0;
  std::set<std::string> leftOut_;  // usernames who asked to be left out
  std::map<ChannelKey, std::string> channelNames_;
  BotAddress::Attention attention_;
  bool sentAway_ = false;
  bool stopped_ = false;
  std::string backing_;  // the vote we last backed, "bpm 120"
};

}  // namespace balcony
