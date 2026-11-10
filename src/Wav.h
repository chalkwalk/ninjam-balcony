#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

// A 32-bit float stereo WAV, written as it goes: for renders and fixtures.
// A WAV's sizes are 32-bit, so it holds at most about 3 hours here (48 kHz);
// past that, and on any failed write (a full disk), it stops writing and
// `failed()` says why. The caller decides what to do; it is never silent.
namespace balcony {

class WavWriter {
public:
  WavWriter(const std::string &path, int sampleRate) : rate_(sampleRate) {
    f_ = std::fopen(path.c_str(), "wb");
    if (f_ != nullptr)
      header(0);
  }
  ~WavWriter() { close(); }
  bool ok() const { return f_ != nullptr; }
  // Empty while every write has succeeded.
  const std::string &failed() const { return failed_; }

  void write(const float *l, const float *r, int n) {
    if (f_ == nullptr || !failed_.empty())
      return;
    if ((uint64_t)frames_ + (uint64_t)n > kMaxFrames) {
      failed_ = "longer than a WAV can hold; stopped writing";
      return;
    }
    for (int i = 0; i < n; ++i) {
      const float frame[2] = {l[i], r[i]};
      if (std::fwrite(frame, 4, 2, f_) != 2) {
        failed_ = "write failed (disk full?)";
        return;
      }
      ++frames_;
    }
  }

  void close() {
    if (f_ == nullptr)
      return;
    std::fseek(f_, 0, SEEK_SET);
    header(frames_);
    if (std::fclose(f_) != 0 && failed_.empty())
      failed_ = "write failed on close (disk full?)";
    f_ = nullptr;
  }

private:
  void u32(uint32_t v) { std::fwrite(&v, 4, 1, f_); }
  void u16(uint16_t v) { std::fwrite(&v, 2, 1, f_); }
  void header(uint32_t frames) {
    const uint32_t data = frames * 8;
    std::fwrite("RIFF", 1, 4, f_);
    u32(36 + data);
    std::fwrite("WAVEfmt ", 1, 8, f_);
    u32(16);
    u16(3);  // IEEE float
    u16(2);
    u32((uint32_t)rate_);
    u32((uint32_t)rate_ * 8);
    u16(8);
    u16(32);
    std::fwrite("data", 1, 4, f_);
    u32(data);
  }

  // 36 header bytes plus 8 per frame must fit the RIFF size.
  static constexpr uint64_t kMaxFrames = (0xFFFFFFFFull - 36) / 8;
  std::FILE *f_ = nullptr;
  std::string failed_;
  int rate_;
  uint32_t frames_ = 0;
};

}  // namespace balcony
