#!/usr/bin/env node
// Stop hook: before Claude ends a turn inside the Strata repo, check that the closing skills ran.
// Code under src/ tests/ cmake/ data/ or CMakeLists.txt was edited in this turn but
// superpowers:verification-before-completion was not called -> stop once and say so; screen code
// (src/app) edited without run-strata -> the same. A second stop in a row always passes
// (stop_hook_active), and bad input or an unreadable transcript never blocks (exit code is always 0).
//   node finish-check.mjs --root "A:/qgis"   (installed by install-superpowers-reminder.mjs)
// User decision 2026-10-03, docs/intent/2026-10-03-finish-check.md.
import { readFileSync } from 'node:fs';
import path from 'node:path';

const VERIFY = 'superpowers:verification-before-completion';
const SCREEN = 'run-strata';
const CODE = ['src/', 'tests/', 'cmake/', 'data/'];
const EDIT_TOOLS = new Set(['Edit', 'Write', 'MultiEdit', 'NotebookEdit']);

const argValue = (name) => {
  const i = process.argv.indexOf(name);
  return i > 0 ? process.argv[i + 1] : undefined;
};
const normalized = (p) => path.resolve(p).replace(/\\/g, '/').replace(/\/+$/, '').toLowerCase();
const inside = (cwd, root) => {
  const c = normalized(cwd);
  const r = normalized(root);
  return c === r || c.startsWith(`${r}/`);
};

// Tool calls made after the last message the person typed (tool results are user entries too).
function thisTurn(lines) {
  const entries = lines.map((l) => { try { return JSON.parse(l); } catch { return null; } }).filter(Boolean);
  const typed = (e) => e.type === 'user' && !e.isMeta && !e.isCompactSummary
    && (typeof e.message?.content === 'string' ? !e.message.content.startsWith('<task-notification>')
        : (Array.isArray(e.message?.content) && e.message.content.some((c) => c?.type !== 'tool_result')));
  let start = 0;
  entries.forEach((e, i) => { if (typed(e)) start = i + 1; });
  return entries.slice(start).filter((e) => e.type === 'assistant')
    .flatMap((e) => (Array.isArray(e.message?.content) ? e.message.content : []))
    .filter((c) => c.type === 'tool_use');
}

// Edited paths relative to the repo (or to its worktree under .claude/worktrees/<name>), lower-case.
function missingSteps(calls, cwd, root) {
  const r = `${normalized(root)}/`;
  const edited = calls.filter((c) => EDIT_TOOLS.has(c.name))
    .map((c) => c.input?.file_path ?? c.input?.notebook_path).filter((f) => typeof f === 'string' && f)
    .map((f) => normalized(path.resolve(cwd, f))).filter((f) => f.startsWith(r))
    .map((f) => f.slice(r.length).replace(/^\.claude\/worktrees\/[^/]+\//, ''));
  const called = calls.filter((c) => c.name === 'Skill' && typeof c.input?.skill === 'string').map((c) => c.input.skill);
  const has = (name) => called.some((s) => s === name || name.endsWith(`:${s}`));
  const code = edited.some((f) => CODE.some((d) => f.startsWith(d)) || f === 'cmakelists.txt');
  const screen = edited.some((f) => f.startsWith('src/app/'));
  const missing = [];
  if (code && !has(VERIFY)) {
    missing.push(`코드를 고쳤는데 「끝나기 전 확인」(${VERIFY})을 부르지 않았습니다. 지금 불러 빌드·시험 결과를 확인한 뒤 끝내세요.`);
  }
  if (screen && !has(SCREEN)) {
    missing.push(`화면 쪽 코드(src/app)를 고쳤는데 「화면 확인」(${SCREEN} 스킬)을 하지 않았습니다. 실제 앱 화면을 찍어 확인하고, 화면과 상관없는 수정이면 그 이유를 한 줄 적은 뒤 끝내세요.`);
  }
  return missing;
}

try {
  const input = JSON.parse(readFileSync(0, 'utf8'));
  const root = argValue('--root');
  const cwd = typeof input.cwd === 'string' && input.cwd ? input.cwd : process.cwd();
  if (root && inside(cwd, root) && !input.stop_hook_active && input.transcript_path) {
    const calls = thisTurn(readFileSync(input.transcript_path, 'utf8').split(/\r?\n/));
    const missing = missingSteps(calls, cwd, root);
    if (missing.length) {
      process.stdout.write(JSON.stringify({
        decision: 'block',
        reason: `마무리 직전 확인: 확인 단계를 빠뜨려 먼저 확인합니다.\n- ${missing.join('\n- ')}`,
      }));
    }
  }
} catch {
  // Never block a turn because this check failed.
}
process.exitCode = 0;
