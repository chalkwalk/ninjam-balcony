# Completed

Done work, with what was learned. Newest last. The open work is `ROADMAP.md`.

## Prior art (2026-11-09)

The question: does an open Ninjam-to-web listener exist that this should contribute to
instead? **No.** What was found, and why each is not it:

| What | What it does | Why not |
|---|---|---|
| `ninjamcast` (justinfrankel/ninjam, `ninjam/ninjamcast`; Brennan Underwood, from the command-line client) | A bot that joins a room as `njcast`, mixes with `NJClient`, encodes MP3 with LAME and feeds a SHOUTcast/Icecast server; sets the stream title from who is playing; reconnects on a timer | Listen-only (no chat), seconds of delay through the stream server, no meters, no loudness matching; built on `NJClient` and WDL (GPLv2), the dependency pile `chalkwalk-ninjam` exists to avoid. How rooms' public streams were made. A hint at size and shape only; no code is taken from it |
| Icecast streams of public rooms (mentioned by the VCV Rack "AK Audio Ninjam" module and by Jamtaba's "live streams" for some servers) | `ninjamcast`-style feeds for particular public servers | Not software we could run; same limits |
| NinjamZap (ninjamzap.com; open core `jacinside/ninjamzap-core`, GPLv2) | iOS client; advertises "Live Web Streams" ("watch active jam sessions from any browser, no app needed") and web chat where "every message reaches the room"; video on its own server fork | The web side is not in the open repositories and is not documented; nothing says it serves rooms outside NinjamZap's hosting or can be self-hosted. Worth watching: the nearest thing to this project in intent |
| ninjam-js (BHSPitMonkey) | JavaScript client; connects, receives, chats | Needs raw TCP, so Electron or a Chrome App: a client, not a web page |
| webjam (bill-auger, 2013) | A native client with a browser front end, from the curses client | Six commits, dormant; a client for one player, not a listener for many |
| ninbot.com | Server list and session recordings | No live listening found |

Leads not followed: the Cockos NINJAM forum's threads were not searched one by one; a
forum post announcing a web listener would have surfaced in the searches above, and none did.

Sources: <https://github.com/justinfrankel/ninjam> (and the local copy in
`/home/programming/ninjam_plugin/references/ninjam/ninjam/ninjamcast/`),
<https://www.ninjamzap.com/>, <https://github.com/jacinside/ninjamzap-core>,
<https://library.vcvrack.com/akaudio/Ninjam>, <https://github.com/BHSPitMonkey/ninjam-js>,
<https://github.com/bill-auger/webjam>, <https://ninbot.com/>.

## Can the VM mix a room in the gap? (2026-11-09, measured in antiphon)

Measured by an antiphon session before this repository took the question over,
and moved here on 2026-11-10 (antiphon keeps a one-paragraph pointer). The
question: decode every channel of an interval, resample, sum and encode one
stereo stream (128 kbit/s Vorbis then; the bot encodes Opus), timed together on
one thread, over the 2025-08-06 jam at 100 bpm / 16 bpi (690 intervals of
9.6 s). `--stack n` decoded each clip n times to stand in for a bigger room.

| Room | Channels per interval | Dev machine: median / worst | VM: median / 95th / worst |
|---|---|---|---|
| as recorded | median 2, max 3-4 | 1.29% / 1.51% (120 ms) | 2.65% / 2.91% / 3.20% (307 ms) |
| stack 5 | median 10, max 15-20 | 2.25% / 2.97% | 4.33% / 5.51% / 6.65% (639 ms) |
| stack 11 | median 22, max 33-44 | 3.77% / 6.25% (600 ms) | 6.85% / 9.33% / 11.85% (1137 ms) |

- **Dev machine:** Ryzen 9 5900HX, -O2, load about 1.4. **VM:** the Oracle A1
  (Neoverse-N1), gcc 15.2 at -O2, load about 0.2, room closed, one of its two
  cores. On the VM the bench was a ~150-line JUCE-free program over
  chalkwalk-ninjam's Vorbis codec, built by hand (no JUCE on the live room's
  host), calibrated against antiphon's `antiphon-stems --bench` on the dev
  machine back to back: median 1.42% against 1.44% as recorded.
- **The VM is about 1.5 to 1.9 times slower** than the dev machine per core.
- **Memory is flat in room size:** 38-40 MB peak resident from 2 to 33
  channels, because each clip is decoded, placed and released before the next.
- **What it means:** even a 44-channel room costs about a second of a 9.6 s
  interval on one VM core, leaving the other to `ninjamsrv`; no redesign. The
  bot's own cost, measured since, is in *The first real mix* below and in
  DESIGN §8; decode and resample dominate, as here.

