// node --test .claude/hooks/commit-gate.test.mjs
// Each test builds a throwaway git repo with fake build and ctest evidence, stages a change and asks
// the hook about `git commit`. Only the test-weakening check is covered here.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync, spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const HOOK = path.join(path.dirname(fileURLToPath(import.meta.url)), 'commit-gate.mjs');

const TEST_CPP = [
  '#include <QtTest>',
  'class T : public QObject {',
  '  Q_OBJECT',
  'private slots:',
  '  void adds() {',
  '    QCOMPARE(1 + 1, 2);',
  '    QVERIFY(true);',
  '  }',
  '};',
  '',
].join('\n');
const CMAKE = 'project(x)\nka_add_qtest(adds ka_adds_tests)\nka_add_qtest_filter(save_open_x ka_adds_tests\n  adds\n  more)\n';

function repoWith(t) {
  const repo = fs.mkdtempSync(path.join(os.tmpdir(), 'commit-gate-'));
  t.after(() => fs.rmSync(repo, { recursive: true, force: true }));
  const git = (...args) => execFileSync('git', ['-c', 'user.name=t', '-c', 'user.email=t@t', '-c', 'core.autocrlf=false', ...args], { cwd: repo, stdio: 'pipe' });
  fs.mkdirSync(path.join(repo, 'tests'));
  fs.writeFileSync(path.join(repo, 'CMakeLists.txt'), CMAKE);
  fs.writeFileSync(path.join(repo, 'tests', 'test_adds.cpp'), TEST_CPP);
  git('init', '-q');
  git('add', '-A');
  git('commit', '-q', '-m', 'base');
  return { repo, git };
}

// Build and ctest evidence newer than every change, so only the weakening check can deny.
function evidence(repo) {
  const release = path.join(repo, 'build', 'Release');
  fs.mkdirSync(path.join(repo, 'build', 'Testing', 'Temporary'), { recursive: true });
  fs.mkdirSync(release, { recursive: true });
  fs.writeFileSync(path.join(repo, 'build', 'CMakeCache.txt'), '');
  const now = Date.now() / 1000;
  for (const exe of ['ka-hgis.exe', 'ka_adds_tests.exe']) {
    fs.writeFileSync(path.join(release, exe), '');
    fs.utimesSync(path.join(release, exe), now + 60, now + 60);
  }
  const log = path.join(repo, 'build', 'Testing', 'Temporary', 'LastTest.log');
  fs.writeFileSync(log, '1/1 Test: adds\nTest Passed.\nEnd testing: now\n');
  fs.utimesSync(log, now + 120, now + 120);
}

function change(t, edit) {
  const { repo, git } = repoWith(t);
  edit(repo);
  git('add', '-A');
  evidence(repo);
  return repo;
}

function ask(repo, command) {
  const input = JSON.stringify({ hook_event_name: 'PreToolUse', tool_name: 'Bash', tool_input: { command }, cwd: repo });
  const r = spawnSync(process.execPath, [HOOK], { input, encoding: 'utf8' });
  assert.equal(r.status, 0, r.stderr);
  return r.stdout.trim() ? JSON.parse(r.stdout).hookSpecificOutput.permissionDecisionReason : null;
}

const editTest = (from, to) => (repo) => {
  const f = path.join(repo, 'tests', 'test_adds.cpp');
  fs.writeFileSync(f, fs.readFileSync(f, 'utf8').replace(from, to));
};

test('a commit with build and test evidence and no weakened test passes', (t) => {
  const repo = change(t, editTest('  }\n};', '  }\n  void more() { QVERIFY(1 < 2); }\n};'));
  assert.equal(ask(repo, 'git commit -m "test: 시험 추가"'), null);
});

test('changing an expected value without a reason line is blocked', (t) => {
  const repo = change(t, editTest('QCOMPARE(1 + 1, 2);', 'QCOMPARE(1 + 1, 3);'));
  const reason = ask(repo, 'git commit -m "fix: 고침"');
  assert.match(reason ?? '', /시험 변경:/);
  assert.match(reason, /tests\/test_adds\.cpp/);
});

test('the reason line in the commit message lets it through', (t) => {
  const repo = change(t, editTest('QCOMPARE(1 + 1, 2);', 'QCOMPARE(1 + 1, 3);'));
  assert.equal(ask(repo, 'git commit -m "fix: 고침" -m "시험 변경: 기댓값이 잘못돼 있었다"'), null);
});

test('the reason line inside a heredoc message counts', (t) => {
  const repo = change(t, editTest('QCOMPARE(1 + 1, 2);', 'QCOMPARE(1 + 1, 3);'));
  const command = "git commit -q -F - <<'EOF'\nfix: 고침\n\n시험 변경: 기댓값이 잘못돼 있었다\nEOF";
  assert.equal(ask(repo, command), null);
});

