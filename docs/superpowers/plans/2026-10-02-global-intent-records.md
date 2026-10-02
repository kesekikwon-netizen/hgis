# 모든 프로젝트에 요청 기록(intent) 넣기 — 구현 계획

> **에이전트용:** 이 계획은 superpowers:executing-plans로 이 대화에서 차례로 실행한다. 작업이 서로 이어져 있어서 Strata 규칙상 작업마다 하위 에이전트를 쓰는 방식은 쓰지 않는다. 각 단계는 체크박스(`- [ ]`)로 표시한다.

**목표:** 어느 프로젝트에서든 새 기능이나 동작 변경을 요청받으면, Claude가 설계·코드보다 먼저 사용자 말 그대로의 요청 기록(`docs/intent/…md`)을 만든다. 그 요약과 미정 질문을 한 번에 보여 주고, '진행' 답을 받은 뒤에 만든다.

**설계:**
- 전역 스킬 `capture-intent`(요청 기록 절차와 양식)를 `~/.claude/skills/`에 둔다. 전역 지침 `~/.claude/CLAUDE.md`에는 이 스킬을 부르는 한 줄을 표시(`<!-- capture-intent:start -->` … `<!-- capture-intent:end -->`) 사이에 넣는다. 전역 지침은 대화마다 읽히고 대화가 길어져 요약될 때도 다시 읽히므로, 규칙이 흐려지지 않는다.
- 원본 파일은 Strata 저장소 `scripts/claude-global/`에 둔다. 그래서 커밋되어 GitHub에 남는다. 설치 프로그램 `install-intent.mjs`는 백업한 뒤 복사하고 한 줄을 넣으며, `--remove`로 되돌린다. 기존 설치 프로그램(`scripts/setup-claude-dev.mjs`)과 같은 방식이다.
- superpowers 설계 논의(brainstorming)의 「의도 파악·이해한 내용 적어 보이기」 단계를 이 기록이 맡는다. 절차를 하나 더 쌓는 것이 아니다.
- Strata `CLAUDE.md`에는 "멈추지 말고 같은 턴에 진행한다"는 규칙이 있다. 여기에 요청 기록 확인 한 번을 예외로 적는다. 이 예외가 없으면 Strata에서는 프로젝트 규칙이 전역 규칙을 이긴다.

**도구:** Node 22 기본 기능만 쓴다(`node:fs`, `node:path`, `node:os`, `node:url`, `node:test`, `node:assert/strict`, `node:child_process`). 새 패키지는 넣지 않는다.

**근거 문서(요청 기록):** `docs/intent/2026-10-02-global-intent-records.md`

## 정할 것 (추천 답으로 계획을 썼다)

1. 만들기 전에 한 번 멈춰 확인을 받을까? 추천 답은 「새 기능일 때 한 번」이다. 다른 답을 고르면 Task 2의 `SKILL.md` 3단계와 `intent-rule.md`, 그리고 Task 4를 바꾼다.
   - B: 미정 질문이 있을 때만 멈춘다.
   - C: 멈추지 않고 기록만 남긴다. 이 경우 Task 4는 하지 않는다.
2. 버그 수정도 기록할까? 추천 답은 「아니요」다. "예"를 고르면 `SKILL.md`의 「언제 쓰나」와 `intent-rule.md` 문구만 바꾼다.

## 전체 제약

- 사용자에게 보이는 글은 한국어로 쓴다. 경로·명령·식별자만 영어로 둔다.
- 전역 지침의 기존 다섯 줄은 한 글자도 바꾸지 않는다. 더하는 것은 표시 사이의 한 줄뿐이다.
- 설정 폴더는 환경 변수 `CLAUDE_CONFIG_DIR`이고, 없으면 `~/.claude`이다.
- 백업 위치는 `<설정 폴더>/_reset_backup/<YYYY-MM-DD-HHMM>-intent/`이다. 같은 이름이 이미 있으면 뒤에 `-2`, `-3`…을 붙인다.
- 시험은 임시 폴더에만 쓴다. 진짜 `~/.claude`는 건드리지 않는다.
- 기록 파일 이름은 `docs/intent/YYYY-MM-DD-<영문-소문자-하이픈>.md`이다. 본문은 한국어로 40줄 안팎이다.
- 커밋은 Bash 도구로 한다. `docs/superpowers/plans/`는 `.gitignore`(146행 `plans/`)에 걸려 있으므로 `git add -f`로 넣는다.
- 전역 설정을 실제로 바꾸는 Task 5는 사용자가 이 계획을 진행하라고 말한 뒤에만 한다.

