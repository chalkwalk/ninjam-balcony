# ninjam-balcony -- Roadmap

Open work only. When an item is done and verified, move it, with what was learned, to
`docs/COMPLETED.md` (made when the first item lands), so this file stays about what is next.
Where things stand right now is [HANDOFF.md](HANDOFF.md).

Ordering for new work: **PRINCIPLES -> GOALS -> DESIGN -> ROADMAP -> HANDOFF**. Items are
named, not numbered, and carry checkboxes. Anything left open in `DESIGN §9` is decided in
`DESIGN.md` before it is built.

## Before building

- [ ] **The owner accepts `PRINCIPLES.md` and `GOALS.md`**, or changes them. Both are drafts
      lifted from the design; the parts marked **(draft)** went beyond what was agreed.
- [ ] **Look for prior art.** A search on 2026-11-06 found no open-source Ninjam-to-web
      listener. Leads still to check: ninbot.com (publishes session recordings, may
      stream); the NINJAM forum; BHSPitMonkey/ninjam-js (a JS client needing Electron or a
      Chrome App for raw TCP). Groups that stream today seem to use OBS. If a good one
      exists, contribute to it instead.
- [ ] **Publish, when it is ready.** Local until the owner decides it is ready, then
      GitHub, probably under GPLv3 (decided then, not now). The libraries it stands on
      (`chalkwalk-*` MIT, libopus BSD, libebur128 MIT) are all GPLv3-compatible. Brings
      `LICENSE`, `THIRDPARTY.md`, `CONTRIBUTING.md`, and a check that no document names a
      private project.
- [ ] **Decide what `DESIGN §9` leaves open**, starting with the WebSocket and Opus
      libraries and the wire format between bot and page.

## The bot, offline first

The server writes every interval of a jam to its session archive, so most of the bot can be
built and measured against a real jam without a room.

- [ ] **Skeleton.** CMake, `chalkwalk-ninjam` and `chalkwalk-dsp` as submodules under
      `libs/` (as `chalkwalk-jambot` takes them), a test target, CI later.
- [ ] **Mix an archived jam.** Decode every channel, measure per-channel loudness over the
      sliding window, gain, pan, sum to about -16 LUFS through a limiter (`DESIGN §6`).
      Write a WAV; measure it. Tune window and slew here.
- [ ] **Encode it.** One Opus stream; measure CPU per interval on this machine, then
      estimate the VM's.

## The bot, live

- [ ] **Join a room.** Against a local `ninjamsrv`: log in (with `H` where configured),
      announce what it is, receive intervals, send nothing (`DESIGN §3`, `§4`).
- [ ] **Neutral votes** on a server that counts the bot, with `Voting.h` (`GOALS A1`).
- [ ] **WebSocket out**: audio and meter frames on one socket; status endpoint.
- [ ] **Chat both ways**: prefixing, arrivals and departures, history on arrival,
      rate limit and length cap (`DESIGN §3`, `§5`).
- [ ] **Admin commands**: `!kick`, `!quiet`, `!unquiet`; listen-only mode.

## The page

- [ ] **Room closed**: next jam in the visitor's time zone, `.ics`, Google Calendar link,
      status polling (`DESIGN §2`).
- [ ] **Room open**: name, sound (WebCodecs, wasm Opus fallback), players and meters,
      chat, listeners, volume and mute.
- [ ] **Screen-reader pass** before the first real jam (`PRINCIPLES §8`).

## On the VM

Read `/home/programming/ecosystem/docs/hosting.md` first. The VM, its ports and
`ninjam-room` are chalkwalk's.

- [ ] **Deploy**: Caddy, ports 443 and 80 in both the security list and iptables, a
      systemd unit after `ninjam.service`, `ninjam-room open|close` starting the bot.
- [ ] **DNS**: `ninjam.chalkwalkmusic.com` (chalkwalk edits DNS).
- [ ] **A dress rehearsal** with two or three people before the first public jam.
- [ ] **Measure a real jam** (CPU, memory, network) and replace the estimates in
      `DESIGN §8` with the numbers (`PRINCIPLES §5`).
