// node --test scripts/claude-hooks/superpowers-reminder.test.mjs
// The per-message superpowers reminder: the hook speaks only inside the Strata repo, and the
// installer edits only its own entry in the Claude settings file.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync, readFileSync, existsSync, readdirSync, rmSync } from 'node:fs';
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
  'capture-intent', // 2026-10-02: new features start with the global request record (intent)
];

const tempDir = (t, prefix) => {
  const dir = mkdtempSync(join(tmpdir(), prefix));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
};

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

// 2026-10-02 「빠르게 진행」: no per-task worker/reviewer loop; plans run in this session and get
// one review when the feature is done.
test('the routing is the light 2026-10-02 workflow', () => {
  const text = reminderOf(runHook(JSON.stringify({ cwd: 'A:\qgis' })));
  assert.ok(text.includes('2026-10-02'), text);
  assert.ok(text.includes('superpowers:executing-plans'), text);
  assert.ok(!text.includes('superpowers:subagent-driven-development로 이어진다'), text);
  assert.ok(text.includes('기능 하나가 끝났을 때') && text.includes('한 번'), text);
});

test('outside the Strata repo nothing is added', () => {
  for (const cwd of ['C:\\Users\\someone\\other', 'A:\\qgis2\\src', 'A:\\']) {
    assert.equal(reminderOf(runHook(JSON.stringify({ cwd }))), '', cwd);
  }
});

test('a broken hook input never blocks the message', (t) => {
  const outside = tempDir(t, 'sp-outside-');
  assert.equal(reminderOf(runHook('not json', { cwd: outside })), '');
  // Without a readable cwd in the input, the process folder decides.
  const text = reminderOf(runHook('', { root: outside, cwd: outside }));
  assert.ok(text.includes('superpowers:systematic-debugging'));
});

const freshConfig = (t, settingsText, prefix = 'sp-config-') => {
  const dir = tempDir(t, prefix);
  if (settingsText !== undefined) writeFileSync(join(dir, 'settings.json'), settingsText);
  return dir;
};
const install = (dir, ...extra) =>
  spawnSync(process.execPath, [INSTALL, '--config-dir', dir, '--root', 'A:/qgis', ...extra], { encoding: 'utf8' });
const settingsOf = (dir) => JSON.parse(readFileSync(join(dir, 'settings.json'), 'utf8'));
const ourHooks = (settings) =>
  (settings.hooks?.UserPromptSubmit ?? []).flatMap((e) => e.hooks).filter((h) => h.command.includes('superpowers-reminder.mjs'));

const SNAPSHOT_STOP = [{ hooks: [{ type: 'command', command: 'node "C:/x/.claude/hooks/worktree-snapshot.mjs"' }] }];
const ORIGINAL = {
  model: 'opus',
  enabledPlugins: { 'superpowers@superpowers-marketplace': true },
  hooks: { Stop: SNAPSHOT_STOP },
};
const FOREIGN_PROMPT_HOOKS = [
  { matcher: '', hooks: [{ type: 'command', command: 'node "C:/x/other-tool.mjs"', timeout: 5 }] },
  { hooks: [{ type: 'command', command: 'echo second-tool' }] },
];

test('install adds one bounded reminder, keeps every other setting and backs up once', (t) => {
  const dir = freshConfig(t, JSON.stringify(ORIGINAL, null, 2));
  for (let i = 0; i < 2; ++i) assert.equal(install(dir).status, 0);
  const settings = settingsOf(dir);
  const ours = ourHooks(settings);
  assert.equal(ours.length, 1);
  assert.ok(ours[0].timeout > 0 && ours[0].timeout <= 30, 'a stuck hook must not hold a message for long');
  assert.deepEqual(settings.hooks.Stop, SNAPSHOT_STOP);
  assert.equal(settings.model, 'opus');
  assert.deepEqual(settings.enabledPlugins, ORIGINAL.enabledPlugins);
  assert.ok(existsSync(join(dir, 'hooks', 'superpowers-reminder.mjs')));
  const backups = readdirSync(join(dir, '_reset_backup')).filter((d) => d.endsWith('-superpowers-reminder'));
  assert.equal(backups.length, 1, 'the second, unchanged install must not add a backup');
  assert.deepEqual(JSON.parse(readFileSync(join(dir, '_reset_backup', backups[0], 'settings.json'), 'utf8')), ORIGINAL);
});

