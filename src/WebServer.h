#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>

// The bot's one door to the web (DESIGN §9.2): a first-party HTTP/1.1 and
// WebSocket (RFC 6455) server, one thread, `poll()` over non-blocking sockets.
//
// Loopback only in production: Caddy in front terminates TLS and HTTP/2 and
// proxies `/status` and `/ws` here. It serves:
//   GET /status   -> the JSON the `status` handler returns
//   GET /ws       -> a WebSocket
//   GET /<file>   -> a file under the static root, if one is set (for running
//                    the page locally without Caddy; off in production)
// and nothing else. No extensions (no permessage-deflate: audio does not
// compress), no TLS, no keep-alive for plain HTTP: one request, one answer.
//
// From a page it takes text messages up to 4 KiB, ping, pong and close; a
// binary message or anything bigger closes the connection. A listener that
// cannot keep up is the caller's to drop: `pendingBytes` says how far behind.

namespace balcony {

class WebServer {
public:
  struct Handlers {
    std::function<std::string()> status;
    std::function<void(int id)> onOpen;
    std::function<void(int id, const std::string &text)> onText;
    std::function<void(int id)> onClose;
  };

  explicit WebServer(Handlers h) : h_(std::move(h)) {}
  ~WebServer();
  WebServer(const WebServer &) = delete;
  WebServer &operator=(const WebServer &) = delete;

  // `port` 0 picks a free one; `port()` then says which.
  bool listen(const std::string &host, int port, std::string *error = nullptr);
  int port() const { return port_; }

  void setStaticRoot(const std::string &dir) { staticRoot_ = dir; }

  // Wait up to `timeoutMs` for something to do, then do it all.
  void poll(int timeoutMs);

  void sendText(int id, const std::string &text);
  void sendBinary(int id, const void *data, size_t size);
  void close(int id, uint16_t code = 1000, const std::string &reason = {});

  size_t pendingBytes(int id) const;
  size_t connectionCount() const;

  // Exposed for the tests: the handshake's accept value for a client key.
  static std::string acceptKey(const std::string &clientKey);

private:
  enum class State { Http, WebSocket, Closing };
  struct Conn {
    int fd = -1;
    State state = State::Http;
    std::string in, out;
    std::string message;  // a fragmented message being assembled
    bool closeAfterWrite = false;
    bool open = false;  // a WebSocket whose close has not been reported yet
  };

  void accept();
  void readFrom(int id, Conn &c);
  void writeTo(int id, Conn &c);
  void handleHttp(int id, Conn &c);
  void handleFrames(int id, Conn &c);
  void frame(Conn &c, uint8_t opcode, const void *data, size_t size);
  void drop(int id);
  void reportClosed(int id, Conn &c);
  void respond(Conn &c, int code, const std::string &type, const std::string &body);

  Handlers h_;
  int listenFd_ = -1;
  int port_ = 0;
  int nextId_ = 1;
  std::string staticRoot_;
  std::map<int, Conn> conns_;
};

}  // namespace balcony
