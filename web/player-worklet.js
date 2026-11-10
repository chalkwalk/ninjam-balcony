// The jitter buffer and the speaker (DESIGN §9.2): decoded audio goes in with
// the sample position the bot stamped on it; samples come out in order at the
// context's rate. It starts once `target` seconds are buffered, and after an
// underrun it waits for `target` again rather than stuttering.
//
// It reports which sample position is playing, so the page can show each
// meter frame when its sound is heard (PRINCIPLES §4).
class BalconyPlayer extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const o = options.processorOptions || {};
    this.size = Math.round(sampleRate * (o.capacitySeconds || 6));
    this.target = Math.round(sampleRate * (o.targetSeconds || 0.4));
    this.l = new Float32Array(this.size);
    this.r = new Float32Array(this.size);
    this.readPos = -1;   // stream position of the next sample out; -1 before the first
    this.writeEnd = -1;  // stream position just past the newest sample in
    this.playing = false;
    this.lastReport = 0;
    this.underruns = 0;
    this.port.onmessage = (e) => this.take(e.data);
  }

  take({ pos, left, right }) {
    if (this.readPos < 0) this.readPos = pos;
    // Too far ahead of what is playing (a stall upstream): jump to it.
    if (pos - this.readPos > this.size - left.length) this.readPos = pos - this.target;
    if (pos + left.length <= this.readPos) return;  // arrived too late to play
    for (let i = 0; i < left.length; i++) {
      const p = pos + i;
      if (p < this.readPos) continue;
      const k = p % this.size;
      this.l[k] = left[i];
      this.r[k] = right[i];
    }
    this.writeEnd = Math.max(this.writeEnd, pos + left.length);
  }

  process(inputs, outputs) {
    const out = outputs[0];
    const n = out[0].length;
    const buffered = this.writeEnd - this.readPos;
    if (!this.playing && this.readPos >= 0 && buffered >= this.target) this.playing = true;
    if (this.playing && buffered < n) {
      this.playing = false;
      this.underruns++;
    }
    if (this.playing) {
      for (let i = 0; i < n; i++) {
        const k = (this.readPos + i) % this.size;
        out[0][i] = this.l[k];
        if (out[1]) out[1][i] = this.r[k];
      }
      this.readPos += n;
    }
    if (currentFrame - this.lastReport >= sampleRate / 30) {
      this.lastReport = currentFrame;
      this.port.postMessage({
        playing: this.playing ? this.readPos : -1,
        buffered: buffered / sampleRate,
        underruns: this.underruns,
      });
    }
    return true;
  }
}
registerProcessor('balcony-player', BalconyPlayer);