## The first real mix (2026-11-10)

`balcony-render` over the 2025-04-02 improv archive (four players, 113 minutes, 698
intervals at 100 bpm / 16; clips at 88.2 kHz, resampled to 48 kHz), on this machine under
load (load average about 12, other agents running), through the mixer at the 60 ms block
size the bot will use.

| | First values (A) | Gentler (B, now the default) |
|---|---|---|
| Window, slew | 60 s, 1 dB/s | 120 s, 0.5 dB/s |
| Gain range | +/-12 dB | -12..+9 dB, no boost below -50 LUFS |
| Mix integrated | -15.9 LUFS | -15.9 LUFS |
| Mix peak | -1.00 dBFS | -1.00 dBFS |
| Decode + resample | 4.6 % of one core | 3.6 % |
| Mix (rider, pan, sum, limiter) | 0.7 % | 0.6 % |

- **The targets hold**: -16 LUFS within 0.1, and the limiter's ceiling is never passed.
- **The cost is the decode and the resampling**, about 0.9-1.2 % of a core per channel on
  this x86 under load; the mix itself is under 1 % for the whole room. A 32-channel room
  would be roughly a third of one core here; the VM's Arm cores are to be measured.
- **What the trace showed**: real channels sit anywhere from -14 to -44 LUFS over a
  minute, and swing by 20 dB as players rest and return. At 1 dB/s the rider followed
  those swings within seconds, which is pumping by another name; at 0.5 dB/s over 120 s it
  follows the evening instead of the phrase.
