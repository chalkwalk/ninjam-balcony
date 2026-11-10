// node --test test/web/schedule.test.js
const test = require('node:test');
const assert = require('node:assert');
const S = require('../../web/schedule.js');

const room = { weekday: 3, nth: 1, hour: 18, minutes: 120, zone: 'America/Los_Angeles' };

test('first Wednesdays', () => {
  assert.strictEqual(S.nthWeekday(2026, 10, 3, 1), 4);   // Nov 2026
  assert.strictEqual(S.nthWeekday(2026, 11, 3, 1), 2);   // Dec 2026
  assert.strictEqual(S.nthWeekday(2027, 8, 3, 1), 1);    // Sep 2027 starts on a Wednesday
});

test('18:00 Pacific is 02:00 UTC in winter and 01:00 UTC in summer', () => {
  assert.strictEqual(new Date(S.zonedToUtc(2026, 11, 2, 18, 0, room.zone)).toISOString(),
                     '2026-12-03T02:00:00.000Z');
  assert.strictEqual(new Date(S.zonedToUtc(2026, 6, 1, 18, 0, room.zone)).toISOString(),
                     '2026-07-02T01:00:00.000Z');
});

test('the next jam rolls over when one ends, not when it starts', () => {
  const nov = S.zonedToUtc(2026, 10, 4, 18, 0, room.zone);
  // Before it: this month's.
  assert.strictEqual(S.nextJam(nov - 3600e3, room).start, nov);
  // During it: still this month's (the page is waiting for the room to open).
  assert.strictEqual(S.nextJam(nov + 3600e3, room).start, nov);
  // After it: next month's.
  const dec = S.zonedToUtc(2026, 11, 2, 18, 0, room.zone);
  assert.strictEqual(S.nextJam(nov + 121 * 60e3, room).start, dec);
});

test('across a year end', () => {
  const late = Date.UTC(2026, 11, 20);
  const j = S.nextJam(late, room);
  assert.deepStrictEqual([j.y, j.m, j.d], [2027, 0, 6]);
});

test('the calendar file carries the monthly rule in the room zone', () => {
  const j = S.nextJam(Date.UTC(2026, 10, 10), room);
  const ics = S.ics(j, room, 'Virtual Open, Electronic Improv', 'https://ninjam.example.com/');
  assert.match(ics, /DTSTART;TZID=America\/Los_Angeles:20261202T180000/);
  assert.match(ics, /DTEND;TZID=America\/Los_Angeles:20261202T200000/);
  assert.match(ics, /RRULE:FREQ=MONTHLY;BYDAY=1WE/);
  assert.match(ics, /SUMMARY:Virtual Open\\, Electronic Improv/);
  assert.ok(ics.split('\r\n').length > 10);
  const g = new URL(S.googleLink(j, room, 'Jam', 'https://x/'));
  assert.strictEqual(g.searchParams.get('dates'), '20261202T180000/20261202T200000');
  assert.strictEqual(g.searchParams.get('ctz'), 'America/Los_Angeles');
  assert.strictEqual(g.searchParams.get('recur'), 'RRULE:FREQ=MONTHLY;BYDAY=1WE');
});