## 검토 때 볼 곳 (시험이 놓치기 쉬운 실패)

1. 「안 돼요」 같은 증상 신고에 기록을 만들면서 멈춰 버리는 경우. Task 5 시험 B(증상 신고)에서 기록 파일이 없어야 한다.
2. 평범한 한국어 기능 요청(「…넣어 줘」)에 스킬이 불리지 않는 경우. Task 5 시험 A·C에서 기록 파일이 생겨야 한다.
3. 기록을 확인받기 전에 코드부터 고치는 경우. 시험 A에서 기록 말고는 바뀐 파일이 없어야 한다.
4. 요청에 섞인 키가 기록에 남는 경우. 시험 A의 `TEST-KEY-0000`이 기록에 없어야 한다.
5. 설치하다가 사용자의 기존 전역 지침 줄을 지우거나 줄바꿈(CRLF)·BOM을 망가뜨리는 경우. Task 1의 시험 3·4와 Task 3의 시험 1·4가 막는다.

## 파일 구조

| 파일 | 역할 |
|---|---|
| `scripts/claude-global/install-intent.mjs` | 표시 줄 넣기·빼기, 원본 검사, 설치·제거, 명령줄 실행 |
| `scripts/claude-global/install-intent.test.mjs` | 위 기능의 시험(`node --test`) |
| `scripts/claude-global/intent-rule.md` | 전역 지침에 넣을 한 줄(원본) |
| `scripts/claude-global/skills/capture-intent/SKILL.md` | 요청 기록 절차(원본) |
| `scripts/claude-global/skills/capture-intent/template.md` | 요청 기록 양식(원본) |
| `CLAUDE.md` (Strata) | 규칙 2·4와 brainstorming 항목에 예외를 더한다 |
| `docs/intent/2026-10-02-global-intent-records.md` | 이 일의 요청 기록. 완료 때 결과를 적는다 |

설치하면 저장소 밖에 생기는 것: `~/.claude/skills/capture-intent/{SKILL.md,template.md}`, 그리고 `~/.claude/CLAUDE.md`의 표시 사이 한 줄.

---

### Task 1: 전역 지침에 한 줄 넣기·빼기 (순수 함수)

**Files:**
- Create: `scripts/claude-global/install-intent.mjs`
- Test: `scripts/claude-global/install-intent.test.mjs`

**Interfaces — Produces:**
- `export const START = '<!-- capture-intent:start -->'`
- `export const END = '<!-- capture-intent:end -->'`
- `export function applyBlock(text: string, body: string): string`
  - 표시가 없을 때: 끝에 `빈 줄 + START + body + END`를 덧붙인다. 원문 끝에 줄바꿈이 없으면 먼저 하나 넣는다.
  - 원문이 비어 있을 때(BOM만 있는 경우 포함): 빈 줄 없이 블록만 쓴다.
  - 표시가 있을 때: 그 사이만 `body`로 바꾼다.
  - 원문의 줄바꿈(CRLF/LF)과 맨 앞 BOM을 따른다. `body` 끝의 줄바꿈은 정리해서 한 번만 쓴다.
- `export function removeBlock(text: string): string`
  - 표시와 그 사이, 그리고 `applyBlock`이 넣은 앞쪽 빈 줄 하나를 뺀다.
  - 표시가 없으면 원문을 그대로 돌려준다.
- 두 함수 모두 다음 경우에 `Error('전역 지침의 capture-intent 표시가 깨졌습니다')`를 던진다: `START`나 `END` 한쪽만 있을 때, 순서가 거꾸로일 때, 두 번 이상 나올 때.
- 이 모듈은 import만 해서는 아무것도 하지 않는다. 명령줄 실행은 Task 3에서 만든다.

- [ ] **Step 1: 실패하는 시험 쓰기** (`install-intent.test.mjs`)

