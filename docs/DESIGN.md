# ninjam-balcony -- design

*2026-11-08. Agreed with the project owner in conversation; marked **(my call)**
where the choice was Claude's and was not put to them. A companion for any
Ninjam server, not part of Antiphon; built on chalkwalk-ninjam. Its origin is
Antiphon's `ROADMAP.md`, "A listener bot, as a companion repository".*

## Why

The monthly improv (first Wednesday, 6 pm Pacific) is open to anyone at work,
but only people who install a Ninjam client come, so only players come. The
listener bot takes the barrier to listening -- and to talking to the band -- to
zero: a colleague opens `ninjam.chalkwalkmusic.com`, types a name, and hears
the jam and joins its chat. Nothing to install, no account.

Success: colleagues who would never install a client drop in, listen, say
something, and some of them come back with an instrument.

## Goals

- One URL. Between jams it says when the next one is; during a jam it is the
  listener.
- Listen with no setup: a name, then sound, in any current browser.
- Chat both ways with the players, under the listener's own name.
- Looks like a client -- the players, their channels, live meters -- without
  being one: nothing to mix.
- The room is unchanged by the bot's presence: votes come out exactly as if it
  were not there.
- Accessible: every control named, chat and arrivals announced, no meter noise
  for a screen reader.

## Non-goals

- Listeners do not play, transmit, or vote.
- No per-listener mix. Everyone hears the same stereo stream; the page offers
  only its own volume and mute.
- No accounts, no login, no stored chat or audio. (The server's own archive is
  a separate thing: `ninjam-room`, `jam-stems`.)
- Not a general Ninjam web client, and not an open relay: the bot connects to
  one configured server.

## Anti-goals (failure even if it works)

- The bot changes a vote's outcome.
- A listener can impersonate a player, or flood the room's chat.
- Chat about "that solo" lands long after the band moved on (extra delay over
  about a second).
- Someone at work hears a jam the players did not know was being heard (the bot
  says what it is, and who is listening).

## What the listener gets

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
  band (as every Ninjam participant hears every other) plus under a second.
  The page says so in a line, so nobody thinks it is broken.
- **The room:** players, each with their channels and a real level meter, sent
  with the audio so they move with what is heard. Meters are hidden from
  assistive technology.
- **Chat:** the room's chat and the other listeners' lines, the last few
  minutes of history on arrival **(my call)**, a box to type into, and a list of
  who else is listening. Chat is an ARIA live region; player joins and leaves
  are announced.
- **Controls:** volume and mute, the listener's own. Nothing else.

## What the room gets

- The bot joins as one user, sends no audio, and says on joining what it is and
  that people are listening on the web **(etiquette, from the roadmap)**.
- **Listener chat is prefixed by the bot:** the players see
  `<listeners> <Alice> nice groove!`. Every listener line carries the bot's name,
  so nobody can pass as a player. A `!vote` typed on the web arrives as
  `<listeners> <Alice> !vote bpm 110`, which the server reads as chat, never as
  a vote.
- **Arrivals and departures** are said in chat: `Alice is listening`,
  `Alice left`. A drop and reconnect within a few seconds is not announced
  twice **(my call)**.

## Votes: the bot never changes an outcome

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

## Moderation

- **Automatic:** per-listener rate limit (a few lines per interval) and a length
  cap on lines.
- **Admin commands,** honoured only from logged-in users (names without the
  `@ip` that anonymous logins carry), from a configured list:
  `!kick <name>` drops that listener and blocks the name and connection for the
  session; `!quiet` / `!unquiet` pause and resume all web chat.
- **Listen-only:** a setting that runs a jam with web chat off from the start;
  listening still works.

## The sound

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
- **Pan** as each player set it.
- **Sum** aimed at about -16 LUFS, a common web listening level, through a
  limiter so it never clips **(my call)**.
- chalkwalk-dsp's `Loudness.h` (libebur128) is the instrument -- the same one
  the tests and VoiceLab use.

**Delivery:** Opus over a WebSocket, decoded and played with Web Audio
(WebCodecs `AudioDecoder` where present; a small wasm Opus decoder otherwise).
Under a second of extra delay, and the meter frames ride the same socket. The
roadmap's earlier lean towards a plain `<audio>` element is withdrawn: it costs
seconds of delay and loses the meters' timing.

## Shape and where it runs

- **This repository**, a peer of Antiphon and chalkwalk-jambot (named by the
  owner: the `chalkwalk-` prefix is for libraries) **(my call:** C++, on
  chalkwalk-ninjam for the protocol, Vorbis and `Voting.h`; chalkwalk-dsp for
  loudness; libopus; a small WebSocket server). The page is plain HTML and
  JavaScript, no framework.
- **On the improv VM** (Oracle A1.Flex, 2 cores, 12 GB, San Jose), beside
  `ninjamsrv`, connecting to it on loopback.
- **Caddy**, always on, serves the static page over HTTPS with automatic
  certificates and, while the room is open, proxies the WebSocket and status to
  the bot. Port 443 (and 80 for the certificate challenge) open in both the
  Oracle security list and the VM's iptables.
- **`ninjam-room open` / `close`** start and stop the bot with the server; the
  bot runs as its own systemd unit, after `ninjam.service`.

## Will it fit the machine

Estimates to be measured in the build, not promises:

- **CPU:** Vorbis decode is of the order of 1% of a core per channel; a full room
  (16 players, 2 channels each) is roughly a third of one core. Loudness and
  the mix are less. One Opus encode for everyone is a few percent. `ninjamsrv`
  relays without decoding. The VM has two cores.
- **Memory:** tens of megabytes against 12 GB.
- **Network:** listeners at about 96-128 kbit/s each -- 50 listeners is about
  6 Mbit/s. The players' own traffic through `ninjamsrv` is tens of Mbit/s at
  worst. The VM has 2 Gbit/s, and a monthly two-hour jam is a few GB of the
  10 TB a month the free tier allows.

## Left to the builder

- WebSocket and Opus libraries; the wire format between bot and page.
- Exact rate limits, line cap, history length, window length and gain slew.
- How the page polls status, and what it shows while connecting.
- The bot's username and join message wording.

## Before building

- **Prior art** (Antiphon's `ROADMAP.md`): ninbot.com, the NINJAM forum. If a good
  Ninjam-to-web listener exists, contribute to it instead.
