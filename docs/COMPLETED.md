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
