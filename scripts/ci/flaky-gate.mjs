// flaky-gate.mjs — decides the CI CTest step after it reran only the failed tests once.
// A retry buys a name in the log, not a pass (docs/intent/2026-10-03-dev-speed-and-flaky.md):
//   - a test that fails the rerun too is broken: red;
//   - a test that passes the rerun is flaky: green only while docs/quality/flaky-quarantine.txt
//     lists it with a review date not yet passed, and named in a ::warning:: either way.
//   node scripts/ci/flaky-gate.mjs --failed <first-run LastTestsFailed.log> --rerun-exit <code>
//                                  --quarantine docs/quality/flaky-quarantine.txt [--today YYYY-MM-DD]
import { readFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const QUARANTINE_FILE = 'docs/quality/flaky-quarantine.txt';

// "136:save_open_saveas" lines, as ctest writes them.
export function parseFailedLog(text) {
  return text
    .split(/\r?\n/)
    .map((line) => /^\s*\d+:(.+?)\s*$/.exec(line))
    .filter(Boolean)
    .map((m) => m[1]);
}

// "test|review_by|reason" lines -> Map(test -> { reviewBy, reason }); # comments and blank lines skip.
export function parseQuarantine(text) {
  const map = new Map();
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    const [name, reviewBy, reason] = line.split('|').map((s) => s.trim());
    const date = /^\d{4}-\d{2}-\d{2}$/.test(reviewBy ?? '') ? new Date(`${reviewBy}T00:00:00Z`) : null;
    if (!name || !date || Number.isNaN(date.getTime()) || date.toISOString().slice(0, 10) !== reviewBy || !reason) {
      throw new Error(`${QUARANTINE_FILE}: 형식이 틀린 줄 "${line}" (시험|검토 기한 YYYY-MM-DD|이유)`);
    }
    map.set(name, { reviewBy, reason });
  }
  return map;
}

export function decide({ failed, rerunPassed, quarantine, today }) {
  if (!failed.length) {
    return { exitCode: 1, lines: ['::error title=흔들림 판정 실패::첫 실행이 실패했는데 실패한 시험 이름을 읽지 못했다. 위 ctest 출력에서 확인한다.'] };
  }
  if (!rerunPassed) {
    return {
      exitCode: 1,
      lines: [`::error title=고장 난 시험::다시 돌려도 실패한 시험이 있다 (첫 실행 실패: ${failed.join(', ')}). 위 ctest 출력에서 확인해 고친다.`],
    };
  }
  const excused = failed.filter((n) => quarantine.has(n) && quarantine.get(n).reviewBy >= today);
  const red = failed.filter((n) => !excused.includes(n));
  const lines = [];
  if (excused.length) lines.push(`::warning title=흔들린 시험(격리 중)::${excused.map((n) => `${n} (검토 ${quarantine.get(n).reviewBy})`).join(', ')}`);
  if (red.length) {
    const why = red.map((n) => (quarantine.has(n) ? `${n} (격리 기한 ${quarantine.get(n).reviewBy} 지남)` : n)).join(', ');
    lines.push(`::error title=흔들린 시험::${why} — 다시 돌려 통과했지만 ${QUARANTINE_FILE}에 기한 안으로 없다. 원인을 고치거나 이유·검토 기한과 함께 올린다.`);
  }
  return { exitCode: red.length ? 1 : 0, lines };
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const arg = (name) => {
    const i = process.argv.indexOf(name);
    return i > 0 ? process.argv[i + 1] : undefined;
  };
  try {
    const failed = parseFailedLog(readFileSync(arg('--failed'), 'utf8'));
    const quarantine = parseQuarantine(readFileSync(arg('--quarantine') ?? QUARANTINE_FILE, 'utf8'));
    const today = arg('--today') ?? new Date().toISOString().slice(0, 10);
    const r = decide({ failed, rerunPassed: arg('--rerun-exit') === '0', quarantine, today });
    for (const line of r.lines) console.log(line);
    process.exitCode = r.exitCode;
  } catch (e) {
    console.log(`::error title=흔들림 판정 실패::${e.message}`);
    process.exitCode = 1;
  }
}
