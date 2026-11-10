#include "WebServer.h"

#include <chalkwalk/ninjam/Sha1.h>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace balcony {

namespace {

constexpr size_t kMaxRequest = 8192;
constexpr size_t kMaxMessage = 4096;

std::string base64(const uint8_t *p, size_t n) {
  static const char *t =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)p[i] << 16;
    if (i + 1 < n) v |= (uint32_t)p[i + 1] << 8;
    if (i + 2 < n) v |= p[i + 2];
    o += t[(v >> 18) & 63];
    o += t[(v >> 12) & 63];
    o += i + 1 < n ? t[(v >> 6) & 63] : '=';
    o += i + 2 < n ? t[v & 63] : '=';
  }
  return o;
}

std::string lower(std::string s) {
  for (auto &c : s)
    c = (char)std::tolower((unsigned char)c);
  return s;
}

std::string trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t"), e = s.find_last_not_of(" \t\r");
  return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}

void nonBlocking(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

const char *contentType(const std::string &path) {
  auto ends = [&](const char *x) {
    size_t n = std::strlen(x);
    return path.size() >= n && path.compare(path.size() - n, n, x) == 0;
  };
  if (ends(".html")) return "text/html; charset=utf-8";
  if (ends(".js")) return "text/javascript; charset=utf-8";
  if (ends(".css")) return "text/css; charset=utf-8";
  if (ends(".svg")) return "image/svg+xml";
  if (ends(".ics")) return "text/calendar";
  return "application/octet-stream";
}

}  // namespace

std::string WebServer::acceptKey(const std::string &clientKey) {
  const std::string s = clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  chalkwalk::ninjam::Sha1 sha;
  sha.add(s.data(), (int)s.size());
  uint8_t digest[20];
  sha.result(digest);
  return base64(digest, 20);
}

WebServer::~WebServer() {
  for (auto &kv : conns_)
    ::close(kv.second.fd);
  if (listenFd_ >= 0)
    ::close(listenFd_);
}

bool WebServer::listen(const std::string &host, int port, std::string *error) {
  listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd_ < 0) {
    if (error) *error = std::strerror(errno);
    return false;
  }
  int one = 1;
  setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons((uint16_t)port);
  if (inet_pton(AF_INET, host.c_str(), &a.sin_addr) != 1 ||
      ::bind(listenFd_, (sockaddr *)&a, sizeof a) != 0 || ::listen(listenFd_, 64) != 0) {
    if (error) *error = std::strerror(errno);
    ::close(listenFd_);
    listenFd_ = -1;
    return false;
  }
  socklen_t len = sizeof a;
  getsockname(listenFd_, (sockaddr *)&a, &len);
  port_ = ntohs(a.sin_port);
  nonBlocking(listenFd_);
  return true;
}

void WebServer::poll(int timeoutMs) {
  std::vector<pollfd> fds;
  std::vector<int> ids;
  fds.push_back({listenFd_, POLLIN, 0});
  ids.push_back(0);
  for (auto &kv : conns_) {
    short ev = POLLIN;
    if (!kv.second.out.empty())
      ev |= POLLOUT;
    fds.push_back({kv.second.fd, ev, 0});
    ids.push_back(kv.first);
  }
  if (::poll(fds.data(), fds.size(), timeoutMs) <= 0)
    return;
  if (fds[0].revents & POLLIN)
    accept();
  for (size_t i = 1; i < fds.size(); ++i) {
    auto it = conns_.find(ids[i]);
    if (it == conns_.end())
      continue;
    if (fds[i].revents & (POLLERR | POLLNVAL)) {
      drop(ids[i]);
      continue;
    }
    if (fds[i].revents & (POLLIN | POLLHUP))
      readFrom(ids[i], it->second);
    it = conns_.find(ids[i]);
    if (it != conns_.end() && (fds[i].revents & POLLOUT))
      writeTo(ids[i], it->second);
  }
}

void WebServer::accept() {
  while (true) {
    int fd = ::accept(listenFd_, nullptr, nullptr);
    if (fd < 0)
      return;
    nonBlocking(fd);
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    Conn c;
    c.fd = fd;
    conns_[nextId_++] = std::move(c);
  }
}

