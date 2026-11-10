// Screenshots of every view at desktop and phone size, for looking at.
// Chrome on :9333; a bot on :18080 (replay); the page alone on :18081.
//   node shoot.mjs <out dir>     env STOP_BOT: shell command that stops the bot;
//   VIEWS=3,4 shoots only those (1 closed, 2 join, 3 listening, 4 ended).
import { writeFileSync, mkdirSync } from 'node:fs';
import { execSync } from 'node:child_process';
const out = process.argv[2]; mkdirSync(out, { recursive: true });
const list = await (await fetch('http://127.0.0.1:9333/json/list')).json();
const ws = new WebSocket(list.find(t => t.type === 'page').webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const waits = new Map();
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && waits.has(m.id)) { waits.get(m.id)(m); waits.delete(m.id); } };
const send = (method, params = {}) => new Promise(r => { const i = ++id; waits.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = (x) => send('Runtime.evaluate', { expression: x, awaitPromise: true, returnByValue: true });
const sleep = (s) => new Promise(r => setTimeout(r, s * 1000));
const sizes = { desktop: { width: 1280, height: 900, deviceScaleFactor: 1, mobile: false },
                phone: { width: 390, height: 844, deviceScaleFactor: 2, mobile: true } };
const views = (process.env.VIEWS || '1,2,3,4').split(',');
async function shot(name) {
  if (!views.includes(name[0])) return;
  for (const [k, m] of Object.entries(sizes)) {
    await send('Emulation.setDeviceMetricsOverride', m);
    await sleep(0.6);
    const r = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true });
    writeFileSync(`${out}/${name}-${k}.png`, Buffer.from(r.result.data, 'base64'));
  }
  await send('Emulation.setDeviceMetricsOverride', sizes.desktop);
}
await send('Page.enable'); await send('Runtime.enable');
await send('Emulation.setTimezoneOverride', { timezoneId: 'America/New_York' });
await send('Page.navigate', { url: 'http://127.0.0.1:18081/' }); await sleep(2);
await shot('1-closed');
await send('Page.navigate', { url: 'http://127.0.0.1:18080/' }); await sleep(2);
await ev(`document.getElementById('name').value='Sam'; 1`);
await shot('2-join');
await ev(`document.getElementById('join-form').requestSubmit(); 1`); await sleep(5);
for (const t of ['hi all, listening from Ohio', 'that bass line!']) {
  await ev(`document.getElementById('say').value=${JSON.stringify(t)}; document.getElementById('say-form').requestSubmit(); 1`); await sleep(1.2);
}
await sleep(+(process.env.LISTEN_SECONDS || 3));
await shot('3-listening');
execSync(process.env.STOP_BOT); await sleep(3);
await shot('4-ended');
ws.close();
