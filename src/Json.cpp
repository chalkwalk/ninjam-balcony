#include "Json.h"

#include <cctype>
#include <cstdio>

namespace balcony::json {

std::string quote(const std::string &s) {
  std::string o = "\"";
  for (unsigned char c : s) {
    switch (c) {
    case '"': o += "\\\""; break;
    case '\\': o += "\\\\"; break;
    case '\n': o += "\\n"; break;
    case '\r': o += "\\r"; break;
    case '\t': o += "\\t"; break;
    default:
      if (c < 0x20) {
        char buf[8];
        std::snprintf(buf, sizeof buf, "\\u%04x", c);
        o += buf;
      } else {
        o += (char)c;  // UTF-8 passes through
      }
    }
  }
  return o + "\"";
}

namespace {

struct P {
  const std::string &s;
  size_t i = 0;
  void ws() {
    while (i < s.size() && std::isspace((unsigned char)s[i]))
      ++i;
  }
  bool eat(char c) {
    ws();
    if (i < s.size() && s[i] == c) {
      ++i;
      return true;
    }
    return false;
  }
  static void utf8(unsigned cp, std::string &o) {
    if (cp < 0x80) {
      o += (char)cp;
    } else if (cp < 0x800) {
      o += (char)(0xC0 | (cp >> 6));
      o += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      o += (char)(0xE0 | (cp >> 12));
      o += (char)(0x80 | ((cp >> 6) & 0x3F));
      o += (char)(0x80 | (cp & 0x3F));
    } else {
      o += (char)(0xF0 | (cp >> 18));
      o += (char)(0x80 | ((cp >> 12) & 0x3F));
      o += (char)(0x80 | ((cp >> 6) & 0x3F));
      o += (char)(0x80 | (cp & 0x3F));
    }
  }
  bool hex4(unsigned &v) {
    if (i + 4 > s.size())
      return false;
    v = 0;
    for (int k = 0; k < 4; ++k) {
      char c = s[i++];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
      else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
      else return false;
    }
    return true;
  }
  bool string(std::string &o) {
    if (!eat('"'))
      return false;
    o.clear();
    while (i < s.size()) {
      char c = s[i++];
      if (c == '"')
        return true;
      if ((unsigned char)c < 0x20)
        return false;
      if (c != '\\') {
        o += c;
        continue;
      }
      if (i >= s.size())
        return false;
      char e = s[i++];
      switch (e) {
      case '"': o += '"'; break;
      case '\\': o += '\\'; break;
      case '/': o += '/'; break;
      case 'b': o += '\b'; break;
      case 'f': o += '\f'; break;
      case 'n': o += '\n'; break;
      case 'r': o += '\r'; break;
      case 't': o += '\t'; break;
      case 'u': {
        unsigned cp;
        if (!hex4(cp))
          return false;
        if (cp >= 0xD800 && cp <= 0xDBFF) {
          unsigned lo;
          if (i + 2 > s.size() || s[i] != '\\' || s[i + 1] != 'u')
            return false;
          i += 2;
          if (!hex4(lo) || lo < 0xDC00 || lo > 0xDFFF)
            return false;
          cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
          return false;
        }
        utf8(cp, o);
        break;
      }
      default:
        return false;
      }
    }
    return false;
  }
  bool scalar(std::string &o) {
    ws();
    if (i < s.size() && s[i] == '"')
      return string(o);
    size_t b = i;
    while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '-' ||
                            s[i] == '+' || s[i] == '.'))
      ++i;
    o = s.substr(b, i - b);
    return !o.empty();
  }
};

}  // namespace

bool parseFlat(const std::string &text, std::map<std::string, std::string> &out) {
  out.clear();
  P p{text};
  if (!p.eat('{'))
    return false;
  if (p.eat('}')) {
    p.ws();
    return p.i == text.size();
  }
  while (true) {
    std::string k, v;
    if (!p.string(k) || !p.eat(':') || !p.scalar(v))
      return false;
    out[k] = v;
    if (p.eat(','))
      continue;
    if (!p.eat('}'))
      return false;
    p.ws();
    return p.i == text.size();
  }
}

}  // namespace balcony::json
