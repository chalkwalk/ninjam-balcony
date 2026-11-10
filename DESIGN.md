# ninjam-balcony -- Design

What the balcony **is**, and how it works. Why it is built this way is
[PRINCIPLES.md](PRINCIPLES.md) (`PRINCIPLES §N`); what it is for and what it
refuses is [GOALS.md](GOALS.md) (`G1`, `N1`, `A1`); what is still to do is
[ROADMAP.md](ROADMAP.md). Sections are cited as `DESIGN §N`; the numbers are
stable anchors.

*2026-11-08. Agreed with the project owner in conversation; marked **(my call)**
where the choice was Claude's and was not put to them. A companion for any
Ninjam server, built on chalkwalk-ninjam. It began as a roadmap idea in a Ninjam
client's repository, "A listener bot, as a companion repository"; this document
supersedes that sketch where they differ.*

*2026-11-09: moved from `docs/DESIGN.md` to the root, its goals, non-goals and
anti-goals moved to `GOALS.md`, and its sections numbered. Then, from the owner:
chalkwalk-ninjam as the spine, chalkwalk-jambot as the framework, shared pieces
built in jambot, and bring-up and tear-down with the server (§7, §7.1). Prior
art searched (§10).*

## 1. Why

The monthly improv (first Wednesday, 6 pm Pacific) is open to anyone at work,
but only people who install a Ninjam client come, so only players come. The
listener bot takes the barrier to listening -- and to talking to the band -- to
zero: a colleague opens `ninjam.chalkwalkmusic.com`, types a name, and hears
the jam and joins its chat. Nothing to install, no account.

Success: colleagues who would never install a client drop in, listen, say
something, and some of them come back with an instrument. The goals, non-goals
and anti-goals that follow from this are in `GOALS.md`.

## 2. What the listener gets

**The URL is public** (owner, 2026-11-09): anyone who finds it may listen and
chat while a jam runs. **When no jam is running it is one page of static
content** and nothing else -- no chat, no names, nothing that talks to the bot.

**Room closed.** A static page: no jam right now; the next one is <date and
time in the visitor's own time zone>, computed in the page (first Wednesday of
the month, 18:00 America/Los_Angeles) so it never needs editing and rolls over
the moment a jam passes. Two ways to keep it: an `.ics` download with a monthly
RRULE (`FREQ=MONTHLY;BYDAY=1WE`, 18:00 Pacific) and a Google Calendar link. The
page polls a tiny status endpoint about once a minute and switches to the
listener view by itself when the room opens **(my call;** the owner asked for
an auto-refresh every 15 minutes -- polling a status is cheaper and lets someone
waiting at 5:58 in on time).

**Room open.** Type a name (unique among current web listeners; a duplicate is
asked to choose another, **my call**), then:

- **Sound:** one stereo stream of the session, about one interval behind the
  band (as every Ninjam participant hears every other) plus under two seconds.
  The page says so in a line, so nobody thinks it is broken.
- **The room:** players, each with their channels and a real level meter, sent
  with the audio so they move with what is heard. Meters are hidden from
  assistive technology.
- **Chat:** the room's chat and the other listeners' lines, the last few
  minutes of history on arrival **(my call)**, a box to type into, and a list of
  who else is listening. Chat is an ARIA live region; player joins and leaves
  are announced.
- **Controls:** volume and mute, the listener's own. Nothing else.
- **Phones:** listening in the foreground is enough (owner, 2026-11-09). Mobile
  browsers suspend Web Audio when the screen locks; the page says so on phones,
  and resumes when brought back.

## 3. What the room gets

- The bot joins as one user, sends no audio, and says on joining what it is and
  that people are listening on the web **(etiquette, from the original sketch)**.
- **The bot is `balcony`** in the user list and in chat (owner, 2026-11-09).
- **Listener chat is prefixed by the bot**, and sent as soon as it is typed,
  within the rate limit (§5); only arrivals and departures are batched: the players see
  `<balcony> <Alice> nice groove!`. Every listener line carries the bot's name,
  so nobody can pass as a player. A `!vote` typed on the web arrives as
  `<balcony> <Alice> !vote bpm 110`, which the server reads as chat, never as
  a vote.
- **Arrivals and departures** are said in chat, **batched: at most one line per
  interval** (owner, 2026-11-09), naming everyone who came and went in it, with
  the count: `Alice and Bob joined the balcony, Carol left (7 listening)`. A drop
  and reconnect within the same interval cancels out **(my call)**.
- **Any player can leave themselves out of the stream** (owner, 2026-11-09):
  `balcony, leave me out`, and the ways people say it. The mix drops their
  channels from the next interval; the page shows them as present but not in
  the stream, with no meter; the bot confirms in the room. It lasts until they
  say `balcony, put me back` (or similar) or leave the room. Their chat still
  reaches listeners -- the room's conversation is not theirs alone to withhold.
  **(my call** on chat, and on the opt-out ending when they leave.)
