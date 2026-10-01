// node --test scripts/claude-hooks/superpowers-reminder.test.mjs
// The per-message superpowers reminder: the hook speaks only inside the Strata repo, and the
// installer edits only its own entry in the Claude settings file.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync, readFileSync, existsSync, readdirSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve, dirname } from 'node:path';
import { spawnSync, execSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const HOOK = resolve(here, 'superpowers-reminder.mjs');
const INSTALL = resolve(here, 'install-superpowers-reminder.mjs');
const ROUTES = [
  'superpowers:brainstorming',
  'superpowers:systematic-debugging',
  'superpowers:test-driven-development',
  'superpowers:requesting-code-review',
  'superpowers:verification-before-completion',
];

const runHook = (stdin, { root = 'A:/qgis', cwd } = {}) =>
  spawnSync(process.execPath, [HOOK, '--root', root], { input: stdin, encoding: 'utf8', cwd });

const reminderOf = (result) => {
  assert.equal(result.status, 0, result.stderr);
  if (!result.stdout.trim()) return '';
  const out = JSON.parse(result.stdout);
  assert.equal(out.hookSpecificOutput.hookEventName, 'UserPromptSubmit');
  return out.hookSpecificOutput.additionalContext;
};

test('inside the Strata repo every message gets the skill routing', () => {
  for (const cwd of ['A:\\qgis', 'A:\\qgis\\.claude\\worktrees\\happy-davinci-a9ee1e', 'a:/QGIS/src']) {
    const text = reminderOf(runHook(JSON.stringify({ cwd, prompt: '안 돼요' })));
    for (const route of ROUTES) assert.ok(text.includes(route), `${cwd}: missing ${route}`);
  }
});

test('outside the Strata repo nothing is added', () => {
  for (const cwd of ['C:\\Users\\someone\\other', 'A:\\qgis2\\src', 'A:\\']) {
    assert.equal(reminderOf(runHook(JSON.stringify({ cwd }))), '', cwd);
  }
});

test('a broken hook input never blocks the message', () => {
  const outside = mkdtempSync(join(tmpdir(), 'sp-outside-'));
  assert.equal(reminderOf(runHook('not json', { cwd: outside })), '');
  // Without a readable cwd in the input, the process folder decides.
  const text = reminderOf(runHook('', { root: outside, cwd: outside }));
  assert.ok(text.includes('superpowers:systematic-debugging'));
});

const freshConfig = (settingsText) => {
  const dir = mkdtempSync(join(tmpdir(), 'sp-config-'));
  if (settingsText !== undefined) writeFileSync(join(dir, 'settings.json'), settingsText);
  return dir;
};
const install = (dir, ...extra) =>
  spawnSync(process.execPath, [INSTALL, '--config-dir', dir, '--root', 'A:/qgis', ...extra], { encoding: 'utf8' });
const settingsOf = (dir) => JSON.parse(readFileSync(join(dir, 'settings.json'), 'utf8'));
const reminderEntries = (settings) =>
  (settings.hooks?.UserPromptSubmit ?? []).filter((e) => e.hooks.some((h) => h.command.includes('superpowers-reminder.mjs')));

const ORIGINAL = {
  model: 'opus',
  enabledPlugins: { 'superpowers@superpowers-marketplace': true },
  hooks: { Stop: [{ hooks: [{ type: 'command', command: 'node "C:/x/.claude/hooks/worktree-snapshot.mjs"' }] }] },
};

test('install adds one reminder, keeps every other setting and backs up the old file', () => {
  const dir = freshConfig(JSON.stringify(ORIGINAL, null, 2));
  for (let i = 0; i < 2; ++i) assert.equal(install(dir).status, 0);
  const settings = settingsOf(dir);
  assert.equal(reminderEntries(settings).length, 1);
  assert.deepEqual(settings.hooks.Stop, ORIGINAL.hooks.Stop);
  assert.equal(settings.model, 'opus');
  assert.deepEqual(settings.enabledPlugins, ORIGINAL.enabledPlugins);
  assert.ok(existsSync(join(dir, 'hooks', 'superpowers-reminder.mjs')));
  const backups = readdirSync(join(dir, '_reset_backup')).filter((d) => d.endsWith('-superpowers-reminder'));
  assert.ok(backups.length >= 1);
  assert.deepEqual(JSON.parse(readFileSync(join(dir, '_reset_backup', backups[0], 'settings.json'), 'utf8')), ORIGINAL);
});

test('the installed command runs on its own and speaks inside the repo', () => {
  const dir = freshConfig(JSON.stringify(ORIGINAL));
  assert.equal(install(dir).status, 0);
  const command = reminderEntries(settingsOf(dir))[0].hooks[0].command;
  const out = execSync(command, { input: JSON.stringify({ cwd: 'A:\\qgis\\.claude\\worktrees\\x' }), encoding: 'utf8' });
  const text = JSON.parse(out).hookSpecificOutput.additionalContext;
  for (const route of ROUTES) assert.ok(text.includes(route), route);
});

test('remove takes out only the reminder', () => {
  const dir = freshConfig(JSON.stringify(ORIGINAL));
  assert.equal(install(dir).status, 0);
  assert.equal(install(dir, '--remove').status, 0);
  const settings = settingsOf(dir);
  assert.equal(reminderEntries(settings).length, 0);
  assert.deepEqual(settings.hooks.Stop, ORIGINAL.hooks.Stop);
  assert.ok(!existsSync(join(dir, 'hooks', 'superpowers-reminder.mjs')));
});

test('install starts a settings file when there is none', () => {
  const dir = freshConfig(undefined);
  assert.equal(install(dir).status, 0);
  assert.equal(reminderEntries(settingsOf(dir)).length, 1);
});

test('a broken settings file is left exactly as it was', () => {
  const dir = freshConfig('{ "model": "opus", oops');
  const result = install(dir);
  assert.notEqual(result.status, 0);
  assert.equal(readFileSync(join(dir, 'settings.json'), 'utf8'), '{ "model": "opus", oops');
  assert.ok(!existsSync(join(dir, 'hooks', 'superpowers-reminder.mjs')));
});
