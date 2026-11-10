# Principles

How to decide. When a proposal conflicts with one of these, the proposal bends, or the
principle is changed deliberately, in its own commit, with the reason given. Cite them as
`PRINCIPLES §N`; the numbers are stable anchors. What the project is for, and what it
refuses, is in [GOALS.md](GOALS.md) (`G1`, `N1`, `A1`); how it works is
[DESIGN.md](DESIGN.md).

> Draft, 2026-11-09, drawn from the design agreed with the owner on 2026-11-08. Not yet
> accepted by the owner. Where the draft goes beyond what that conversation settled, it says
> **(draft)**.

## North star

Anyone with the link can hear the jam and talk to the band, and the room is no different for
it except that it has an audience it knows about.

When two principles pull apart, the reading that leaves the players' jam exactly as it would
have been, while letting one more person in to hear it, wins. The room is the host; the
balcony is a guest.

## The gate

Before a proposal reaches `DESIGN.md`, it answers each of these, and the principle named owns
the test:

1. **Guest** -- does the room behave exactly as it would without the bot? (§1)
2. **Protocol** -- does it work with a stock `ninjamsrv` and every stock client? (§2)
3. **Cost** -- does a new listener cost bytes, or CPU? (§3)
4. **Delay** -- what does it add to the time between the band and the listener? (§4)
5. **Evidence** -- what is the number, and how was it measured? (§5)
6. **Names** -- can anyone now speak as someone they are not, or vote? (§6)
7. **Memory** -- does it keep anything about a listener after they leave? (§7)
8. **Reader** -- is every control named, every arrival and line announced, and the meters
   silent to a screen reader? (§8)
9. **One home** -- does every rule it needs (protocol, votes, loudness) have exactly one? (§9)
10. **Page** -- does the page stay plain HTML and JavaScript? (§10)
11. **Footprint** -- does it fit beside `ninjamsrv` on the free VM? (§11)
12. **Goals** -- does it clear `GOALS.md`, the anti-goals above all?

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
nobody has touched it for a year. **(draft)**

## §11 Small enough for the free VM

The bot shares a 2-core Always Free VM with `ninjamsrv` and must never make the room worse
to play in. If it ever competes with the server for CPU, the server wins and the balcony
degrades (fewer meter frames, then a lower bitrate) before the players notice anything.
**(draft)**
