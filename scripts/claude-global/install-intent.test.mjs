// node --test scripts/claude-global/install-intent.test.mjs
// Every test writes only to its own temporary folder; the real ~/.claude is never touched.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import {
  applyBlock, removeBlock, START, END, validateSource, installIntent, removeIntent, replaceFile,
} from './install-intent.mjs';

const BOM = String.fromCharCode(0xfeff);
const ORIGINAL = '# 전역 지침\n\n- 하나\n- 둘\n';
const BODY = '- 새 줄';
const HERE = path.dirname(fileURLToPath(import.meta.url));

const read = (dir, rel) => fs.readFileSync(path.join(dir, rel), 'utf8');

// A fresh temporary folder holding `files` ({ relative path: text }), removed after the test.
function tempConfig(t, files) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'intent-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  for (const [rel, text] of Object.entries(files)) {
    const file = path.join(dir, rel);
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(file, text);
  }
  return dir;
}

function copySourceToTemp(t) {
  const dir = tempConfig(t, {});
  fs.cpSync(path.join(HERE, 'skills'), path.join(dir, 'skills'), { recursive: true });
  fs.copyFileSync(path.join(HERE, 'intent-rule.md'), path.join(dir, 'intent-rule.md'));
  return dir;
}

function replaceIn(dir, rel, from, to) {
  const text = read(dir, rel);
  assert.ok(text.includes(from), `${rel} has no '${from}'`);
  fs.writeFileSync(path.join(dir, rel), text.replace(from, to));
}

test('applyBlock adds the block once at the end', () => {
  const once = applyBlock(ORIGINAL, BODY);
  assert.equal(once, `${ORIGINAL}\n${START}\n${BODY}\n${END}\n`);
  assert.equal(applyBlock(once, BODY), once);
});

test('applyBlock replaces an older block in place', () => {
  const old = applyBlock(ORIGINAL, '- 옛 줄') + '- 뒤에 쓴 줄\n';
  const updated = applyBlock(old, BODY);
  assert.ok(updated.includes(BODY) && !updated.includes('옛 줄'));
  assert.ok(updated.startsWith(ORIGINAL) && updated.endsWith(`${END}\n- 뒤에 쓴 줄\n`));
});

test('removeBlock restores the original text', () => {
  assert.equal(removeBlock(applyBlock(ORIGINAL, BODY)), ORIGINAL);
  assert.equal(removeBlock(ORIGINAL), ORIGINAL);
});

test('applyBlock keeps CRLF and BOM', () => {
  const crlf = `${BOM}# 전역 지침\r\n- 하나\r\n`;
  const out = applyBlock(crlf, BODY);
  assert.ok(out.startsWith(crlf));
  assert.equal(out.replace(/\r\n/g, '').includes('\n'), false);
  assert.equal(removeBlock(out), crlf);
});

test('applyBlock on empty text writes only the block', () => {
  assert.equal(applyBlock('', BODY), `${START}\n${BODY}\n${END}\n`);
  assert.equal(removeBlock(applyBlock('', BODY)), '');
});

test('broken markers are refused', () => {
  assert.throws(() => applyBlock(`${ORIGINAL}${START}\n- 반쪽\n`, BODY), /표시가 깨졌습니다/);
  assert.throws(() => removeBlock(`${END}\n${START}\n`), /표시가 깨졌습니다/);
  assert.throws(() => applyBlock(`${START}\n${END}\n${START}\n${END}\n`, BODY), /표시가 깨졌습니다/);
});

test('the shipped skill files pass validation', () => {
  const { description, rule } = validateSource(HERE);
  assert.ok(description.includes('docs/intent') && description.length <= 1024);
  assert.ok(rule.startsWith('- ') && rule.includes('`capture-intent`'));
});

test('validateSource rejects a skill without its name', (t) => {
  const dir = copySourceToTemp(t);
  replaceIn(dir, 'skills/capture-intent/SKILL.md', 'name: capture-intent', 'name: other');
  assert.throws(() => validateSource(dir), /SKILL\.md/);
});

