// 요청 기록(capture-intent) 전역 설치 / 제거.
//
// The global CLAUDE.md gets one rule line between two markers, so it can be added, updated and
// removed again without touching the person's own lines. The original line ending (CRLF/LF) and a
// leading BOM are kept.
import fs from 'node:fs';
import path from 'node:path';

export const START = '<!-- capture-intent:start -->';
export const END = '<!-- capture-intent:end -->';
export const TEMPLATE_HEADINGS = ['## 요청 원문', '## 문제', '## 바라는 결과', '## 영향받는 곳', '## 지켜야 할 것',
  '## 이번에 하지 않는 것', '## 미정 질문', '## 확인 방법', '## 결과'];

const SKILL = 'capture-intent';
const BOM = String.fromCharCode(0xfeff);
const BROKEN = '전역 지침의 capture-intent 표시가 깨졌습니다';

const occurrences = (text, needle) => text.split(needle).length - 1;
const eolOf = (text) => (text.includes('\r\n') ? '\r\n' : '\n');
const stripBom = (text) => (text.startsWith(BOM) ? text.slice(BOM.length) : text);

function readSource(sourceDir, rel) {
  const file = path.join(sourceDir, rel);
  if (!fs.existsSync(file)) throw new Error(`${path.basename(rel)}: 파일이 없습니다 (${file})`);
  return stripBom(fs.readFileSync(file, 'utf8')).replace(/\r\n/g, '\n');
}

// Checks the files the installer copies, so a broken source never reaches ~/.claude.
export function validateSource(sourceDir) {
  const skill = readSource(sourceDir, `skills/${SKILL}/SKILL.md`);
  const head = /^---\n([\s\S]*?)\n---(\n|$)/.exec(skill);
  if (!head) throw new Error('SKILL.md: 맨 위 머리말(---)이 없습니다');
  if (!new RegExp(`^name: ${SKILL}[ \\t]*$`, 'm').test(head[1])) {
    throw new Error(`SKILL.md: 머리말에 name: ${SKILL} 이 없습니다`);
  }
  const description = (/^description:(.*)$/m.exec(head[1])?.[1] ?? '').trim();
  if (!description || description.length > 1024) throw new Error('SKILL.md: description 이 비었거나 1024자를 넘습니다');
  if (!description.includes('docs/intent')) throw new Error('SKILL.md: description 에 docs/intent 가 없습니다');

  const template = readSource(sourceDir, `skills/${SKILL}/template.md`).split('\n').map((line) => line.trimEnd());
  const missing = TEMPLATE_HEADINGS.filter((heading) => !template.includes(heading));
  if (missing.length) throw new Error(`template.md: ${missing.join(', ')} 칸이 없습니다`);

  const rule = readSource(sourceDir, 'intent-rule.md').split('\n').filter((line) => line.trim());
  if (rule.length !== 1 || !rule[0].startsWith('- ') || !rule[0].includes(`\`${SKILL}\``)) {
    throw new Error(`intent-rule.md: '- '로 시작하고 \`${SKILL}\`을 담은 한 줄이어야 합니다`);
  }
  return { description, rule: rule[0].trimEnd() };
}

// null when there are no markers; throws when they are half there, reversed or repeated.
function findMarkers(text) {
  const starts = occurrences(text, START);
  const ends = occurrences(text, END);
  if (starts === 0 && ends === 0) return null;
  if (starts !== 1 || ends !== 1) throw new Error(BROKEN);
  const start = text.indexOf(START);
  const end = text.indexOf(END);
  if (end < start) throw new Error(BROKEN);
  return { start, end: end + END.length };
}

export function applyBlock(text, body) {
  const eol = eolOf(text);
  const lines = body.replace(/\r\n/g, '\n').replace(/\n+$/, '').split('\n').join(eol);
  const block = `${START}${eol}${lines}${eol}${END}`;
  const markers = findMarkers(text);
  if (markers) return text.slice(0, markers.start) + block + text.slice(markers.end);
  const bom = text.startsWith(BOM) ? BOM : '';
  if (text.length === bom.length) return `${bom}${block}${eol}`;
  const base = text.endsWith('\n') ? text : text + eol;
  return `${base}${eol}${block}${eol}`;
}

export function removeBlock(text) {
  const markers = findMarkers(text);
  if (!markers) return text;
  const eol = eolOf(text);
  let from = markers.start;
  let to = markers.end;
  if (text.startsWith(eol, to)) to += eol.length;
  if (text.slice(0, from).endsWith(eol + eol)) from -= eol.length;
  return text.slice(0, from) + text.slice(to);
}
