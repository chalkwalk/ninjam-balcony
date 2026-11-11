# ninjam-balcony -- Roadmap

Open work only. When an item is done and verified, move it, with what was learned, to
[docs/COMPLETED.md](docs/COMPLETED.md). Where things stand right now is
[HANDOFF.md](HANDOFF.md).

Ordering for new work: **PRINCIPLES -> GOALS -> DESIGN -> ROADMAP -> HANDOFF**. Items are
named, not numbered, and carry checkboxes.

## Before the first jam

- [ ] **The rumble filter, again on real bass.** Blind on 2025-06-04, the owner leaned to
      6 dB/octave at 100 Hz (3 of 4) but heard little difference; kept at 12 dB/octave at
      35 Hz for now (owner). Judge again on a jam with a bass player, or live.
- [ ] **Listen to it.** Nobody has heard the stream yet: replay an archive into a real
      browser and judge the mix, the rider (`Q1` in the period plan: should a quiet
      passage of the whole band stay quiet?) and the 0.4 s buffer on a phone.
- [ ] **Say when the browser cannot play at all.** Firefox with no PulseAudio socket
      (pipewire-pulse off on the dev desktop) decoded every frame but its AudioContext
      stayed `suspended` forever, even created in a click; "Start sound" did nothing. If
      the context is still suspended a few seconds after that click, say "this browser
      is not playing sound; check it plays sound on other sites" instead.
- [ ] **Screen-reader pass** (`PRINCIPLES §8`) with a real reader (Orca, VoiceOver,
      NVDA). The accessibility tree is checked by `test/browser/drive-a11y.mjs`: every
      control named, headings in order, the chat a live log, no meter nodes.
- [ ] **A dress rehearsal** with two or three people before the first public jam. A first
      one ran 2026-11-11: the owner playing, four practice bots (`jambot-band`, branch
      `band-runner` in chalkwalk-jambot), two web listeners on different machines; it
      worked. Still to do: several humans playing, a phone, Safari.
- [ ] **Measure a real jam** (CPU, memory, network; `ninjam-room metrics`) and replace
      the estimates in `DESIGN §8` (`PRINCIPLES §5`).

## In chalkwalk-jambot

- [ ] **The balcony's phrases in jambot's corpus** ("leave me out", "put me back"), if
      they turn out to be jambot's requests too. For now they are the balcony's own
      matching (`src/Room.cpp`).
- [ ] **SocketClient on Windows** (Winsock), when a host needs it.

## Later

- [ ] **Publish**: GitHub, probably GPLv3, when the owner decides. `LICENSE`,
      `THIRDPARTY.md`, `CONTRIBUTING.md`, and a check that no document names a private
      project.
- [ ] **A base repository**, only if the balcony ends up using jambot's client but not
      its band (`DESIGN §7`). So far jambot as a submodule fits.