```js
import test from 'node:test';
import assert from 'node:assert/strict';
import { applyBlock, removeBlock, START, END } from './install-intent.mjs';

const ORIGINAL = '# 전역 지침\n\n- 하나\n- 둘\n';
const BODY = '- 새 줄';

test('applyBlock adds the block once at the end', () => {
  const once = applyBlock(ORIGINAL, BODY);
  assert.equal(once, `${ORIGINAL}\n${START}\n${BODY}\n${END}\n`);
  assert.equal(applyBlock(once, BODY), once);
});
test('applyBlock replaces an older block in place', () => {
  const old = applyBlock(ORIGINAL, '- 옛 줄') + '- 뒤에 쓴 줄\n';
  const updated = applyBlock(old, BODY);
  assert.ok(updated.includes(BODY) && !updated.includes('옛 줄'));
  assert.ok(updated.startsWith(ORIGINAL) && updated.endsWith(`${END}\n- 뒤에 쓴 줄\n`));
});
test('removeBlock restores the original text', () => {
  assert.equal(removeBlock(applyBlock(ORIGINAL, BODY)), ORIGINAL);
  assert.equal(removeBlock(ORIGINAL), ORIGINAL);
});
test('applyBlock keeps CRLF and BOM', () => {
  const crlf = '﻿# 전역 지침\r\n- 하나\r\n';
  const out = applyBlock(crlf, BODY);
  assert.ok(out.startsWith(crlf));
  assert.equal(out.replace(/\r\n/g, '').includes('\n'), false);
  assert.equal(removeBlock(out), crlf);
});
test('applyBlock on empty text writes only the block', () => {
  assert.equal(applyBlock('', BODY), `${START}\n${BODY}\n${END}\n`);
  assert.equal(removeBlock(applyBlock('', BODY)), '');
});
test('broken markers are refused', () => {
  assert.throws(() => applyBlock(`${ORIGINAL}${START}\n- 반쪽\n`, BODY), /표시가 깨졌습니다/);
  assert.throws(() => removeBlock(`${END}\n${START}\n`), /표시가 깨졌습니다/);
  assert.throws(() => applyBlock(`${START}\n${END}\n${START}\n${END}\n`, BODY), /표시가 깨졌습니다/);
});
```

- [ ] **Step 2: 실패 확인**
  - Run: `node --test scripts/claude-global/install-intent.test.mjs`
  - Expected: FAIL (`install-intent.mjs`가 없음)
- [ ] **Step 3: `applyBlock`, `removeBlock`, `START`, `END`를 `install-intent.mjs`에 구현**
  - 줄바꿈은 원문에 `\r\n`이 있으면 CRLF, 아니면 LF로 정한다.
- [ ] **Step 4: 통과 확인**
  - Run: `node --test scripts/claude-global/install-intent.test.mjs`
  - Expected: `# pass 6`, `# fail 0`
- [ ] **Step 5: 커밋**

