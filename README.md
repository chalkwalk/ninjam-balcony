# ninjam-balcony

A listener bot for any Ninjam server: it joins the room as one user, and anyone
with the link can hear the jam in a browser and talk to the band, with nothing
to install. Seats in the balcony: you listen and you can call out, but you
don't play.

**Status: working locally, not yet deployed.** It joins a room, streams it to
the page and carries chat both ways, tested against a local `ninjamsrv`; it
has not yet run beside the real room.

## What it will do

- **One link.** Between jams it says when the next one is, in your own time
  zone, with a calendar invite. When the room opens, the page becomes the
  listener by itself.
- **Listen.** Type a name and hear one even mix of the whole band, about one
  interval behind them (as every Ninjam player hears every other), with each
  player's level meters moving with the sound. The mix is evened out and
  mastered automatically: every player brought to one level and caught quickly
  if they suddenly turn up, rumble filtered out, players spread a little
  across the stereo field and set in one quiet shared room, and the whole
  gently compressed and limited to a steady listening level, about -14 LUFS.
- **Nothing else.** No account, no install, no recording. Listeners don't
  mix for themselves, play or vote, and the bot never changes how the room's
  votes come out.
- **Talk to the band.** Your lines reach the room's chat as
  `<balcony> <you> ...`; theirs reach you. The players see who is listening.
- **The players decide.** Any player can leave themselves out of the stream
  (`balcony, leave me out`, and `balcony, put me back`) or send the balcony
  away for the rest of the jam (`balcony, leave`). Everyone who arrives is told
  the balcony is there. The room's admins can remove a listener (`!kick <name>`)
  or pause web chat (`!quiet`, `!unquiet`), by private message to the balcony.

## The documents

- [PRINCIPLES.md](PRINCIPLES.md) -- how decisions are made here.
- [GOALS.md](GOALS.md) -- goals, non-goals and anti-goals.
- [DESIGN.md](DESIGN.md) -- how it works.
- [ROADMAP.md](ROADMAP.md) -- what is left to do.
- [HANDOFF.md](HANDOFF.md) -- where things stand right now.

MIT licensed ([LICENSE](LICENSE)), like the chalkwalk libraries it is built on; what it
builds in is listed in [THIRDPARTY.md](THIRDPARTY.md).
