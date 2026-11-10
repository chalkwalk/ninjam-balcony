# Handoff -- where things stand

Rewrite *State* and *Next* when you stop. The open work is [ROADMAP.md](ROADMAP.md).

## State, 2026-11-10

**Works locally; not deployed; nobody has listened yet.** Built in an autonomous stretch
on 11-09/10 (log and open questions in `.autonomy/PLAN.md`, untracked; summary in
`docs/COMPLETED.md`).

- `balcony --server host:port` joins a room and streams it; `balcony --replay <archive>`
  plays a session archive as if live. Both serve the page with `--page web/` for local
  runs. `balcony-render` mixes an archive to WAV; `balcony-fake-player` stands in for a
  player against a local `ninjamsrv`.
- **Build needs the library branches**, which are local only:
  ```sh
  cmake -B build -DCHALKWALK_DSP_DIR=/home/programming/chalkwalk-dsp-balcony \
    -DCHALKWALK_NINJAM_DIR=/home/programming/chalkwalk-ninjam-balcony \
    -DCHALKWALK_JAMBOT_DIR=/home/programming/chalkwalk-jambot-balcony
  ```
- Tests: the balcony's suite and the page's schedule test pass; jambot's socket client
  has its own. The live behaviour was checked by hand against a loopback `ninjamsrv`
  (config: ACL allows 127.0.0.0/8 only), with fake players and headless Chrome.
- Decisions made alone and open to reversal are D1-D13 in `.autonomy/PLAN.md`; the
  ones that change the design are in `DESIGN §4`, `§5`, `§9.6`.

- **The mix is mixed and mastered** (DESIGN §6.1), the default since the owner chose
  it blind 4 of 4; the first chain is `balcony-render --plain`. Unmasking is built
  and off (inaudible in the blind round). Q1 is superseded.

## Next

1. Q2 in the period plan (the bot's account and admins on the improv server).
2. Merge the three library branches; bump the submodules.
3. Listen to it; other browsers; then `deploy/README.md`.
