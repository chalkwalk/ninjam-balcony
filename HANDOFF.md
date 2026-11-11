# Handoff -- where things stand

Rewrite *State* and *Next* when you stop. The open work is [ROADMAP.md](ROADMAP.md).

## State, 2026-11-10 (evening)

**Works locally; not deployed; nobody has listened yet.** Two autonomous stretches on
11-09/10 (chalkwalk's, then mounty's; plans and logs in `.autonomy/PLAN.md`, untracked;
summaries in `docs/COMPLETED.md`).

- `balcony --server host:port` joins a room and streams it; `balcony --replay <archive>`
  plays a session archive as if live. Both serve the page with `--page web/` for local
  runs. `balcony-render` mixes an archive to WAV; `balcony-fake-player` stands in for a
  player against a local `ninjamsrv` (`--at T "/msg <user> <words>"` sends privately).
- **Build needs the ninjam and jambot branches** until they are pushed (waiting on the
  owner's OK); chalkwalk-dsp is merged, and Opus is vendored:
  ```sh
  cmake -B build-$(id -un) -DCHALKWALK_NINJAM_DIR=/home/programming/chalkwalk-ninjam-balcony \
    -DCHALKWALK_JAMBOT_DIR=/home/programming/chalkwalk-jambot-balcony
  ```
  (`$USER` can be empty in an agent's shell; `id -un` is safe.)
- Tests: the full ctest passes (6 suites). The D11 change (private-only admin) is unit
  tested, not yet run live: there is no `ninjamsrv` on the machine right now
  (`antiphon/scripts/testserver.sh` builds one into /tmp/njbuild).
- The owner confirmed D1-D13 (D4 and D11 changed) and answered Q2.
- **The mix is mixed and mastered** (DESIGN §6.1), chosen blind 4 of 4. A blind round on
  the rumble filter (`rumble-filter`) waits in blindweb.

## Next

1. The owner: OK to push the ninjam and jambot `balcony` branches; then bump the
   submodules and drop the `displayName` workaround (ROADMAP).
2. The owner: score `rumble-filter`.
3. A live check of the private admin commands once a `ninjamsrv` exists.
4. Listen to it; other browsers; then `deploy/README.md`.
