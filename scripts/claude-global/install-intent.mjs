// 요청 기록(capture-intent) 전역 설치 / 제거.
//
//   node scripts/claude-global/install-intent.mjs            설치 (다시 실행하면 새 원본으로 갱신)
//   node scripts/claude-global/install-intent.mjs --remove   제거
//
// Install copies skills/capture-intent/SKILL.md (the record template is inside it) into
// <config>/skills/capture-intent/, replacing that folder, and puts the lines from intent-rule.md
// (the request-record rule and the skill-routing rule) into <config>/CLAUDE.md between two
// markers, so they can be updated and removed
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

  // The record template sits inside SKILL.md, which reaches the model with the skill itself; a
  // separate file under ~/.claude lies outside the project and reading it can be refused.
  const lines = skill.split('\n').map((line) => line.trimEnd());
  const missing = TEMPLATE_HEADINGS.filter((heading) => !lines.includes(heading));
  if (missing.length) throw new Error(`SKILL.md: 양식에 ${missing.join(', ')} 칸이 없습니다`);

  // One "- " line per rule: the request-record rule, plus the skill-routing rule beside it.
  const rule = readSource(sourceDir, 'intent-rule.md').split('\n').filter((line) => line.trim())
    .map((line) => line.trimEnd());
  if (!rule.length || rule.some((line) => !line.startsWith('- '))
      || !rule.some((line) => line.includes(`\`${SKILL}\``))) {
    throw new Error(`intent-rule.md: 줄마다 '- '로 시작하고, 그중 한 줄은 \`${SKILL}\`을 담아야 합니다`);
  }
  return { description, rule: rule.join('\n') };
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
const NOT_UTF8 = [String.fromCharCode(0xfffd), String.fromCharCode(0)];

// An ANSI (cp949) or UTF-16 CLAUDE.md read as UTF-8 shows replacement or NUL characters; writing
// that text back would destroy the person's lines, so such a file is left alone.
function readClaudeMd(file) {
  const text = readIfExists(file);
  if (text !== null && NOT_UTF8.some((ch) => text.includes(ch))) {
    throw new Error('CLAUDE.md 파일이 UTF-8이 아니라서 건드리지 않았습니다. 메모장에서 UTF-8로 다시 저장한 뒤 실행하세요.');
  }
  return text;
}

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

const sleep = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
const BUSY = ['EPERM', 'EACCES', 'EBUSY'];

// Writes a temporary file and renames it over `file`. Windows refuses that rename for a moment
// while another program (a virus scanner, a running Claude Code) has the file open, so it keeps
// trying for about seven seconds; when it gives up, the temporary file is removed again.
export function replaceFile(file, text, { rename = fs.renameSync, wait = sleep, tries = 20 } = {}) {
  const tmp = `${file}.intent-tmp`;
  fs.writeFileSync(tmp, text, 'utf8');
  for (let attempt = 1; ; attempt++) {
    try {
      rename(tmp, file);
      return;
    } catch (error) {
      const busy = BUSY.includes(error.code);
      if (!busy || attempt >= tries) {
        fs.rmSync(tmp, { force: true });
        if (busy) throw new Error(`${path.basename(file)} 파일을 다른 프로그램이 쓰고 있어 바꾸지 못했습니다. 잠시 뒤 다시 실행하세요.`);
        throw error;
      }
      wait(Math.min(50 * attempt, 500));
    }
  }
}

export function installIntent({ configDir, sourceDir, stamp }) {
  const { rule } = validateSource(sourceDir);
  const claudeMd = path.join(configDir, 'CLAUDE.md');
  const skillDir = path.join(configDir, 'skills', SKILL);
  const next = applyBlock(readClaudeMd(claudeMd) ?? '', rule);

  const backupDir = newBackupDir(configDir, stamp);
  backUp(backupDir, claudeMd, skillDir);
  // CLAUDE.md first: it is the step another program can block, and if it fails the skill folder
  // has not been touched yet.
  replaceFile(claudeMd, next);
  fs.rmSync(skillDir, { recursive: true, force: true });
  fs.mkdirSync(skillDir, { recursive: true });
  fs.copyFileSync(path.join(sourceDir, 'skills', SKILL, 'SKILL.md'), path.join(skillDir, 'SKILL.md'));
  return { backupDir };
}

export function removeIntent({ configDir, stamp }) {
  const claudeMd = path.join(configDir, 'CLAUDE.md');
  const skillDir = path.join(configDir, 'skills', SKILL);
  const old = readClaudeMd(claudeMd);
  const next = old === null ? null : removeBlock(old);
  const hasRule = old !== null && next !== old;
  const hasSkill = fs.existsSync(skillDir);
  if (!hasRule && !hasSkill) return { backupDir: null };

  const backupDir = newBackupDir(configDir, stamp);
  backUp(backupDir, claudeMd, skillDir);
  if (hasRule) {
    if (next.trim() === '') fs.rmSync(claudeMd);
    else replaceFile(claudeMd, next);
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
  const unknown = args.filter((arg) => arg !== '--remove');
  if (unknown.length) {
    console.error(`[중단] 알 수 없는 옵션: ${unknown.join(' ')} (쓸 수 있는 것: --remove)`);
    return 1;
  }
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
