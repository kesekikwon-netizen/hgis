// 요청 기록(capture-intent) 전역 설치 / 제거.
//
//   node scripts/claude-global/install-intent.mjs            설치 (다시 실행하면 새 원본으로 갱신)
//   node scripts/claude-global/install-intent.mjs --remove   제거
//
// Install copies skills/capture-intent/ into <config>/skills/ and puts the one line from
// intent-rule.md into <config>/CLAUDE.md between two markers, so it can be updated and removed
// again without touching the person's own lines (their line ending and a leading BOM are kept).
// <config> is CLAUDE_CONFIG_DIR, else ~/.claude. Before changing anything it copies the old
// CLAUDE.md and skill folder to <config>/_reset_backup/<YYYY-MM-DD-HHMM>-intent/.
// A broken source or broken markers stop it before any file changes (exit 1).
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

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

const readIfExists = (file) => (fs.existsSync(file) ? fs.readFileSync(file, 'utf8') : null);

// <config>/_reset_backup/<stamp>-intent, or -2, -3 … when that name is already taken.
function newBackupDir(configDir, stamp) {
  const base = path.join(configDir, '_reset_backup', `${stamp}-intent`);
  let dir = base;
  for (let n = 2; fs.existsSync(dir); n++) dir = `${base}-${n}`;
  fs.mkdirSync(dir, { recursive: true });
  return dir;
}

function backUp(backupDir, claudeMd, skillDir) {
  if (fs.existsSync(claudeMd)) fs.copyFileSync(claudeMd, path.join(backupDir, 'CLAUDE.md'));
  if (fs.existsSync(skillDir)) fs.cpSync(skillDir, path.join(backupDir, 'skills', SKILL), { recursive: true });
}

function writeAtomically(file, text) {
  const tmp = `${file}.intent-tmp`;
  fs.writeFileSync(tmp, text, 'utf8');
  fs.renameSync(tmp, file);
}

export function installIntent({ configDir, sourceDir, stamp }) {
  const { rule } = validateSource(sourceDir);
  const claudeMd = path.join(configDir, 'CLAUDE.md');
  const skillDir = path.join(configDir, 'skills', SKILL);
  const next = applyBlock(readIfExists(claudeMd) ?? '', rule);

  const backupDir = newBackupDir(configDir, stamp);
  backUp(backupDir, claudeMd, skillDir);
  fs.rmSync(skillDir, { recursive: true, force: true });
  fs.mkdirSync(skillDir, { recursive: true });
  for (const name of ['SKILL.md', 'template.md']) {
    fs.copyFileSync(path.join(sourceDir, 'skills', SKILL, name), path.join(skillDir, name));
  }
  writeAtomically(claudeMd, next);
  return { backupDir };
}

export function removeIntent({ configDir, stamp }) {
  const claudeMd = path.join(configDir, 'CLAUDE.md');
  const skillDir = path.join(configDir, 'skills', SKILL);
  const old = readIfExists(claudeMd);
  const next = old === null ? null : removeBlock(old);
  const hasRule = old !== null && next !== old;
  const hasSkill = fs.existsSync(skillDir);
  if (!hasRule && !hasSkill) return { backupDir: null };

  const backupDir = newBackupDir(configDir, stamp);
  backUp(backupDir, claudeMd, skillDir);
  if (hasRule) {
    if (next.trim() === '') fs.rmSync(claudeMd);
    else writeAtomically(claudeMd, next);
  }
  fs.rmSync(skillDir, { recursive: true, force: true });
  return { backupDir };
}

function stampNow(date = new Date()) {
  const two = (n) => String(n).padStart(2, '0');
  return `${date.getFullYear()}-${two(date.getMonth() + 1)}-${two(date.getDate())}-`
    + `${two(date.getHours())}${two(date.getMinutes())}`;
}

function main(args) {
  const configDir = process.env.CLAUDE_CONFIG_DIR || path.join(os.homedir(), '.claude');
  const sourceDir = path.dirname(fileURLToPath(import.meta.url));
  try {
    if (args.includes('--remove')) {
      const { backupDir } = removeIntent({ configDir, stamp: stampNow() });
      if (!backupDir) {
        console.log('설치된 것이 없습니다.');
        return 0;
      }
      console.log(`요청 기록 스킬과 전역 지침의 규칙 한 줄을 뺐습니다: ${configDir}`);
      console.log(`백업: ${backupDir}`);
    } else {
      const { backupDir } = installIntent({ configDir, sourceDir, stamp: stampNow() });
      console.log(`요청 기록 스킬을 설치했습니다: ${path.join(configDir, 'skills', SKILL)}`);
      console.log(`전역 지침에 규칙 한 줄을 넣었습니다: ${path.join(configDir, 'CLAUDE.md')}`);
      console.log(`백업: ${backupDir}`);
    }
    console.log('새 대화부터 적용됩니다.');
    return 0;
  } catch (error) {
    console.error(`[중단] ${error.message}`);
    return 1;
  }
}

// Importing this module (the tests do) runs nothing; only `node install-intent.mjs` does.
const invokedDirectly = Boolean(process.argv[1])
  && path.resolve(process.argv[1]).toLowerCase() === path.resolve(fileURLToPath(import.meta.url)).toLowerCase();
if (invokedDirectly) process.exitCode = main(process.argv.slice(2));
