# Principles

How to decide. When a proposal conflicts with one of these, the proposal bends, or the
principle is changed deliberately, in its own commit, with the reason given. Cite them as
`PRINCIPLES §N`; the numbers are stable anchors. What the project is for, and what it
refuses, is in [GOALS.md](GOALS.md) (`G1`, `N1`, `A1`); how it works is
[DESIGN.md](DESIGN.md).

> Accepted by the owner, 2026-11-09.

## North star

Anyone with the link can hear the jam and talk to the band, and the room is no different for
it except that it has an audience it knows about.

When two principles pull apart, the reading that leaves the players' jam exactly as it would
have been, while letting one more person in to hear it, wins. The room is the host; the
balcony is a guest.

## The gate

Before a proposal reaches `DESIGN.md`, it goes through each rung. A rung **clears** when the
proposal meets the test stated; anything short of it is a **fail**, and "well, except..."
is a fail. A fail means the proposal changes until it clears, or the principle named is
changed first, deliberately (see above). The principle named owns the test.

1. **Guest** (§1). Clears: with the bot in the room, every player sees the same votes'
   outcomes, the same tempo and interval, and the same audio as without it; the bot holds
   one user slot and transmits no audio. Fails: anything a player could notice beyond the
   bot's own chat lines and its one entry in the user list.
2. **Protocol** (§2). Clears: works against an unmodified `ninjamsrv` and is seen correctly
   by unmodified clients. Fails: needs a server patch, a server setting other than the
   bot's own account, or a client that understands something new.
3. **Cost** (§3). Clears: CPU and memory spent per listener are constant and small (a
   socket and its buffers); decode, mix and encode happen once for all. Fails: any
   per-listener decode, mix, encode or loudness work.
4. **Delay** (§4). Clears: the listener hears the band at most one interval plus one
   second behind, and the meters move with the audio they describe. Fails: a second or
   more of added delay, or meters that lead or lag the sound.
5. **Evidence** (§5). Clears: every claim about loudness, CPU, memory or bandwidth comes
   with a measured number, the method, and the load when it was taken; an unmeasured
   figure is labelled an estimate. Fails: a number from intuition presented as fact, or a
   loudness judged by ear.
6. **Names** (§6). Clears: every line a listener causes in the room carries the bot's
   name, a listener's `!vote` reaches the server as chat, and admin commands are taken only
   from listed logged-in users. Fails: any path by which a listener's text arrives under
   another name, counts as a vote, or acts as an admin.
7. **Memory** (§7). Clears: when a listener leaves, nothing about them stays except chat
   history held in memory for the running session. Fails: anything written to disk, logged
   with names or addresses, or kept across sessions.
8. **Reader** (§8). Clears: every new control has an accessible name, every new arrival,
   departure or chat line is announced, nothing that changes many times a second (meters)
   is exposed to assistive technology, and it has been tried with a screen reader. Fails:
   any one of those missing.
9. **One home** (§9). Clears: protocol, vote arithmetic and loudness come from
   `chalkwalk-ninjam` and `chalkwalk-dsp`, and a missing piece is added there. Fails: a
   local copy or reimplementation of something a library already has, or a library fix
   patched locally.
10. **Page** (§10). Clears: the page is HTML, CSS and JavaScript served as written, plus
    the one wasm decoder. Fails: a framework, a bundler or build step for the page, or a
    runtime dependency fetched from a third-party CDN.
11. **Footprint** (§11). Clears: measured (or, before the VM, estimated per §5) to fit in
    the VM beside `ninjamsrv` with a full room, and it degrades before the server is
    starved. Fails: a cost that grows with listeners beyond bandwidth, or anything that can
    take CPU the server needs.
12. **Goals**. Clears: serves a goal in `GOALS.md` and touches no non-goal or anti-goal.
    Fails: touches an anti-goal, however it is framed; touching a non-goal needs the
    owner's decision first.

## §1 The room is the host; the balcony is a guest

The bot takes one user slot, sends no audio, says what it is on joining, and announces who is
listening. A vote's outcome is identical to the bot's absence (`DESIGN §4`). The room's own
admins can quiet or remove listeners from inside the room, with the client they already use.

## §2 Interoperate; do not extend

The bot speaks stock Ninjam to a stock server. Nothing it offers requires a patched server,
a cooperating client, or a convention the players must learn. What the players see of the
balcony is ordinary chat.

## §3 One mix for everyone

The bot decodes, mixes and encodes once. A listener costs a share of the outgoing bandwidth
and nothing else, so fifty listeners cost the CPU of one.

## §4 The interval is the delay; everything else is budget

A listener hears the band one interval late, as every participant hears every other. Beyond
that the balcony may add under a second, and the meters ride with the sound so they move
with what is heard (`GOALS A3`).

## §5 Measure; do not guess

Loudness is BS.1770, measured with the shared instrument. CPU, memory and bandwidth claims
are measured on the VM during a real jam, and the load is noted. An estimate is labelled an
estimate until then.

## §6 Nobody can pass as a player

Every listener line reaches the room carrying the bot's name. A listener's `!vote` is chat.
Admin commands are honoured only from logged-in users on a configured list.

## §7 Keep nothing

No accounts, no stored chat, no stored audio, no listener logs beyond the running session.
History shown on arrival is the last few minutes, held in memory.

## §8 Accessible by construction

Every control has a name; chat is a live region; joins and leaves are announced; meters are
hidden from assistive technology. Checked with a screen reader before a release, not after.

## §9 One definition, shared

The protocol, the vote arithmetic and the loudness meter come from `chalkwalk-ninjam` and
`chalkwalk-dsp`. A fix belongs in the library, and this project picks it up; no local copies.

## §10 A plain page

The listener page is plain HTML and JavaScript, no framework. It must keep working when
nobody has touched it for a year.

## §11 Small enough for the free VM

The bot shares a 2-core Always Free VM with `ninjamsrv` and must never make the room worse
to play in. If it ever competes with the server for CPU, the server wins and the balcony
degrades (fewer meter frames, then a lower bitrate) before the players notice anything.
