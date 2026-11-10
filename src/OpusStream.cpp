#include "OpusStream.h"

#include <opus.h>

namespace balcony {

OpusStream::OpusStream(int bitrate) : interleaved_(kFrameSamples * 2) {
  int err = 0;
  enc_ = opus_encoder_create(kSampleRate, 2, OPUS_APPLICATION_AUDIO, &err);
  if (err != OPUS_OK) {
    enc_ = nullptr;
    return;
  }
  opus_encoder_ctl(enc_, OPUS_SET_BITRATE(bitrate));
  opus_encoder_ctl(enc_, OPUS_SET_VBR(1));
  opus_encoder_ctl(enc_, OPUS_SET_COMPLEXITY(8));
  opus_encoder_ctl(enc_, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
}

OpusStream::~OpusStream() {
  if (enc_ != nullptr)
    opus_encoder_destroy(enc_);
}

bool OpusStream::encode(const float *left, const float *right,
                        std::vector<uint8_t> &out) {
  if (enc_ == nullptr)
    return false;
  for (int i = 0; i < kFrameSamples; ++i) {
    interleaved_[(size_t)i * 2] = left[i];
    interleaved_[(size_t)i * 2 + 1] = right[i];
  }
  out.resize(1500);
  const int n = opus_encode_float(enc_, interleaved_.data(), kFrameSamples,
                                  out.data(), (opus_int32)out.size());
  if (n < 0) {
    out.clear();
    return false;
  }
  out.resize((size_t)n);
  return true;
}

}  // namespace balcony
