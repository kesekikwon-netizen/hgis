#!/usr/bin/env node
// UserPromptSubmit hook: adds the Strata superpowers routing to every user message, so the rule
// does not fade in long conversations (the plugin's own reminder comes only at session start).
// Speaks only when the session works inside --root (the repo, which also holds every worktree).
// Never blocks a message: bad input means no reminder, and the exit code is always 0.
//   node superpowers-reminder.mjs --root "A:/qgis"   (installed by install-superpowers-reminder.mjs)
import { readFileSync } from 'node:fs';
import path from 'node:path';

const REMINDER = [
  '[슈퍼파워 규칙 — 사용자 지시 2026-10-01 「무조건 해당상황일때 무조건 사용하라」, 예외 없음]',
  '이 메시지에 답하거나 무엇이든 하기 전에, 해당하는 superpowers 스킬을 Skill 도구로 먼저 부른다.',
  '- 새 기능·동작 변경(진행 중인 계획에 끼어든 요청 포함): superpowers:brainstorming → superpowers:writing-plans → superpowers:subagent-driven-development',
  '- 증상·버그 신고·스크린샷·「안 돼요」·「예전으로 돌아갔다」: superpowers:systematic-debugging',
  '- 코드를 고칠 때(한두 줄 수정 포함): superpowers:test-driven-development',
  '- 여러 파일을 바꾼 뒤: superpowers:requesting-code-review',
  '- 「다 됐다」고 말하기 전: superpowers:verification-before-completion',
  "어떤 스킬을 쓰는지 사용자에게 한국어로 밝힌다('설계 논의', '원인 찾기', '시험 먼저', '코드 검토', '끝나기 전 확인'). " +
    '맞는 스킬이 없는 단순 질문이면 그대로 답한다. 답은 한국어로만 쓴다.',
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