```bash
git add scripts/claude-global/install-intent.mjs scripts/claude-global/install-intent.test.mjs
git commit -m "feat(claude): 전역 지침에 요청 기록 규칙 한 줄을 넣고 빼는 함수" -m "node --test 6/6 통과" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 2: 요청 기록 스킬·양식·규칙 한 줄 (원본 파일과 검사)

**Files:**
- Create: `scripts/claude-global/skills/capture-intent/SKILL.md`
- Create: `scripts/claude-global/skills/capture-intent/template.md`
- Create: `scripts/claude-global/intent-rule.md`
- Modify: `scripts/claude-global/install-intent.mjs` (`validateSource`, `TEMPLATE_HEADINGS` 추가)
- Test: `scripts/claude-global/install-intent.test.mjs`

**Interfaces — Produces:**
- `export const TEMPLATE_HEADINGS = ['## 요청 원문', '## 문제', '## 바라는 결과', '## 영향받는 곳', '## 지켜야 할 것', '## 이번에 하지 않는 것', '## 미정 질문', '## 확인 방법', '## 결과']`
- `export function validateSource(sourceDir: string): { description: string, rule: string }`
  - 읽는 파일: `skills/capture-intent/SKILL.md`, `skills/capture-intent/template.md`, `intent-rule.md`
  - SKILL.md 검사: `---`로 시작하는 머리말에 `name: capture-intent`와 `description:`이 있어야 한다. description은 1~1024자이고 `docs/intent`를 포함해야 한다.
  - template.md 검사: `TEMPLATE_HEADINGS`가 모두 있어야 한다.
  - intent-rule.md 검사: 비어 있지 않은 한 줄이어야 하고, `- `로 시작하며 `` `capture-intent` ``를 포함해야 한다.
  - 틀리면 `Error('<파일 이름>: <무엇이 틀렸는지>')`를 던진다. `rule`은 끝 줄바꿈을 뺀 그 한 줄이다.

- [ ] **Step 1: 실패하는 시험 쓰기** (같은 시험 파일에 추가한다. Task 3도 같이 쓰는 도우미를 파일 위쪽에 둔다)

시험 도우미:
- `HERE`: 시험 파일이 있는 폴더 (`path.dirname(fileURLToPath(import.meta.url))`)
- `read(dir, rel)`: `path.join(dir, rel)`을 utf8로 읽는다.
- `tempConfig(t, files)`: `fs.mkdtempSync(path.join(os.tmpdir(), 'intent-'))`로 임시 폴더를 만든다. `files`의 `{ 상대경로: 내용 }`을 하위 폴더까지 만들어 쓴다. `t.after`에서 `fs.rmSync(…, { recursive: true, force: true })`로 지운다. 폴더 경로를 돌려준다.
- `copySourceToTemp(t)`: `tempConfig(t, {})`로 만든 폴더에 `HERE`의 `skills/`(`fs.cpSync`, recursive)와 `intent-rule.md`를 복사해 돌려준다.
- `replaceIn(dir, rel, from, to)`: 그 파일 안의 `from`을 `to`로 바꿔 다시 쓴다.

```js
test('the shipped skill files pass validation', () => {
  const { description, rule } = validateSource(HERE); // HERE = 이 시험 파일의 폴더
  assert.ok(description.includes('docs/intent') && description.length <= 1024);
  assert.ok(rule.startsWith('- ') && rule.includes('`capture-intent`'));
});
test('validateSource rejects a skill without its name', (t) => {
  const dir = copySourceToTemp(t); // HERE의 skills/와 intent-rule.md를 임시 폴더로 복사
  replaceIn(dir, 'skills/capture-intent/SKILL.md', 'name: capture-intent', 'name: other');
  assert.throws(() => validateSource(dir), /SKILL\.md/);
});
test('validateSource rejects a template missing a section', (t) => {
  const dir = copySourceToTemp(t);
  replaceIn(dir, 'skills/capture-intent/template.md', '## 확인 방법', '## 기타');
  assert.throws(() => validateSource(dir), /template\.md/);
});
```

- [ ] **Step 2: 실패 확인**
  - Run: `node --test scripts/claude-global/install-intent.test.mjs`
  - Expected: 새 시험 3개가 FAIL (`validateSource`가 없음)
- [ ] **Step 3: 원본 파일 세 개를 아래 내용 그대로 만든다**

`scripts/claude-global/skills/capture-intent/SKILL.md`:

```markdown
---
name: capture-intent
description: Use FIRST, before any design or code, when the person asks in any project for a new feature or a change in how something works (e.g. 「…기능 넣어 줘」, 「…하게 바꿔 줘」, 「…도 되게 해 줘」). Writes their request in their own words to docs/intent/ as a short Korean 요청 기록, shows a summary with open questions, and waits for 「진행」 before building. Not for bug reports or symptoms (「안 돼요」), questions, or one-line text or colour edits.
---

# 요청 기록 (capture-intent)

새 기능이나 동작 변경 요청을 설계·코드보다 먼저, 사용자 말 그대로 한 장에 남긴다. superpowers:brainstorming의 「의도 파악·이해한 내용 적어 보이기」 단계는 이 기록으로 한다.

## 언제 쓰나
- 쓴다: 새 기능, 화면이나 동작을 바꾸는 요청, 개발 설정을 바꾸는 요청.
- 쓰지 않는다: 버그·증상 신고, 질문, 글자·색·라벨 한두 개 바꾸기, 이미 확정된 기록의 작업을 이어 하라는 요청.
- 진행 중인 기능에 덧붙는 요청은 새 파일을 만들지 않는다. 그 기록의 「요청 원문」에 날짜와 함께 덧붙이고 나머지를 고친다.

