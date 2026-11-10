// Drive headless Chrome over the DevTools protocol: join, listen, chat, report.
const port = 9333;
const list = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
const page = list.find(t => t.type === 'page');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const waits = new Map(); const logs = [];
ws.onmessage = (e) => {
  const m = JSON.parse(e.data);
  if (m.id && waits.has(m.id)) { waits.get(m.id)(m); waits.delete(m.id); }
  if (m.method === 'Runtime.consoleAPICalled') logs.push(m.params.type + ': ' + m.params.args.map(a => a.value ?? a.description).join(' '));
  if (m.method === 'Runtime.exceptionThrown') logs.push('EXC: ' + m.params.exceptionDetails.exception?.description);
};
const send = (method, params = {}) => new Promise(r => { const i = ++id; waits.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (expr) => (await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true })).result.result.value;
await send('Runtime.enable');
await send('Page.enable');
await send('Page.navigate', { url: 'http://127.0.0.1:18080/' });
await new Promise(r => setTimeout(r, 2000));
console.log('view before:', await ev(`['closed','join','listening','ended'].filter(v => !document.getElementById(v).hidden).join()`));
console.log('next-jam:', await ev(`document.getElementById('next-jam').textContent`));
await ev(`document.getElementById('name').value = 'Tester'; document.getElementById('join-form').requestSubmit(); 1`);
await new Promise(r => setTimeout(r, 6000));
await ev(`document.getElementById('say').value = 'hello band'; document.getElementById('say-form').requestSubmit(); 1`);
await new Promise(r => setTimeout(r, 6000));
console.log(JSON.stringify(await ev(`({
  view: ['closed','join','listening','ended'].filter(v => !document.getElementById(v).hidden).join(),
  unsupported: !document.getElementById('unsupported').hidden,
  delay: document.getElementById('delay-note').textContent,
  buffer: document.getElementById('buffer').textContent,
  players: document.getElementById('players').innerText,
  listeners: document.getElementById('listeners').innerText,
  log: document.getElementById('log').innerText,
  meterWidths: [...document.querySelectorAll('.meter .level')].map(e => e.style.width),
  ctx: typeof AudioDecoder,
})`), null, 1));
console.log('console:', logs.join('\n'));
ws.close();
