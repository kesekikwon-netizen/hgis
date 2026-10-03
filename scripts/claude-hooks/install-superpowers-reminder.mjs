#!/usr/bin/env node
// Installs the Strata Claude hooks in the Claude Code user settings, or takes them out with
// --remove: the per-message superpowers reminder (UserPromptSubmit) and the closing-step check
// (finish-check.mjs on Stop, docs/intent/2026-10-03-finish-check.md), plus, for every project, the Jev
// stage hint (jev-skill-route.mjs with its helper jev-ask.mjs, docs/intent/2026-10-03-jev-stage-hint.md). Copies each script to
// <config>/hooks/ (so it survives the worktree) and keeps exactly one hook entry for each. Every other setting and every other tool's
// hook stays as it was, and settings.json is backed up to
// <config>/_reset_backup/<stamp>-superpowers-reminder/ before a change.
// A settings.json that is not valid JSON is left untouched.
//   node scripts/claude-hooks/install-superpowers-reminder.mjs [--remove] [--config-dir DIR] [--root DIR]
// --config-dir defaults to CLAUDE_CONFIG_DIR or ~/.claude; --root to the main checkout of this repo.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HOOKS = [
  { mark: 'superpowers-reminder.mjs', event: 'UserPromptSubmit' },
  { mark: 'finish-check.mjs', event: 'Stop' },
  { mark: 'jev-skill-route.mjs', event: 'UserPromptSubmit', everyProject: true, helper: { name: 'jev-ask.mjs', from: '../jev/jev-ask.mjs' } },
];
const here = path.dirname(fileURLToPath(import.meta.url));
const argValue = (name) => {
  const i = process.argv.indexOf(name);
  return i > 0 ? process.argv[i + 1] : undefined;
};
const slashes = (p) => p.replace(/\\/g, '/');

// The main checkout (for example A:\qgis) also holds every desktop worktree under .claude\worktrees.
const repoRoot = () =>
  path.dirname(execFileSync('git', ['rev-parse', '--path-format=absolute', '--git-common-dir'],
                            { cwd: here, encoding: 'utf8' }).trim());

const remove = process.argv.includes('--remove');
const configDir = path.resolve(argValue('--config-dir') || process.env.CLAUDE_CONFIG_DIR || path.join(os.homedir(), '.claude'));
const settingsFile = path.join(configDir, 'settings.json');
const hookFileOf = (mark) => path.join(configDir, 'hooks', mark);

let text = null;
let settings = {};
try {
  if (fs.existsSync(settingsFile)) text = fs.readFileSync(settingsFile, 'utf8');
  const body = (text ?? '').replace(/^\uFEFF/, '');
  if (body.trim()) settings = JSON.parse(body);
  if (!settings || typeof settings !== 'object' || Array.isArray(settings)) throw new Error('맨 바깥이 { } 가 아닙니다.');
} catch (error) {
  console.error(`설정 파일을 읽지 못해 아무것도 바꾸지 않았습니다: ${settingsFile}\n${error.message}`);
  process.exit(1);
}

const before = JSON.stringify(settings);
const hooks = settings.hooks ?? {};
// Drop only our own hook; a neighbour hook in the same entry stays.
const without = (mark) => (entry) => {
  const all = entry?.hooks ?? [];
  const kept = all.filter((h) => !String(h?.command ?? '').includes(mark));
  if (kept.length === all.length) return entry;
  return kept.length ? { ...entry, hooks: kept } : null;
};
const root = remove ? null : slashes(path.resolve(argValue('--root') || repoRoot()));
for (const { mark, event, everyProject } of HOOKS) {
  const others = (hooks[event] ?? []).map(without(mark)).filter(Boolean);
  if (remove) {
    if (others.length) hooks[event] = others;
    else delete hooks[event];
  } else {
    const command = `node "${slashes(hookFileOf(mark))}"` + (everyProject ? '' : ` --root "${root}"`);
    hooks[event] = [...others, { hooks: [{ type: 'command', command, timeout: everyProject ? 3 : 10 }] }];
  }
}
if (Object.keys(hooks).length) settings.hooks = hooks;
else delete settings.hooks;

// Each installed file and where it comes from in the repo.
const files = HOOKS.flatMap(({ mark, helper }) => [
  { name: mark, from: path.join(here, mark) },
  ...(helper ? [{ name: helper.name, from: path.join(here, helper.from) }] : []),
]);
const current = ({ name, from }) => fs.existsSync(hookFileOf(name))
  && fs.readFileSync(hookFileOf(name), 'utf8') === fs.readFileSync(from, 'utf8');
const settingsChanged = JSON.stringify(settings) !== before || (text === null && !remove);
if (!settingsChanged && (remove ? files.every(({ name }) => !fs.existsSync(hookFileOf(name))) : files.every(current))) {
  console.log(remove ? '알림, 마무리 직전 확인, Jev 단계 추천 장치가 이미 없습니다. 바꾼 것이 없습니다.' : '알림, 마무리 직전 확인, Jev 단계 추천 장치가 이미 설치되어 있습니다. 바꾼 것이 없습니다.');
  process.exit(0);
}

if (text !== null && settingsChanged) {
  const now = new Date();
  const two = (n) => String(n).padStart(2, '0');
  const stamp = `${now.getFullYear()}-${two(now.getMonth() + 1)}-${two(now.getDate())}-` +
                `${two(now.getHours())}${two(now.getMinutes())}${two(now.getSeconds())}`;
  let backupDir = path.join(configDir, '_reset_backup', `${stamp}-superpowers-reminder`);
  for (let n = 2; fs.existsSync(backupDir); ++n)
    backupDir = path.join(configDir, '_reset_backup', `${stamp}-${n}-superpowers-reminder`);
  fs.mkdirSync(backupDir, { recursive: true });
  fs.copyFileSync(settingsFile, path.join(backupDir, 'settings.json'));
  console.log(`예전 설정을 백업했습니다: ${backupDir}`);
}
if (!remove) {
  // The hook files go in before settings point at them, and come out only after they stop.
  fs.mkdirSync(path.join(configDir, 'hooks'), { recursive: true });
  for (const { name, from } of files) fs.copyFileSync(from, hookFileOf(name));
}
if (settingsChanged) {
  fs.mkdirSync(configDir, { recursive: true });
  // Running sessions watch settings.json; write a whole file and swap it in.
  const temporary = `${settingsFile}.superpowers-reminder.tmp`;
  fs.writeFileSync(temporary, `${JSON.stringify(settings, null, 2)}\n`);
  fs.renameSync(temporary, settingsFile);
}
if (remove) for (const { name } of files) fs.rmSync(hookFileOf(name), { force: true });
console.log(remove
  ? '메시지마다 붙던 슈퍼파워 알림, 마무리 직전 확인, Jev 단계 추천 장치를 뺐습니다. 새로 여는 대화부터 확실히 적용됩니다.'
  : '메시지마다 슈퍼파워 알림, 마무리 직전 확인, Jev 단계 추천(모든 프로젝트) 장치를 설치했습니다. 새로 여는 대화부터 확실히 적용됩니다.');
