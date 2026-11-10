// Join; the bot dies (SIGKILL, no goodbye); the page retries; the bot returns.
import { execSync } from 'node:child_process';
const list = await (await fetch('http://127.0.0.1:9333/json/list')).json();
const ws = new WebSocket(list.find(t => t.type === 'page').webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const waits = new Map();
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && waits.has(m.id)) { waits.get(m.id)(m); waits.delete(m.id); } };
const send = (method, params = {}) => new Promise(r => { const i = ++id; waits.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (x) => (await send('Runtime.evaluate', { expression: x, awaitPromise: true, returnByValue: true })).result.result?.value;
const sleep = (s) => new Promise(r => setTimeout(r, s * 1000));
const view = () => ev(`['closed','join','listening','ended'].filter(v => !document.getElementById(v).hidden).join() + ' / ' + document.getElementById('ended-why').textContent + ' / ' + document.getElementById('buffer').textContent`);
await send('Runtime.enable'); await send('Page.enable');
await send('Page.navigate', { url: 'http://127.0.0.1:18080/' }); await sleep(1.5);
await ev(`document.getElementById('name').value='Rae'; document.getElementById('join-form').requestSubmit(); 1`);
await sleep(4); console.log('joined:', await view());
execSync(process.env.KILL_BOT); await sleep(2); console.log('bot killed:', await view());
execSync(process.env.START_BOT); await sleep(33); console.log('after retry:', await view());
ws.close();
