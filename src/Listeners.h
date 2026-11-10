#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

// The people in the balcony, and what they may say (DESIGN §3, §5, §9.5).
//
// No sockets and no clock of its own, so every rule here is testable on its
// own: the caller says who connected, what they sent and what time it is, and
// gets back what to tell whom.

namespace balcony {

using Clock = std::chrono::steady_clock;

struct ChatLine {
  std::string from;
  std::string text;
  std::string kind;  // "player", "listener", "room", "bot"
  long long at = 0;  // unix seconds
};

// Name rules: 1-24 characters; letters (any script), digits, `_ - .` and
// single inner spaces; nothing that could pass for a Ninjam name with an
// address (`@`) or for a chat prefix (`<`, `>`).
bool validName(const std::string &name);

// Make a listener's line safe to relay: control characters dropped, runs of
// space folded, trimmed, cut to `maxChars` characters (not bytes).
std::string cleanLine(const std::string &text, size_t maxChars);

// Case-insensitive, for names (ASCII folding; other scripts compare exactly).
std::string foldName(const std::string &name);

class Listeners {
public:
  struct Config {
    int burst = 3;
    double linesPerSecond = 0.3;  // 3 lines per 10 s
    size_t maxChars = 200;
    std::chrono::seconds history{300};
    size_t historyLines = 100;
  };

  explicit Listeners(Config c);
  Listeners() : Listeners(Config{}) {}

  enum class Join { Ok, NameTaken, NameInvalid, Blocked };
  // `players` are the display names of the players in the room now.
  Join join(int id, const std::string &name, const std::set<std::string> &players);
  void leave(int id);
  bool has(int id) const { return byId_.count(id) != 0; }
  std::string name(int id) const;
  std::vector<std::string> names() const;
  std::vector<int> ids() const;
  size_t size() const { return byId_.size(); }

  // A listener's line: false if over the rate limit (`why` says so), else the
  // cleaned text to relay (possibly empty: nothing left to say).
  bool say(int id, const std::string &text, Clock::time_point now,
           std::string &cleaned, std::string &why);

  // `!kick name`: drops them and blocks the name for the session.
  // Returns the id to disconnect, or -1.
  int kick(const std::string &name);

  // The room's conversation, kept for arrivals.
  void remember(const ChatLine &line, Clock::time_point now);
  std::vector<ChatLine> history(Clock::time_point now) const;

  // Who came and went since the last call, for one batched line per interval
  // (DESIGN §3). A listener who came and went in the same batch is in
  // neither list; one who went and came back is in neither either.
  void takeChanges(std::vector<std::string> &joined, std::vector<std::string> &left);

private:
  struct Person {
    std::string name;
    double tokens = 0.0;
    Clock::time_point last{};
  };

  Config cfg_;
  std::map<int, Person> byId_;
  std::set<std::string> blocked_;  // folded names
  std::deque<std::pair<Clock::time_point, ChatLine>> history_;
  std::map<std::string, int> change_;  // folded name -> +1 joined / -1 left
  std::map<std::string, std::string> changeName_;
};

// "Alice and Bob joined the balcony, Carol left (7 listening)", or empty.
std::string batchedLine(const std::vector<std::string> &joined,
                        const std::vector<std::string> &left, size_t listening);

}  // namespace balcony
