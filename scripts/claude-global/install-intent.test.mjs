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
  applyBlock, removeBlock, START, END, validateSource, installIntent, removeIntent,
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

test('validateSource rejects a template missing a section', (t) => {
  const dir = copySourceToTemp(t);
  replaceIn(dir, 'skills/capture-intent/template.md', '## 확인 방법', '## 기타');
  assert.throws(() => validateSource(dir), /template\.md/);
});

test('installIntent copies the skill and keeps the old CLAUDE.md in the backup', (t) => {
  const cfg = tempConfig(t, { 'CLAUDE.md': ORIGINAL });
  const { backupDir } = installIntent({ configDir: cfg, sourceDir: HERE, stamp: '2026-10-02-0900' });
  for (const f of ['SKILL.md', 'template.md']) {
    assert.equal(read(cfg, `skills/capture-intent/${f}`), read(HERE, `skills/capture-intent/${f}`));
  }
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
