// WebDriver BiDi: join the replay in Firefox and report.
const ws = new WebSocket('ws://127.0.0.1:9334/session');
await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
let id = 0; const waits = new Map(); const logs = [];
ws.onmessage = (e) => { const m = JSON.parse(e.data);
  if (m.id && waits.has(m.id)) { waits.get(m.id)(m); waits.delete(m.id); }
  if (m.method === 'log.entryAdded') logs.push(m.params.level + ': ' + m.params.text); };
const send = (method, params = {}) => new Promise(r => { const i = ++id; waits.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const s = await send('session.new', { capabilities: {} });
if (s.error) { console.log('session error', s.error, s.message); process.exit(1); }
await send('session.subscribe', { events: ['log.entryAdded'] });
const tree = await send('browsingContext.getTree', {});
const ctx = tree.result.contexts[0].context;
const ev = async (x) => { const r = await send('script.evaluate', { expression: x, target: { context: ctx }, awaitPromise: true, userActivation: true, resultOwnership: 'none' }); return r.result?.result?.value ?? r; };
await send('browsingContext.navigate', { context: ctx, url: 'http://127.0.0.1:18080/', wait: 'complete' });
await new Promise(r => setTimeout(r, 1500));
console.log('support:', await ev(`(async () => { if (typeof AudioDecoder === 'undefined') return 'no AudioDecoder'; const s = await AudioDecoder.isConfigSupported({codec:'opus', sampleRate:48000, numberOfChannels:2}); return 'opus ' + s.supported + ', worklet ' + (typeof AudioWorkletNode); })()`));
await ev(`document.getElementById('name').value='Fox'; document.getElementById('join-form').requestSubmit(); 1`);
await new Promise(r => setTimeout(r, 6000));
console.log(await ev(`JSON.stringify({view: ['closed','join','listening','ended'].filter(v => !document.getElementById(v).hidden).join(), unsupported: !document.getElementById('unsupported').hidden, buffer: document.getElementById('buffer').textContent, players: document.getElementById('players').innerText, meters: [...document.querySelectorAll('.meter .level')].map(e => e.style.width), stats: window.balconyStats, startSound: !document.getElementById('start-sound').hidden})`));
console.log('ctx:', await ev(`(async () => { const c = new AudioContext({sampleRate:48000}); const st = c.state; await c.close(); return st; })()`));
console.log('logs:', logs.slice(0, 8));
await send('session.end');
ws.close();
