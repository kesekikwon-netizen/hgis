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
  'Strata 저장소의 작업 방식(CLAUDE.md 「superpowers 사용 방식」, 사용자가 2026-10-03에 「해당 시 적극 사용」으로 바꿈): 해당 단계가 되면 스킬을 Skill 도구로 실제로 불러 적극 사용한다. 앞에서 읽었거나 대화 요약 전에 부른 것으로 대신하지 않고, 단계마다 다시 부른다.',
  '- 증상·버그 신고·스크린샷·「안 돼요」·「예전으로 돌아갔다」는 superpowers:systematic-debugging으로 원인부터 찾는다.',
  '- 코드 수정은 superpowers:test-driven-development로 시험을 먼저 쓴다.',
  '- 화면이 바뀌는 수정은 run-strata 스킬로 실제 앱 화면을 찍어 확인한다.',
  '- 새 기능이나 동작 변경은 전역 capture-intent 스킬로 요청 기록(docs/intent/)부터 만들고, 질문과 함께 한 번 보여 「진행」을 받은 뒤 만든다. 「진행」이 설계 승인이다.',
  '- superpowers:brainstorming은 현장 쓰임새를 물어야 하는 새 기능에만 쓴다. 스펙·계획 문서는 여러 단계짜리 큰 기능에서만 쓴다.',
  '- 계획이 있어도 과제마다 작업자·검토자를 따로 돌리지 않는다(2026-10-02 「빠르게 진행」). 이 대화에서 차례로 만들고(superpowers:executing-plans), 기능 하나가 끝났을 때 superpowers:requesting-code-review로 한 번 검토받는다.',
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
