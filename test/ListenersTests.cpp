#include "Listeners.h"

#include <catch2/catch_test_macros.hpp>

using namespace balcony;
using namespace std::chrono_literals;

TEST_CASE("names: what a listener may be called", "[listeners]") {
  CHECK(validName("Alice"));
  CHECK(validName("al_ice-2.0"));
  CHECK(validName("Mary Jane"));
  CHECK(validName("Zoë"));
  CHECK(validName("小明"));
  CHECK(!validName(""));
  CHECK(!validName(" Alice"));
  CHECK(!validName("Alice "));
  CHECK(!validName("Mary  Jane"));
  CHECK(!validName("alice@10.0.0.x"));
  CHECK(!validName("<balcony>"));
  CHECK(!validName(std::string(25, 'a')));
  CHECK(validName(std::string(24, 'a')));
}

TEST_CASE("a name is unique among listeners and players, ignoring case",
          "[listeners]") {
  Listeners l;
  CHECK(l.join(1, "Alice", {"jgorbe"}) == Listeners::Join::Ok);
  CHECK(l.join(2, "alice", {"jgorbe"}) == Listeners::Join::NameTaken);
  CHECK(l.join(3, "JGorbe", {"jgorbe"}) == Listeners::Join::NameTaken);
  CHECK(l.join(4, "bad@name", {}) == Listeners::Join::NameInvalid);
  l.leave(1);
  CHECK(l.join(5, "alice", {}) == Listeners::Join::Ok);
}

TEST_CASE("lines are cleaned and capped by characters, not bytes", "[listeners]") {
  CHECK(cleanLine("  hello \n\t world  ", 200) == "hello world");
  CHECK(cleanLine(std::string("bell\x07here"), 200) == "bell here");
  CHECK(cleanLine("abcdef", 3) == "abc");
  CHECK(cleanLine("\xc3\xa9\xc3\xa9\xc3\xa9", 2) == "\xc3\xa9\xc3\xa9");
  CHECK(cleanLine("   ", 200).empty());
}

TEST_CASE("three lines, then a few every ten seconds", "[listeners]") {
  Listeners l;
  REQUIRE(l.join(1, "Alice", {}) == Listeners::Join::Ok);
  auto t = Clock::now();
  std::string out, why;
  CHECK(l.say(1, "one", t, out, why));
  CHECK(l.say(1, "two", t, out, why));
  CHECK(l.say(1, "three", t, out, why));
  CHECK(!l.say(1, "four", t, out, why));
  CHECK(!why.empty());
  CHECK(!l.say(1, "still", t + 2s, out, why));
  CHECK(l.say(1, "later", t + 4s, out, why));
  CHECK(out == "later");
  CHECK(!l.say(2, "not here", t, out, why));
}

TEST_CASE("a kicked name is blocked for the session", "[listeners]") {
  Listeners l;
  REQUIRE(l.join(7, "Troll", {}) == Listeners::Join::Ok);
  CHECK(l.kick("troll") == 7);
  l.leave(7);
  CHECK(l.join(8, "TROLL", {}) == Listeners::Join::Blocked);
}

TEST_CASE("history keeps the last minutes, and at most so many lines",
          "[listeners]") {
  Listeners::Config c;
  c.historyLines = 3;
  Listeners l(c);
  auto t = Clock::now();
  for (int i = 0; i < 5; ++i)
    l.remember({"p", "line " + std::to_string(i), "player", 0}, t + std::chrono::seconds(i));
  auto h = l.history(t + 5s);
  REQUIRE(h.size() == 3);
  CHECK(h.front().text == "line 2");
  CHECK(l.history(t + 400s).empty());
}

TEST_CASE("arrivals are batched, and a quick drop and return cancels out",
          "[listeners]") {
  Listeners l;
  l.join(1, "Alice", {});
  l.join(2, "Bob", {});
  std::vector<std::string> j, g;
  l.takeChanges(j, g);
  CHECK(batchedLine(j, g, l.size()) == "Alice and Bob joined the balcony (2 listening)");

  l.leave(1);
  l.join(3, "Alice", {});  // back within the same interval
  l.join(4, "Carol", {});
  l.leave(2);
  l.takeChanges(j, g);
  CHECK(batchedLine(j, g, l.size()) == "Carol joined the balcony, Bob left (2 listening)");

  l.takeChanges(j, g);
  CHECK(batchedLine(j, g, l.size()).empty());
}
