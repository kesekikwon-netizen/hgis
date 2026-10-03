#!/usr/bin/env node
// UserPromptSubmit hook: adds the Strata superpowers routing to every user message, so the rule
// does not fade in long conversations (the plugin's own reminder comes only at session start).
// Speaks only when the session works inside --root (the repo, which also holds every worktree).
// Never blocks a message: bad input means no reminder, and the exit code is always 0.
//   node superpowers-reminder.mjs --root "A:/qgis"   (installed by install-superpowers-reminder.mjs)
import { readFileSync } from 'node:fs';
import path from 'node:path';

// Written as facts about the repo, not as orders: the hook docs warn that text framed as
// out-of-band system commands can trip prompt-injection defenses. The full rules are CLAUDE.md
// 「일하는 방식」. The Jev stage hint (jev-skill-route.mjs) names the first stage only when it is
// sure, and finish-check.mjs closes turns that edited code under src/ tests/ cmake/ data/, so this
// keeps the debugging route plus what lies between (2026-10-03,
// docs/intent/2026-10-03-dev-setup-cleanup.md).
const REMINDER = [
  'Strata 저장소는 CLAUDE.md 「일하는 방식」을 따른다. 스킬은 해당 단계에서 적극 사용하고, 단계의 첫 동작 전에 Skill 도구로 다시 부른다(대화 요약 전에 부른 것은 치지 않는다).',
  '- 증상·버그·스크린샷은 superpowers:systematic-debugging으로 원인부터 찾는다.',
  '- 고칠 때는 superpowers:test-driven-development로 시험을 먼저 쓰고, 기능 하나가 끝났을 때 superpowers:requesting-code-review로 한 번 검토받는다.',
  '- 과제마다 작업자·검토자를 따로 돌리지 않고 이 대화에서 차례로 만든다(2026-10-02 「빠르게 진행」, superpowers:executing-plans).',
  '- 지금 단계를 한국어로 밝힌다. 맞는 스킬이 없는 단순 질문은 바로 답한다.',
].join('\n');

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

let input = {};
try {
  const raw = readFileSync(0, 'utf8');
  if (raw.trim()) input = JSON.parse(raw);
} catch {
  input = {};
}
const root = argValue('--root');
const cwd = input && typeof input.cwd === 'string' && input.cwd ? input.cwd : process.cwd();
if (root && inside(cwd, root)) {
  process.stdout.write(
    JSON.stringify({ hookSpecificOutput: { hookEventName: 'UserPromptSubmit', additionalContext: REMINDER } }));
}
process.exitCode = 0;
