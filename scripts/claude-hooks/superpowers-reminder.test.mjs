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
// 2026-10-03 (docs/intent/2026-10-03-dev-setup-cleanup.md): the first stage is named by the Jev stage
// hint and CLAUDE.md rule 1, the last by the finish check, so the reminder keeps only the middle.
// The debugging route stays: Jev shows no hint below 0.6 (2 of 7 debugging picks on 2026-10-03).
const ROUTES = [
  'superpowers:systematic-debugging',
  'superpowers:test-driven-development',
  'superpowers:requesting-code-review',
  'superpowers:executing-plans',
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
// 2026-10-03 「가볍게 쓴다 문구수정하라 해당시 적극사용한다로」: a long conversation called no skill in 8 of the
// 10 requests after its context was compacted, so the reminder says to call each skill again at its stage.
test('skills are used actively at their stage, called again after a compaction', () => {
  const text = reminderOf(runHook(JSON.stringify({ cwd: 'A:\\qgis' })));
  assert.ok(!text.includes('가볍게'), text);
  assert.ok(text.includes('적극 사용') && text.includes('Skill 도구'), text);
  assert.ok(text.includes('대화 요약') && text.includes('다시 부른다'), text);
  assert.ok(text.includes('첫 동작 전에'), text);
});

test('the reminder stays short and points at CLAUDE.md for the full rules', () => {
  const text = reminderOf(runHook(JSON.stringify({ cwd: 'A:\\qgis' })));
  assert.ok(text.length <= 450, `${text.length} characters`);
  assert.ok(text.includes('CLAUDE.md'), text);
});

test('the routing keeps the 2026-10-02 no-loop rule', () => {
  const text = reminderOf(runHook(JSON.stringify({ cwd: 'A:\\qgis' })));
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
  assert.ok(text.includes('Skill 도구'));
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

const finishHooks = (settings) =>
  (settings.hooks?.Stop ?? []).flatMap((e) => e.hooks).filter((h) => h.command.includes('finish-check.mjs'));
const routeHooks = (settings) =>
  (settings.hooks?.UserPromptSubmit ?? []).flatMap((e) => e.hooks).filter((h) => h.command.includes('jev-skill-route.mjs'));

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
  assert.deepEqual(settings.hooks.Stop.slice(0, 1), SNAPSHOT_STOP);
  assert.equal(finishHooks(settings).length, 1);
  assert.ok(existsSync(join(dir, 'hooks', 'finish-check.mjs')));
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
  assert.equal(prompt.length, 4);
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
  assert.ok(!existsSync(join(dir, 'hooks', 'finish-check.mjs')));
});

// 2026-10-03 docs/intent/2026-10-03-finish-check.md: the same installer puts the closing check on Stop.
test('the installed finish check stops a code turn that skipped the before-done check', (t) => {
  const dir = freshConfig(t, JSON.stringify(ORIGINAL));
  assert.equal(install(dir).status, 0);
  const command = finishHooks(settingsOf(dir))[0].command;
  const cwd = 'A:\\qgis\\.claude\\worktrees\\x';
  const transcript = join(dir, 'transcript.jsonl');
  writeFileSync(transcript, [
    { type: 'user', message: { role: 'user', content: '고쳐 줘' } },
    { type: 'assistant', message: { content: [{ type: 'tool_use', name: 'Edit', input: { file_path: `${cwd}\\src\\core\\A.cpp` } }] } },
  ].map((l) => JSON.stringify(l)).join('\n'));
  const out = execSync(command, { input: JSON.stringify({ cwd, transcript_path: transcript, stop_hook_active: false }), encoding: 'utf8' });
  assert.equal(JSON.parse(out).decision, 'block');
});

// 2026-10-03 docs/intent/2026-10-03-jev-stage-hint.md: the Jev stage hint runs in every project.
test('the Jev stage hint is installed for every project with its helper, and removed with it', (t) => {
  const dir = freshConfig(t, JSON.stringify(ORIGINAL));
  assert.equal(install(dir).status, 0);
  const hooks = routeHooks(settingsOf(dir));
  assert.equal(hooks.length, 1);
  assert.doesNotMatch(hooks[0].command, /--root/);
  assert.ok(hooks[0].timeout <= 3, 'it runs on every message in every project');
  assert.ok(existsSync(join(dir, 'hooks', 'jev-skill-route.mjs')));
  assert.ok(existsSync(join(dir, 'hooks', 'jev-ask.mjs')));
  const out = execSync(hooks[0].command, { input: 'not json', encoding: 'utf8' });
  assert.equal(out.trim(), '');
  assert.equal(install(dir, '--remove').status, 0);
  assert.equal(routeHooks(settingsOf(dir)).length, 0);
  assert.ok(!existsSync(join(dir, 'hooks', 'jev-skill-route.mjs')));
  assert.ok(!existsSync(join(dir, 'hooks', 'jev-ask.mjs')));
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
