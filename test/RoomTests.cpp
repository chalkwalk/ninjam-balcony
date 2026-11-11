// What the room can ask of the balcony, and the vote it may back. The live
// path (a real ninjamsrv) is exercised by hand; see .autonomy/PLAN.md item 6.

#include "Room.h"

#include <catch2/catch_test_macros.hpp>

using balcony::Room;

namespace {

BotAddress::Room roomOf(std::initializer_list<const char *> names) {
  BotAddress::Room room;
  for (const char *n : names) {
    BotAddress::Participant p;
    p.username = n;
    std::string handle = balcony::displayName(n);
    for (auto &c : handle)
      c = (char)std::tolower((unsigned char)c);
    p.handle = handle;
    room.participants.push_back(p);
  }
  room.resolveHandles();
  return room;
}

Room::Ask ask(const std::string &text, const std::string &from = "alice@1.2.3.x",
              bool isPrivate = false) {
  const auto room = roomOf({"balcony@1.2.3.x", "alice@1.2.3.x", "bob@1.2.3.x"});
  BotAddress::Attention attention;
  return Room::readAsk(room, "balcony@1.2.3.x", from, text, isPrivate, attention);
}

// The client as the room shows it; records what the bot says.
struct FakeClient : BotClient::Client {
  std::vector<BotClient::Member> who;
  std::vector<std::string> said, privately;
  void addListener(BotClient::Listener *) override {}
  void removeListener(BotClient::Listener *) override {}
  void setSampleRate(double) override {}
  void setChannels(const std::vector<std::string> &) override {}
  void setDefaultRecvEnabled(bool) override {}
  void connect(const std::string &, int, const std::string &, const std::string &) override {}
  void disconnect() override {}
  bool isConnected() const override { return true; }
  std::vector<BotClient::Member> members() const override { return who; }
  std::vector<BotClient::Peer> peers() const override { return {}; }
  void setRecv(const std::string &, int, bool) override {}
  void sendChat(const std::string &t) override { said.push_back(t); }
  void sendPrivate(const std::string &, const std::string &t) override { privately.push_back(t); }
  void transmit(const float *, const float *, int) override {}
  std::unique_ptr<BotClient::Timer> createTimer(std::function<void()>) override { return nullptr; }
};

}  // namespace

TEST_CASE("A player can leave themselves out and come back", "[room]") {
  CHECK(ask("balcony, leave me out") == Room::Ask::LeaveMeOut);
  CHECK(ask("Balcony: please leave me out") == Room::Ask::LeaveMeOut);
  CHECK(ask("balcony don't stream me") == Room::Ask::LeaveMeOut);
  CHECK(ask("balcony, count me out") == Room::Ask::LeaveMeOut);
  CHECK(ask("balcony, put me back") == Room::Ask::PutMeBack);
  CHECK(ask("balcony stream me again") == Room::Ask::PutMeBack);
  CHECK(ask("leave me out", "alice@1.2.3.x", true) == Room::Ask::LeaveMeOut);
}

TEST_CASE("Sending the balcony away works as it does for every jambot", "[room]") {
  CHECK(ask("balcony, leave") == Room::Ask::SendAway);
  CHECK(ask("leave") == Room::Ask::SendAway);
  CHECK(ask("go away") == Room::Ask::SendAway);
  CHECK(ask("balcony, please leave now") == Room::Ask::SendAway);
}

TEST_CASE("What is not for the balcony asks nothing of it", "[room]") {
  CHECK(ask("leave me out of this argument") == Room::Ask::None);  // nobody named
  CHECK(ask("bob, leave me out") == Room::Ask::None);
  CHECK(ask("balcony, don't leave") == Room::Ask::None);
  CHECK(ask("nice groove") == Room::Ask::None);
  CHECK(ask("who is the balcony?") == Room::Ask::None);
}

TEST_CASE("The join message names the page and the two commands", "[room]") {
  const auto m = Room::joinMessage("https://ninjam.example/");
  CHECK(m.find("https://ninjam.example/") != std::string::npos);
  CHECK(m.find("balcony, leave me out") != std::string::npos);
  CHECK(m.find("balcony, leave\"") != std::string::npos);
}

TEST_CASE("The balcony backs a vote only when the humans alone carried it", "[room]") {
  balcony::Balcony::Config bc;
  bc.port = 0;
  balcony::Balcony b(bc);
  FakeClient client;
  client.who = {{"alice@1.2.3.x", 1}, {"bob@1.2.3.x", 1}, {"carol", 1}};
  std::string stoppedWhy;
  Room::Config rc;
  rc.voteThreshold = 75;  // 3 humans need 2; with us counted, 4 need 3
  rc.selfName = [] { return std::string("balcony@1.2.3.x"); };
  Room room(client, b, rc, [&](const std::string &w) { stoppedWhy = w; });
  room.onConnected();
  client.said.clear();

  room.onChatMessage("MSG", "",
                     "[voting system] leading candidate: 1/3 votes for 100 BPM [each vote "
                     "expires in 60s]");
  CHECK(client.said.empty());
  room.onChatMessage("MSG", "",
                     "[voting system] leading candidate: 2/3 votes for 100 BPM [each vote "
                     "expires in 60s]");
  REQUIRE(client.said.size() == 1);
  CHECK(client.said[0] == "!vote bpm 100");
  // Our own vote comes back as another line; we do not vote twice.
  room.onChatMessage("MSG", "",
                     "[voting system] leading candidate: 2/3 votes for 100 BPM [each vote "
                     "expires in 60s]");
  CHECK(client.said.size() == 1);

  SECTION("hidden, it never votes") {
    Room::Config hidden = rc;
    hidden.hidden = true;
    FakeClient c2;
    c2.who = client.who;
    Room r2(c2, b, hidden, [](const std::string &) {});
    r2.onChatMessage("MSG", "",
                     "[voting system] leading candidate: 2/3 votes for 100 BPM [each vote "
                     "expires in 60s]");
    CHECK(c2.said.empty());
  }
}

TEST_CASE("Admin commands are taken from listed logged-in names only", "[room]") {
  balcony::Balcony::Config bc;
  bc.port = 0;
  balcony::Balcony b(bc);
  FakeClient client;
  Room::Config rc;
  rc.admins = {"carol"};
  rc.selfName = [] { return std::string("balcony@1.2.3.x"); };
  std::string why;
  Room room(client, b, rc, [&](const std::string &w) { why = w; });
  room.onConnected();
  // An anonymous name that matches the list is still anonymous: not an admin,
  // and "balcony, quiet" from them is not a request to leave either.
  room.onChatMessage("MSG", "carol@1.2.3.x", "balcony, quiet");
  CHECK(why.empty());
  CHECK(client.privately.empty());
  // A listed admin's command in the room is not acted on; they are told,
  // privately, to send it as a private message.
  room.onChatMessage("MSG", "carol", "balcony, quiet");
  CHECK(why.empty());
  REQUIRE(client.privately.size() == 1);
  CHECK(client.privately[0].find("private message") != std::string::npos);
  client.privately.clear();
  room.onChatMessage("PRIVMSG", "carol", "!kick Pat");
  CHECK(why.empty());
  CHECK(client.privately.empty());
  // A late arrival is told privately what is going on.
  room.onRoomMembershipChange("dave@1.2.3.x", true);
  REQUIRE(client.privately.size() == 1);
  CHECK(client.privately[0].find("listening") != std::string::npos);
  // Being sent away is reported once, as "sent-away".
  room.onChatMessage("MSG", "dave@1.2.3.x", "balcony, leave");
  CHECK(why == "sent-away");
}
