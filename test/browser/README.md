# Browser checks

Headless checks of the page against a running bot, driven over the browsers' own
debugging protocols from node 22 (its built-in `WebSocket`; no packages). They
are run by hand, not by `ctest`: they need a browser, and the live one a
`ninjamsrv`. Everything stays on loopback.

Start a bot on `127.0.0.1:18080` serving the page:

```sh
build/balcony --replay work/<archive> --loop --listen 127.0.0.1:18080 --page web/
```

Chrome (`google-chrome --headless=new --remote-debugging-port=9333
--user-data-dir=<scratch> --autoplay-policy=no-user-gesture-required about:blank`):

| Script | What it checks |
|---|---|
| `drive.mjs` | join, sound buffered, meters moving, chat echoed |
| `drive-retry.mjs` | the bot killed and restarted: the page rejoins (env `KILL_BOT`, `START_BOT`: shell commands) |
| `drive-closed.mjs` | no bot, the page served statically on `:18081`: the closed view, the next jam in London time, the `.ics` |
| `drive-a11y.mjs` | the accessibility tree in the join and listening views: every control named, headings, one live log, no meters exposed |
| `drive-live.mjs` | with `--server`: two listeners through the live scene below |

Firefox (`firefox --headless --profile <dir> --remote-debugging-port 9334
--remote-allow-system-access about:blank`; with the snap, the profile must be under
`~/snap/firefox/common/`): `drive-ff.mjs` joins over WebDriver BiDi and reports
`window.balconyStats`. Headless Firefox has no audio device here, so it shows decoding,
not playback.

## The live scene

`ninjamsrv-local.cfg` is a test server: port 22049, loopback only by ACL (the server
cannot bind to one interface), voting at 75 %, and a logged-in `admin`. The binary is
built by Antiphon's `scripts/testserver.sh`. Then, together:

```sh
ninjamsrv test/browser/ninjamsrv-local.cfg
build/balcony-fake-player 127.0.0.1 22049 alice --tone 330 --at 5 "nice to have company" \
  --at 16 "balcony, leave me out" --at 26 "balcony, put me back" --at 42 "balcony, leave"
build/balcony --server 127.0.0.1:22049 --url http://balcony.test/ --admins admin \
  --vote-threshold 75 --listen 127.0.0.1:18080 --page web/
build/balcony-fake-player 127.0.0.1 22049 bob --tone 550 --at 30 "!vote bpm 100"
BALCONY_TEST_PASSWORD=testpw build/balcony-fake-player 127.0.0.1 22049 admin --login \
  --at 20 "balcony, quiet" --at 23 "balcony, chat on" --at 26 "balcony, kick Pat" \
  --at 31 "!vote bpm 100"
node test/browser/drive-live.mjs
```

The players' logs show the room's side; the script prints the page's.