test('other tools that also listen to every message keep their entries', (t) => {
  const withOthers = { ...ORIGINAL, hooks: { ...ORIGINAL.hooks, UserPromptSubmit: FOREIGN_PROMPT_HOOKS } };
  const dir = freshConfig(t, JSON.stringify(withOthers));
  assert.equal(install(dir).status, 0);
  let prompt = settingsOf(dir).hooks.UserPromptSubmit;
  assert.deepEqual(prompt.slice(0, 2), FOREIGN_PROMPT_HOOKS);
  assert.equal(prompt.length, 3);
  assert.equal(install(dir, '--remove').status, 0);
  prompt = settingsOf(dir).hooks.UserPromptSubmit;
  assert.deepEqual(prompt, FOREIGN_PROMPT_HOOKS);
});

test('a hook sharing an entry with the reminder is kept', (t) => {
  const shared = { hooks: [{ type: 'command', command: 'echo neighbour' },
                           { type: 'command', command: 'node "C:/old/hooks/superpowers-reminder.mjs" --root "A:/qgis"' }] };
  const dir = freshConfig(t, JSON.stringify({ hooks: { UserPromptSubmit: [shared] } }));
  assert.equal(install(dir).status, 0);
  const commands = () => settingsOf(dir).hooks.UserPromptSubmit.flatMap((e) => e.hooks).map((h) => h.command);
  assert.ok(commands().includes('echo neighbour'));
  assert.equal(ourHooks(settingsOf(dir)).length, 1);
  assert.equal(install(dir, '--remove').status, 0);
  assert.deepEqual(commands(), ['echo neighbour']);
});

test('the installed command runs on its own and speaks inside the repo', (t) => {
  const dir = freshConfig(t, JSON.stringify(ORIGINAL), 'sp config 한글 ');
  assert.equal(install(dir).status, 0);
  const command = ourHooks(settingsOf(dir))[0].command;
  const out = execSync(command, { input: JSON.stringify({ cwd: 'A:\\qgis\\.claude\\worktrees\\x' }), encoding: 'utf8' });
  const text = JSON.parse(out).hookSpecificOutput.additionalContext;
  for (const route of ROUTES) assert.ok(text.includes(route), route);
});

test('remove takes out only the reminder', (t) => {
  const dir = freshConfig(t, JSON.stringify(ORIGINAL));
  assert.equal(install(dir).status, 0);
  assert.equal(install(dir, '--remove').status, 0);
  const settings = settingsOf(dir);
  assert.equal(ourHooks(settings).length, 0);
  assert.deepEqual(settings.hooks.Stop, SNAPSHOT_STOP);
  assert.ok(!existsSync(join(dir, 'hooks', 'superpowers-reminder.mjs')));
});

test('remove with no settings file creates nothing', (t) => {
  const dir = freshConfig(t, undefined);
  assert.equal(install(dir, '--remove').status, 0);
  assert.ok(!existsSync(join(dir, 'settings.json')));
});

test('install starts a settings file when there is none, is empty, or begins with a BOM', (t) => {
  for (const text of [undefined, '', '  \n', '\uFEFF' + JSON.stringify(ORIGINAL)]) {
    const dir = freshConfig(t, text);
    const result = install(dir);
    assert.equal(result.status, 0, `${JSON.stringify(text?.slice(0, 3))}: ${result.stderr}`);
    assert.equal(ourHooks(settingsOf(dir)).length, 1);
  }
});

test('a broken settings file is left exactly as it was', (t) => {
  const dir = freshConfig(t, '{ "model": "opus", oops');
  const result = install(dir);
  assert.notEqual(result.status, 0);
  assert.equal(readFileSync(join(dir, 'settings.json'), 'utf8'), '{ "model": "opus", oops');
  assert.ok(!existsSync(join(dir, 'hooks', 'superpowers-reminder.mjs')));
});

test('an empty CLAUDE_CONFIG_DIR means the home folder, not the current folder', (t) => {
  const home = tempDir(t, 'sp-home-');
  const elsewhere = tempDir(t, 'sp-cwd-');
  const result = spawnSync(process.execPath, [INSTALL, '--root', 'A:/qgis'], {
    encoding: 'utf8', cwd: elsewhere,
    env: { ...process.env, CLAUDE_CONFIG_DIR: '', USERPROFILE: home, HOME: home },
  });
  assert.equal(result.status, 0, result.stderr);
  assert.ok(existsSync(join(home, '.claude', 'settings.json')));
  assert.ok(!existsSync(join(elsewhere, 'settings.json')));
});

test('a relative root is stored as an absolute folder', (t) => {
  const dir = freshConfig(t, undefined);
  const repo = tempDir(t, 'sp-repo-');
  const result = spawnSync(process.execPath, [INSTALL, '--config-dir', dir, '--root', '.'], { encoding: 'utf8', cwd: repo });
  assert.equal(result.status, 0, result.stderr);
  const command = ourHooks(settingsOf(dir))[0].command;
  assert.ok(command.includes(`--root "${repo.replace(/\\/g, '/')}"`), command);
});
