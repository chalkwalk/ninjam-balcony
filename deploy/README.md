# Deploying the balcony beside the improv room

**Deployed 2026-11-11** on the Oracle VM `ninjam`
(the maintainers' hosting notes have its ports and services), by these steps, and tried with the
room open: a player's tone heard in a browser at https://ninjam.chalkwalkmusic.com/,
chat both ways. The VM, `ninjam-room` and DNS are chalkwalk's; both accounts here
reach the VM with their own `~/.ssh/oracle-ninjam` (`ssh ninjam-vm`), and opening or
closing the room stays the owner's call.

On the VM, each file changed has its original beside it: `improv.cfg.before-balcony`,
`/etc/caddy/Caddyfile.default`, `/etc/iptables/rules.v4.before-balcony`. The source
is cloned at `~ubuntu/balcony-src`.

## Updating

```sh
ssh ninjam-vm
cd ~/balcony-src && git pull --recurse-submodules && git submodule update --init --recursive
nice cmake --build build -j2
sudo install -m 755 build/balcony /opt/balcony/bin/balcony && sudo cp -r web/. /opt/balcony/web/
```

Install while the room is closed; the next `ninjam-room open` runs the new bot. A
changed `deploy/balcony.service` or `Caddyfile` is copied to its place by hand
(`systemctl daemon-reload`, `systemctl reload caddy`).

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

1. **The libraries** are pushed and pinned; the balcony builds from its submodules.
2. **The source**: `git clone --recurse-submodules
   https://github.com/chalkwalk/ninjam-balcony.git ~/balcony-src`.
3. **Build on the VM** (aarch64, about two minutes on 2 cores; Opus is vendored):
   ```sh
   sudo apt install build-essential cmake git
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

## Measured on the first try

One player, one listener: the bot about 28 MB resident and 2 % of a core, the VM
95 % idle. A real jam's numbers are still to come (`ninjam-room metrics`).
