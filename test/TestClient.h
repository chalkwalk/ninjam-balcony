#pragma once

// A deliberately dumb WebSocket client over a blocking socket, for the tests:
// it speaks the protocol by hand so the server is checked against RFC 6455,
// not against itself.

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace balcony::test {

inline int connectLoopback(int port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons((uint16_t)port);
  inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
  if (::connect(fd, (sockaddr *)&a, sizeof a) != 0) {
    ::close(fd);
    return -1;
  }
  timeval tv{5, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  return fd;
}

inline void sendAll(int fd, const std::string &s) {
  size_t off = 0;
  while (off < s.size()) {
    ssize_t n = ::send(fd, s.data() + off, s.size() - off, MSG_NOSIGNAL);
    if (n <= 0)
      return;
    off += (size_t)n;
  }
}

// Read until `marker` appears (or the peer closes / times out).
inline std::string readUntil(int fd, const std::string &marker) {
  std::string got;
  char buf[4096];
  while (got.find(marker) == std::string::npos) {
    ssize_t n = ::recv(fd, buf, sizeof buf, 0);
    if (n <= 0)
      break;
    got.append(buf, (size_t)n);
  }
  return got;
}

inline bool readExact(int fd, std::string &out, size_t n) {
  out.clear();
  char buf[4096];
  while (out.size() < n) {
    ssize_t r = ::recv(fd, buf, std::min(sizeof buf, n - out.size()), 0);
    if (r <= 0)
      return false;
    out.append(buf, (size_t)r);
  }
  return true;
}

struct Client {
  int fd = -1;
  std::string handshake;

  bool open(int port, const std::string &path = "/ws") {
    fd = connectLoopback(port);
    if (fd < 0)
      return false;
    sendAll(fd, "GET " + path + " HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\n"
                "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n\r\n");
    handshake = readUntil(fd, "\r\n\r\n");
    return handshake.find("101") != std::string::npos;
  }

  void sendFrame(uint8_t opcode, const std::string &payload, bool fin = true) {
    std::string f;
    f += (char)((fin ? 0x80 : 0) | opcode);
    const uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    if (payload.size() < 126) {
      f += (char)(0x80 | payload.size());
    } else {
      f += (char)(0x80 | 126);
      f += (char)(payload.size() >> 8);
      f += (char)(payload.size() & 0xFF);
    }
    f.append((const char *)mask, 4);
    for (size_t i = 0; i < payload.size(); ++i)
      f += (char)(payload[i] ^ mask[i & 3]);
    sendAll(fd, f);
  }
  void sendText(const std::string &s) { sendFrame(0x1, s); }

  // One whole frame from the server: opcode and payload.
  bool recvFrame(uint8_t &opcode, std::string &payload) {
    std::string h;
    if (!readExact(fd, h, 2))
      return false;
    opcode = (uint8_t)h[0] & 0x0F;
    if ((uint8_t)h[1] & 0x80)
      return false;  // a server must not mask
    uint64_t len = (uint8_t)h[1] & 0x7F;
    if (len == 126) {
      if (!readExact(fd, h, 2)) return false;
      len = ((uint64_t)(uint8_t)h[0] << 8) | (uint8_t)h[1];
    } else if (len == 127) {
      if (!readExact(fd, h, 8)) return false;
      len = 0;
      for (int k = 0; k < 8; ++k)
        len = (len << 8) | (uint8_t)h[(size_t)k];
    }
    return readExact(fd, payload, (size_t)len);
  }

  // Skip frames until a text frame; returns it.
  std::string recvText() {
    uint8_t op;
    std::string p;
    while (recvFrame(op, p))
      if (op == 0x1)
        return p;
    return {};
  }

  ~Client() {
    if (fd >= 0)
      ::close(fd);
  }
};

}  // namespace balcony::test
