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
- **Builds from the submodules alone**: every library change is pushed and pinned, and
  Opus is vendored. `cmake -B build-$(id -un)` with no overrides (`$USER` can be empty in
  an agent's shell; `id -un` is safe).
- Tests: the full ctest passes (6 suites). The D11 change (private-only admin) is unit
  tested, not yet run live: there is no `ninjamsrv` on the machine right now
  (`antiphon/scripts/testserver.sh` builds one into /tmp/njbuild).
- The owner confirmed D1-D13 (D4 and D11 changed) and answered Q2.
- **The mix is mixed and mastered** (DESIGN §6.1), chosen blind 4 of 4. The rumble-filter
  round is scored: 100 Hz 6 dB/oct preferred 3 of 4, with little difference heard.

## Next

1. The rumble filter: the owner chose 100 Hz 6 dB/oct 3 of 4, but heard little difference; decide whether to make it the default.
2. A live check of the private admin commands once a `ninjamsrv` exists.
3. Listen to it; other browsers; then `deploy/README.md`.