// The template lives inside SKILL.md: a separate file under ~/.claude is outside the project, and
// reading it was refused in the behaviour trials, so the record came out without the template.
test('validateSource rejects a skill whose template misses a section', (t) => {
  const dir = copySourceToTemp(t);
  replaceIn(dir, 'skills/capture-intent/SKILL.md', '## 확인 방법', '## 기타');
  assert.throws(() => validateSource(dir), /SKILL\.md/);
});

test('installIntent copies the skill and keeps the old CLAUDE.md in the backup', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL, 'skills/capture-intent/template.md': 'old' });
  const { backupDir } = installIntent({ configDir: cfg, sourceDir: HERE, stamp: '2026-10-02-0900' });
  assert.equal(read(cfg, 'skills/capture-intent/SKILL.md'), read(HERE, 'skills/capture-intent/SKILL.md'));
  assert.equal(fs.existsSync(path.join(cfg, 'skills/capture-intent/template.md')), false);
  const rule = read(HERE, 'intent-rule.md').trim();
  assert.equal(read(cfg, 'CLAUDE.md'), `${ORIGINAL}\n${START}\n${rule}\n${END}\n`);
  assert.equal(read(backupDir, 'CLAUDE.md'), ORIGINAL);
  assert.equal(backupDir, path.join(cfg, '_reset_backup', '2026-10-02-0900-intent'));
});

test('installIntent twice changes nothing the second time', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL });
  installIntent({ configDir: cfg, sourceDir: HERE, stamp: 's' });
  const first = read(cfg, 'CLAUDE.md');
  const { backupDir } = installIntent({ configDir: cfg, sourceDir: HERE, stamp: 's' });
  assert.equal(read(cfg, 'CLAUDE.md'), first);
  assert.ok(backupDir.endsWith('s-intent-2'));
});

test('installIntent works without a CLAUDE.md and removeIntent cleans up', (t) => {
  const cfg = tempConfig(t, {});
  installIntent({ configDir: cfg, sourceDir: HERE, stamp: 'a' });
  removeIntent({ configDir: cfg, stamp: 'b' });
  assert.equal(fs.existsSync(path.join(cfg, 'CLAUDE.md')), false);
  assert.equal(fs.existsSync(path.join(cfg, 'skills/capture-intent')), false);
});

test('removeIntent keeps other skills and other lines', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL, 'skills/other/SKILL.md': 'x' });
  installIntent({ configDir: cfg, sourceDir: HERE, stamp: 'a' });
  removeIntent({ configDir: cfg, stamp: 'b' });
  assert.equal(read(cfg, 'CLAUDE.md'), ORIGINAL);
  assert.equal(read(cfg, 'skills/other/SKILL.md'), 'x');
});

test('a broken CLAUDE.md marker leaves everything untouched', (t) => {
  const broken = `${ORIGINAL}${START}\n- 반쪽\n`;
  const cfg = tempConfig(t, { 'CLAUDE.md': broken });
  assert.throws(() => installIntent({ configDir: cfg, sourceDir: HERE, stamp: 'a' }), /표시가 깨졌습니다/);
  assert.equal(read(cfg, 'CLAUDE.md'), broken);
  assert.equal(fs.existsSync(path.join(cfg, 'skills/capture-intent')), false);
  assert.equal(fs.existsSync(path.join(cfg, '_reset_backup')), false);
});

// Seen on the real install: Windows refused the rename for a moment (EPERM) while another program
// had CLAUDE.md open; the same rename worked a minute later.
test('replaceFile retries while the file is busy', (t) => {
  const dir = tempConfig(t, { 'CLAUDE.md': ORIGINAL });
  const file = path.join(dir, 'CLAUDE.md');
  let calls = 0;
  const rename = (from, to) => {
    calls += 1;
    if (calls <= 2) throw Object.assign(new Error('busy'), { code: 'EPERM' });
    fs.renameSync(from, to);
  };
  replaceFile(file, 'new', { rename, wait: () => {} });
  assert.equal(calls, 3);
  assert.equal(read(dir, 'CLAUDE.md'), 'new');
  assert.equal(fs.existsSync(`${file}.intent-tmp`), false);
});