test('skipping a test is blocked', (t) => {
  const repo = change(t, editTest('    QCOMPARE(1 + 1, 2);', '    QSKIP("later");\n    QCOMPARE(1 + 1, 2);'));
  assert.match(ask(repo, 'git commit -m "x"') ?? '', /시험 변경:/);
});

test('deleting a test file is blocked', (t) => {
  const repo = change(t, (r) => fs.rmSync(path.join(r, 'tests', 'test_adds.cpp')));
  assert.match(ask(repo, 'git commit -m "x"') ?? '', /시험 변경:/);
});

test('unregistering a test in CMakeLists.txt is blocked', (t) => {
  const repo = change(t, (r) => fs.writeFileSync(path.join(r, 'CMakeLists.txt'), 'project(x)\n'));
  assert.match(ask(repo, 'git commit -m "x"') ?? '', /시험 변경:/);
});

// Review findings 2026-10-03 (each probe passed through or was falsely blocked before the fix).
test('changing the expected value on the second line of an assertion is blocked', (t) => {
  const repo = change(t, editTest('QCOMPARE(1 + 1, 2);', 'QCOMPARE(1 + 1,\n             2);'));
  assert.equal(ask(repo, 'git commit -m "style: 줄바꿈"'), null);
  const weak = change(t, editTest('QCOMPARE(1 + 1, 2);', 'QCOMPARE(1 + 1,\n             3);'));
  assert.match(ask(weak, 'git commit -m "x"') ?? '', /시험 변경:/);
});

test('a renamed test file is compared with its old content', (t) => {
  const moved = (weaken) => (r) => {
    const text = fs.readFileSync(path.join(r, 'tests', 'test_adds.cpp'), 'utf8');
    fs.rmSync(path.join(r, 'tests', 'test_adds.cpp'));
    fs.writeFileSync(path.join(r, 'tests', 'test_sums.cpp'), weaken ? text.replace('QCOMPARE(1 + 1, 2);', 'QCOMPARE(1 + 1, 3);') : text);
  };
  assert.equal(ask(change(t, moved(false)), 'git commit -m "refactor: 이름"'), null);
  assert.match(ask(change(t, moved(true)), 'git commit -m "x"') ?? '', /시험 변경:/);
});

test('dropping a name from a multi-line test filter list is blocked', (t) => {
  const repo = change(t, (r) => fs.writeFileSync(path.join(r, 'CMakeLists.txt'), CMAKE.replace('  more)', ')')));
  assert.match(ask(repo, 'git commit -m "x"') ?? '', /시험 변경:/);
});

test('a new test with a conditional skip is not weakening', (t) => {
  const repo = change(t, editTest('  }\n};', '  }\n  void optIn() {\n    if (qEnvironmentVariableIsEmpty("RUN"))\n      QSKIP("opt-in");\n    QVERIFY(true);\n  }\n};'));
  assert.equal(ask(repo, 'git commit -m "test: 선택 시험"'), null);
});

test('only a real reason line in the message unlocks', (t) => {
  const weak = () => change(t, editTest('QCOMPARE(1 + 1, 2);', 'QCOMPARE(1 + 1, 3);'));
  assert.match(ask(weak(), 'git commit -m "x" -m "시험 변경: 없음"') ?? '', /시험 변경:/);
  assert.match(ask(weak(), 'git log --grep "시험 변경:" && git commit -m "x"') ?? '', /시험 변경:/);
});

test('amending without a new message keeps the earlier reason line', (t) => {
  const repo = change(t, editTest('QCOMPARE(1 + 1, 2);', 'QCOMPARE(1 + 1, 3);'));
  execFileSync('git', ['-c', 'user.name=t', '-c', 'user.email=t@t', 'commit', '-q', '-m', 'fix', '-m', '시험 변경: 기댓값이 틀렸다'], { cwd: repo });
  fs.appendFileSync(path.join(repo, 'tests', 'test_adds.cpp'), '// note\n');
  execFileSync('git', ['add', '-A'], { cwd: repo });
  evidence(repo);
  assert.equal(ask(repo, 'git commit --amend --no-edit'), null);
});

test('a diff prefix setting in git config does not open the lock', (t) => {
  const repo = change(t, editTest('QCOMPARE(1 + 1, 2);', 'QCOMPARE(1 + 1, 3);'));
  execFileSync('git', ['config', 'diff.noprefix', 'true'], { cwd: repo });
  assert.match(ask(repo, 'git commit -m "x"') ?? '', /시험 변경:/);
});

test('moving or re-indenting an assertion unchanged is not weakening', (t) => {
  const repo = change(t, editTest('    QCOMPARE(1 + 1, 2);\n    QVERIFY(true);', '    QVERIFY(true);\n      QCOMPARE(1 + 1, 2);'));
  assert.equal(ask(repo, 'git commit -m "style: 순서"'), null);
});
