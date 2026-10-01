#!/usr/bin/env node
// Installs the per-message superpowers reminder in the Claude Code user settings, or takes it out
// with --remove. Copies superpowers-reminder.mjs to <config>/hooks/ (so it survives the worktree)
// and keeps exactly one UserPromptSubmit entry for it. Every other setting stays as it was, and
// settings.json is backed up to <config>/_reset_backup/<stamp>-superpowers-reminder/ before a change.
// A settings.json that is not valid JSON is left untouched.
//   node scripts/claude-hooks/install-superpowers-reminder.mjs [--remove] [--config-dir DIR] [--root DIR]
// --config-dir defaults to CLAUDE_CONFIG_DIR or ~/.claude; --root to the main checkout of this repo.
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const MARK = 'superpowers-reminder.mjs';
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
const configDir = path.resolve(argValue('--config-dir') ?? process.env.CLAUDE_CONFIG_DIR ?? path.join(os.homedir(), '.claude'));
const settingsFile = path.join(configDir, 'settings.json');
const hookFile = path.join(configDir, 'hooks', MARK);

const text = fs.existsSync(settingsFile) ? fs.readFileSync(settingsFile, 'utf8') : null;
let settings = {};
try {
  if (text !== null) settings = JSON.parse(text);
  if (!settings || typeof settings !== 'object' || Array.isArray(settings)) throw new Error('맨 바깥이 { } 가 아닙니다.');
} catch (error) {
  console.error(`설정 파일을 읽지 못해 아무것도 바꾸지 않았습니다: ${settingsFile}\n${error.message}`);
  process.exit(1);
}

const before = JSON.stringify(settings);
const hooks = settings.hooks ?? {};
const ours = (entry) => (entry?.hooks ?? []).some((h) => String(h?.command ?? '').includes(MARK));
const others = (hooks.UserPromptSubmit ?? []).filter((entry) => !ours(entry));
if (remove) {
  if (others.length) hooks.UserPromptSubmit = others;
  else delete hooks.UserPromptSubmit;
} else {
  const root = slashes(argValue('--root') ?? repoRoot());
  const command = `node "${slashes(hookFile)}" --root "${root}"`;
  hooks.UserPromptSubmit = [...others, { hooks: [{ type: 'command', command }] }];
}
if (Object.keys(hooks).length) settings.hooks = hooks;
else delete settings.hooks;

const source = path.join(here, MARK);
const hookCurrent = fs.existsSync(hookFile) && fs.readFileSync(hookFile, 'utf8') === fs.readFileSync(source, 'utf8');
const settingsChanged = JSON.stringify(settings) !== before || text === null;
if (!settingsChanged && (remove ? !fs.existsSync(hookFile) : hookCurrent)) {
  console.log(remove ? '알림이 이미 없습니다. 바꾼 것이 없습니다.' : '알림이 이미 설치되어 있습니다. 바꾼 것이 없습니다.');
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
  fs.writeFileSync(path.join(backupDir, 'settings.json'), text);
  console.log(`예전 설정을 백업했습니다: ${backupDir}`);
}
if (remove) {
  fs.rmSync(hookFile, { force: true });
} else {
  fs.mkdirSync(path.dirname(hookFile), { recursive: true });
  fs.copyFileSync(source, hookFile);
}
if (settingsChanged) {
  fs.mkdirSync(configDir, { recursive: true });
  fs.writeFileSync(settingsFile, `${JSON.stringify(settings, null, 2)}\n`);
}
console.log(remove
  ? '메시지마다 붙던 슈퍼파워 알림을 뺐습니다. 새로 여는 대화부터 적용됩니다.'
  : '메시지마다 슈퍼파워 알림이 붙도록 설치했습니다. 새로 여는 대화부터 적용됩니다.');
