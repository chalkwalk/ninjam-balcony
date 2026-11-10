#include "Archive.h"

#include <chalkwalk/ninjam/VorbisCodec.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;
namespace archive = chalkwalk::ninjam::archive;

namespace balcony {

std::string displayName(const std::string &user) {
  const size_t at = user.find('@');
  return at == std::string::npos ? user : user.substr(0, at);
}

namespace {

bool readFile(const fs::path &p, std::string &out) {
  std::ifstream in(p, std::ios::binary);
  if (!in)
    return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

}  // namespace

ArchiveReader::ArchiveReader(const std::string &path, double outRate)
    : outRate_(outRate) {
  std::error_code ec;
  std::vector<fs::path> dirs;
  if (fs::exists(fs::path(path) / "clipsort.log", ec)) {
    dirs.push_back(path);
  } else {
    for (const auto &e : fs::directory_iterator(path, ec))
      if (e.is_directory() && fs::exists(e.path() / "clipsort.log"))
        dirs.push_back(e.path());
    std::sort(dirs.begin(), dirs.end());
  }
  if (dirs.empty()) {
    error_ = "no clipsort.log in " + path + " or its subdirectories";
    return;
  }
  for (const auto &d : dirs) {
    std::string text;
    if (!readFile(d / "clipsort.log", text))
      continue;
    archive::Log log = archive::parseClipsortLog(text);
    skippedLines_ += log.skippedLines;
    Segment seg;
    seg.dir = d.string();
    for (auto &c : log.clips)
      seg.intervals[c.interval].push_back(std::move(c));
    segments_.push_back(std::move(seg));
  }
  for (size_t s = 0; s < segments_.size(); ++s)
    for (const auto &kv : segments_[s].intervals)
      order_.emplace_back(s, kv.first);
}

void ArchiveReader::rewind() {
  cursor_ = 0;
  resamplers_.clear();
}

bool ArchiveReader::decodeClip(const Segment &seg, const archive::Clip &clip,
                               int samples, ChannelAudio &out) {
  std::string bytes;
  const fs::path dir(seg.dir);
  if (!readFile(dir / archive::clipPath(clip.guid, "OGG"), bytes) &&
      !readFile(dir / archive::clipPath(clip.guid, "ogg"), bytes))
    return false;

  chalkwalk::ninjam::VorbisDecoder dec;
  std::vector<float> interleaved;
  dec.decode(bytes.data(), (int)bytes.size());
  while (dec.available() > 0) {
    const int n = dec.available();
    interleaved.insert(interleaved.end(), dec.pcm(), dec.pcm() + n);
    dec.skip(n);
  }
  const int channels = dec.numChannels();
  const int rate = dec.sampleRate();
  if (channels <= 0 || rate <= 0)
    return false;

  const size_t frames = interleaved.size() / (size_t)channels;
  std::vector<float> l(frames), r(frames);
  for (size_t i = 0; i < frames; ++i) {
    l[i] = interleaved[i * (size_t)channels];
    r[i] = interleaved[i * (size_t)channels + (channels > 1 ? 1 : 0)];
  }

  ChannelKey key{clip.username, clip.channel};
  Resamplers &rs = resamplers_[key];
  if (rs.inRate != rate) {
    rs.inRate = rate;
    rs.left = std::make_unique<chalkwalk::dsp::Resampler>(rate, outRate_);
    rs.right = std::make_unique<chalkwalk::dsp::Resampler>(rate, outRate_);
  }
  out.key = key;
  out.channelName = clip.channelName;
  out.left.clear();
  out.right.clear();
  rs.left->process(l.data(), (int)frames, out.left);
  rs.right->process(r.data(), (int)frames, out.right);
  out.left.resize((size_t)samples, 0.0f);
  out.right.resize((size_t)samples, 0.0f);
  return true;
}

bool ArchiveReader::next(ArchiveInterval &out) {
  if (cursor_ >= order_.size())
    return false;
  const auto [s, n] = order_[cursor_++];
  const Segment &seg = segments_[s];
  const auto &clips = seg.intervals.at(n);

  out.segment = fs::path(seg.dir).filename().string();
  out.index = n;
  out.bpm = clips.front().bpm;
  out.bpi = clips.front().bpi;
  out.samples = out.bpm > 0
                    ? (int)std::lround(outRate_ * 60.0 * out.bpi / out.bpm)
                    : 0;
  out.channels.clear();
  for (const auto &clip : clips) {
    ChannelAudio a;
    if (decodeClip(seg, clip, out.samples, a))
      out.channels.push_back(std::move(a));
    else
      ++missingClips_;
  }
  return true;
}

}  // namespace balcony
