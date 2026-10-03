// node --test scripts/ci/flaky-gate.test.mjs
// The CI step reruns only the failed tests once; this gate decides what that means.
import test from 'node:test';
import assert from 'node:assert/strict';

import { decide, parseFailedLog, parseQuarantine } from './flaky-gate.mjs';

const QUARANTINE = '# test|review_by|reason\nperf_engine|2027-01-01|부하에서 흔들림\n';

test('a test that also fails the rerun is broken and keeps the job red', () => {
  const r = decide({ failed: ['workflow_engine'], rerunPassed: false, quarantine: parseQuarantine(QUARANTINE), today: '2026-10-03' });
  assert.equal(r.exitCode, 1);
  assert.match(r.lines.join('\n'), /::error/);
});

test('a quarantined test that passed the rerun stays green but is named in a warning', () => {
  const r = decide({ failed: ['perf_engine'], rerunPassed: true, quarantine: parseQuarantine(QUARANTINE), today: '2026-10-03' });
  assert.equal(r.exitCode, 0);
  assert.match(r.lines.join('\n'), /::warning[^\n]*perf_engine/);
});

test('a flaky test that is not quarantined turns the job red by name', () => {
  const r = decide({ failed: ['perf_engine', 'workflow_engine'], rerunPassed: true, quarantine: parseQuarantine(QUARANTINE), today: '2026-10-03' });
  assert.equal(r.exitCode, 1);
  assert.match(r.lines.join('\n'), /::error[^\n]*workflow_engine/);
});

test('a quarantine entry past its review date no longer excuses the test', () => {
  const r = decide({ failed: ['perf_engine'], rerunPassed: true, quarantine: parseQuarantine(QUARANTINE), today: '2027-01-02' });
  assert.equal(r.exitCode, 1);
  assert.match(r.lines.join('\n'), /2027-01-01/);
});

test('a failed first run with no test names cannot pass on the rerun', () => {
  const r = decide({ failed: [], rerunPassed: true, quarantine: parseQuarantine(QUARANTINE), today: '2026-10-03' });
  assert.equal(r.exitCode, 1);
  assert.match(r.lines.join('\n'), /::error/);
});

test('the failed list is read from LastTestsFailed.log lines', () => {
  assert.deepEqual(parseFailedLog('2:workflow_engine\r\n136:save_open_saveas\r\n'), ['workflow_engine', 'save_open_saveas']);
});

test('a malformed quarantine line is an error, not an excuse', () => {
  assert.throws(() => parseQuarantine('perf_engine|soon|x\n'), /flaky-quarantine/);
  assert.throws(() => parseQuarantine('perf_engine|2027-01-01|\n'), /flaky-quarantine/);
});
