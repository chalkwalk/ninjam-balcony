// The balcony page (DESIGN §2, §9.3). Plain JavaScript, no framework, no build.
(function () {
  'use strict';

  const $ = (id) => document.getElementById(id);
  const body = document.body;
  const schedule = {
    weekday: +body.dataset.scheduleWeekday, nth: +body.dataset.scheduleNth,
    hour: +body.dataset.scheduleHour, minutes: +body.dataset.scheduleMinutes,
    zone: body.dataset.scheduleTz,
  };
  const title = body.dataset.roomTitle;
  const here = new URL('.', location.href);
  const statusUrl = new URL(body.dataset.statusPath, here);
  const wsUrl = new URL(body.dataset.wsPath, here);
  wsUrl.protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';

  const RATE = 48000;
  const views = ['closed', 'join', 'listening', 'ended'];
  function show(view) {
    for (const v of views) $(v).hidden = v !== view;
  }

  // ---- Closed: when is the next one ------------------------------------
  function renderClosed() {
    const now = Date.now();
    const j = Schedule.nextJam(now, schedule);
    if (!j) return;
    const when = new Intl.DateTimeFormat(undefined, {
      weekday: 'long', month: 'long', day: 'numeric', hour: 'numeric', minute: '2-digit',
      timeZoneName: 'short',
    }).format(new Date(j.start));
    $('next-jam').textContent = now >= j.start
      ? `The jam should be starting now (${when}). Waiting for the room to open…`
      : `The next one is ${when}, your time.`;
    const ics = Schedule.ics(j, schedule, title, here.href);
    const old = $('ics').href;
    $('ics').href = URL.createObjectURL(new Blob([ics], { type: 'text/calendar' }));
    if (old.startsWith('blob:')) URL.revokeObjectURL(old);
    $('gcal').href = Schedule.googleLink(j, schedule, title, here.href);
  }

  // ---- Status: is the room open ----------------------------------------
  let open = false;
  let pollTimer = null;
  async function pollStatus() {
    clearTimeout(pollTimer);
    let s = { open: false };
    try {
      const r = await fetch(statusUrl, { cache: 'no-store' });
      if (r.ok) s = await r.json();
    } catch (e) { /* closed, or the network: either way, not open */ }
    const wasOpen = open;
    open = !!s.open;
    if (!socket) {
      if (open && !wasOpen) show('join');
      if (!open) { show($('ended').hidden ? 'closed' : 'ended'); renderClosed(); }
    }
    pollTimer = setTimeout(pollStatus, open ? 30000 : 60000);
  }

  // ---- Audio -------------------------------------------------------------
  let ctx = null, gain = null, player = null, decoder = null;
  let playingPos = -1;
  const meterQueue = [];  // [{pos, meters: Map(id -> [peak, level])}]

  function canDecode() {
    return typeof AudioDecoder !== 'undefined' && typeof AudioWorkletNode !== 'undefined';
  }

  // For checking a browser without ears (headless tests): how much came in
  // and how much the decoder gave back. Read-only; nothing depends on it.
  const stats = { messages: 0, decoded: 0, decodeErrors: 0 };
  window.balconyStats = stats;

  async function startAudio() {
    ctx = new AudioContext({ sampleRate: RATE, latencyHint: 'playback' });
    // Created inside the click, but some browsers still start it suspended
    // (Safari, Firefox with autoplay blocked): ask, and offer a button if not.
    ctx.resume().catch(() => {});
    ctx.onstatechange = showStartSound;
    await ctx.audioWorklet.addModule('player-worklet.js');
    player = new AudioWorkletNode(ctx, 'balcony-player', {
      numberOfOutputs: 1, outputChannelCount: [2],
      processorOptions: { targetSeconds: 0.4, capacitySeconds: 6 },
    });
    gain = ctx.createGain();
    player.connect(gain).connect(ctx.destination);
    setVolume();
    player.port.onmessage = (e) => {
      playingPos = e.data.playing;
      $('buffer').textContent = e.data.playing < 0 ? 'buffering…'
        : `${e.data.buffered.toFixed(1)} s buffered`;
    };
    const config = { codec: 'opus', sampleRate: RATE, numberOfChannels: 2 };
    const support = await AudioDecoder.isConfigSupported(config).catch(() => ({ supported: false }));
    if (!support.supported) throw new Error('opus unsupported');
    decoder = new AudioDecoder({
      output: (data) => {
        const n = data.numberOfFrames;
        const left = new Float32Array(n), right = new Float32Array(n);
        data.copyTo(left, { planeIndex: 0, format: 'f32-planar' });
        data.copyTo(right, { planeIndex: data.numberOfChannels > 1 ? 1 : 0, format: 'f32-planar' });
        const pos = Math.round(data.timestamp * RATE / 1e6);
        data.close();
        stats.decoded += 1;
        player.port.postMessage({ pos, left, right }, [left.buffer, right.buffer]);
      },
      error: (e) => { stats.decodeErrors += 1; console.warn('decoder', e); },
    });
    decoder.configure(config);
    if (ctx.sampleRate !== RATE) console.warn('context runs at', ctx.sampleRate);
  }

  function showStartSound() {
    const wasHidden = $('start-sound').hidden;
    $('start-sound').hidden = !ctx || ctx.state === 'running' || ctx.state === 'closed';
    if (wasHidden && !$('start-sound').hidden)
      line('notice', '', 'Your browser is holding the sound back. Press "Start sound" to hear the band.');
  }
  $('start-sound').addEventListener('click', () => {
    if (ctx) ctx.resume().then(showStartSound, () => {});
  });

  function stopAudio() {
    try { decoder && decoder.state !== 'closed' && decoder.close(); } catch (e) { /* gone */ }
    try { ctx && ctx.close(); } catch (e) { /* gone */ }
    ctx = gain = player = decoder = null;
    $('start-sound').hidden = true;
    playingPos = -1;
    meterQueue.length = 0;
  }

  function setVolume() {
    if (!gain) return;
    const v = +$('volume').value / 100;
    const muted = $('mute').getAttribute('aria-pressed') === 'true';
    gain.gain.setTargetAtTime(muted ? 0 : v * v, ctx.currentTime, 0.02);
  }

  // One binary message: kind, frame count, u32 position, Opus frames, meters.
  function onAudio(buf) {
    stats.messages += 1;
    const v = new DataView(buf);
    if (v.getUint8(0) !== 1) return;
    const frames = v.getUint8(1);
    let pos = v.getUint32(2, true);
    let off = 6;
    for (let f = 0; f < frames; f++) {
      const len = v.getUint16(off, true);
      off += 2;
      if (decoder && decoder.state === 'configured') {
        decoder.decode(new EncodedAudioChunk({
          type: 'key', timestamp: Math.round((pos + f * 960) * 1e6 / RATE),
          data: new Uint8Array(buf, off, len),
        }));
      }
      off += len;
    }
    const count = v.getUint8(off++);
    const meters = new Map();
    for (let i = 0; i < count; i++, off += 4)
      meters.set(v.getUint16(off, true), [v.getUint8(off + 2), v.getUint8(off + 3)]);
    meterQueue.push({ pos, meters });
    if (meterQueue.length > 400) meterQueue.splice(0, meterQueue.length - 400);
  }

  // Meters move with what is HEARD: each frame is shown when its position plays.
  function drawMeters() {
    requestAnimationFrame(drawMeters);
    if (playingPos < 0) return;
    let frame = null;
    while (meterQueue.length && meterQueue[0].pos <= playingPos) frame = meterQueue.shift();
    if (!frame) return;
    for (const el of document.querySelectorAll('.meter[data-id]')) {
      const m = frame.meters.get(+el.dataset.id);
      const peak = m ? m[0] / 240 : 0, level = m ? m[1] / 240 : 0;
      const bar = el.firstChild;
      bar.style.width = `${(level * 100).toFixed(1)}%`;
      bar.style.background = peak > 0.97 ? 'var(--meter-high)'
        : peak > 0.85 ? 'var(--meter-mid)' : 'var(--meter-low)';
      el.lastChild.style.left = `calc(${(peak * 100).toFixed(1)}% - 2px)`;
    }
  }
  requestAnimationFrame(drawMeters);

  // ---- The conversation ----------------------------------------------------
  let socket = null, me = null, players = new Map(), endedBy = null, roomSeen = false;

  function line(kind, from, text) {
    const li = document.createElement('li');
    li.className = `kind-${kind}`;
    if (from && kind !== 'bot' && kind !== 'system') {
      const f = document.createElement('span');
      f.className = 'from';
      f.textContent = from + (kind === 'listener' ? ' (balcony)' : '');
      li.append(f, ': ');
    }
    li.append(text);
    const log = $('log');
    const atBottom = log.scrollHeight - log.scrollTop - log.clientHeight < 40;
    log.append(li);
    while (log.children.length > 300) log.firstChild.remove();
    if (atBottom) log.scrollTop = log.scrollHeight;
  }

  function renderRoom(msg) {
    const before = new Set(players.keys());
    const ul = $('players');
    ul.textContent = '';
    players = new Map();
    for (const p of msg.players) {
      players.set(p.name, p);
      const li = document.createElement('li');
      const name = document.createElement('div');
      name.className = 'player-name';
      name.textContent = p.name;
      li.append(name);
      for (const c of p.channels) {
        const row = document.createElement('div');
        row.className = 'channel';
        const label = document.createElement('span');
        label.textContent = c.name || 'channel';
        row.append(label);
        if (c.inStream) {
          const meter = document.createElement('div');
          meter.className = 'meter';
          meter.dataset.id = c.id;
          meter.setAttribute('aria-hidden', 'true');
          const lv = document.createElement('div'); lv.className = 'level';
          const pk = document.createElement('div'); pk.className = 'peak';
          meter.append(lv, pk);
          row.append(meter);
        } else {
          const out = document.createElement('span');
          out.className = 'out';
          out.textContent = 'not in the stream';
          row.append(out);
        }
        li.append(row);
      }
      ul.append(li);
    }
    // Who was already playing is in the list; only later changes are news.
    if (roomSeen) {
      for (const n of players.keys()) if (!before.has(n)) line('system', '', `${n} is playing`);
      for (const n of before) if (!players.has(n)) line('system', '', `${n} has left the room`);
    }
    roomSeen = true;
    const lu = $('listeners');
    lu.textContent = '';
    for (const n of msg.listeners) {
      const li = document.createElement('li');
      li.textContent = n === me ? `${n} (you)` : n;
      lu.append(li);
    }
  }

  function connect(name) {
    socket = new WebSocket(wsUrl);
    socket.binaryType = 'arraybuffer';
    socket.onopen = () => socket.send(JSON.stringify({ t: 'hello', name }));
    socket.onmessage = (e) => {
      if (typeof e.data !== 'string') { onAudio(e.data); return; }
      let m;
      try { m = JSON.parse(e.data); } catch (err) { return; }
      switch (m.t) {
        case 'welcome': {
          me = m.you;
          show('listening');
          setTimeout(showStartSound, 1000);
          $('listening-heading').focus();
          const seconds = m.bpm ? (60 * m.bpi / m.bpm) : 0;
          $('delay-note').textContent = seconds
            ? `You hear the band about ${seconds.toFixed(0)} seconds behind (one interval), as they hear each other.`
            : '';
          break;
        }
        case 'refused':
          show('join');
          $('name-error').textContent = {
            'name-taken': 'Someone here already has that name; please choose another.',
            'name-invalid': 'That name has a character it cannot have.',
            blocked: 'That name has been asked to leave for this jam.',
            full: 'The balcony is full right now.',
          }[m.why] || 'That name will not do.';
          $('name').focus();
          stopAudio();
          socket.onclose = null;
          socket.close();
          socket = null;
          break;
        case 'history':
          for (const l of m.lines) line(l.kind, l.from, l.text);
          break;
        case 'chat':
          line(m.kind, m.from, m.text);
          break;
        case 'notice':
          line('notice', '', m.text);
          break;
        case 'room':
          renderRoom(m);
          break;
        case 'config': {
          const seconds = 60 * m.bpi / m.bpm;
          line('system', '', `The room changed to ${m.bpm} bpm, ${m.bpi} beats an interval.`);
          $('delay-note').textContent = `You hear the band about ${seconds.toFixed(0)} seconds behind (one interval), as they hear each other.`;
          break;
        }
        case 'closing':
          endedBy = m.why;
          break;
      }
    };
    socket.onclose = () => {
      socket = null;
      stopAudio();
      if (!me) return;
      me = null;
      const why = {
        'sent-away': 'The players asked the balcony to leave.',
        kicked: 'You were asked to leave by the room.',
        'server-closed': 'The jam is over.',
      }[endedBy];
      // An ending the room chose is final. A dropped connection is not: try
      // again every 30 s while the room is open (DESIGN §9.5).
      $('ended-why').textContent = why || 'The connection was lost. Trying again every 30 seconds…';
      show('ended');
      $('ended-heading').focus();
      if (!why) retryTimer = setTimeout(retry, 30000);
      else pollStatus();
    };
  }

  let lastName = '', retryTimer = null;

  async function retry() {
    retryTimer = null;
    let s = { open: false };
    try {
      const r = await fetch(statusUrl, { cache: 'no-store' });
      if (r.ok) s = await r.json();
    } catch (e) { /* still down */ }
    if (!s.open) {
      retryTimer = setTimeout(retry, 30000);
      return;
    }
    await begin(lastName);
  }

  async function begin(name) {
    lastName = name;
    if (canDecode()) {
      try { await startAudio(); } catch (err) {
        stopAudio();
        $('unsupported').hidden = false;
      }
    } else {
      $('unsupported').hidden = false;
    }
    endedBy = null;
    roomSeen = false;
    $('log').textContent = '';
    connect(name);
  }

  $('join-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    $('name-error').textContent = '';
    const name = $('name').value.trim().replace(/\s+/g, ' ');
    if (!name) return;
    clearTimeout(retryTimer);
    await begin(name);
  });

  $('say-form').addEventListener('submit', (e) => {
    e.preventDefault();
    const text = $('say').value.trim();
    if (!text || !socket || socket.readyState !== WebSocket.OPEN) return;
    socket.send(JSON.stringify({ t: 'say', text }));
    $('say').value = '';
  });

  $('volume').addEventListener('input', setVolume);
  $('mute').addEventListener('click', () => {
    const b = $('mute');
    const on = b.getAttribute('aria-pressed') !== 'true';
    b.setAttribute('aria-pressed', String(on));
    // The name stays "Mute"; aria-pressed says whether it is on. Changing both
    // would make a screen reader announce "Unmute, pressed".
    setVolume();
  });

  $('title').textContent = title;
  document.title = `${title}: balcony`;
  renderClosed();
  pollStatus();
})();
