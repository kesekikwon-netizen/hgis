#!/usr/bin/env node
// UserPromptSubmit hook: adds the Strata superpowers routing to every user message, so the rule
// does not fade in long conversations (the plugin's own reminder comes only at session start).
// Speaks only when the session works inside --root (the repo, which also holds every worktree).
// Never blocks a message: bad input means no reminder, and the exit code is always 0.
//   node superpowers-reminder.mjs --root "A:/qgis"   (installed by install-superpowers-reminder.mjs)
import { readFileSync } from 'node:fs';
import path from 'node:path';

// Written as facts about the repo, not as orders: the hook docs warn that text framed as
// out-of-band system commands can trip prompt-injection defenses. Same rule as CLAUDE.md
// 「superpowers 사용 방식」 1·3·5.
const REMINDER = [
  'Strata 저장소의 작업 방식(CLAUDE.md 「superpowers 사용 방식」, 사용자가 2026-10-01에 정함): 해당 상황마다 superpowers 스킬을 쓴다.',
  '- 새 기능이나 동작 변경(진행 중인 계획에 끼어든 요청 포함)은 superpowers:brainstorming으로 시작한다. 여러 단계짜리 큰 기능이면 superpowers:writing-plans와 superpowers:subagent-driven-development로 이어진다.',
  '- 증상·버그 신고·스크린샷·「안 돼요」·「예전으로 돌아갔다」는 superpowers:systematic-debugging으로 다룬다.',
  '- 코드 수정은 한두 줄이라도 superpowers:test-driven-development로 한다.',
  '- 여러 파일을 바꾼 뒤에는 superpowers:requesting-code-review로 검토를 받는다.',
  '- 「다 됐다」고 말하기 전에는 superpowers:verification-before-completion으로 확인한다.',
  "- 사용자에게는 지금 단계를 한국어로 알린다('설계 논의', '원인 찾기', '시험 먼저', '코드 검토', '끝나기 전 확인'). 맞는 스킬이 없는 단순 질문은 바로 답한다.",
  '- 답은 한국어로 쓰고, 사용자가 직접 입력하거나 누를 경로·명령만 영어로 둔다.',
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