- **Any player can send the balcony away**, in words (owner, 2026-11-09; the
  jambots' rule, `JAMBOT §8`). The bot tells its listeners the room asked it to
  leave, closes their streams, says goodbye in the room and exits; it does not
  come back until the room is next opened. The phrasing it recognises comes
  from jambot's addressing and language (`BotAddress`, `BotLanguage`), so
  `balcony, leave` and the ways people actually say it both work.

## 4. Votes: the bot never changes an outcome

Same rule as the practice room's bots: if the humans alone reach the threshold,
the bot adds a supporting vote; if not, it does not. The outcome is identical
to the bot's absence. Listeners never vote.

How `ninjamsrv` counts (justinfrankel/ninjam `server/usercon.cpp`, the `!vote`
handler, at f4c0eff): the threshold is `(vucnt * threshold + 50) / 100`, where
`vucnt` is every authenticated user **without the `H` (hidden) privilege** --
channels or not. So:

- **On a server we configure** (the improv room), the bot logs in with its own
  account carrying `H` (and chat). It is then outside the count and never needs
  to vote. Preferred.
- **On any other server**, it is counted, and it applies the neutral rule with
  chalkwalk-ninjam's `Voting.h` -- the arithmetic the jambots use -- not a third
  copy of it.

## 5. Moderation

Sending the whole balcony away is any player's (§3). The rest is for the
room's admins:

- **Automatic:** per-listener rate limit (a few lines per interval) and a length
  cap on lines.
- **Admin commands,** honoured only from logged-in users (names without the
  `@ip` that anonymous logins carry), from a configured list:
  `!kick <name>` drops that listener and blocks the name and connection for the
  session; `!quiet` / `!unquiet` pause and resume all web chat.
- **Listen-only:** a setting that runs a jam with web chat off from the start;
  listening still works.

## 6. The sound

The bot already decodes every player's Vorbis intervals; it mixes once and
encodes once, so the cost per listener is one stream however many play.

- **Per-channel loudness:** BS.1770 loudness of each channel over a sliding
  window of a minute or two, gated so silence does not count **(my call;** the
  owner proposed an integrated measurement -- one accumulated over the whole
  jam stops responding late in the evening, so the window is the same idea kept
  current).
- **Gain** brings each channel toward a common level, moving slowly and smoothly
  so nobody pumps; a channel that has not yet sounded gets no gain until it
  does, so a quiet player is not lifted into noise.
- **Pan** as each player set it (needs `BotClient` to carry it, §7).
- **Playout** on the bot's own free-running interval grid (jambot's
  `IntervalPump`), as every client does: a channel's interval received during
  interval N is mixed into interval N+1. Interval phase between clients is
  free, so following nobody's grid costs nothing audible.
- **Sum** aimed at about -16 LUFS, a common web listening level, through a
  limiter so it never clips **(my call)**.
- chalkwalk-dsp's `Loudness.h` (libebur128) is the instrument -- the same one
  the libraries' tests use.

**Delivery:** Opus over a WebSocket, decoded and played with Web Audio
(WebCodecs `AudioDecoder` where present; a small wasm Opus decoder otherwise).
Under two seconds of extra delay (`PRINCIPLES §4`), and the meter frames ride the same socket. The
original sketch's lean towards a plain `<audio>` element is withdrawn: it costs
seconds of delay and loses the meters' timing.

## 7. Shape and where it runs

- **This repository**, a peer of the Ninjam client and chalkwalk-jambot, not a
  library (named by the owner: the `chalkwalk-` prefix is for libraries).
  C++17 **(my call:** on chalkwalk-ninjam for the protocol, Vorbis and
  `Voting.h`; chalkwalk-dsp for loudness; libopus; a small WebSocket server).
  The page is plain HTML and JavaScript, no framework.
- **The spine is chalkwalk-ninjam** (owner, 2026-11-09). It builds and parses
  messages and decodes Vorbis, and is deliberately not a client: no socket, no
  thread. `ChannelMix.h` is the mix's starting point.
- **chalkwalk-jambot is the framework** for a bot that lives in a room. Its
  `BotClient.h` is the room as a bot sees it -- connect, chat, members and
  their channels, `setRecv`, decoded intervals delivered whole
  (`onIntervalReceived`), and timers on the client's one callback thread -- and
  the balcony is a `BotClient::Listener` like any of its bots. Its
  `IntervalPump.h` is the free-running interval grid the mix plays out on
  (§6). Its rules for how a bot behaves in a room (`JAMBOT §8`: an ordinary
  client, as easy to get rid of as any player) are the balcony's too.
- **Shared code** (owner, 2026-11-09): our libraries are C++17 and MIT. A piece
  the balcony needs that jambot should also have is built **in jambot**, with
  clean edges, and the balcony takes jambot as a submodule. If the balcony ends
  up wanting only those pieces and not the band, they move to a base repository
  both submodule.
- **The first shared piece is the client.** `chalkwalk-ninjam` is the protocol
  but deliberately no socket; jambot has the interface but, in its own words,
  "no client yet. A JUCE-free socket client is what stands between this library
  and `jambot --connect host:port`." The only implementation of `BotClient` is
  inside the GPLv3 plugin, on JUCE. So a JUCE-free, headless
  `BotClient::Client` over a socket -- login, keepalive, interval assembly and
  Vorbis decode, chat -- is built in jambot, and both the balcony and a
  command-line band use it.
