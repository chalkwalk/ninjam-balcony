// The accessibility tree in each view: every control named, headings in order,
// the chat a live log, and nothing of the meters (G6). Chrome on :9333, a bot on
// :18080.
const list = await (await fetch('http://127.0.0.1:9333/json/list')).json();
const ws = new WebSocket(list.find(t => t.type === 'page').webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const waits = new Map();
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && waits.has(m.id)) { waits.get(m.id)(m); waits.delete(m.id); } };
const send = (method, params = {}) => new Promise(r => { const i = ++id; waits.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (x) => (await send('Runtime.evaluate', { expression: x, awaitPromise: true, returnByValue: true })).result.result?.value;
const controls = new Set(['button', 'textbox', 'slider', 'link', 'checkbox', 'combobox', 'searchbox']);
let problems = 0;
async function audit(label) {
  const { result } = await send('Accessibility.getFullAXTree');
  const nodes = result.nodes.filter(n => !n.ignored);
  const named = (n) => (n.name?.value ?? '').trim();
  const unnamed = nodes.filter(n => controls.has(n.role?.value) && !named(n));
  const headings = nodes.filter(n => n.role?.value === 'heading').map(n => named(n));
  const live = nodes.filter(n => n.role?.value === 'log').length;
  const meters = nodes.filter(n => /meter|progressbar/.test(n.role?.value ?? '')).length;
  const ctl = nodes.filter(n => controls.has(n.role?.value)).map(n => `${n.role.value}:${named(n)}`);
  console.log(`[${label}] headings: ${headings.join(' > ')}\n  controls: ${ctl.join(', ')}\n  unnamed: ${unnamed.length}, logs: ${live}, meter nodes: ${meters}`);
  problems += unnamed.length + meters;
}
await send('Runtime.enable'); await send('Page.enable'); await send('Accessibility.enable');
await send('Page.navigate', { url: 'http://127.0.0.1:18080/' }); await new Promise(r => setTimeout(r, 1500));
await audit('join');
await ev(`document.getElementById('name').value='Ada'; document.getElementById('join-form').requestSubmit(); 1`);
await new Promise(r => setTimeout(r, 4000));
await audit('listening');
await ev(`document.getElementById('mute').click(); 1`);
const mute = (await send('Accessibility.getFullAXTree')).result.nodes.find(n => n.role?.value === 'button' && /Mute/.test(n.name?.value ?? ''));
console.log('mute after press:', mute?.name?.value, JSON.stringify((mute?.properties ?? []).filter(p => p.name === 'pressed').map(p => p.value.value)));
console.log('start-sound hidden:', await ev("document.getElementById('start-sound').hidden"), 'stats:', JSON.stringify(await ev('window.balconyStats')), 'buffer:', await ev("document.getElementById('buffer').textContent"));
console.log(problems ? `PROBLEMS: ${problems}` : 'no problems found');
ws.close();