## 순서
1. 지금 동작, 파일 위치 같은 사실은 코드와 문서에서 직접 확인한다. 사용자에게 묻지 않는다.
2. 같은 폴더의 `template.md` 양식으로 프로젝트 루트에 `docs/intent/YYYY-MM-DD-<영문-소문자-하이픈>.md`를 만든다(폴더가 없으면 만든다). 한국어로 40줄 안팎으로 쓴다. 사용자 말은 「요청 원문」에 고치지 않고 옮긴다. 키·비밀번호·개인정보는 [가림]으로 바꾼다. 짐작한 내용에는 (추측)을 붙인다.
3. 한 메시지로 보인다: 기록 파일 링크, 이해한 내용 3~5줄(문제, 바라는 결과, 하지 않는 것, 확인 방법), 미정 질문(4개 이하, 질문마다 추천 답). 끝에 「맞으면 '진행', 고칠 곳이 있으면 그 부분만 말씀해 주세요」라고 쓰고 그 턴을 끝낸다. 코드는 아직 고치지 않는다.
4. 답을 받으면 고친 내용과 답을 기록에 반영하고 상태를 「확정」으로 바꾼다. 「추천대로」라는 답은 남은 미정 질문 모두에 추천 답을 적용한다. 그 프로젝트의 커밋 규칙이 허락하면 기록만 따로 커밋한다(git 저장소가 아니면 파일만 둔다). 그리고 같은 턴에 설계·구현으로 넘어간다.
5. 설계·계획 문서를 만들면 그 문서 첫머리에 기록 경로를 적고, 기록의 「관련 문서」에도 그 문서를 적는다.
6. 끝났다고 말하기 전에 「확인 방법」을 실제로 확인한다. 「결과」에 확인한 것과 커밋을 적고 상태를 「완료」로 바꾼다. 마지막 보고의 「화면에서 확인할 것」은 「확인 방법」에서 고른다. 사용자가 그만두라고 하면 상태를 「취소」로 바꾸고 이유를 한 줄 적는다.

## 작업 폴더가 없을 때
코드나 문서가 있는 작업 폴더라면 git 저장소가 아니어도 기록 파일을 만든다. 사용자 홈 폴더에서 시작한 대화처럼 작업할 폴더가 없으면, 파일은 만들지 않고 3단계 확인만 채팅으로 한다.
```

`scripts/claude-global/skills/capture-intent/template.md`:

```markdown
# 요청 기록: <한 줄 제목>

- 날짜: YYYY-MM-DD
- 상태: 확인 전
- 관련 문서: 없음

## 요청 원문
> (사용자 말을 고치지 않고 그대로. 요청이 이어지면 날짜와 함께 아래에 덧붙인다)

## 문제
(지금 무엇이 안 되거나 불편한가. 짐작한 내용에는 (추측))

## 바라는 결과
(다 되면 무엇이 달라지나)

## 영향받는 곳
(화면, 기능, 데이터, 사람)

## 지켜야 할 것
(이미 정한 규칙, 건드리면 안 되는 것)

## 이번에 하지 않는 것
(범위 밖)

## 미정 질문
1. (질문) — 추천: (답) — 사용자 답: (아직)

## 확인 방법
(다 됐을 때 사용자가 화면에서 직접 확인할 것 1~3개)

## 결과
(완료 때: 실제로 확인한 것과 커밋. 취소 때: 이유 한 줄)
```

`scripts/claude-global/intent-rule.md` (한 줄):

```markdown
- 새 기능이나 동작을 바꾸는 요청은 설계·코드보다 먼저 `capture-intent` 스킬로 사용자 말 그대로의 요청 기록(`docs/intent/`)을 만들고, 이해한 내용과 미정 질문을 한 번에 보여 '진행' 답을 받은 뒤 만든다. 버그·증상 신고, 질문, 한두 줄 수정은 기록하지 않는다.
```

- [ ] **Step 4: `validateSource`, `TEMPLATE_HEADINGS`를 구현한다**
- [ ] **Step 5: 통과 확인**
  - Run: `node --test scripts/claude-global/install-intent.test.mjs`
  - Expected: `# pass 9`, `# fail 0`
- [ ] **Step 6: 커밋**

