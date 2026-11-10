# Goals, non-goals, anti-goals

**Goals** are what the project is for. **Non-goals** are things it could do but chooses not
to: someone else may, and we won't spend on them. **Anti-goals** are things it must actively
avoid: doing them would make the project worse even if asked. Cite them as `G1`, `N1`, `A1`;
the numbers are stable.

> Accepted by the owner, 2026-11-09.

## Who it is for

People who would never install a Ninjam client: colleagues, friends and family of the
players, the curious. They want to hear the jam and say something to the band, from a link,
in the browser they already have. Second, the players, who get an audience they can see and
talk to without anything changing about how they play.

## Goals

- **G1. One URL.** Between jams it says when the next one is, in the visitor's own time
  zone; during a jam it is the listener. It never needs editing.
- **G2. Listen with no setup.** A name, then sound, in any current browser. No install, no
  account, no plugin.
- **G3. Chat both ways with the players**, under the listener's own name, as part of the
  room's conversation rather than beside it.
- **G4. Looks like a client without being one.** The players, their channels and live
  meters that move with what is heard; nothing to mix.
- **G5. The room is unchanged by the bot's presence.** Votes come out exactly as if it were
  not there; it takes one user slot however many listen; the players know it is there.
- **G6. Accessible.** Every control named, chat and arrivals announced, no meter noise for
  a screen reader.
- **G7. Sounds even.** Every player audible at a common loudness, the whole at a web
  listening level, never clipping.

## The first release

The monthly improv room on the Oracle VM, at `ninjam.chalkwalkmusic.com`, for one real jam.
Complete means a colleague with nothing installed can open the link at 5:58 pm, be let in
when the room opens, hear an even mix about an interval behind the band, say something the
players see, and be seen arriving and leaving. The bot's cost on the VM is measured during
that jam, not estimated.

## Non-goals

- **N1. Listeners playing, transmitting or voting.** They are in the balcony. Someone who
  wants to play installs a client; the page can say how.
- **N2. A per-listener mix.** Everyone hears the same stereo stream; the page offers only
  its own volume and mute.
- **N3. Accounts, logins, stored chat or stored audio.** The server's own session archive
  is a separate thing, outside this project.
- **N4. A general Ninjam web client.** The bot connects to one configured server.
- **N5. Recording or re-broadcasting** to other platforms (YouTube, Twitch, a podcast feed).
 

## Anti-goals

- **A1. The bot changes a vote's outcome.** Whether by voting, by being counted, or by
  letting a listener's `!vote` reach the server as a vote.
- **A2. A listener can impersonate a player, or flood the room's chat.** Every listener
  line carries the bot's name; every listener is rate-limited; the room's admins can
  silence one listener or all of them.
- **A3. Chat about "that solo" lands long after the band moved on.** Extra delay of two
  seconds or more, beyond the interval every participant already hears, is a defect
  (`PRINCIPLES §4`).
- **A4. An unannounced or unwanted audience.** Someone at work hears a jam the players did
  not know was being heard, or a player is streamed who asked not to be. The bot says what
  it is and who is listening; any player can leave themselves out of the stream, or send
  the balcony away.
- **A5. An open relay.** The bot never connects anywhere a web visitor asks it to; the
  server is configuration, not input.' (Implied by N4, and stated because it is a safety
  property, not a scope choice.)
