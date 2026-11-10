// Two web listeners in turn against the live balcony, on a timeline (seconds).
const T0 = Date.now(); const at = (s) => new Promise(r => setTimeout(r, Math.max(0, T0 + s * 1000 - Date.now())));
const list = await (await fetch('http://127.0.0.1:9333/json/list')).json();
const ws = new WebSocket(list.find(t => t.type === 'page').webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const waits = new Map();
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && waits.has(m.id)) { waits.get(m.id)(m); waits.delete(m.id); } };
const send = (method, params = {}) => new Promise(r => { const i = ++id; waits.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (x) => (await send('Runtime.evaluate', { expression: x, awaitPromise: true, returnByValue: true })).result.result?.value;
const snap = async (label) => console.log(`--- ${label} @${((Date.now() - T0) / 1000).toFixed(1)}s`, JSON.stringify(await ev(`({
  view: ['closed','join','listening','ended'].filter(v => !document.getElementById(v).hidden).join(),
  players: document.getElementById('players').innerText.replace(/\\n/g,' | '),
  listeners: document.getElementById('listeners').innerText.replace(/\\n/g,' | '),
  buffer: document.getElementById('buffer').textContent,
  log: document.getElementById('log').innerText.split('\\n').slice(-6),
  ended: document.getElementById('ended-why').textContent,
})`), null, 0));
const join = async (name) => { await send('Page.navigate', { url: 'http://127.0.0.1:18080/' }); await new Promise(r => setTimeout(r, 1500));
  await ev(`document.getElementById('name').value=${JSON.stringify(name)}; document.getElementById('join-form').requestSubmit(); 1`); };
const say = (t) => ev(`document.getElementById('say').value=${JSON.stringify(t)}; document.getElementById('say-form').requestSubmit(); 1`);
await send('Runtime.enable'); await send('Page.enable');
await at(1); await send('Page.navigate', { url: 'http://127.0.0.1:18080/' }); await at(2.5); await snap('before join');
await at(4); await join('Pat');
await at(9); await say('hello band, Pat here'); await snap('Pat listening');
await at(17); await snap('alice left out');
await at(21); await say('can you hear me while quiet?'); await at(21.5); await snap('quiet');
await at(27); await snap('Pat after kick');
await at(29); await join('Quinn');
await at(35); await snap('Quinn, after vote');
await at(43); await snap('Quinn after sent away');
ws.close();