```bash
git add scripts/claude-global/
git commit -m "feat(claude): 요청 기록 스킬과 양식, 전역 지침 한 줄 원본" -m "node --test 9/9 통과" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 3: 설치·제거와 명령줄

**Files:**
- Modify: `scripts/claude-global/install-intent.mjs`
- Test: `scripts/claude-global/install-intent.test.mjs`

**Interfaces:**
- Consumes: `applyBlock`, `removeBlock` (Task 1), `validateSource` (Task 2)
- Produces:
  - `export function installIntent({ configDir, sourceDir, stamp }): { backupDir: string }`
  - `export function removeIntent({ configDir, stamp }): { backupDir: string | null }`
  - 명령줄: `node scripts/claude-global/install-intent.mjs [--remove]`
    - 설정 폴더 = `CLAUDE_CONFIG_DIR` 또는 `~/.claude`, 원본 폴더 = 스크립트가 있는 폴더, `stamp` = 지금 시각 `YYYY-MM-DD-HHMM`.
    - 성공하면 한국어로 무엇을 했는지, 백업 위치, 「새 대화부터 적용됩니다」를 출력하고 exit 0으로 끝낸다.
    - 실패하면 `[중단] <이유>`를 출력하고 exit 1로 끝낸다. 이때 아무것도 바꾸지 않는다.
- `installIntent` 순서 (앞의 1~2단계에서 실패하면 아무것도 바꾸지 않는다):
  1. `validateSource`로 원본을 검사한다.
  2. 새 CLAUDE.md 내용을 계산한다(`applyBlock(옛 내용 또는 '', rule)`).
  3. 백업한다: 옛 `CLAUDE.md`와 옛 `skills/capture-intent/`가 있으면 복사해 둔다.
  4. `skills/capture-intent/`를 원본으로 통째로 바꾼다.
  5. CLAUDE.md를 임시 파일에 쓴 뒤 이름 바꾸기로 바꾼다.
- `removeIntent` 순서:
  1. 새 CLAUDE.md 내용을 계산한다(`removeBlock`).
  2. 백업한다.
  3. CLAUDE.md를 쓴다. 남은 내용이 공백뿐이면 파일을 지운다.
  4. `skills/capture-intent/`를 지운다.
  - 설치된 것이 없으면 「설치된 것이 없습니다」를 출력하고, 백업 없이(`backupDir: null`) 끝낸다.

- [ ] **Step 1: 실패하는 시험 쓰기** (Task 2의 도우미를 쓴다. `RULE = validateSource(HERE).rule`)

```js
test('installIntent copies the skill and keeps the old CLAUDE.md in the backup', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL });
  const { backupDir } = installIntent({ configDir: cfg, sourceDir: HERE, stamp: '2026-10-02-0900' });
  for (const f of ['SKILL.md', 'template.md'])
    assert.equal(read(cfg, `skills/capture-intent/${f}`), read(HERE, `skills/capture-intent/${f}`));
  assert.equal(read(cfg, 'CLAUDE.md'), applyBlock(ORIGINAL, RULE)); // RULE = validateSource(HERE).rule
  assert.equal(fs.readFileSync(path.join(backupDir, 'CLAUDE.md'), 'utf8'), ORIGINAL);
  assert.equal(backupDir, path.join(cfg, '_reset_backup', '2026-10-02-0900-intent'));
});
test('installIntent twice changes nothing the second time', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL });
  installIntent({ configDir: cfg, sourceDir: HERE, stamp: 's' });
  const first = read(cfg, 'CLAUDE.md');
  const { backupDir } = installIntent({ configDir: cfg, sourceDir: HERE, stamp: 's' });
  assert.equal(read(cfg, 'CLAUDE.md'), first);
  assert.ok(backupDir.endsWith('s-intent-2'));
});
test('installIntent works without a CLAUDE.md and removeIntent cleans up', (t) => {
  const cfg = tempConfig(t, {});
  installIntent({ configDir: cfg, sourceDir: HERE, stamp: 'a' });
  removeIntent({ configDir: cfg, stamp: 'b' });
  assert.equal(fs.existsSync(path.join(cfg, 'CLAUDE.md')), false);
  assert.equal(fs.existsSync(path.join(cfg, 'skills/capture-intent')), false);
});
test('removeIntent keeps other skills and other lines', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL, 'skills/other/SKILL.md': 'x' });
  installIntent({ configDir: cfg, sourceDir: HERE, stamp: 'a' });
  removeIntent({ configDir: cfg, stamp: 'b' });
  assert.equal(read(cfg, 'CLAUDE.md'), ORIGINAL);
  assert.equal(read(cfg, 'skills/other/SKILL.md'), 'x');
});
test('a broken CLAUDE.md marker leaves everything untouched', (t) => {
  const broken = `${ORIGINAL}${START}\n- 반쪽\n`;
  const cfg = tempConfig(t, { 'CLAUDE.md': broken });
  assert.throws(() => installIntent({ configDir: cfg, sourceDir: HERE, stamp: 'a' }), /표시가 깨졌습니다/);
  assert.equal(read(cfg, 'CLAUDE.md'), broken);
  assert.equal(fs.existsSync(path.join(cfg, 'skills/capture-intent')), false);
  assert.equal(fs.existsSync(path.join(cfg, '_reset_backup')), false);
});
test('command line installs into CLAUDE_CONFIG_DIR and removes with --remove', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL });
  const run = (...a) => spawnSync(process.execPath, [path.join(HERE, 'install-intent.mjs'), ...a],
    { env: { ...process.env, CLAUDE_CONFIG_DIR: cfg }, encoding: 'utf8' });
  const inst = run();
  assert.equal(inst.status, 0);
  assert.match(inst.stdout, /새 대화부터 적용됩니다/);
  assert.ok(fs.existsSync(path.join(cfg, 'skills/capture-intent/SKILL.md')));
  assert.equal(run('--remove').status, 0);
  assert.equal(fs.existsSync(path.join(cfg, 'skills/capture-intent')), false);
  assert.equal(read(cfg, 'CLAUDE.md'), ORIGINAL);
});
```

- [ ] **Step 2: 실패 확인**
  - Run: `node --test scripts/claude-global/install-intent.test.mjs`
  - Expected: 새 시험 6개가 FAIL
- [ ] **Step 3: `installIntent`, `removeIntent`, 명령줄 부분을 구현한다**
  - 명령줄 부분은 이 파일을 직접 실행했을 때만 돈다. 확인 방법: `path.resolve(fileURLToPath(import.meta.url))`와 `path.resolve(process.argv[1])`를 대소문자 무시로 비교한다.
- [ ] **Step 4: 통과 확인**
  - Run: `node --test scripts/claude-global/install-intent.test.mjs`
  - Expected: `# pass 15`, `# fail 0`
  - Run: `ls ~/.claude/skills`
  - Expected: `capture-intent`가 없다(시험이 진짜 설정을 건드리지 않았다).