- **What `BotClient` lacks for the balcony:** each channel's volume and pan as
  its player set them (`chalkwalk-ninjam` already parses both from
  `USERINFO_CHANGE_NOTIFY`; `BotClient::Channel` does not carry them). Added to
  the interface in jambot, not worked around here.
- **On the improv VM** (Oracle A1.Flex, 2 cores, 12 GB, San Jose), beside
  `ninjamsrv`, connecting to it on loopback.
- **Caddy**, always on, serves the static page over HTTPS with automatic
  certificates and, while the room is open, proxies the WebSocket and status to
  the bot. Port 443 (and 80 for the certificate challenge) open in both the
  Oracle security list and the VM's iptables.
- **`ninjam-room open` / `close`** start and stop the bot with the server; the
  bot runs as its own systemd unit, after `ninjam.service`.

### 7.1 Up and down with the server

The URL is fixed and always answers; the bot exists only while the room is
open. So the page and its status never depend on the bot running:

- **Always on (Caddy):** the static page, and the status endpoint's answer when
  the bot is not there. Caddy proxies status to the bot and, when the bot does
  not answer, returns a static "closed" status itself **(my call)**. The page
  computes the next jam's date on its own (`DESIGN §2`), so a closed room needs
  nothing from the VM but the page.
- **Bring-up:** `ninjam-room open` starts the server, waits until it accepts
  connections, then starts the bot. The bot reports "open" only once it is in
  the room and receiving **(my call)**. Waiting pages switch over on their next
  poll.
- **No reconnecting.** `BotClient::Client::disconnect` is terminal and there is
  no retry, by jambot's rule: a bot that reconnects is a bot nobody can get rid
  of. If the bot is kicked, or its connection drops, it tells its listeners and
  exits; only `ninjam-room open` (or a person restarting the unit) brings it
  back. A dropped connection mid-jam therefore ends the balcony for that jam
  unless someone restarts it **(my call;** the alternative, an automatic rejoin
  after a drop but never after a kick, needs the bot to tell the two apart, and
  the server does not say).
- **Tear-down:** `ninjam-room close` stops the bot first, so it can tell its
  listeners the jam has ended and close their sockets cleanly, then the server
  **(my call)**. If the server goes first (a crash, or closed by hand), the bot
  says the same, reports "closed", and exits; systemd does not restart it into
  an empty room.
- **A restart mid-jam** (the bot crashes, or is redeployed) costs listeners a
  reconnect: the page retries on its own, and the room sees the bot leave and
  rejoin once.

## 8. Will it fit the machine

Estimates to be measured in the build, not promises (`PRINCIPLES §5`):

- **CPU:** Vorbis decode is of the order of 1% of a core per channel; a full room
  (16 players, 2 channels each) is roughly a third of one core. Loudness and
  the mix are less. One Opus encode for everyone is a few percent. `ninjamsrv`
  relays without decoding. The VM has two cores.
- **Memory:** tens of megabytes against 12 GB.
- **Network:** listeners at about 96-128 kbit/s each -- 50 listeners is about
  6 Mbit/s. The players' own traffic through `ninjamsrv` is tens of Mbit/s at
  worst. The VM has 2 Gbit/s, and a monthly two-hour jam is a few GB of the
  10 TB a month the free tier allows.

## 9. Left to the builder

Each of these is decided here, in this document, before it is built:

- WebSocket and Opus libraries; the wire format between bot and page.
- The headless client's interface: `BotClient.h` as it stands, or a narrower
  one it can grow into (`DESIGN §7`).
- Exact rate limits, line cap, history length, window length and gain slew.
- How the page polls status, and what it shows while connecting.
- The bot's username and join message wording.

## 10. Prior art

Searched 2026-11-09 (`docs/COMPLETED.md`, *Prior art*). Nothing open does what
`GOALS.md` asks, so there is nothing to contribute to instead:

- **ninjamcast**, in the reference Ninjam source (`ninjam/ninjamcast`): a bot
  that joins a room and feeds an MP3 mix to a SHOUTcast server. The ancestor of
  the "public streams" some rooms ran. Listen-only, seconds of delay, no chat,
  no meters, no loudness matching, and built on `NJClient` and WDL (GPLv2): the
  dependency pile a small, clean `chalkwalk-ninjam` exists to avoid. It is
  read for the size and shape of the job (how little a listening bot needs,
  what it reconnects and when) and **nothing is taken from it**: no code, no
  structure copied line for line, as with every reference Ninjam source
  (`chalkwalk-ninjam` README, *Provenance*).
- **NinjamZap** (an iOS client with a hosted service) advertises live web streams
  and web chat that "reaches the room". Its open core (GPLv2) is the client;
  how the web side works, and whether it serves rooms outside its own hosting,
  is not published. Not self-hostable as far as can be seen.
- **ninjam-js** and **webjam** are clients, not listeners: the first needs
  Electron or a Chrome App for raw TCP; the second (2013, six commits) is a
  native client with a browser front end.

