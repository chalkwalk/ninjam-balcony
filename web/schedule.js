// When the next jam is: the nth weekday of the month at a wall-clock time in
// the room's time zone (DESIGN §2: first Wednesday, 18:00 America/Los_Angeles),
// computed here so the page never needs editing and rolls over by itself.
//
// No library: Intl knows every zone's offsets, and finding the instant of a
// wall-clock time is two rounds of "what does this instant read as there".
(function (root) {
  'use strict';

  // The UTC instant (ms) at which `zone` reads y-m-d h:mi.
  function zonedToUtc(y, m, d, h, mi, zone) {
    const fmt = new Intl.DateTimeFormat('en-US', {
      timeZone: zone, hourCycle: 'h23', year: 'numeric', month: 'numeric',
      day: 'numeric', hour: 'numeric', minute: 'numeric',
    });
    const wall = Date.UTC(y, m, d, h, mi);
    let guess = wall;
    for (let i = 0; i < 3; i++) {
      const p = {};
      for (const part of fmt.formatToParts(new Date(guess))) p[part.type] = part.value;
      const seen = Date.UTC(+p.year, +p.month - 1, +p.day, +p.hour % 24, +p.minute);
      guess += wall - seen;
    }
    return guess;
  }

  // The day of the month of the nth `weekday` (0 = Sunday) in y-m.
  function nthWeekday(y, m, weekday, nth) {
    const first = new Date(Date.UTC(y, m, 1)).getUTCDay();
    return 1 + ((weekday - first + 7) % 7) + 7 * (nth - 1);
  }

  // The next jam that has not yet ENDED, as {start, end, y, m, d} (ms, UTC).
  function nextJam(now, s) {
    const today = new Date(now);
    let y = today.getUTCFullYear(), m = today.getUTCMonth() - 1;
    for (let k = 0; k < 15; k++, m++) {
      if (m > 11) { m -= 12; y++; }
      if (m < 0) { m += 12; y--; }
      const d = nthWeekday(y, m, s.weekday, s.nth);
      const start = zonedToUtc(y, m, d, s.hour, 0, s.zone);
      const end = start + s.minutes * 60000;
      if (end > now) return { start, end, y, m, d };
    }
    return null;
  }

  function pad(n) { return String(n).padStart(2, '0'); }
  function localStamp(j, s, minutesAfter) {
    // The wall-clock time in the room's zone, for calendars that take TZID.
    const t = new Date(Date.UTC(j.y, j.m, j.d, s.hour, minutesAfter));
    return `${t.getUTCFullYear()}${pad(t.getUTCMonth() + 1)}${pad(t.getUTCDate())}T` +
           `${pad(t.getUTCHours())}${pad(t.getUTCMinutes())}00`;
  }
  const DAYS = ['SU', 'MO', 'TU', 'WE', 'TH', 'FR', 'SA'];
  function rrule(s) { return `FREQ=MONTHLY;BYDAY=${s.nth}${DAYS[s.weekday]}`; }

  // A calendar file with a monthly rule. Calendars resolve TZID by its IANA
  // name; the VTIMEZONE block is there for the ones that insist.
  function ics(j, s, title, url) {
    const now = new Date();
    const stamp = `${now.getUTCFullYear()}${pad(now.getUTCMonth() + 1)}${pad(now.getUTCDate())}T` +
                  `${pad(now.getUTCHours())}${pad(now.getUTCMinutes())}00Z`;
    const esc = (t) => t.replace(/[\;,]/g, (c) => '\\' + c);
    const lines = [
      'BEGIN:VCALENDAR', 'VERSION:2.0', 'PRODID:-//ninjam-balcony//EN', 'CALSCALE:GREGORIAN',
    ];
    if (s.zone === 'America/Los_Angeles') {
      lines.push('BEGIN:VTIMEZONE', 'TZID:America/Los_Angeles',
        'BEGIN:DAYLIGHT', 'TZOFFSETFROM:-0800', 'TZOFFSETTO:-0700', 'TZNAME:PDT',
        'DTSTART:19700308T020000', 'RRULE:FREQ=YEARLY;BYMONTH=3;BYDAY=2SU', 'END:DAYLIGHT',
        'BEGIN:STANDARD', 'TZOFFSETFROM:-0700', 'TZOFFSETTO:-0800', 'TZNAME:PST',
        'DTSTART:19701101T020000', 'RRULE:FREQ=YEARLY;BYMONTH=11;BYDAY=1SU', 'END:STANDARD',
        'END:VTIMEZONE');
    }
    lines.push('BEGIN:VEVENT',
      `UID:ninjam-balcony-${s.nth}${DAYS[s.weekday]}-${s.hour}@${url.replace(/^https?:\/\//, '').replace(/\/.*$/, '')}`,
      `DTSTAMP:${stamp}`,
      `DTSTART;TZID=${s.zone}:${localStamp(j, s, 0)}`,
      `DTEND;TZID=${s.zone}:${localStamp(j, s, s.minutes)}`,
      `RRULE:${rrule(s)}`,
      `SUMMARY:${esc(title)}`,
      `DESCRIPTION:${esc('Listen and chat in your browser: ' + url)}`,
      `URL:${url}`,
      'END:VEVENT', 'END:VCALENDAR');
    return lines.join('\r\n') + '\r\n';
  }

  function googleLink(j, s, title, url) {
    const q = new URLSearchParams({
      action: 'TEMPLATE', text: title,
      dates: `${localStamp(j, s, 0)}/${localStamp(j, s, s.minutes)}`,
      ctz: s.zone, recur: `RRULE:${rrule(s)}`,
      details: `Listen and chat in your browser: ${url}`,
    });
    return `https://calendar.google.com/calendar/render?${q}`;
  }

  const api = { zonedToUtc, nthWeekday, nextJam, ics, googleLink };
  if (typeof module !== 'undefined') module.exports = api;
  else root.Schedule = api;
})(this);
