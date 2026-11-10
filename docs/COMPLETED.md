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