test('replaceFile gives up cleanly when the file stays busy', (t) => {
  const dir = tempConfig(t, { 'CLAUDE.md': ORIGINAL });
  const file = path.join(dir, 'CLAUDE.md');
  const rename = () => { throw Object.assign(new Error('busy'), { code: 'EPERM' }); };
  assert.throws(() => replaceFile(file, 'new', { rename, wait: () => {}, tries: 3 }), /다른 프로그램이 쓰고 있어/);
  assert.equal(read(dir, 'CLAUDE.md'), ORIGINAL);
  assert.equal(fs.existsSync(`${file}.intent-tmp`), false);
});

test('installIntent leaves the old skill in place when CLAUDE.md cannot be written', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL, 'skills/capture-intent/SKILL.md': 'old skill' });
  fs.mkdirSync(path.join(cfg, 'CLAUDE.md.intent-tmp')); // the temporary file cannot be created
  assert.throws(() => installIntent({ configDir: cfg, sourceDir: HERE, stamp: 'a' }));
  assert.equal(read(cfg, 'skills/capture-intent/SKILL.md'), 'old skill');
  assert.equal(read(cfg, 'CLAUDE.md'), ORIGINAL);
});

// Review finding: a cp949 (ANSI) or UTF-16 CLAUDE.md was read as UTF-8 and written back as garbage.
test('installIntent and removeIntent refuse a CLAUDE.md that is not UTF-8', (t) => {
  const cp949 = Buffer.from([0x23, 0x20, 0xc0, 0xfc, 0xbf, 0xaa, 0x0a, 0x2d, 0x20, 0xc7, 0xcf, 0xb3, 0xaa, 0x0a]);
  const utf16 = Buffer.from(`${BOM}# 전역 지침\r\n- 하나\r\n`, 'utf16le');
  for (const bytes of [cp949, utf16]) {
    const cfg = tempConfig(t, {});
    fs.writeFileSync(path.join(cfg, 'CLAUDE.md'), bytes);
    assert.throws(() => installIntent({ configDir: cfg, sourceDir: HERE, stamp: 'a' }), /UTF-8/);
    assert.throws(() => removeIntent({ configDir: cfg, stamp: 'b' }), /UTF-8/);
    assert.deepEqual(fs.readFileSync(path.join(cfg, 'CLAUDE.md')), bytes);
    assert.equal(fs.existsSync(path.join(cfg, 'skills')), false);
    assert.equal(fs.existsSync(path.join(cfg, '_reset_backup')), false);
  }
});

// Review finding: `remove` (no dashes) or `—remove` (autocorrected dash) installed instead.
test('command line refuses an unknown option and changes nothing', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL });
  for (const arg of ['remove', '—remove', '--Remove']) {
    const run = spawnSync(process.execPath, [path.join(HERE, 'install-intent.mjs'), arg],
      { env: { ...process.env, CLAUDE_CONFIG_DIR: cfg }, encoding: 'utf8' });
    assert.equal(run.status, 1, `${arg}: ${run.stdout}`);
    assert.match(run.stderr, /알 수 없는 옵션/);
  }
  assert.equal(read(cfg, 'CLAUDE.md'), ORIGINAL);
  assert.equal(fs.existsSync(path.join(cfg, 'skills')), false);
});

test('command line installs into CLAUDE_CONFIG_DIR and removes with --remove', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL });
  const run = (...args) => spawnSync(process.execPath, [path.join(HERE, 'install-intent.mjs'), ...args],
    { env: { ...process.env, CLAUDE_CONFIG_DIR: cfg }, encoding: 'utf8' });
  const install = run();
  assert.equal(install.status, 0, install.stderr);
  assert.match(install.stdout, /새 대화부터 적용됩니다/);
  assert.ok(fs.existsSync(path.join(cfg, 'skills/capture-intent/SKILL.md')));
  const remove = run('--remove');
  assert.equal(remove.status, 0, remove.stderr);
  assert.equal(fs.existsSync(path.join(cfg, 'skills/capture-intent')), false);
  assert.equal(read(cfg, 'CLAUDE.md'), ORIGINAL);
});