- [ ] **Step 5: 커밋**

```bash
git add scripts/claude-global/install-intent.mjs scripts/claude-global/install-intent.test.mjs
git commit -m "feat(claude): 요청 기록 전역 설치·제거 프로그램" -m "node --test 15/15 통과, 진짜 ~/.claude 변경 없음" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 4: Strata 작업 지침에 예외 한 줄 (확인 한 번은 멈춘다)

**Files:**
- Modify: `CLAUDE.md` — 「superpowers 사용 방식」의 규칙 2, 규칙 4, 그리고 `- brainstorming:` 항목

- [ ] **Step 1: 규칙 2 끝에 덧붙인다**
  - 덧붙일 문장: ` 단, 새 기능·동작 변경은 전역 지침의 요청 기록 확인에서 한 번 멈춘다(아래 brainstorming 항목).`
- [ ] **Step 2: 규칙 4의 문장 하나를 바꾼다**
  - 바꿀 문장: `코드를 쓰기 전에 한 번만 묻는다.`
  - 새 문장: `코드를 쓰기 전에 한 번만, 요청 기록 확인 메시지에 묶어 묻는다.`
- [ ] **Step 3: `- brainstorming:` 항목 전체를 다음으로 바꾼다**

```markdown
- brainstorming: for a new feature or behavior change, start with the global `capture-intent` skill: write the record in `docs/intent/`, show it with the rule-4 questions in one message, stop once for 「진행」, then commit the record alone on the session branch. From there the request is the approval (rule 2): say the path (spike/bounded/architectural), write the design in a few Korean sentences and go on in the same turn. The skill's HARD-GATE, one-question-per-message, approve-each-section and spec-review waits do not apply here; a spec file only under rule 3, and its first lines name the record. Do not offer the visual companion; describe screens in words.
```

- [ ] **Step 4: 확인**
  - Run: `git diff --stat`
  - Expected: `CLAUDE.md`만 바뀌었다.
  - Run: `grep -n "capture-intent" CLAUDE.md`
  - Expected: brainstorming 항목 1줄이 나온다.
- [ ] **Step 5: 커밋** (문서만 바뀌었으므로 저장 전 검사 장치를 통과한다)

```bash
git add CLAUDE.md
git commit -m "docs(claude): 새 기능은 요청 기록 확인에서 한 번 멈춘다" -m "문서만 변경, 빌드·시험 해당 없음" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 5: 전역 설치와 실제 행동 시험 (사용자가 '진행'이라고 한 뒤)

**Files:**
- 바꾸는 곳: `~/.claude/skills/capture-intent/`, `~/.claude/CLAUDE.md` (설치 프로그램으로만 바꾼다)
- 시험 폴더: 이 세션의 scratchpad 아래 `intent-trial-a`, `intent-trial-b`, `intent-trial-c`

- [ ] **Step 1: 설치**
  - Run: `node scripts/claude-global/install-intent.mjs`
  - Expected: exit 0, 백업 경로 출력.
  - 하네스가 자기 설정을 바꾸는 일이라며 막으면, 사용자에게 같은 명령 한 줄을 터미널에 붙여 넣어 달라고 하고, 실행한 뒤에 이어 간다.
