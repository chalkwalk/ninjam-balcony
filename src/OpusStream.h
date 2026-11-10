#pragma once

#include <cstdint>
#include <vector>

struct OpusEncoder;

// The one encode for every listener (PRINCIPLES §3): 48 kHz stereo Opus in
// 20 ms frames, VBR, application AUDIO (DESIGN §9.2).
namespace balcony {

class OpusStream {
public:
  static constexpr int kSampleRate = 48000;
  static constexpr int kFrameSamples = 960;  // 20 ms

  explicit OpusStream(int bitrate = 96000);
  ~OpusStream();
  OpusStream(const OpusStream &) = delete;
  OpusStream &operator=(const OpusStream &) = delete;

  bool ok() const { return enc_ != nullptr; }

  // Encode exactly one frame of planar stereo; the packet replaces `out`.
  bool encode(const float *left, const float *right, std::vector<uint8_t> &out);

private:
  OpusEncoder *enc_ = nullptr;
  std::vector<float> interleaved_;
};

}  // namespace balcony
