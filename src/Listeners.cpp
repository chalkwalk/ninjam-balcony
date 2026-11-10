#include "Listeners.h"

#include <algorithm>
#include <cctype>

namespace balcony {

namespace {

// UTF-8 code point starts.
bool isLead(unsigned char c) { return (c & 0xC0) != 0x80; }

std::string joinNames(const std::vector<std::string> &v) {
  if (v.empty())
    return {};
  if (v.size() == 1)
    return v[0];
  std::string s;
  for (size_t i = 0; i + 1 < v.size(); ++i)
    s += (i ? ", " : "") + v[i];
  return s + " and " + v.back();
}

}  // namespace

bool validName(const std::string &name) {
  if (name.empty() || name.front() == ' ' || name.back() == ' ')
    return false;
  size_t chars = 0;
  char prev = 0;
  for (unsigned char c : name) {
    if (isLead(c))
      ++chars;
    if (c >= 0x80) {
      prev = (char)c;
      continue;  // any non-ASCII script
    }
    if (std::isalnum(c) || c == '_' || c == '-' || c == '.') {
      prev = (char)c;
      continue;
    }
    if (c == ' ' && prev != ' ') {
      prev = ' ';
      continue;
    }
    return false;
  }
  return chars >= 1 && chars <= 24;
}

std::string foldName(const std::string &name) {
  std::string s = name;
  for (auto &c : s)
    if ((unsigned char)c < 0x80)
      c = (char)std::tolower((unsigned char)c);
  return s;
}

std::string cleanLine(const std::string &text, size_t maxChars) {
  std::string o;
  size_t chars = 0;
  bool space = false;
  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char c = (unsigned char)text[i];
    if (c < 0x20 || c == 0x7F) {
      space = !o.empty();
      continue;
    }
    if (c == ' ') {
      space = !o.empty();
      continue;
    }
    if (isLead(c)) {
      if (chars + (space ? 1 : 0) >= maxChars)
        break;
      if (space) {
        o += ' ';
        ++chars;
        space = false;
      }
      ++chars;
    }
    o += (char)c;
  }
  // A cut can land inside a multi-byte character only if the input was not
  // UTF-8; drop a dangling lead so the output is.
  while (!o.empty() && (unsigned char)o.back() >= 0xC0)
    o.pop_back();
  return o;
}

Listeners::Listeners(Config c) : cfg_(c) {}

Listeners::Join Listeners::join(int id, const std::string &name,
                                const std::set<std::string> &players) {
  if (!validName(name))
    return Join::NameInvalid;
  const std::string f = foldName(name);
  if (blocked_.count(f))
    return Join::Blocked;
  for (const auto &kv : byId_)
    if (foldName(kv.second.name) == f)
      return Join::NameTaken;
  for (const auto &p : players)
    if (foldName(p) == f)
      return Join::NameTaken;
  Person p;
  p.name = name;
  p.tokens = cfg_.burst;
  byId_[id] = p;
  change_[f] += 1;
  changeName_[f] = name;
  return Join::Ok;
}

void Listeners::leave(int id) {
  auto it = byId_.find(id);
  if (it == byId_.end())
    return;
  const std::string f = foldName(it->second.name);
  change_[f] -= 1;
  changeName_[f] = it->second.name;
  byId_.erase(it);
}

std::string Listeners::name(int id) const {
  auto it = byId_.find(id);
  return it == byId_.end() ? std::string() : it->second.name;
}

std::vector<std::string> Listeners::names() const {
  std::vector<std::string> v;
  for (const auto &kv : byId_)
    v.push_back(kv.second.name);
  std::sort(v.begin(), v.end(), [](const std::string &a, const std::string &b) {
    return foldName(a) < foldName(b);
  });
  return v;
}

std::vector<int> Listeners::ids() const {
  std::vector<int> v;
  for (const auto &kv : byId_)
    v.push_back(kv.first);
  return v;
}

bool Listeners::say(int id, const std::string &text, Clock::time_point now,
                    std::string &cleaned, std::string &why) {
  auto it = byId_.find(id);
  if (it == byId_.end()) {
    why = "not in the balcony";
    return false;
  }
  Person &p = it->second;
  if (p.last != Clock::time_point{}) {
    const double dt = std::chrono::duration<double>(now - p.last).count();
    p.tokens = std::min((double)cfg_.burst, p.tokens + dt * cfg_.linesPerSecond);
  }
  p.last = now;
  if (p.tokens < 1.0) {
    why = "slow down: a few lines every ten seconds";
    return false;
  }
  cleaned = cleanLine(text, cfg_.maxChars);
  if (!cleaned.empty())
    p.tokens -= 1.0;
  return true;
}

int Listeners::kick(const std::string &name) {
  const std::string f = foldName(name);
  blocked_.insert(f);
  for (const auto &kv : byId_)
    if (foldName(kv.second.name) == f)
      return kv.first;
  return -1;
}

void Listeners::remember(const ChatLine &line, Clock::time_point now) {
  history_.emplace_back(now, line);
  while (history_.size() > cfg_.historyLines)
    history_.pop_front();
  while (!history_.empty() && now - history_.front().first > cfg_.history)
    history_.pop_front();
}

std::vector<ChatLine> Listeners::history(Clock::time_point now) const {
  std::vector<ChatLine> v;
  for (const auto &e : history_)
    if (now - e.first <= cfg_.history)
      v.push_back(e.second);
  return v;
}

void Listeners::takeChanges(std::vector<std::string> &joined,
                            std::vector<std::string> &left) {
  joined.clear();
  left.clear();
  for (const auto &kv : change_) {
    if (kv.second > 0)
      joined.push_back(changeName_[kv.first]);
    else if (kv.second < 0)
      left.push_back(changeName_[kv.first]);
  }
  change_.clear();
  changeName_.clear();
}

std::string batchedLine(const std::vector<std::string> &joined,
                        const std::vector<std::string> &left, size_t listening) {
  if (joined.empty() && left.empty())
    return {};
  std::string s;
  if (!joined.empty())
    s += joinNames(joined) + " joined the balcony";
  if (!left.empty())
    s += (s.empty() ? "" : ", ") + joinNames(left) + " left";
  return s + " (" + std::to_string(listening) + " listening)";
}

}  // namespace balcony