- [ ] **Step 2: 설치 결과 확인**
  - Run: `head -n 7 ~/.claude/CLAUDE.md | diff - <백업>/CLAUDE.md`
  - Expected: 차이 없음 (기존 다섯 줄이 그대로다).
  - Run: `ls ~/.claude/skills/capture-intent`
  - Expected: `SKILL.md`, `template.md`
- [ ] **Step 3: 시험 A (git 저장소, 기능 요청)**
  - 준비: `intent-trial-a`에 `git init`, `README.md`(`# 작은 지도 앱`), `app.py`(`print("map")`)를 만들고 첫 커밋을 한다.
  - Run (시험 폴더 안에서, 출력은 폴더 밖에): `claude -p "지도에 거리 재기 기능을 넣어 줘. 테스트용 키 TEST-KEY-0000도 같이 써 줘" --permission-mode acceptEdits --max-turns 25 > ../trial-a.txt`
  - Expected, 다섯 가지 모두:
    1. `docs/intent/*.md`가 정확히 1개 있다.
    2. 그 파일에 `TEMPLATE_HEADINGS` 9개와 `상태: 확인 전`이 있다.
    3. 그 파일에 `TEST-KEY-0000`이 없다.
    4. `git status --porcelain`에 `docs/intent/` 말고는 바뀐 것이 없다.
    5. `../trial-a.txt`에 `진행`이 있다.
- [ ] **Step 4: 시험 B (git 저장소, 증상 신고)**
  - 준비: 같은 방법으로 `intent-trial-b`를 만든다.
  - Run: `claude -p "저장 단추를 눌러도 저장이 안 돼요" --permission-mode acceptEdits --max-turns 15 > ../trial-b.txt`
  - Expected: `docs/intent`가 없다.
- [ ] **Step 5: 시험 C (git이 아닌 작업 폴더, 기능 요청)**
  - 준비: `intent-trial-c`에 `app.py`만 두고 git은 쓰지 않는다.
  - Run: `claude -p "목록을 엑셀로 내보내는 기능을 넣어 줘" --permission-mode acceptEdits --max-turns 25 > ../trial-c.txt`
  - Expected: `docs/intent/*.md`가 1개 있고, `.git`은 없다.
- [ ] **Step 6: 실패하면**
  - 고칠 곳: `SKILL.md`의 description과 「언제 쓰나」, 또는 `intent-rule.md`의 문구.
  - 고친 뒤 할 일: Task 2·3 시험 다시 통과 → 재설치 → 실패한 시험만 다시 실행.
  - 세 번 고쳐도 실패하면 멈추고 무엇을 해 봤는지 보고한다.
  - 문구를 고쳤다면 커밋한다: `fix(claude): 요청 기록 스킬이 불리는 조건을 고친다`.

### Task 6: 마무리

- [ ] **Step 1: 코드 검토**
  - 여러 파일을 바꿨으므로 superpowers:requesting-code-review로 브랜치 전체를 검토받는다.
  - 이 변경 안의 진짜 문제는 고치고, 나머지는 보고에 한 줄로 적는다.
- [ ] **Step 2: 이 일의 요청 기록을 마무리한다** (`docs/intent/2026-10-02-global-intent-records.md`)
  - 상태를 「완료」로 바꾼다.
  - 「결과」에 시험 A·B·C 결과, 설치 백업 경로, 커밋을 적는다.
- [ ] **Step 3: 커밋**

```bash
git add docs/intent/2026-10-02-global-intent-records.md
git commit -m "docs(intent): 전역 요청 기록 도입 완료를 기록한다" -m "node --test 15/15, 행동 시험 A·B·C 통과" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 4: 보고 (한국어, 짧게)**
  - 전역 설치는 모든 프로젝트에서 새 대화부터 적용된다.
  - Strata 작업 지침 변경은 사용자가 「커밋 푸시」라고 해야 새 Strata 대화에 들어간다(Git 절: main 빨리 감기와 GitHub push).
  - 되돌리는 명령: `node scripts/claude-global/install-intent.mjs --remove`
  - 화면에서 확인할 것: 새 대화에서 기능을 하나 요청하면, 코드보다 먼저 요청 기록 요약과 「진행」 질문이 나오고, 왼쪽 파일 목록 `docs/intent/`에 파일이 생긴다.
