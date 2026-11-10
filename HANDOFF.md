# Handoff -- where things stand

Rewrite *State* and *Next* when you stop. The open work is [ROADMAP.md](ROADMAP.md).

## State, 2026-11-09

Designed, not built. No code yet.

- `DESIGN.md` is the design agreed with the owner on 2026-11-08 (moved to the root and
  numbered on 11-09; no decision changed).
- `PRINCIPLES.md` and `GOALS.md` accepted by the owner on 11-09, after a round of questions:
  a public URL that is one static page between jams; the bot named `balcony`; any player
  can send it away or leave themselves out; arrivals batched per interval, listener chat
  immediate; phones in the foreground only; the delay budget two seconds (`PRINCIPLES §4`).
- Prior art searched 11-09: nothing to contribute to instead (`DESIGN §10`,
  `docs/COMPLETED.md`).
- chalkwalk-jambot is the framework; the socket client and volume/pan on `BotClient` are to
  be built there first (`DESIGN §7`).
- The repository is local only (no remote yet). It goes to GitHub when the owner decides
  it is ready, probably under GPLv3; the licence is decided then.

## Next

1. Decide what `DESIGN §9` leaves open (WebSocket and Opus libraries, wire format).
2. The skeleton, then mixing an archived jam offline.
3. In jambot: the socket client, volume and pan on `BotClient`, the balcony's phrases.
