# Deploying the balcony beside the improv room

**Nothing here has been deployed.** These are the files and the steps for the
Oracle VM `ninjam` (`/home/programming/ecosystem/docs/hosting.md`); the VM, its
ssh key, `ninjam-room` and DNS are chalkwalk's, and every step below is theirs
to take or to approve.

| File | Goes to | What it does |
|---|---|---|
| `balcony.service` | `/etc/systemd/system/` | The bot, up and down with `ninjam.service` |
| `balcony.env.example` | `/etc/ninjam/balcony.env` (0600) | The bot's password and the admins |
| `Caddyfile` | `/etc/caddy/Caddyfile` | HTTPS, the page, and "closed" when the bot is down |

## How it fits the room

`balcony.service` is `WantedBy=` and `PartOf=` `ninjam.service`, the same way
`ninjam-metrics.service` is. So **`ninjam-room open` and `close` need no
change**: starting the room starts the bot, which waits up to 30 s for the
server to listen and then joins; stopping the room stops the bot first, which
tells its listeners the jam has ended and says goodbye in the room. If the room
sends the balcony away, it exits and stays down until the room is next opened.

Caddy runs all the time. Between jams it serves the page, and `/status`
answers `{"open":false}` because the bot is not there to answer; the page
works out the next jam by itself.

## Steps

1. **Merge the library branches** (local only so far, nothing pushed):
   `chalkwalk-ninjam`, `chalkwalk-dsp` and `chalkwalk-jambot`, branch `balcony`
   in the `/home/programming/chalkwalk-<lib>-balcony` worktrees. Then bump the
   balcony's submodules, so it builds without the `CHALKWALK_*_DIR` overrides.
2. **Get the source to the VM.** The balcony has no remote yet: push it
   somewhere (GitHub, when the licence is decided) or `rsync` the tree,
   submodules included.
3. **Build on the VM** (aarch64; the packages are a one-off install):
   ```sh
   sudo apt install build-essential cmake pkg-config libopus-dev git
   cmake -B build -DCMAKE_BUILD_TYPE=Release -DBALCONY_TESTS=OFF
   cmake --build build -j2
   sudo install -D build/balcony /opt/balcony/bin/balcony
   sudo mkdir -p /opt/balcony/web && sudo cp web/* /opt/balcony/web/
   ```
4. **The bot's account.** In `/etc/ninjam/improv.cfg`, a `User balcony
   <password> CH` line: chat (`C`) and hidden (`H`), so it is not counted in
   votes and never votes (DESIGN §4). Put the same password in
   `/etc/ninjam/balcony.env`, with the admins' logged-in names.
5. **Caddy.** `sudo apt install caddy`, the `Caddyfile` above, and ports 80 and
   443 open in **both** the VCN security list and `/etc/iptables/rules.v4`.
   `ninjam.chalkwalkmusic.com` already points at the VM.
6. **Enable the unit**: `sudo systemctl daemon-reload && sudo systemctl enable
   balcony` (enabling attaches it to `ninjam.service`; it does not start it).
7. **Try it** outside a jam: `ninjam-room open`, open the page, listen, say
   something from a client, `ninjam-room close`. Then the first real jam, with
   `ninjam-room metrics` afterwards for the bot's real cost (first release:
   measured, not estimated).

## Not checked here

- The Caddyfile has not been run (Caddy is not installed on the dev machine);
  `caddy validate --config Caddyfile` on the VM is the first check.
- `balcony.service` passes `systemd-analyze verify` apart from the binary not
  being installed.
