#pragma once

#include <chalkwalk/dsp/Resampler.h>
#include <chalkwalk/ninjam/SessionArchive.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// A jam, read back from the server's session archive, one interval at a time.
//
// The archive is the balcony's test fixture (DESIGN §9.7): most of what the
// bot does to sound can be built and measured against a real jam with no room.
// This reads the files; `chalkwalk-ninjam` reads the log.
//
// A jam is several directories (the server rotates every half hour and numbers
// intervals from 1 again in each), so `path` may be one `*.ninjam` directory or
// a directory of them, read in name order, which is time order.
//
// Every channel comes out at `outRate`, through one resampler per channel kept
// across intervals, so a player's stream converts as one stream. Each interval
// is exactly as long as its tempo says; a clip that is a little short is padded
// with silence and one a little long is cut, as a client playing it would.

namespace balcony {

struct ChannelKey {
  std::string user;  // as the server names them, `name@ip` for anonymous
  int channel = 0;
  bool operator<(const ChannelKey &o) const {
    return user != o.user ? user < o.user : channel < o.channel;
  }
  bool operator==(const ChannelKey &o) const {
    return user == o.user && channel == o.channel;
  }
};

// A player's name without the `@ip` an anonymous login carries.
std::string displayName(const std::string &user);

struct ChannelAudio {
  ChannelKey key;
  std::string channelName;
  std::vector<float> left, right;  // planar, `samples` long
};

struct ArchiveInterval {
  std::string segment;  // the *.ninjam directory it came from
  int index = 0;        // the interval number in that segment
  int bpm = 0, bpi = 0;
  int samples = 0;      // the interval's length at the output rate
  std::vector<ChannelAudio> channels;
};

class ArchiveReader {
public:
  explicit ArchiveReader(const std::string &path, double outRate = 48000.0);

  bool ok() const { return error_.empty(); }
  const std::string &error() const { return error_; }

  int intervalCount() const { return (int)order_.size(); }
  int skippedLogLines() const { return skippedLines_; }
  int missingClips() const { return missingClips_; }

  // Decode the next interval; false at the end.
  bool next(ArchiveInterval &out);

  // Back to the first interval, with fresh resamplers.
  void rewind();

private:
  struct Segment {
    std::string dir;
    std::map<int, std::vector<chalkwalk::ninjam::archive::Clip>> intervals;
  };
  struct Resamplers {
    int inRate = 0;
    std::unique_ptr<chalkwalk::dsp::Resampler> left, right;
  };

  bool decodeClip(const Segment &seg, const chalkwalk::ninjam::archive::Clip &clip,
                  int samples, ChannelAudio &out);

  double outRate_;
  std::string error_;
  std::vector<Segment> segments_;
  std::vector<std::pair<size_t, int>> order_;  // (segment, interval)
  size_t cursor_ = 0;
  std::map<ChannelKey, Resamplers> resamplers_;
  int skippedLines_ = 0;
  int missingClips_ = 0;
};

}  // namespace balcony
