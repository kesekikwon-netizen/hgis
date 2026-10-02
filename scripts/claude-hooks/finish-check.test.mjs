// node --test scripts/claude-hooks/finish-check.test.mjs
// The Stop hook reads the turn from a transcript; every test writes its own transcript to a temp folder.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HOOK = path.join(path.dirname(fileURLToPath(import.meta.url)), 'finish-check.mjs');

const user = (text) => ({ type: 'user', message: { role: 'user', content: text } });
const toolResult = () => ({ type: 'user', message: { role: 'user', content: [{ type: 'tool_result', tool_use_id: 't', content: 'ok' }] } });
const tool = (name, input) => ({ type: 'assistant', message: { role: 'assistant', content: [{ type: 'tool_use', id: 't', name, input }] } });
const edit = (repo, rel) => tool('Edit', { file_path: path.join(repo, rel) });
const skill = (name) => tool('Skill', { skill: name });
const VERIFY = 'superpowers:verification-before-completion';

function run(t, lines, { stopHookActive = false, inside = true, raw, cwdSub } = {}) {
  const repo = fs.mkdtempSync(path.join(os.tmpdir(), 'finish-check-'));
  t.after(() => fs.rmSync(repo, { recursive: true, force: true }));
  const transcript = path.join(repo, 'transcript.jsonl');
  fs.writeFileSync(transcript, (typeof lines === 'function' ? lines(repo) : lines).map((l) => JSON.stringify(l)).join('\n'));
  const cwd = inside ? (cwdSub ? path.join(repo, cwdSub) : repo) : os.tmpdir();
  const input = raw ?? JSON.stringify({ cwd, transcript_path: transcript,
    hook_event_name: 'Stop', stop_hook_active: stopHookActive });
  const result = spawnSync(process.execPath, [HOOK, '--root', repo], { input, encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr);
  return result.stdout.trim() ? JSON.parse(result.stdout) : null;
}

test('a code edit without the before-done check is stopped once', (t) => {
  const out = run(t, (repo) => [user('고쳐 줘'), edit(repo, 'src/core/LayerOps.cpp'), toolResult()]);
  assert.equal(out?.decision, 'block');
  assert.match(out.reason, /끝나기 전 확인/);
  assert.match(out.reason, /verification-before-completion/);
});

test('a code edit with the before-done check passes', (t) => {
  assert.equal(run(t, (repo) => [user('고쳐 줘'), edit(repo, 'tests/test_x.cpp'), toolResult(), skill(VERIFY)]), null);
});

test('a screen-code edit also needs the screen check', (t) => {
  const out = run(t, (repo) => [user('고쳐 줘'), edit(repo, 'src/app/KaRibbon.cpp'), toolResult(), skill(VERIFY)]);
  assert.equal(out?.decision, 'block');
  assert.match(out.reason, /화면 확인/);
  assert.match(out.reason, /run-strata/);
  assert.doesNotMatch(out.reason, /verification-before-completion/);
});

test('a screen-code edit with both checks passes', (t) => {
  assert.equal(run(t, (repo) => [user('고쳐 줘'), edit(repo, 'src/app/KaRibbon.cpp'), skill('run-strata'), skill(VERIFY)]), null);
});

test('a docs-only turn and a question-only turn are not stopped', (t) => {
  assert.equal(run(t, (repo) => [user('문서 고쳐 줘'), edit(repo, 'docs/intent/x.md'), toolResult()]), null);
  assert.equal(run(t, [user('이게 뭐야?')]), null);
});

test('edits from an earlier request do not count', (t) => {
  assert.equal(run(t, (repo) => [user('고쳐 줘'), edit(repo, 'src/core/A.cpp'), user('고마워')]), null);
});

test('a screenshot sent without text starts a new request', (t) => {
  const picture = { type: 'user', message: { role: 'user', content: [{ type: 'image', source: { type: 'base64', data: '' } }] } };
  assert.equal(run(t, (repo) => [user('고쳐 줘'), edit(repo, 'src/core/A.cpp'), picture]), null);
});

test('a background-task notice is not a new request', (t) => {
  const notice = user('<task-notification>\n<task-id>b1</task-id>\n<status>completed</status>\n</task-notification>');
  assert.equal(run(t, (repo) => [user('고쳐 줘'), edit(repo, 'src/core/A.cpp'), notice])?.decision, 'block');
});

test('a code edit is seen with a different drive-letter case or after the folder changed', (t) => {
  const upper = (repo) => tool('Edit', { file_path: path.join(repo, 'SRC', 'core', 'A.cpp').toUpperCase() });
  assert.equal(run(t, (repo) => [user('고쳐 줘'), upper(repo)])?.decision, 'block');
  const moved = (repo) => tool('Edit', { file_path: path.join(repo, 'src', 'core', 'A.cpp') });
  assert.equal(run(t, (repo) => [user('고쳐 줘'), moved(repo)], { cwdSub: 'build' })?.decision, 'block');
});

test('a code edit in a worktree of the Strata repo is seen', (t) => {
  const wt = (repo) => edit(repo, path.join('.claude', 'worktrees', 'w1', 'tests', 'test_x.cpp'));
  assert.equal(run(t, (repo) => [user('고쳐 줘'), wt(repo)])?.decision, 'block');
});

test('the before-done check called without the plugin prefix counts', (t) => {
  assert.equal(run(t, (repo) => [user('고쳐 줘'), edit(repo, 'src/core/A.cpp'), skill('verification-before-completion')]), null);
});

test('a second stop in a row always passes', (t) => {
  assert.equal(run(t, (repo) => [user('고쳐 줘'), edit(repo, 'src/core/A.cpp')], { stopHookActive: true }), null);
});

test('outside the Strata repo nothing happens', (t) => {
  assert.equal(run(t, (repo) => [user('고쳐 줘'), edit(repo, 'src/core/A.cpp')], { inside: false }), null);
});

test('broken input never blocks', (t) => {
  assert.equal(run(t, [], { raw: 'not json' }), null);
});
