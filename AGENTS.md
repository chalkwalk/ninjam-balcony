# AGENTS.md -- ninjam-balcony orientation

`CLAUDE.md` and `GEMINI.md` here are symlinks to this file. The workspace rules in
`/home/programming/AGENTS.md` come first (builds `-j4`, `build-$USER/` beside another
user's tree, sub-agents, autonomous periods); this file adds what is particular to this
project.

## What this project is

A listener bot for a Ninjam server. It joins the room as one user, mixes the jam to one
loudness-matched stereo stream, and serves it, with the room's chat, to any number of
browsers over a WebSocket. Built for the monthly improv room on the Oracle VM, but it must
work with any stock server. C++17 on `chalkwalk-ninjam` (the spine) and `chalkwalk-dsp`;
`chalkwalk-jambot` is the framework: its `BotClient` interface, its interval
grid, and its rules for how a bot of ours lives in a room. A plain
HTML/JavaScript page. **Designed, not built.**

The bot runs only while the room is open: `ninjam-room open` brings it up after the
server and `close` takes it down first. The URL is always up, and says when the next jam
is (`DESIGN §7.1`).

## The documents and how to use them

- **`PRINCIPLES.md`** -- how to decide: a north star, the gate, short numbered principles.
  Cited as `PRINCIPLES §N`.
- **`GOALS.md`** -- goals, non-goals and anti-goals. Cited as `G1`, `N1`, `A1`.
- **`DESIGN.md`** -- how it works. Cited as `DESIGN §N`.
- **`ROADMAP.md`** -- open work only. Done work and its findings go to
  `docs/COMPLETED.md`.
- **`HANDOFF.md`** -- where things stand. Read it before starting; rewrite its State and
  Next when you stop.
- **`JOURNAL.md`** -- made when the first autonomous stretch runs: what was tried, what
  went wrong, what to learn.
- `.autonomy/PLAN.md` (untracked) -- the working doc of an autonomous period, when one runs.

Order for new work: **PRINCIPLES -> GOALS -> DESIGN -> ROADMAP -> HANDOFF**.

## Ground rules

- **Test against a local `ninjamsrv`, never a public room.** A bot that chats is a nuisance
  in a stranger's jam.
- **The VM is chalkwalk's.** Read `/home/programming/ecosystem/docs/hosting.md` before
  deploying anything; `ninjam-room`, the VM's ssh key and DNS are chalkwalk's. Don't open
  or close the room, or touch `ninjam.service`, without the owner's say-so.
- **The session archive is the test fixture.** Most of the sound work can be built and
  measured offline against an archived jam; prefer that to a live room.
- Changes to the shared libraries go to `chalkwalk-ninjam` / `chalkwalk-dsp`, not to a
  local copy (`PRINCIPLES §9`). Our libraries are C++17 and MIT.
- **The reference Ninjam sources are read, never copied.** `NJClient`, WDL, ReaNinjam and
  `ninjamcast` may tell you the size and shape of a job; no code comes from them, verbatim
  or paraphrased line by line. `chalkwalk-ninjam` exists because that code is a dependency
  pile, and its README's *Provenance* is the rule.
- **A piece jambot should also have is built in jambot**, with clean edges and its own
  tests, and used from here through the submodule; the socket client is the first
  (`DESIGN §7`). Read `../chalkwalk-jambot/README.md` and `src/BotClient.h` before
  writing anything that talks to the room.

## Canonical commands

```sh
git submodule update --init --recursive
cmake -B build                       # build-$USER/ if someone else's build/ is there
cmake --build build -j4              # -j2 and `nice` when machine-load says BUSY
ctest --test-dir build --output-on-failure

# Quick: skip the libraries' own suites (the full run is what "verified" means)
cmake -B build -DBALCONY_LIBRARY_TESTS=OFF

# A library change on a local branch, before it is pushed and the submodule bumped
cmake -B build -DCHALKWALK_DSP_DIR=/home/programming/chalkwalk-dsp-<branch>
```

`work/` (untracked) holds local test material: copies of real session archives, which are
never committed, published or uploaded, and renders made from them.

## Documentation maintenance

`PRINCIPLES.md`, `GOALS.md` and `DESIGN.md` are cited by number from the other docs and
from source comments. **The numbers are stable anchors.** If a renumber is ever
unavoidable, sweep every reference in the docs *and* in `src/` in the same change.

A behaviour change that a listener or a player would notice reaches `README.md`, not only
a commit message.
