# ninjam-balcony -- Roadmap

Open work only. When an item is done and verified, move it, with what was learned, to
[docs/COMPLETED.md](docs/COMPLETED.md). Where things stand right now is
[HANDOFF.md](HANDOFF.md).

Ordering for new work: **PRINCIPLES -> GOALS -> DESIGN -> ROADMAP -> HANDOFF**. Items are
named, not numbered, and carry checkboxes.

## Before the first jam

- [ ] **Merge the library branches.** `chalkwalk-ninjam`, `chalkwalk-dsp` and
      `chalkwalk-jambot` each have a local `balcony` branch (worktrees at
      `/home/programming/chalkwalk-<lib>-balcony`); review, merge, push, then bump the
      submodules here so the build needs no `CHALKWALK_*_DIR` overrides.
- [ ] **The bot's account on the improv server**: a `User balcony <pw> CH` line, and the
      admins' names (`deploy/README.md`).
- [ ] **Deploy** by `deploy/README.md`: build on the VM, Caddy, ports 80/443 in both the
      security list and iptables, enable the unit. The owner's to do or approve.
- [ ] **The owner's rumble filter**, if wanted: their 6 dB/octave at 100 Hz against the
      12 dB/octave at 35 Hz built (`balcony-render --hp 100 --hp-order 1`), blind.
- [ ] **Listen to it.** Nobody has heard the stream yet: replay an archive into a real
      browser and judge the mix, the rider (`Q1` in the period plan: should a quiet
      passage of the whole band stay quiet?) and the 0.4 s buffer on a phone.
- [ ] **Other browsers.** Chrome: plays, checked headless. Firefox 159: WebCodecs Opus
      decodes the stream without error (checked headless), but headless Firefox has no
      audio device here, so playback itself is unheard; check it with ears. Safari and
      phones: untested (WebCodecs audio is recent in Safari); a wasm Opus fallback if
      one lacks it.
- [ ] **Screen-reader pass** (`PRINCIPLES §8`) with a real reader (Orca, VoiceOver,
      NVDA). The accessibility tree is checked by `test/browser/drive-a11y.mjs`: every
      control named, headings in order, the chat a live log, no meter nodes.
- [ ] **A dress rehearsal** with two or three people before the first public jam.
- [ ] **Measure a real jam** (CPU, memory, network; `ninjam-room metrics`) and replace
      the estimates in `DESIGN §8` (`PRINCIPLES §5`).

## In chalkwalk-jambot

- [ ] **Handles of anonymous humans**: `BotNames::handleOf` keeps `@ip`, so a bot cannot
      be addressed as `name` when it joined anonymously, nor a human as theirs. The
      balcony works around it; the fix belongs there, with a corpus line first.
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
