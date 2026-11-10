#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

// A 32-bit float stereo WAV, written as it goes: for renders and fixtures.
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

  void write(const float *l, const float *r, int n) {
    if (f_ == nullptr)
      return;
    for (int i = 0; i < n; ++i) {
      std::fwrite(&l[i], 4, 1, f_);
      std::fwrite(&r[i], 4, 1, f_);
    }
    frames_ += (uint32_t)n;
  }

  void close() {
    if (f_ == nullptr)
      return;
    std::fseek(f_, 0, SEEK_SET);
    header(frames_);
    std::fclose(f_);
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

  std::FILE *f_ = nullptr;
  int rate_;
  uint32_t frames_ = 0;
};

}  // namespace balcony