- **Open**: at about 32 minutes the whole band went quiet together (every channel near
  -40 LUFS) and both variants lifted it: every channel to its boost cap, and the master
  about +9 dB on top. Whether a quiet passage of the whole band should stay quiet is a
  question for the owner (the period's Questions).

Renders, local only: `work/render-2025-04-02-A-fast.wav`, `work/render-2025-04-02-B-gentle.wav`
(48 kHz float stereo); trace of B in `work/render-B.log`.

## Built in the first autonomous stretch (2026-11-09/10)

Each verified as stated; the commits carry the detail.

- **DESIGN §9 settled**: libopus (system), a first-party WebSocket/HTTP server, the
  wire format, WebCodecs on the page.
- **Skeleton** on `chalkwalk-ninjam`, `chalkwalk-dsp` and `chalkwalk-jambot` as
  submodules, Catch2, `BALCONY_TESTS` (ninjam forces `BUILD_TESTING` off).
- **Offline mix** of an archived jam: archive parser (in ninjam), sliding loudness,
  limiter and a windowed-sinc resampler (in dsp; the archive's clips are 88.2 kHz).
- **Replay to the browser**: Opus, WebSocket, status, the closed and open page, meters
  moving with what is heard. Checked in headless Chrome against a replayed archive.
- **The socket client in jambot** (`SocketClient`), and volume/pan on
  `BotClient::Channel`. Tested against a scripted server and live against a loopback
  `ninjamsrv`. **Found:** the stock server closes a connection on any frame payload
  over 16 KiB; audio now goes in 8 KiB frames, and ninjam's `PROTOCOL.md` says so.
- **Live in a room**: join message (and privately to later arrivals), chat both ways,
  batched arrivals, opt-out and back, sent away, admin kick and quiet, neutral votes.
  Checked live with three fake players and two web listeners. **Found:** (1) jambot's
  handles keep `@ip`, so the balcony addresses by display name; (2) the stock server
  swallows unknown `!` commands, so admin commands are words (`DESIGN §5`); (3) the
  cautious vote rule can block a vote when the bot is counted, hence
  `--vote-threshold` (`DESIGN §4`); (4) a disconnect dropped the goodbye.
- **Admin commands by private message, live (2026-11-11)**: the scene in
  `test/browser/README.md` against a loopback `ninjamsrv`, with two web listeners.
  `/msg balcony !quiet`, `!unquiet` and `!kick Pat` took effect: the page paused and
  resumed chat and ended Pat's view. "balcony, quiet" and "balcony, kick Pat" in the
  room got the private hint and did nothing. A bare `!quiet` in the room never reaches
  the bot, because the server answers it privately as an unknown command.
- **Deploy kit** in `deploy/`: the unit (attached to `ninjam.service`, so
  `ninjam-room` is unchanged), Caddy, the steps.
- **Deployed (2026-11-11)** on the VM: built there (aarch64; the library suites pass),
  the hidden `balcony` account, Caddy with a Let's Encrypt certificate for
  `ninjam.chalkwalkmusic.com`, ports 80/443 in the security list and iptables. Tried
  with the room open: the owner listened in a browser, heard a fake player's tone, and
  chat went both ways. One player, one listener: the bot about 28 MB and 2 % of a core.
- **First rehearsal (2026-11-11)**: the owner playing (Antiphon) and four practice bots
  sent from the dev machine with `jambot-band` (chalkwalk-jambot, branch `band-runner`),
  two web listeners on different machines. Every channel metered in the stream; the bot
  about 61 MB and 2 % of a core, the VM 91 % idle. **Found:** an older Antiphon joins and
  declares its channel but sends nothing until Sync and a transport restart; the
  balcony shows the player and streams silence. The session archive's `clipsort.log`
  (no `user` lines) and a probe client told that apart from a balcony fault in minutes.

## The mixed-and-mastered chain, built and measured (2026-11-10)

Built at the owner's ask (DESIGN §6.1), as a second chain beside the first so the two
can be compared blind. The library half (compressor, tilt, one-pole split, small room,
a meter with two windows) is on chalkwalk-dsp's `balcony` branch.

**Calibrated over all 12 archived jams** (2024-07 to 2025-08, tilt off): the usual
low/high balance about 700 Hz is 5.2 dB (median; 2.9 to 9.0). At a +6 dB master
boost cap the master sat on its cap and the jams came out near -16 LUFS; the cap is
+10 and the master aims at -13 before the glue.

**The 2025-06-04 jam, first 130 minutes** (four players; after about 146 minutes the
room is empty and the server archives nothing but silence), this machine, quiet:

| | Current (B) | Mastered (D) | Mastered + unmasking (E) |
|---|---|---|---|
| Integrated | -15.9 LUFS | -14.9 LUFS | -14.9 LUFS |
| 3 s loudness, 10th / 50th / 95th | -21.8 / -16.5 / -13.1 | -20.1 / -15.1 / -13.0 | -20.1 / -15.1 / -13.0 |
| Spread (95th - 10th) | 8.7 LU | 7.1 LU | 7.1 LU |
| Peak | -1.00 dBFS | -1.50 dBFS | -1.50 dBFS |
| Mix, of one core | 0.42 % | 1.25 % | 1.55 % |

- **Louder and more even**, as asked: a dB up on the whole, 1.6 LU less spread,
  mostly by lifting the quiet tenth.
- **The limits barely work:** the glue takes 1.3 dB on average; the soft limit more
  than 1 dB in 0.2 % of blocks, the brick wall in none. The room sits 22 dB under
  the dry sum.
- **The cost is still the decode** (about 3 % of a core); the mastered mix triples
  the mix's part to about 1.3 %, unmasking adds 0.3 %.
- Excerpts for listening, local: blind round `mastered-chain` (four one-minute
  moments, current vs mastered and mastered vs unmasked) and browse set
  `ninjam-balcony-mastered` (80-88 min, all three).

**The owner's verdict, blind (2026-11-10):** the mastered chain over the current one
in all four pairs; mastered against mastered-with-unmasking, two ties and one marginal
choice each way ("very very close", "can't hear a difference"). Unmasking's dips were
real, traced over the same 130 minutes at mostly 0.5-1.5 dB and at times the full 2 dB,
so it is inaudible at that depth rather than idle. **The mastered chain is the default;
unmasking stays off.** (The Q1 round, the master rider over a quiet passage, came out
current 2, band dynamics 1, one tie, and is superseded by this.)


## Decisions confirmed, and the libraries readied (2026-11-10, evening)

- **The owner went through D1-D13** of the first stretch: all kept but two. **D4**: Opus
  is vendored (`modules/opus`, v1.6.1, static), so no machine needs `libopus-dev`.
  **D11**: admin commands are private messages only (`!kick <name>`, `!quiet`,
  `!unquiet`); one typed in the room gets a private hint instead. Q2: the admin is
  `danielm`, and the account's `User` line goes on the VM at deploy.
- **jambot's `balcony` branch was rebuilt on GitHub's `main`.** It had been made on a
  local `main` carrying four unpushed September commits; those stay local (owner).
- **Built standalone, the jambot branch did not compile**: its pinned chalkwalk-dsp
  predated the resampler, and inside the balcony the balcony's own dsp hid that. Bumped.
  Its tests also used `MSG_NOSIGNAL`, which macOS lacks; jambot's CI builds macOS.
  Lesson: a library branch is verified by building it alone, as its CI does, not only
  from the app.
- **Anonymous handles fixed in jambot**: `handleOf("dave@1.2.3.x")` is `dave`.