void WebServer::drop(int id) {
  auto it = conns_.find(id);
  if (it == conns_.end())
    return;
  ::close(it->second.fd);
  const bool open = it->second.open;
  conns_.erase(it);
  if (open && h_.onClose)
    h_.onClose(id);
}

void WebServer::reportClosed(int id, Conn &c) {
  if (!c.open)
    return;
  c.open = false;
  if (h_.onClose)
    h_.onClose(id);
}

void WebServer::readFrom(int id, Conn &c) {
  char buf[16384];
  while (true) {
    ssize_t n = ::recv(c.fd, buf, sizeof buf, 0);
    if (n > 0) {
      c.in.append(buf, (size_t)n);
      continue;
    }
    if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
      drop(id);
      return;
    }
    break;
  }
  if (c.state == State::Http)
    handleHttp(id, c);
  else
    handleFrames(id, c);
}

void WebServer::writeTo(int id, Conn &c) {
  while (!c.out.empty()) {
    ssize_t n = ::send(c.fd, c.out.data(), c.out.size(), MSG_NOSIGNAL);
    if (n > 0) {
      c.out.erase(0, (size_t)n);
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return;
    drop(id);
    return;
  }
  if (c.closeAfterWrite)
    drop(id);
}

void WebServer::respond(Conn &c, int code, const std::string &type,
                        const std::string &body) {
  const char *reason = code == 200 ? "OK" : code == 404 ? "Not Found" : "Bad Request";
  c.out += "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\n";
  c.out += "Content-Type: " + type + "\r\n";
  c.out += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  c.out += "Cache-Control: no-store\r\nConnection: close\r\n\r\n";
  c.out += body;
  c.closeAfterWrite = true;
}

void WebServer::handleHttp(int id, Conn &c) {
  const size_t end = c.in.find("\r\n\r\n");
  if (end == std::string::npos) {
    if (c.in.size() > kMaxRequest)
      drop(id);
    return;
  }
  std::string head = c.in.substr(0, end);
  c.in.erase(0, end + 4);

  std::map<std::string, std::string> headers;
  size_t lineEnd = head.find("\r\n");
  const std::string requestLine = head.substr(0, lineEnd);
  size_t pos = lineEnd == std::string::npos ? head.size() : lineEnd + 2;
  while (pos < head.size()) {
    size_t e = head.find("\r\n", pos);
    if (e == std::string::npos) e = head.size();
    const std::string line = head.substr(pos, e - pos);
    const size_t colon = line.find(':');
    if (colon != std::string::npos)
      headers[lower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
    pos = e + 2;
  }

  const size_t sp1 = requestLine.find(' ');
  const size_t sp2 = requestLine.find(' ', sp1 + 1);
  if (sp1 == std::string::npos || sp2 == std::string::npos ||
      requestLine.substr(0, sp1) != "GET") {
    respond(c, 400, "text/plain", "GET only\n");
    return;
  }
  std::string path = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);
  const size_t q = path.find('?');
  if (q != std::string::npos)
    path.resize(q);

  if (path == "/status") {
    respond(c, 200, "application/json", h_.status ? h_.status() : "{}");
    return;
  }
  if (path == "/ws") {
    const auto key = headers.find("sec-websocket-key");
    if (lower(headers["upgrade"]) != "websocket" || key == headers.end()) {
      respond(c, 400, "text/plain", "WebSocket upgrade expected\n");
      return;
    }
    c.out += "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
             "Connection: Upgrade\r\nSec-WebSocket-Accept: " +
             acceptKey(key->second) + "\r\n\r\n";
    c.state = State::WebSocket;
    c.open = true;
    if (h_.onOpen)
      h_.onOpen(id);
    if (conns_.count(id) && !c.in.empty())
      handleFrames(id, c);
    return;
  }
  if (!staticRoot_.empty()) {
    if (path == "/")
      path = "/index.html";
    if (path.find("..") == std::string::npos) {
      std::ifstream f(staticRoot_ + path, std::ios::binary);
      if (f) {
        std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        respond(c, 200, contentType(path), body);
        return;
      }
    }
  }
  respond(c, 404, "text/plain", "not here\n");
}

void WebServer::handleFrames(int id, Conn &c) {
  while (true) {
    if (c.in.size() < 2)
      return;
    const uint8_t b0 = (uint8_t)c.in[0], b1 = (uint8_t)c.in[1];
    const bool fin = b0 & 0x80;
    const uint8_t opcode = b0 & 0x0F;
    const bool masked = b1 & 0x80;
    uint64_t len = b1 & 0x7F;
    size_t hdr = 2;
    if (len == 126) {
      if (c.in.size() < 4) return;
      len = ((uint64_t)(uint8_t)c.in[2] << 8) | (uint8_t)c.in[3];
      hdr = 4;
    } else if (len == 127) {
      if (c.in.size() < 10) return;
      len = 0;
      for (int k = 0; k < 8; ++k)
        len = (len << 8) | (uint8_t)c.in[2 + (size_t)k];
      hdr = 10;
    }
    if (!masked || len > kMaxMessage) {
      close(id, 1009, "too big or unmasked");
      return;
    }
    if (c.in.size() < hdr + 4 + len)
      return;
    const uint8_t *mask = (const uint8_t *)c.in.data() + hdr;
    std::string payload = c.in.substr(hdr + 4, (size_t)len);
    for (size_t k = 0; k < payload.size(); ++k)
      payload[k] = (char)(payload[k] ^ mask[k & 3]);
    c.in.erase(0, hdr + 4 + (size_t)len);

    switch (opcode) {
    case 0x0:  // continuation
    case 0x1:  // text
      c.message += payload;
      if (c.message.size() > kMaxMessage) {
        close(id, 1009, "too big");
        return;
      }
      if (fin) {
        std::string m;
        m.swap(c.message);
        if (h_.onText && c.state == State::WebSocket)
          h_.onText(id, m);
        if (!conns_.count(id))
          return;
      }
      break;
    case 0x2:
      close(id, 1003, "text only");
      return;
    case 0x8:
      if (c.state == State::WebSocket)
        frame(c, 0x8, payload.data(), std::min<size_t>(payload.size(), 2));
      c.state = State::Closing;
      c.closeAfterWrite = true;
      reportClosed(id, c);
      return;
    case 0x9:
      frame(c, 0xA, payload.data(), payload.size());
      break;
    case 0xA:
      break;
    default:
      close(id, 1002, "bad opcode");
      return;
    }
  }
}

void WebServer::frame(Conn &c, uint8_t opcode, const void *data, size_t size) {
  uint8_t h[10];
  size_t n = 2;
  h[0] = (uint8_t)(0x80 | opcode);
  if (size < 126) {
    h[1] = (uint8_t)size;
  } else if (size < 65536) {
    h[1] = 126;
    h[2] = (uint8_t)(size >> 8);
    h[3] = (uint8_t)size;
    n = 4;
  } else {
    h[1] = 127;
    for (int k = 0; k < 8; ++k)
      h[2 + k] = (uint8_t)((uint64_t)size >> (56 - 8 * k));
    n = 10;
  }
  c.out.append((const char *)h, n);
  c.out.append((const char *)data, size);
}

void WebServer::sendText(int id, const std::string &text) {
  auto it = conns_.find(id);
  if (it != conns_.end() && it->second.state == State::WebSocket)
    frame(it->second, 0x1, text.data(), text.size());
}

void WebServer::sendBinary(int id, const void *data, size_t size) {
  auto it = conns_.find(id);
  if (it != conns_.end() && it->second.state == State::WebSocket)
    frame(it->second, 0x2, data, size);
}

void WebServer::close(int id, uint16_t code, const std::string &reason) {
  auto it = conns_.find(id);
  if (it == conns_.end())
    return;
  Conn &c = it->second;
  if (c.state == State::WebSocket) {
    std::string p;
    p += (char)(code >> 8);
    p += (char)(code & 0xFF);
    p += reason.substr(0, 120);
    frame(c, 0x8, p.data(), p.size());
    c.state = State::Closing;
  }
  c.closeAfterWrite = true;
  reportClosed(id, c);
}

size_t WebServer::pendingBytes(int id) const {
  auto it = conns_.find(id);
  return it == conns_.end() ? 0 : it->second.out.size();
}

size_t WebServer::connectionCount() const { return conns_.size(); }

}  // namespace balcony
