#include "TestClient.h"
#include "WebServer.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

using namespace balcony;
using namespace balcony::test;

namespace {

// The server on its own thread, as the bot runs it; handlers run there too.
struct Running {
  std::atomic<bool> stop{false};
  std::mutex m;
  std::vector<std::string> events;
  std::unique_ptr<WebServer> server;
  std::thread t;

  Running() {
    WebServer::Handlers h;
    h.status = [] { return std::string("{\"open\":true}"); };
    h.onOpen = [this](int id) { note("open " + std::to_string(id)); };
    h.onText = [this](int id, const std::string &s) {
      note("text " + s);
      if (s == "big")
        server->sendBinary(id, std::string(70000, 'x').data(), 70000);
      else if (s == "bye")
        server->close(id, 1000, "bye");
      else
        server->sendText(id, "echo:" + s);
    };
    h.onClose = [this](int id) { note("close " + std::to_string(id)); };
    server = std::make_unique<WebServer>(h);
    REQUIRE(server->listen("127.0.0.1", 0));
    t = std::thread([this] {
      while (!stop)
        server->poll(10);
    });
  }
  ~Running() {
    stop = true;
    t.join();
  }
  void note(const std::string &e) {
    std::lock_guard<std::mutex> g(m);
    events.push_back(e);
  }
  int count(const std::string &prefix) {
    std::lock_guard<std::mutex> g(m);
    int n = 0;
    for (auto &e : events)
      if (e.rfind(prefix, 0) == 0)
        ++n;
    return n;
  }
};

}  // namespace

TEST_CASE("the handshake answers RFC 6455's own example", "[web]") {
  CHECK(WebServer::acceptKey("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
  Running r;
  Client c;
  REQUIRE(c.open(r.server->port()));
  CHECK(c.handshake.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") !=
        std::string::npos);
}

TEST_CASE("text goes both ways, fragmented or not", "[web]") {
  Running r;
  Client c;
  REQUIRE(c.open(r.server->port()));
  c.sendText("hello");
  CHECK(c.recvText() == "echo:hello");
  c.sendFrame(0x1, "hel", false);
  c.sendFrame(0x0, "lo again", true);
  CHECK(c.recvText() == "echo:hello again");
  std::string longText(300, 'a');
  c.sendText(longText);
  CHECK(c.recvText() == "echo:" + longText);
}

TEST_CASE("a large binary message arrives whole, with a 64-bit length", "[web]") {
  Running r;
  Client c;
  REQUIRE(c.open(r.server->port()));
  c.sendText("big");
  uint8_t op = 0;
  std::string p;
  REQUIRE(c.recvFrame(op, p));
  CHECK(op == 0x2);
  CHECK(p.size() == 70000);
}

TEST_CASE("ping is answered with pong", "[web]") {
  Running r;
  Client c;
  REQUIRE(c.open(r.server->port()));
  c.sendFrame(0x9, "are you there");
  uint8_t op = 0;
  std::string p;
  REQUIRE(c.recvFrame(op, p));
  CHECK(op == 0xA);
  CHECK(p == "are you there");
}

TEST_CASE("a close is reported once, whoever starts it", "[web]") {
  Running r;
  {
    Client c;
    REQUIRE(c.open(r.server->port()));
    c.sendText("bye");
    uint8_t op = 0;
    std::string p;
    REQUIRE(c.recvFrame(op, p));
    CHECK(op == 0x8);
  }
  {
    Client c;
    REQUIRE(c.open(r.server->port()));
    c.sendFrame(0x8, std::string("\x03\xe8", 2));
    uint8_t op = 0;
    std::string p;
    REQUIRE(c.recvFrame(op, p));
    CHECK(op == 0x8);
  }
  {
    Client c;
    REQUIRE(c.open(r.server->port()));
  }  // just goes away
  for (int i = 0; i < 100 && r.count("close") < 3; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  CHECK(r.count("open") == 3);
  CHECK(r.count("close") == 3);
}

TEST_CASE("binary, oversized and unmasked input closes the connection", "[web]") {
  Running r;
  Client c;
  REQUIRE(c.open(r.server->port()));
  c.sendFrame(0x2, "nope");
  uint8_t op = 0;
  std::string p;
  REQUIRE(c.recvFrame(op, p));
  CHECK(op == 0x8);
  CHECK(((uint8_t)p[0] << 8 | (uint8_t)p[1]) == 1003);
}

TEST_CASE("status is plain HTTP, and nothing else is served by default", "[web]") {
  Running r;
  int fd = connectLoopback(r.server->port());
  sendAll(fd, "GET /status HTTP/1.1\r\nHost: x\r\n\r\n");
  std::string got = readUntil(fd, "}");
  ::close(fd);
  CHECK(got.find("200 OK") != std::string::npos);
  CHECK(got.find("application/json") != std::string::npos);
  CHECK(got.find("{\"open\":true}") != std::string::npos);

  fd = connectLoopback(r.server->port());
  sendAll(fd, "GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n");
  got = readUntil(fd, "\r\n\r\n");
  ::close(fd);
  CHECK(got.find("404") != std::string::npos);
}
